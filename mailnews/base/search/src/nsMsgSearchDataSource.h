/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

#include "nsMsgRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsIMsgSearchHitNotify.h"
#include "nsISupportsArray.h"


#include "nsCOMPtr.h"

class nsMsgSearchDataSource : public nsMsgRDFDataSource,
                              public nsIMsgSearchHitNotify
{
 public:
  nsMsgSearchDataSource();
  virtual ~nsMsgSearchDataSource();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRDFDATASOURCE
  NS_DECL_NSIMSGSEARCHHITNOTIFY

  nsresult Init();

  private:
  nsCOMPtr<nsISupportsArray> mObservers;
  nsCOMPtr<nsIRDFResource> mSearchRoot;

  nsresult notifyObserversAssert(nsIRDFResource *aSource,
                                 nsIRDFResource *aTarget,
                                 nsIRDFResource *aProperty);

  // nsISupportsArray callback
  static PRBool notifyAssert(nsISupports* aElement, void *aData);


  static nsrefcnt gInstanceCount;
  static nsCOMPtr<nsIRDFResource> kNC_MessageChild;
  
};
