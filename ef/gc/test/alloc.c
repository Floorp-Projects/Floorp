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
                            Author: Warren Harris
*******************************************************************************/

#include "sm.h"
#include "smgen.h"
#include <stdio.h>

void
foo(unsigned int num)
{
    unsigned int bin1, bin2;
    SM_SIZE_BIN(bin1, num);
    SM_GET_ALLOC_BUCKET(bin2, num);
    printf("num = %-10u bin1 = %2d bin2 = %2d\n",
           num, bin1, bin2);
}

int
main()
{
    int i;
    for (i = 2; i < 32; i++) {
        unsigned int b = 1 << i;
        unsigned int p = 1 << (i - 1);

        printf("--- %d\n", i);
        foo(b - 1);
        foo(b);
        foo(b + 1);
        foo(b + p - 1);
        foo(b + p);
        foo(b + p + 1);
    }
}
