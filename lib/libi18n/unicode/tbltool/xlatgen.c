/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*
	single filename1 filename2 
*/
#include <stdio.h>
#include <stdlib.h>
/*
#define DEBUG 1
*/
#ifdef DEBUG
#define Trace(a)	{fprintf(stderr,"Trace: %s\n",a); fflush(stderr); }
#else
#define Trace(a)
#endif

static char *name1;
static char *name2;

static unsigned short array1[256],array2[256];

void TraceNum(int a)
{
	char buf[20];
	sprintf(buf, "[%X]",a);
	Trace(buf);
}
void usage()
{
	fprintf(stderr,"Usage: xlatgen filename1 filename2 csid1 csid2\n");
	exit(-1);
}
void InitArray(unsigned short array[])
{
	int i;
	for(i=0;i<32;i++)
		array[i]= i;
	for(i=32;i<256;i++)
		array[i]= 0xFFFD;
	array[0x7F]= 0x7F;
	array[0xFF]= 0xFF;
}
void TraceArray(char* name,unsigned short array[])
{
	int i,j;
	char buf[128];
	Trace(name);
	for(i=0;i<256;i+=16)
	{
		sprintf(buf,"0x%2X: ",i);
		for(j=0;j<16;j++)
		{
			sprintf(buf,"%s %4X",buf, array[i+j]);
		}
		Trace(buf);
	}
}
void Quit(char* str)
{
	Trace(str);
	exit(-1);
}
void ReadArray(char* name,unsigned short array[])
{
	int i;
	char buf[80];
	FILE  *fd;
	fd = fopen(name, "r");

	if(fd == NULL)
		Quit("Cannot open file\n");
	Trace("File open ok");
	while(fgets(buf,80,fd))
	{
		if(buf[0] != '#')
		{
			int from;
			int to;
			sscanf(buf,"%x %x", &from, &to);
			array[(from & 0x00FF)] = (to & 0x0000FFFF);
		}
	}
	fclose(fd);
}
void ReportUnmap( unsigned short array1[], unsigned short array2[])
{
	int i,j,found;
	int k;
	k=0;
	for(i=0;i<256;i++)
	{
		for(found=0,j=0;j<256;j++)
		{
			if(array1[i] == array2[j])
			{	
				found = 1;
				break;
			}
		}
		if(found == 0)
		{
			printf("/* %2X is unmap !!! */\n", i);
			k++;
		}
	}
	if(k!=0)
	{
		printf("/* There are total %d character unmap !! */\n",k);
	}
}
void GenMap(char* name1, char* name2,  char* csid1, char* csid2,
	unsigned short array1[], unsigned short array2[])
{
	int i,j,found;

	printf("%s_TO_%s RCDATA\nBEGIN\n",csid1, csid2);
	printf("/*     Translation %s -> %s   */\n",name1, name2);
	ReportUnmap(array1,array2);
        for(i=0;i<256;i+=2)
        {
		if((i%16) == 0)
			printf("/*%Xx*/  ",i/16);
		printf("0x");

                for(found=0,j=0;j<256;j++)
                {
                        if(array1[i+1] == array2[j])
                        {
				printf("%02X",j);
                                found = 1;
                                break;
                        }
                }
                if(found == 0)
			printf("%2X",i+1);

                for(found=0,j=0;j<256;j++)
                {
                        if(array1[i] == array2[j])
                        {
				printf("%02X",j);
                                found = 1;
                                break;
                        }
                }
                if(found == 0)
			printf("%2X",i);

		printf(", ");
		if((i%16) == 14)
			printf("\n");
        }
	printf("END /* End of %s_To_%s */\n\n", csid1, csid2);
}
main(int argc, char* argv[])
{

	if(argc!=5)
		usage();

	InitArray(array1);
	InitArray(array2);

	Trace(argv[1]);
	ReadArray(argv[1],array1);
	TraceArray(argv[1],array1);

	Trace(argv[2]);
	ReadArray(argv[2],array2);
	TraceArray(argv[2],array2);

	GenMap(argv[1], argv[2], argv[3], argv[4],array1,array2 );
	printf("\n\n\n\n\n");
	GenMap(argv[2], argv[1], argv[4], argv[3],  array2,array1 );
}
