/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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

#define VC_EXTRALEAN        // Exclude rarely-used stuff from Windows headers

//
// These headers are very evil, as they will define DEBUG if _DEBUG is
//  defined, which is lame and not what we want for things like
//  MOZ_TRACE_MALLOC and other tools.
// If we do not detect this, various MOZ/NS debug symbols are undefined
//  and we can not build.
// /MDd defines _DEBUG automagically to have the right debug C LIB
//  functions get called (so we can get symbols and hook into malloc).
//
#if !defined(DEBUG)
#define THERECANBENODEBUG
#endif

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdtctl.h>        // MFC support for Internet Explorer 4 Common Controls
#include <afxpriv.h>        // Needed for MFC MBCS/Unicode Conversion Macros
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>            // MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#if defined(THERECANBENODEBUG) && defined(DEBUG)
#undef DEBUG
#endif

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
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellTreeItem.h"
#include "nsIClipboardCommands.h"
#include "nsIWebBrowserPersist.h"
#include "nsIContextMenuListener2.h"
#include "nsITooltipListener.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsReadableUtils.h"
#include "nsIPrompt.h"
#include "nsEmbedAPI.h"         
#include "nsISHistory.h"
#include "nsISHEntry.h"
#include "nsIPref.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIProfileChangeStatus.h"
#include "nsIObserverService.h"
#include "imgIContainer.h"
#ifdef MOZ_OLD_CACHE
#include "nsINetDataCacheManager.h"
#endif
#include "nsError.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIEmbeddingSiteWindow2.h"
#include "nsIWebBrowserFind.h"
#include "nsIWebBrowserFocus.h"

// Printer Includes
#include "nsIWebBrowserPrint.h"
#include "nsIDOMWindow.h"

// MfcEmbed #defines

// USE_PROFILES - If defined, nsIProfile will be used which allows for
// multiple profiles. If not defined, a standalone directory service provider
// will be used to provide "profile" locations to one specified directory.
// In the case, the mozilla profile DLL is not needed.

#define USE_PROFILES 1

#endif //_STDAFX_H
