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

#ifndef nspr_sunos_defs_h___
#define nspr_sunos_defs_h___

#undef	HAVE_LONG_LONG
#define HAVE_ALIGNED_DOUBLES
#define HAVE_ALIGNED_LONGLONGS
#define HAVE_DLL

#define NEED_TIME_R
#define USE_DLFCN

/*
** Missing function prototypes
*/

extern int socket (int domain, int type, int protocol);
extern int getsockname (int s, struct sockaddr *name, int *namelen);
extern int accept (int s, struct sockaddr *addr, int *addrlen);
extern int listen (int s, int backlog);
extern int brk(void *);
extern void *sbrk(int);

#endif /* nspr_sunos_defs_h___ */
