/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This file should be the first Mcom file included
 *
 * All cross-platform definitions, regardless of project, should be
 *  contained in this file or its includes
 */

#ifndef _MCOM_H_
#define _MCOM_H_

#include "prtypes.h"
#include <time.h>

#ifdef XP_MAC
PR_BEGIN_EXTERN_C

extern char * strdup(const char *);
extern time_t GetTimeMac();
extern time_t Mactime(time_t *timer);
extern struct tm *Macgmtime(const time_t *timer);
extern time_t Macmktime (struct tm *timeptr);
extern char * Macctime(const time_t *);
extern struct tm *Maclocaltime(const time_t *);

PR_END_EXTERN_C

#define XP_TIME()	GetTimeMac()
#define time(t)		Mactime(t)
#define gmtime(t)	Macgmtime(t)
#define mktime(t)	Macmktime(t)
#define ctime(t)	Macctime(t)
#define localtime(t)	Maclocaltime(t)
#define UNIXMINUSMACTIME 2082844800UL
#else
#define XP_TIME()	time(0)
#endif

#endif /* _MCOM_H_ */
