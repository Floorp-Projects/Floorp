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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsDummyLayoutRequest_h__
#define nsDummyLayoutRequest_h__

#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"
#include "nsWeakPtr.h"

//----------------------------------------------------------------------
//
// DummyLayoutRequest
//
//   This is a dummy request implementation that we add to the document's load
//   group. It ensures that EndDocumentLoad() in the docshell doesn't fire
//   before we've finished all of layout.
//

class nsDummyLayoutRequest : public nsIChannel
{
protected:
  nsDummyLayoutRequest(nsIPresShell* aPresShell);
  virtual ~nsDummyLayoutRequest();

  static PRInt32 gRefCnt;
  static nsIURI* gURI;

  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsWeakPtr mPresShell;

public:
  static nsresult
  Create(nsIRequest** aResult, nsIPresShell* aPresShell);

  NS_DECL_ISUPPORTS

	// nsIRequest
  NS_IMETHOD GetName(nsACString &result) { 
    result.AssignLiteral("about:layout-dummy-request");
    return NS_OK;
  }
  NS_IMETHOD IsPending(PRBool *_retval) { *_retval = PR_TRUE; return NS_OK; }
  NS_IMETHOD GetStatus(nsresult *status) { *status = NS_OK; return NS_OK; } 
  NS_IMETHOD Cancel(nsresult status);
  NS_IMETHOD Suspend(void) { return NS_OK; }
  NS_IMETHOD Resume(void)  { return NS_OK; }
  NS_IMETHOD GetLoadGroup(nsILoadGroup * *aLoadGroup) { *aLoadGroup = mLoadGroup; NS_IF_ADDREF(*aLoadGroup); return NS_OK; }
  NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup) { mLoadGroup = aLoadGroup; return NS_OK; }
  NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags) { *aLoadFlags = nsIRequest::LOAD_NORMAL; return NS_OK; }
  NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags) { return NS_OK; }

 	// nsIChannel
  NS_IMETHOD GetOriginalURI(nsIURI* *aOriginalURI) { *aOriginalURI = gURI; NS_ADDREF(*aOriginalURI); return NS_OK; }
  NS_IMETHOD SetOriginalURI(nsIURI* aOriginalURI) { gURI = aOriginalURI; NS_ADDREF(gURI); return NS_OK; }
  NS_IMETHOD GetURI(nsIURI* *aURI) { *aURI = gURI; NS_ADDREF(*aURI); return NS_OK; }
  NS_IMETHOD SetURI(nsIURI* aURI) { gURI = aURI; NS_ADDREF(gURI); return NS_OK; }
  NS_IMETHOD Open(nsIInputStream **_retval) { *_retval = nsnull; return NS_OK; }
  NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt) { return NS_OK; }
  NS_IMETHOD GetOwner(nsISupports * *aOwner) { *aOwner = nsnull; return NS_OK; }
  NS_IMETHOD SetOwner(nsISupports * aOwner) { return NS_OK; }
  NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks) { *aNotificationCallbacks = nsnull; return NS_OK; }
  NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks) { return NS_OK; }
  NS_IMETHOD GetSecurityInfo(nsISupports * *aSecurityInfo) { *aSecurityInfo = nsnull; return NS_OK; } 
  NS_IMETHOD GetContentType(nsACString &aContentType) { aContentType.Truncate(); return NS_OK; } 
  NS_IMETHOD SetContentType(const nsACString &aContentType) { return NS_OK; } 
  NS_IMETHOD GetContentCharset(nsACString &aContentCharset) { aContentCharset.Truncate(); return NS_OK; } 
  NS_IMETHOD SetContentCharset(const nsACString &aContentCharset) { return NS_OK; } 
  NS_IMETHOD GetContentLength(PRInt32 *aContentLength) { return NS_OK; }
  NS_IMETHOD SetContentLength(PRInt32 aContentLength) { return NS_OK; }
};

#endif
