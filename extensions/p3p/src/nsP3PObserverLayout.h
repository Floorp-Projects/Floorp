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

#ifndef nsP3PObserverLayout_h__
#define nsP3PObserverLayout_h__

#include "nsIP3PCService.h"

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include "nsIObserverService.h"

#include "nsIDocShell.h"
#include "nsIURI.h"
#include "nsIURILoader.h"


class nsP3PObserverLayout : public nsIObserver,
                            public nsSupportsWeakReference {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER

  // nsP3PObserverLayout methods
  nsP3PObserverLayout( );
  virtual ~nsP3PObserverLayout( );

  NS_METHOD             Init( );

protected:
  NS_METHOD             GetRequestMethod( nsIDocShell  *aDocShell,
                                          nsIURI       *aURI,
                                          nsString&     aRequestMethod );

  NS_METHOD             GetP3PService( );


  nsCOMPtr<nsIP3PCService>        mP3PService;        // The P3P Service

  PRBool                          mP3PIsEnabled;      // An indicator as to whether the P3P Service is enabled

  nsCOMPtr<nsIObserverService>    mObserverService;   // The Observer Service

  nsCOMPtr<nsIURILoader>          mURILoader;         // The URI Loader service
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PObserverLayout( nsIObserver **aObserverLayout );

#endif                                           /* nsP3PObserverLayout_h__   */
