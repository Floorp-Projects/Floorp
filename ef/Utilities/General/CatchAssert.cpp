/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <stdio.h>
#include <stdlib.h>
#include "CatchAssert.h"
#include "CatchAssert_md.h"
#include "JavaVM.h"

#ifdef _WIN32
/* 
 * catchAssert
 *
 * This function replaces the existing assert function.  
 * If the catchHardwareExceptions flag is true, then we just
 * print the assert to stdout.  If the flag is false, then 
 * we call the regular assert function for each platform.
 *
 */
NS_EXTERN void __cdecl catchAssert(void *exp, void *filename, unsigned linenumber) {

    if (VM::theVM.getCatchHardwareExceptions()) {
    
        /* Print the assert. */    
        printf("STATUS:ASSERTION.\n");
        printf("File: %s\n", filename);
        printf("Linenumber: %i\n", linenumber);

        exit(1);

    } else {

        /* Call the regular assert. */    
        throwAssert(exp, filename, linenumber);

    }

}
#endif

