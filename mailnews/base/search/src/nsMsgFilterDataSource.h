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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

#ifndef __nsMsgFilterDataSource_h
#define __nsMsgFilterDataSource_h

#include "nsIRDFDataSource.h"
#include "nsMsgRDFDataSource.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"

class nsIMsgFilter;

class nsMsgFilterDataSource : public nsMsgRDFDataSource
{
public:
    nsMsgFilterDataSource();
    virtual ~nsMsgFilterDataSource();
  
    NS_DECL_ISUPPORTS
    NS_IMETHOD GetTargets(nsIRDFResource *source,
                          nsIRDFResource *property,
                          PRBool aTruthValue,
                          nsISimpleEnumerator **_retval);
    
    NS_IMETHOD GetTarget(nsIRDFResource *aSource,
                         nsIRDFResource *aProperty,
                         PRBool aTruthValue,
                         nsIRDFNode **aResult);
    
    NS_IMETHOD ArcLabelsOut(nsIRDFResource *source,
                            nsISimpleEnumerator **_retval);

  
private:

    // GetTargets helpers
    nsresult getFilterTarget(nsIMsgFilter *aFilter,
                             nsIRDFResource *aProperty,
                             PRBool aTruthValue,
                             nsIRDFNode **aResult);
    
    nsresult getFilterListTarget(nsIMsgFilterList *aFilter,
                                 nsIRDFResource *aProperty,
                                 PRBool aTruthValue,
                                 nsIRDFNode **aResult);
    
    nsresult getFilterListTargets(nsIMsgFilterList *aFilter,
                                  nsIRDFResource *aSource,
                                  nsIRDFResource *aProperty,
                                  PRBool aTruthValue,
                                  nsISupportsArray *aResult);
    
    static nsrefcnt mGlobalRefCount;
  
    static nsresult initGlobalObjects(nsIRDFService* rdf);
    static nsresult cleanupGlobalObjects();
    static nsresult getServerArcsOut();
    static nsresult getFilterArcsOut();

    // arcs out
    static nsCOMPtr<nsISupportsArray> mFilterListArcsOut;
    static nsCOMPtr<nsISupportsArray> mFilterArcsOut;

    // resources used
    static nsCOMPtr<nsIRDFResource> kNC_Child;
    static nsCOMPtr<nsIRDFResource> kNC_Name;
    static nsCOMPtr<nsIRDFResource> kNC_Enabled;
    static nsCOMPtr<nsIRDFLiteral> kTrueLiteral;

};

#endif
