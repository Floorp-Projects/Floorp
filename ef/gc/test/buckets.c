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

static void
foo(unsigned int size)
{
    PRUword bin, bucket;
    SM_SIZE_BIN(bin, size);
    SM_GET_ALLOC_BUCKET(bucket, size);
    printf("size = %-10u bin = %2d, bucket = %2d\n",
           size, bin, bucket);
}

int
main()
{
    int i;
    for (i = SM_MIN_SMALLOBJECT_BITS-1; i <= SM_MAX_SMALLOBJECT_BITS; i++) {
        unsigned int b = 1 << i;
        unsigned int p = 1 << (i - 1);

        printf("--- %d\n", i);
        foo(b - 1);
        foo(b);
        if (i < SM_MAX_SMALLOBJECT_BITS) {
            foo(b + 1);
            foo(b + p - 1);
            foo(b + p);
            foo(b + p + 1);
        }
    }
    printf("page size=%u max=%u min=%u\n", 
           SM_PAGE_SIZE, SM_MAX_SMALLOBJECT_SIZE, SM_MIN_SMALLOBJECT_SIZE);
    printf("p2buckets=%u midsize=%u firstMid=%u buckets=%u\n",
           SM_POWER_OF_TWO_BUCKETS, SM_MIDSIZE_BUCKETS, 
           SM_FIRST_MIDSIZE_BUCKET, SM_ALLOC_BUCKETS);
}
