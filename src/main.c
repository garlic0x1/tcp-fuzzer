/* For sockaddr_in */
#include <netinet/in.h>
/* For socket functions */
#include <sys/socket.h>
/* For gethostbyname */
#include <netdb.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#define MAXLINE (size_t)1024

char *host = NULL;
char *path = NULL;
char *wordlist = NULL;

void print_help_and_die()
{
	fprintf(stderr, "Usage: tcp-fuzzer [-wu] [file...]\n");
	exit(EXIT_FAILURE);
}

int get_url(char *host, char *path)
{
	char query[MAXLINE];
	sprintf(query, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, host);

	struct sockaddr_in sin;
	struct hostent *h;
	const char *cp;
	int fd;
	ssize_t n_written, remaining;
	char buf[MAXLINE];

	/* Lookup host (maybe not threadsafe?) */
	h = gethostbyname(host);
	if (!h || h->h_addrtype != AF_INET) {
		fprintf(stderr, "Couldn't lookup %s: %s", host,
			hstrerror(h_errno));
		return 1;
	}

	/* Allocate a new socket */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return 1;
	}

	/* Connect to the remote host. */
	sin.sin_family = AF_INET;
	sin.sin_port = htons(80);
	sin.sin_addr = *(struct in_addr *)h->h_addr;
	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin))) {
		perror("connect");
		close(fd);
		return 1;
	}

	/* Write the query. */
	cp = query;
	remaining = strlen(query);
	while (remaining) {
		n_written = send(fd, cp, remaining, 0);
		if (n_written <= 0) {
			perror("send");
			return 1;
		}
		remaining -= n_written;
		cp += n_written;
	}

	/* Read the response. */
	while (1) {
		ssize_t result = recv(fd, buf, sizeof(buf), 0);
		if (result == 0) {
			break;
		} else if (result < 0) {
			perror("recv");
			close(fd);
			return 1;
		}
		fwrite(buf, 1, result, stdout);
		printf("\n");
		break;
	}

	close(fd);
	return 0;
}

int read_words(void (*f)(char *, int))
{
	FILE *fd = fopen(wordlist, "r");

	size_t read_size = MAXLINE;
	char *line[read_size];
	int nread;

	while (!feof(fd)) {
		nread = getline(line, &read_size, fd);
		f(*line, nread);
	}

	return 0;
}

void handle_word(char *word, int len)
{
	get_url(host, word);
}

int main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "w:h:p:")) != -1) {
		switch (opt) {
		case 'w':
			wordlist = optarg;
			break;
		case 'h':
			host = optarg;
			break;
		case 'p':
			path = optarg;
			break;
		default:
			print_help_and_die();
		}
	}

	if (!host || !wordlist)
		print_help_and_die();

	read_words(handle_word);
}
