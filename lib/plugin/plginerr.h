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

#ifndef _PLGINERR_H_
#define _PLGINERR_H_
/* plginerr.h - Defines the error interface between the Nav and its plugin */

#if !defined(XP_OS2) && !defined(OS2)
#include <windows.h>
#endif

#include "npapi.h"

/* error handling */
#define NPERR_BASE                      0
#define NPERR_NO_ERROR                  (NPERR_BASE + 0)
#define NPERR_GENERIC_ERROR             (NPERR_BASE + 1)
#define NPERR_INVALID_INSTANCE_ERROR    (NPERR_BASE + 2)
#define NPERR_INVALID_FUNCTABLE_ERROR   (NPERR_BASE + 3)
#define NPERR_MODULE_LOAD_FAILED_ERROR  (NPERR_BASE + 4)
#define NPERR_OUT_OF_MEMORY_ERROR       (NPERR_BASE + 5)
#define NPERR_INVALID_PLUGIN_ERROR      (NPERR_BASE + 6)
#define NPERR_INVALID_PLUGIN_DIR_ERROR  (NPERR_BASE + 7)

/* option handling */
#define NPOPT_NO_OPTION             0
#define NPOPT_CAN_SEEK              (1 << 1)

typedef unsigned long NPOption; /* consts def'd as bit positions */

/* reason handling */
#define NPRES_BASE                  0
#define NPRES_NETWORK_ERR           (NPRES_BASE + 0)
#define NPRES_USER_BREAK            (NPRES_BASE + 1)
#define NPRES_DONE                  (NPRES_BASE + 3)


#endif // _PLGINERR_H_

