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

#ifndef _RESDEF_H_
#define _RESDEF_H_

#include "xp_core.h"

#define RES_OFFSET 7000


#ifndef RESOURCE_STR

#ifdef WANT_ENUM_STRING_IDS

#define RES_START
#if defined(XP_WIN) && _MSC_VER == 1100
/* VC50 has a bug, where large enumerations cause an
 *  internal compiler error.  Do some hack here to fix without
 *  breaking the other platforms.
 */
#define BEGIN_STR(arg)
#define ResDef(name,id,msg) enum { name=id };
#define END_STR(arg)
#else /* XP_WIN _MSC_VER */
#define BEGIN_STR(arg)   enum {
#define ResDef(name,id,msg)	name=id,
#if defined(XP_WIN)
#define END_STR(arg)	};
#else
#define END_STR(arg)     arg=0 };
#endif
#endif /* XP_WIN _MSC_VER */

#else /* WANT_ENUM_STRING_IDS */

#define RES_START
#define BEGIN_STR(arg)
#ifdef XP_WIN16
/*  Get these ints out of DGROUP (/Gt3 compiler switch)
 *      so we can increase the stack size
 */
#define ResDef(name,id,msg)	int __far name = (id);
#else
#define ResDef(name,id,msg)	int name = (id);
#endif
#define END_STR(arg)

#endif /* WANT_ENUM_STRING_IDS */

#else  /* RESOURCE_STR, the definition here is for building resources */
#if defined(XP_WIN) || defined(XP_OS2)

#ifndef MOZILLA_CLIENT
#define RES_START
#define BEGIN_STR(arg) static char * (arg) (int16 i) { switch (i) {
#define ResDef(name,id,msg)	case (id)+RES_OFFSET: return (msg);
#define END_STR(arg) } return NULL; }
#else /* MOZILLA_CLIENT */
#define RES_START STRINGTABLE DISCARDABLE
#define BEGIN_STR(arg)  BEGIN
#define ResDef(name,id,msg)	id+RES_OFFSET msg
#define END_STR(arg)   END
#endif /* not MOZILLA_CLIENT */

#elif defined(XP_MAC)
	/* Do nothing -- leave ResDef() to be perl'ized via MPW */
#define ResDef(name,id,msg)	ResDef(name,id,msg)

#elif defined(XP_UNIX)
#ifdef RESOURCE_STR_X
#define RES_START
#define BEGIN_STR(arg) static char *(arg)(void) {
#define ResDef(name,id,msg)	output((id)+RES_OFFSET, (msg));
#define END_STR(arg) }
#else
#define RES_START
#define BEGIN_STR(arg) static char *(arg)(int16 i) { switch (i) {
#define ResDef(name,id,msg)	case (id)+RES_OFFSET: return (msg);
#define END_STR(arg) } return NULL; }
#endif /* RESOURCE_STR_X */
#endif   /*  XP_WIN  */
#endif /* RESOURCE_STR   */


#endif /* _RESDEF_H_ */
