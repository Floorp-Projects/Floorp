/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   mozilla.h --- generic includes for the X front end.
   Created: Jamie Zawinski <jwz@netscape.com>, 22-Jun-94.
 */


#ifndef __xfe_mozilla_h_
#define __xfe_mozilla_h_

#include "xp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <memory.h>
#include <time.h>

#if defined(__linux)
extern int putenv (const char *);
#endif

#if defined(LINUX) && defined(__GLIBC__) && (__GLIBC__ >= 2)
#define LINUX_GLIBC_2
#endif

#endif /* __xfe_mozilla_h_ */

