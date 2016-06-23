#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024

int main(int argc, char **argv)
{
    char buffer[BUFFER_SIZE];
    bzero((void*)buffer, sizeof(char) * BUFFER_SIZE);

    int count = 0;
    char* bufferptr[BUFFER_SIZE];
    memset(bufferptr, 0, sizeof(char*) * BUFFER_SIZE);

    int res = read(STDIN_FILENO, (void*)buffer, (size_t)BUFFER_SIZE);

    if (res == -1) {
        printf("Reading from STDIN is fail");
        return 1;
    }

    if (res == BUFFER_SIZE && buffer[BUFFER_SIZE - 1] != '\0') {
        printf("Buffer overflow");
        return 1;
    }

    // Создаём (если отсутствует), зануляем (если присутствует) и открываем файл для вывода.
    int fd = open("/home/kisselev/Kurs/5/542/shell/result.out", O_WRONLY | O_CREAT | O_TRUNC, 0666);

    // Уходим, если ничего не прочитали.
    if (res == 0) {
        //close(fd);
        return 0;
    }

    if (buffer[res - 1] == '\n') {
        buffer[--res] = '\0';
    }

////------ Test -------------------------
//    write(fd, buffer, res);
//    close(fd);

//    return 0;
////-------------------------------------

    int j = 0;
    while (j < res) {
        // Проматываем служебные символы
        while ((j < res && buffer[j] != '\0') && (buffer[j] == ' ' || buffer[j] == '\t' || buffer[j] == '|')) {
            j++;
        }

        // Прерываемся, если дошли до конца.
        if (j == res || buffer[j] == '\0') {
            break;
        }

        // Запоминаем адрес очередной команды.
        bufferptr[count] = buffer + j;
        count++;

        // Ищем конец команды
        while (j < res && buffer[j] != '|' && buffer[j] != '\0') {
            j++;
        }

        if (j < res && buffer[j] == '|') {
            buffer[j] = '\0';
            j++;
        }

        // Прерываемся, если дошли до конца.
        if (j == res || buffer[j] == '\0') {
            break;
        }
    }

    // Уходим, если не нашли команд.
    if (count == 0) {
        //close(fd);
        return 0;
    }

////------ Test -------------------------
//    bufferptr[count] = buffer + res;
//    int l;
//    for (l = 0; l < count; l++) {
//        write(fd, bufferptr[l], (bufferptr[l+1] - bufferptr[l]));
//    }
//    close(fd);
//    return 0;
////------------------------------------

    int pfd[2];
    int pid;
    int i;

    // Запускаем цепочку процессов по количеству команд (минимум 1 команда).
    for (i = 0; i < count; i++) {

        // Начинаем создавать каналы для связи, если команд больше 1.
        if (i != 0 && count > 1) {
            pipe(pfd);
        }

        // Запоминаем pid текущего процесса.
        /*pid = getpid();*/

        // Создаём дочерний процесс.
        pid = fork();
/*
 * Дочерний процесс.
 *      i: (_0_) - дочерний-1 (3-я команда); (_1_) - дочерний-2 (2-я команда); (_2_) - дочерний-3 (1-я команда).
 * Направление fork():
 *      основной   ---> дочерний-1(in)---(out)(pipe-1)(in)--->(out)дочерний-2(in)---(out)(pipe-2)(in)--->(out)дочерний-3.
 * Направление команд:
 *      файл(in)<---(out)команда-3(in)<---(out)(pipe-1)(in)---(out)команда-2(in)<---(out)(pipe-2)(in)---(out)команда-1.
*/
        if (pid == 0/*getppid()*/) {
            // Привязали stdout первого дочернего процесса (последняя выполняемая команда)
            // к файловому дескриптору открытого файла на запись.
            if (i == 0) {
                close(STDOUT_FILENO);
                dup2(fd, STDOUT_FILENO);

            // Привязали stdout дочернего процесса к файловому дескриптору нового канала на запись.
            } else {
                close(STDOUT_FILENO);
                dup2(pfd[1], STDOUT_FILENO);
                close(pfd[1]);
            }
            close(fd);

            // Закрываем stdin канала у последнего дочернего процесса (первая выполняемая команда).
            if (i == (count - 1) && count > 1) { // он не будет родителем процесса, но будет родителем команд
                //close(STDIN_FILENO);
                close(pfd[0]);
            }
/*
 * Текущий (родительский) процесс.
 *      i: (0) - основной (нет команд), (1) - дочерний-1 (3-я команда); (2) - дочерний-2  (2-я команда);
*/
        } else {
            if (i != 0) {
                // Привязали stdin текущего процесса к каналу на чтение.
                close(STDIN_FILENO);
                dup2(pfd[0], STDIN_FILENO);
                close(pfd[0]);
            } else {
                close(fd);
            }

            close(pfd[1]);

            // Выходим из цикла ждать окончания дочернего процесса.
            break;
        }
    }

    // Ждём завершения дочернего процесса, если они есть.
    if (i < count) {
        int status;
        wait(&status);
    }

    // Завершаем основной процесс.
    if (i == 0) {
        return 0;
    }

    char *flag = bufferptr[count - i];

    // Определяем длину строки команды.
    bufferptr[count] =  buffer + res;
    int len = bufferptr[count - (i - 1)] - bufferptr[count - i];

    // Буфер команды.
    char cmd[BUFFER_SIZE];
    memset(cmd, 0, sizeof(char) * BUFFER_SIZE);
    int k = 0;

    // Массив параметров.
    char *par[BUFFER_SIZE];
    memset(par, 0, sizeof(char*) * BUFFER_SIZE);
    int p = 0;

    while (k < len) {
        // Запоминаем адрес текущего параметра.
        par[p] = cmd + k;
        while (k < len && *flag != ' ' && *flag != '\t' && *flag != '|' && *flag != '\0') {
            cmd[k] = *flag;
            k++;
            flag++;
        }
        if (k < len) {
            cmd[k] = '\0';
            k++;
            len++;
        }

        // Вставляем в текущий конец массива параметров NULL.
        p++;
        par[p] = NULL;

        // Проматываем служебные символы
        while ((k < len && *flag != '|' && *flag != '\0') && (*flag == ' ' || *flag == '\t')) {
            len--;
            flag++;
        }

        // Добрались до конца команды.
        if (k == len || *flag == '|' || *flag == '\0') {
            break;
        }
    }

    // Запускаем соответствующую команду в текущем процессе, кроме основного.
    execvp(par[0], par);
}
