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

#ifndef EudoraDebugLog_h___
#define EudoraDebugLog_h___

#ifdef NS_DEBUG
#define IMPORT_DEBUG	1
#endif

// Use PR_LOG for logging.
#include "prlog.h"
extern PRLogModuleInfo *EUDORALOGMODULE;  // Logging module

#define IMPORT_LOG0(x)          PR_LOG(EUDORALOGMODULE, PR_LOG_DEBUG, (x))
#define IMPORT_LOG1(x, y)       PR_LOG(EUDORALOGMODULE, PR_LOG_DEBUG, (x, y))
#define IMPORT_LOG2(x, y, z)    PR_LOG(EUDORALOGMODULE, PR_LOG_DEBUG, (x, y, z))
#define IMPORT_LOG3(a, b, c, d) PR_LOG(EUDORALOGMODULE, PR_LOG_DEBUG, (a, b, c, d))



#endif /* EudoraDebugLog_h___ */
