/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s): 
 */

// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently


#include "xp_core.h"
#include "jscompat.h"

#include "prthread.h"
#include "prprf.h"
#include "plevent.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "nsViewsCID.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsICookieService.h"

#include "nsIPref.h"
#include "nsIURL.h"
#include "nsIBaseWindow.h"
#include "nsIWebBrowser.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserSetup.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIDocumentLoader.h"
#include "nsIContentViewer.h"
#include "nsISearchContext.h"
#include "nsIContentViewerEdit.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsISelection.h"
#include "nsISelectionListener.h"
#include "nsIPresContext.h"
#include "nsIPrompt.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerEdit.h"

#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIStreamListener.h"
#include "nsUnitConversion.h"
#include "nsVoidArray.h"
#include "nsCRT.h"

#include "nsIDocumentViewer.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventReceiver.h"

#include "nsIPrintListener.h"
