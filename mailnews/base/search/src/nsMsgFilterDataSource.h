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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
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
