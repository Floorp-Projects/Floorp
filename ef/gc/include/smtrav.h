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

#ifndef __SMTRAV__
#define __SMTRAV__

#include "smpriv.h"
#include "smobj.h"
#include "smgen.h"

SM_BEGIN_EXTERN_C

/******************************************************************************/

typedef PRWord
(*SMTraverseFieldProc)(SMObject* field, PRUword fieldOffset, void* closure);

SM_EXTERN(PRWord)
SM_TraverseObjectFields(SMObject* obj,
                        SMTraverseFieldProc traverseProc, void* traverseClosure);

typedef PRWord
(*SMTraverseObjectProc)(void* obj, SMSmallObjCount index, 
                        PRUword objSize, void* closure);

SM_EXTERN(PRWord)
SM_TraversePageObjects(SMPage* page,
                       SMTraverseObjectProc traverseProc, void* traverseClosure);

typedef PRWord
(*SMTraversePageProc)(SMPage* page, void* closure);

SM_EXTERN(PRWord)
SM_TraverseGenPages(SMGenNum genNum,
                    SMTraversePageProc traverseProc, void* traverseClosure);

SM_EXTERN(PRWord)
SM_TraverseGenObjects(SMGenNum genNum,
                      SMTraverseObjectProc traverseProc, void* traverseClosure);

SM_EXTERN(PRWord)
SM_TraverseAllPages(SMTraversePageProc traverseProc, void* traverseClosure);

SM_EXTERN(PRWord)
SM_TraverseAllObjects(SMTraverseObjectProc traverseProc, void* traverseClosure);

/******************************************************************************/

#ifdef SM_DUMP	/* define this if you want dump code in the final product */

#include <stdio.h>

typedef enum SMDumpFlag {
    SMDumpFlag_None     = 0,
    SMDumpFlag_Detailed = 1, /* adds hex dump of objects */
    SMDumpFlag_GCState  = 2, /* adds dump of unmarked and forwarded nodes */
    SMDumpFlag_Abbreviate = 4   /* only prints the beginning of large objects */
} SMDumpFlag;

SM_EXTERN(void)
SM_DumpMallocHeap(FILE* out, PRUword flags);

SM_EXTERN(void)
SM_DumpGCHeap(FILE* out, PRUword flags);

SM_EXTERN(void)
SM_DumpPage(FILE* out, void* addr, PRUword flags);

SM_EXTERN(void)
SM_DumpHeap(FILE* out, PRUword flags);

SM_EXTERN(void)
SM_DumpPageMap(FILE* out, PRUword flags);

#endif /* SM_DUMP */

/******************************************************************************/

SM_END_EXTERN_C

#endif /* __SMTRAV__ */
/******************************************************************************/
