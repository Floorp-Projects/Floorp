/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

#ifndef nsP3PHTTPNotify_h___
#define nsP3PHTTPNotify_h___

#include "nsIP3PCService.h"

#include <nsCOMPtr.h>
#include <nsISupports.h>
#include <nsIHttpNotify.h>

#include <nsINetModuleMgr.h>

#include <nsIHTTPChannel.h>

#include <nsIDocShellTreeItem.h>

#include <nsIAtom.h>


class nsP3PHTTPNotify : public nsIHTTPNotify {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIHTTPNotify methods
  NS_DECL_NSIHTTPNOTIFY

  // nsP3PHTTPNotify methods
  nsP3PHTTPNotify( );
  virtual ~nsP3PHTTPNotify( );

  NS_METHOD             Init( );

protected:
  NS_METHOD_( void )    ProcessRequestHeaders( nsIHTTPChannel  *aHTTPChannel );

  NS_METHOD_( void )    ProcessResponseHeaders( nsIHTTPChannel  *aHTTPChannel );

  NS_METHOD             ProcessP3PHeaders( nsCString&       aP3PValues,
                                           nsIHTTPChannel  *aHTTPChannel );
  NS_METHOD             ProcessIndividualP3PHeader( nsCString&       aP3PValue,
                                                    nsIHTTPChannel  *aHTTPChannel );
  NS_METHOD_( PRBool )  IsHTMLorXML( nsCString&  aContentType );

  NS_METHOD             GetDocShellTreeItem( nsIHTTPChannel       *aHTTPChannel,
                                             nsIDocShellTreeItem **aDocShellTreeItem );

  NS_METHOD             DeleteCookies( nsIHTTPChannel  *aHTTPChannel );

  NS_METHOD             GetP3PService( );


  nsCOMPtr<nsIP3PCService>   mP3PService;             // The P3P Service

  PRBool                     mP3PIsEnabled;           // An indicator as to whether the P3P Service is enabled

  nsCOMPtr<nsINetModuleMgr>  mNetModuleMgr;           // The Network Module Manager

  nsCOMPtr<nsIAtom>          mP3PHeader,              // Various request and response header types
                             mSetCookieHeader,
                             mContentTypeHeader,
                             mRefererHeader;
};


extern
NS_EXPORT NS_METHOD     NS_NewP3PHTTPNotify( nsIHTTPNotify **aHTTPNotify );

#endif                                           /* nsP3PHTTPNotify_h___      */
