/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Chak Nanga <chak@netscape.com> 
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

// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#ifndef _STDAFX_H
#define _STDAFX_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#include <afxpriv.h>		// Needed for MFC MBCS/Unicode Conversion Macros
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsCWebBrowser.h"
#include "nsXPIDLString.h"
#include "nsWidgetsCID.h"
#include "nsIDocShell.h"
#include "nsIWebBrowser.h"
#include "nsIBaseWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebProgressListener.h"
#include "nsIWebProgress.h"
#include "nsIWindowCreator.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellTreeItem.h"
#include "nsIClipboardCommands.h"
#include "nsIWebBrowserPersist.h"
#include "nsIContextMenuListener.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsReadableUtils.h"
#include "nsIPrompt.h"
#include "nsEmbedAPI.h"		 
#include "nsISHistory.h"
#include "nsISHEntry.h"
#include "nsIPref.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIProfileChangeStatus.h"
#include "nsIObserverService.h"
#ifdef MOZ_OLD_CACHE
#include "nsINetDataCacheManager.h"
#endif
#include "nsError.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIWebBrowserFind.h"
#include "nsIWebBrowserFocus.h"
#include "nsIServiceManager.h"

// Printer Includes
#include "nsIPrintOptions.h"
#include "nsIWebBrowserPrint.h"

// qa additions
#include "nsIGlobalHistory.h"
#include "nsIBrowserHistory.h"
#include "nsILocalFile.h"
#include "nsIProfile.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIURIContentListener.h"
#include "nsIHelperAppLauncherDialog.h"

#include "nsIDOMWindow.h"
#include "nsIDOMRange.h"
#include "nsIDOMBarProp.h"
#include "nsIDOMWindowCollection.h"
#include "nsISelection.h"
#include "nsITooltipListener.h"

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif //_STDAFX_H
