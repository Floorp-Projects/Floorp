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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef cssruledatasource___h___
#define cssruledatasource___h___

#define NS_CSSRULEDATASOURCE_ID "Inspector_CSSRules"

#include "nsICSSRuleDataSource.h"
#include "nsDOMDSResource.h"

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsString.h"

#include "nsIDOMElement.h"

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIStyleSheet.h"
#include "nsIURI.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStyleSet.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIStyleContext.h"
#include "nsIStyleRule.h"
#include "nsICSSStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsIRuleNode.h"

#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFObserver.h"
#include "nsIRDFNode.h"
#include "nsRDFCID.h"
#include "rdf.h"

#include "nsIComponentManager.h"
#include "nsIDOMWindowInternal.h"

#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsSpecialSystemDirectory.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsEnumeratorUtils.h"
#include "nsEscape.h"
#include "nsIAtom.h"
#include "prprf.h"

class nsCSSRuleDataSource : public nsICSSRuleDataSource,
                            public nsIRDFDataSource
{
  // nsCSSRuleDataSource
public:
	nsCSSRuleDataSource();
	virtual ~nsCSSRuleDataSource();
	nsresult Init();

private:
  nsIRDFService* GetRDFService();
  nsresult GetTargetsForElement(nsIDOMElement* aElement, nsIRDFResource* aProperty, nsISupportsArray* aArcs);
  nsresult GetResourceForObject(nsISupports* aObject, nsIRDFResource** _retval);
  nsresult CreateStyleRuleTarget(nsICSSStyleRule* aRule, nsIRDFResource* aProperty, nsIRDFNode **aResult);
  nsresult CreateLiteral(nsString& str, nsIRDFNode **aResult);

  nsIRDFService* mRDFService;
  nsIRDFService* mService;
  nsIDOMElement* mElement;
  nsSupportsHashtable mObjectTable;
  
  // nsISupports
	NS_DECL_ISUPPORTS

	// nsICSSRuleDataSource
	NS_DECL_NSICSSRULEDATASOURCE

  // nsIRDFDataSource
  NS_DECL_NSIRDFDATASOURCE

};

#endif // cssruledatasource___h___
