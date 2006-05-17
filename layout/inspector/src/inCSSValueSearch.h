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

#ifndef __inCSSValueSearch_h__
#define __inCSSValueSearch_h__

#include "inICSSValueSearch.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "inISearchObserver.h"
#include "nsVoidArray.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsCSSValue.h"

class inCSSValueSearch : public inICSSValueSearch
{
public:
  inCSSValueSearch();
  ~inCSSValueSearch();

protected:
  PRBool mIsActive;
  PRInt32 mResultCount;
  PRBool mHoldResults;
  nsVoidArray* mResults;
  nsAutoString* mLastResult;
  nsCOMPtr<inISearchObserver> mObserver;

  nsCOMPtr<nsIDOMDocument> mDocument;
  nsAutoString* mBaseURL;
  PRBool mReturnRelativeURLs;
  PRBool mNormalizeChromeURLs;
  nsAutoString* mTextCriteria;
  nsCSSProperty* mProperties;
  PRUint32 mPropertyCount;

  nsresult InitSearch();
  nsresult KillSearch(PRInt16 aResult);
  nsresult SearchStyleSheet(nsIStyleSheet* aStyleSheet);
  nsresult SearchStyleRule(nsIStyleRule* aStyleRule);
  nsresult SearchStyleValue(nsICSSDeclaration* aDec, nsCSSProperty aProp);
  nsresult EqualizeURL(nsAutoString* aURL);

public:
  NS_DECL_ISUPPORTS

  NS_DECL_INISEARCHPROCESS

  NS_DECL_INICSSVALUESEARCH
};

//////////////////////////////////////////////////////////////

// {4D977F60-FBE7-4583-8CB7-F5ED882293EF}
#define IN_CSSVALUESEARCH_CID \
{ 0x4d977f60, 0xfbe7, 0x4583, { 0x8c, 0xb7, 0xf5, 0xed, 0x88, 0x22, 0x93, 0xef } }

#define IN_CSSVALUESEARCH_CONTRACTID \
"@mozilla.org/inspector/search;1?type=cssvalue"

#endif
