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

#ifndef prunixos_h___
#define prunixos_h___
/*
 * OS-specific defines common to all known Unixes.
 */

#define DIRECTORY_SEPARATOR	'/'
#define DIRECTORY_SEPARATOR_STR	"/"
#define PATH_SEPARATOR		':'

#include <unistd.h>
#include <stdlib.h>

#define GCPTR
#define CALLBACK
typedef int (*FARPROC)();
#define gcmemcpy(a,b,c) memcpy(a,b,c)

#define FlipSlashes(a,b)

#ifdef SUNOS4
#include "sunos4.h"
#endif

typedef int PROSFD;

#endif /* prunixos_h___ */
