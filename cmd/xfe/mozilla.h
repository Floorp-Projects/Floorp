/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#ifndef HAVE_SYSERRLIST
#define HAVE_SYSERRLIST
#endif
#endif

#endif /* __xfe_mozilla_h_ */

