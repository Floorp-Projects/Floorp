/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns <edburns@acm.org>
 *                 Ashutosh Kulkarni <ashuk@eng.sun.com>
 *
 */


/**

 * Global data

 */

#include "nsIWebShell.h" // for nsIWebShell
#include "nsIEventQueueService.h" // for PLEventQueue


#ifndef ns_globals_h
#define ns_globals_h

#include "prlog.h"
extern PRLogModuleInfo *prLogModuleInfo; // defined in WrapperFactoryImpl.cpp

extern const char *gImplementedInterfaces[]; // defined in WrapperFactoryImpl.cpp

typedef enum {
    WINDOW_CONTROL_INDEX = 0,
    NAVIGATION_INDEX,
    CURRENT_PAGE_INDEX,
    HISTORY_INDEX,
    EVENT_REGISTRATION_INDEX,
    BOOKMARKS_INDEX,
    PREFERENCES_INDEX,
    PROFILE_MANAGER_INDEX
} WEBCLIENT_INTERFACES;


/**

 * Lifetime:

 * Lazily created in NativeEventThread.cpp InitMozillaStuff().

 * Set to nsnull in WrapperFactoryImpl.cpp nativeTerminate().

 */

class nsISHistory;
extern nsISHistory *gHistory; // defined in NativeEventThread.cpp

#endif // ns_globals_h
