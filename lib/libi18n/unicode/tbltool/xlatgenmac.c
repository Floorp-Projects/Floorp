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

void GenMap(char* name1, char* name2, 
	unsigned short array1[], unsigned short array2[])
{
	int i,j,found;

	printf("/*     Translation %s -> %s   */\n",name1, name2);
	ReportUnmap(array1,array2);
	printf("/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */\n");
        for(i=0;i<256;i++)
        {
		if((i%16) == 0)
			printf("/*%Xx*/  $\"",i/16);
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
                {
			printf("%2X",i);
                }
		if((i%16) == 15)
			printf("\"\n");
		else if(i%2)
			printf(" ");
        }
}
