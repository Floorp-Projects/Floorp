/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; -*-  
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


/*******************************************************************************
                            S P O R T   M O D E L    
                              _____
                         ____/_____\____
                        /__o____o____o__\        __
                        \_______________/       (@@)/
                         /\_____|_____/\       x~[]~
             ~~~~~~~~~~~/~~~~~~~|~~~~~~~\~~~~~~~~/\~~~~~~~~~

    Advanced Technology Garbage Collector
    Copyright (c) 1997 Netscape Communications, Inc. All rights reserved.
    Recovered by: Warren Harris
*******************************************************************************/

#include "sm.h"
#include "smgen.h"
#include <stdio.h>
#include <time.h>

#define ITERATIONS (10*1024*1024)
#define SIZE       1024
int divTable[SIZE];

int
main(void)
{
    int i, j;
    clock_t t1, t2, te;
    int ans1[SIZE], ans2[SIZE];
    fprintf(stdout, "SportModel: Division algorithm timing analysis test\n");
    
    t1 = clock();
    for (i = 0, j = 0; i < ITERATIONS; i++) {
        ans1[SIZE - 1 - j] = j;
        if (++j == SIZE) j = 0;
    }
    t2 = clock();
    te = t2 - t1;
    
    fprintf(stdout, "empty loop = %ldms\n", te * 1000 / CLOCKS_PER_SEC);
    
    t1 = clock();
    i = ITERATIONS;
    SM_UNROLLED_WHILE(i, {
        ans1[SIZE - 1 - j] = j;
        if (++j == SIZE) j = 0;
    });
    t2 = clock();
    
    fprintf(stdout, "unrolled loop = %ldms\n", (t2 - t1) * 1000 / CLOCKS_PER_SEC);
    
    t1 = clock();
    for (i = 0, j = 0; i < ITERATIONS; i++) {
        ans1[SIZE - 1 - j] = j / 12;
        if (++j == SIZE) j = 0;
    }
    t2 = clock();
    
    fprintf(stdout, "div loop = %ldms\n", (t2 - t1 - te) * 1000 / CLOCKS_PER_SEC);
    
    /* initialize divTable */
    for (j = 0; j < SIZE; j++) {
        divTable[j] = j / 12;
    }

    t1 = clock();
    for (i = 0, j = 0; i < ITERATIONS; i++) {
        ans1[SIZE - 1 - j] = divTable[j];
        if (++j == SIZE) j = 0;
    }
    t2 = clock();
    
    fprintf(stdout, "lookup loop = %ldms\n", (t2 - t1 - te) * 1000 / CLOCKS_PER_SEC);

    t1 = clock();
    for (i = 0, j = 0; i < ITERATIONS; i++) {
        ans2[SIZE - 1 - j] = ((j + 1) * 21845) >> 18;
        if (++j == SIZE) j = 0;
    }
    t2 = clock();
    
    fprintf(stdout, "rad loop = %ldms\n", (t2 - t1 - te) * 1000 / CLOCKS_PER_SEC);

#if 0
    for (i = 0; i < SIZE; i++) {
        fprintf(stdout, "%8d %8d %8d (%8d) %8d %8d\n",
                i, ans1[i], ans2[i], (ans1[i] - ans2[i]), divTable[i],
                SM_DIV(i, SM_FIRST_MIDSIZE_BUCKET));
    }
#endif
}

/******************************************************************************/
