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
#include "nsIMsgSearchNotify.h"
#include "nsISupportsArray.h"
#include "nsIMsgSearchSession.h"

#include "nsCOMPtr.h"

class nsMsgSearchDataSource : public nsMsgRDFDataSource,
                              public nsIMsgSearchNotify
{
public:
    nsMsgSearchDataSource();
    virtual ~nsMsgSearchDataSource();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIMSGSEARCHNOTIFY

    NS_IMETHOD GetURI(char **aResult);

    NS_IMETHOD GetTargets(nsIRDFResource *aSource,
                          nsIRDFResource *aProperty,
                          PRBool aTruthValue,
                          nsISimpleEnumerator **aResult);

    NS_IMETHOD HasAssertion(nsIRDFResource *aSource,
                            nsIRDFResource *aProperty,
                            nsIRDFNode *aTarget,
                            PRBool aTruthValue,
                            PRBool *aResult);
    
    NS_IMETHOD HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *_retval);

    NS_IMETHOD ArcLabelsOut(nsIRDFResource *aSource,
                            nsISimpleEnumerator **aResult);
    nsresult Init();

private:
    nsCOMPtr<nsIRDFResource> mSearchRoot;
    nsCOMPtr<nsIMsgSearchSession> mSearchSession;

    nsCOMPtr<nsISupportsArray> mSearchResults;

    PRUint32 mURINum;

    static nsrefcnt gInstanceCount;
    static PRUint32 gCurrentURINum;
    static nsCOMPtr<nsIRDFResource> kNC_MessageChild;
  
};
