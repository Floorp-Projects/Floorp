/* Convert a hex-encoded byte stream to a raw byte stream. */

#include <stdio.h>

int hexval(int c) {
    if (c <= '9') return c - '0';
    if (c <= 'F') return c - 'A' + 10;
    return c - 'a' + 10;
}

int main(int argc, char** argv)
{
    int c;
    while ((c = getchar()) != EOF) {
	int d = getchar();
	putchar((hexval(c) << 4) | hexval(d));
    }
}
