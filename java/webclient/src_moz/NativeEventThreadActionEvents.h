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
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Ed Burns <edburns@acm.org>
 */

/*
 * nsActions.h
 */

#ifndef NativeEventThreadActionEvents_h___
#define NativeEventThreadActionEvents_h___

#include "org_mozilla_webclient_impl_wrapper_0005fnative_NativeEventThread.h"
#include "CBrowserContainer.h"

#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsActions.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsCWebBrowser.h"
#include "nsISHistory.h"

#ifdef XP_UNIX
#include <unistd.h>
#include "gdksuperwin.h"
#include "gtkmozarea.h"
#endif

#ifdef XP_PC
#include <windows.h>
#endif

struct NativeBrowserControl;

class wsRealizeBrowserEvent : public nsActionEvent {
public:
                        wsRealizeBrowserEvent (NativeBrowserControl * yourInitContext);
                       ~wsRealizeBrowserEvent ();
        void    *       handleEvent    (void);

protected:
	NativeBrowserControl * mInitContext;
};

#endif /* NativeEventThreadActionEvents_h___ */

// EOF
