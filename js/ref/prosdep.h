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

#ifndef prosdep_h___
#define prosdep_h___
/*
 * OS (and machine, and compiler XXX) dependent information.
 */

#ifdef XP_PC
#include "prpcos.h"
#ifdef _WIN32
#include "os/win32.h"
#else
#include "os/win16.h"
#endif
#endif /* XP_PC */

#ifdef XP_MAC
#include "prmacos.h"
#endif

#ifdef XP_UNIX
#include "prunixos.h"

/* Get endian-ness */
#include "prcpucfg.h"

/*
 * Hack alert!
 */
extern void PR_SetPollHook(int fd, int (*func)(int));

/*
 * Get OS specific header information.
 */
#if defined(AIXV3)
#include "os/aix.h"

#elif defined(BSDI)
#include "os/bsdi.h"

#elif defined(HPUX)
#include "os/hpux.h"

#elif defined(IRIX)
#include "os/irix.h"

#elif defined(LINUX)
#include "os/linux.h"

#elif defined(OSF1)
#include "os/osf1.h"

#elif defined(SCO)
#include "os/scoos.h"

#elif defined(SOLARIS)
#include "os/solaris.h"

#elif defined(SUNOS4)
#include "os/sunos.h"

#elif defined(UNIXWARE)
#include "os/unixware.h"
#endif

#endif /* XP_UNIX */

#endif /* prosdep_h___ */
