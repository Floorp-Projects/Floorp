/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Compute the log of the least power of 2 greater than or equal to n.
 */
#include "prtypes.h"

PR_PUBLIC_API(int32)
PR_CeilingLog2(uint32 n)
{
    int32 log2;
    uint32 t;

    if (n == 0)
	return -1;
    log2 = (n & (n-1)) ? 1 : 0;
    if ((t = n >> 16) != 0)
	log2 += 16, n = t;
    if ((t = n >> 8) != 0)
	log2 += 8, n = t;
    if ((t = n >> 4) != 0)
	log2 += 4, n = t;
    if ((t = n >> 2) != 0)
	log2 += 2, n = t;
    if (n >> 1)
	log2++;
    return log2;
}
