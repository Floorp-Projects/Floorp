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

#ifndef nsP3PObserverFormSubmit_h__
#define nsP3PObserverFormSubmit_h__

#include "nsIP3PCService.h"

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIObserver.h"
#include "nsIContent.h"  // nsIFormSubmitObserver.h does not include nsIContent...
#include "nsIFormSubmitObserver.h"

#include "nsIObserverService.h"

#include "nsIContent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIURI.h"


class nsP3PObserverFormSubmit : public nsIObserver,
                                public nsIFormSubmitObserver {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER

  // nsIFromSubmitObserver
  // NS_DECL_NSIFORMSUBMITOBSERVER
  NS_IMETHOD            Notify( nsIContent            *aContent,
                                nsIDOMWindowInternal  *aDOMWindowInternal,
                                nsIURI                *aURI,
                                PRBool                *aCancelSubmit );

  // nsP3PObserver HTML methods
  nsP3PObserverFormSubmit( );
  virtual ~nsP3PObserverFormSubmit( );

  NS_METHOD             Init( );

protected:
  NS_METHOD             GetP3PService( );


  nsCOMPtr<nsIP3PCService>        mP3PService;        // The P3P Service

  PRBool                          mP3PIsEnabled;      // An indicator as to whether the P3P Service is enabled

  nsCOMPtr<nsIObserverService>    mObserverService;   // The Observer Service
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PObserverFormSubmit( nsIObserver **aObserverFormSubmit );

#endif                                           /* nsP3PObserverFormSubmit_h__ */
