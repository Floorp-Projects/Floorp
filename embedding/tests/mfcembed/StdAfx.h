/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
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
#include "nsEmbedString.h"
#include "nsCWebBrowser.h"
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
