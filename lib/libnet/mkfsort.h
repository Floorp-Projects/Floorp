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

#ifndef MKFSORT_H
#define MKFSORT_H

#include "mksort.h"

#include <time.h>

#ifdef XP_UNIX
#include <sys/types.h>
#endif /* XP_UNIX */

extern void NET_FreeEntryInfoStruct(NET_FileEntryInfo *entry_info);
extern NET_FileEntryInfo * NET_CreateFileEntryInfoStruct (void);
extern int NET_CompareEntryInfoStructs (void *ent1, void *ent2);
extern void NET_DoFileSort (SortStruct * sort_list);

#endif /* MKFSORT_H */
