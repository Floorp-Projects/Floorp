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

#ifndef _HPPA_FUNCS_H_
#define _HPPA_FUNCS_H_

#include "prtypes.h"


#if defined(hppa)
#define HPPAFuncAddress(func) (func)
#else
#define HPPAFuncAddress(func) NULL
#endif /* defined(hppa) */

PR_BEGIN_EXTERN_C

extern PRUint32* HPPASpecialCodeBegin;
extern PRUint32* HPPASpecialCodeEnd;

extern PRUint32* HPPAremI;
extern PRUint32* HPPAremU;
extern PRUint32* HPPAdivI;
extern PRUint32* HPPAdivU;

PR_END_EXTERN_C

#endif /* _HPPA_FUNCS_H_ */
