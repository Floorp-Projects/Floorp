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

#ifndef cssdecdatasource___h___
#define cssdecdatasource___h___

#define NS_CSSDECDATASOURCE_ID "Inspector_CSSDec"

#include "nsICSSDecDataSource.h"
#include "nsDOMDSResource.h"
#include "nsCSSDecIntHolder.h"

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsString.h"

#include "nsIDOMElement.h"
#include "nsIDOMCSSStyleRule.h"
#include "nsIDOMCSSStyleDeclaration.h"

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsString.h"
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

class nsCSSDecDataSource : public nsICSSDecDataSource,
                           public nsIRDFDataSource
{
  // nsCSSDecDataSource
public:
	nsCSSDecDataSource();
	virtual ~nsCSSDecDataSource();
	nsresult Init();

private:
  nsIRDFService* mRDFService;
  nsIDOMCSSStyleRule* mStyleRule;
  nsSupportsHashtable mObjectTable;

  nsIRDFService* GetRDFService();
  nsresult GetTargetsForStyleRule(nsIDOMCSSStyleRule* aRule, nsIRDFResource* aProperty, nsISupportsArray* aArcs);
  nsresult GetResourceForObject(nsISupports* aObject, nsIRDFResource** _retval);
  nsresult CreatePropertyTarget(PRUint32 aIndex, nsIRDFResource* aProperty, nsIRDFNode **aResult);
  nsresult CreateLiteral(nsString& str, nsIRDFNode **aResult);

public:
	// nsISupports
	NS_DECL_ISUPPORTS

	// nsICSSDecDataSource
	NS_DECL_NSICSSDECDATASOURCE

  // nsIRDFDataSource
  NS_DECL_NSIRDFDATASOURCE

};

#endif // cssdecdatasource___h___
