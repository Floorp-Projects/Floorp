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

#ifndef nsP3PObserverHTML_h__
#define nsP3PObserverHTML_h__

#include "nsIP3PCService.h"

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIObserver.h"
#include "nsIElementObserver.h"
#include "nsWeakReference.h"

#include "nsIObserverService.h"


extern
const char        *gObserveElements[];

class nsP3PObserverHTML : public nsIObserver,
                          public nsIElementObserver,
                          public nsSupportsWeakReference {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER

  // nsIElementObserver
  // NS_DECL_NSIELEMENTOBSERVER
  NS_IMETHOD_(const char *)  GetTagNameAt( PRUint32 aTagIndex );
  NS_IMETHOD                 Notify( PRUint32              aDocumentID,
                                     eHTMLTags             aTag,
                                     PRUint32              aNumOfAttributes,
                                     const PRUnichar      *aNameArray[],
                                     const PRUnichar      *aValueArray[] );
  NS_IMETHOD                 Notify( PRUint32              aDocumentID,
                                     const PRUnichar      *aTag,
                                     PRUint32              aNumOfAttributes,
                                     const PRUnichar      *aNameArray[],
                                     const PRUnichar      *aValueArray[] );
  NS_IMETHOD                 Notify( nsISupports          *aDocumentID,
                                     const PRUnichar      *aTag,
                                     const nsStringArray  *aKeys,
                                     const nsStringArray  *aValues );

  // nsP3PObserver HTML methods
  nsP3PObserverHTML( );
  virtual ~nsP3PObserverHTML( );

  NS_METHOD             Init( );

protected:
  NS_METHOD             GetP3PService( );


  nsCOMPtr<nsIP3PCService>        mP3PService;        // The P3P Service

  PRBool                          mP3PIsEnabled;      // An indicator as to whether the P3P Service is enabled

  nsCOMPtr<nsIObserverService>    mObserverService;   // The Observer Service
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PObserverHTML( nsIObserver **aObserverHTML );

#endif                                           /* nsP3PObserverHTML_h__     */
