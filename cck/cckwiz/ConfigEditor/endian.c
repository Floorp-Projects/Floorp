#include	<stdio.h>
#include	<stdlib.h>

#define	MD5_WORD	unsigned int

union	{
	char	bytes[4];
	MD5_WORD n;
	} u;

void main()
{
	u.n=0x03020100;
	if (u.bytes[0] == 3)
		printf("#define	MD5_BIG_ENDIAN\n");
	else if (u.bytes[0] == 0)
		printf("#define	MD5_LITTLE_ENDIAN\n");
	else
	{
		printf("#error No endians!\n");
		exit(1);
	}
	exit (0);
}
