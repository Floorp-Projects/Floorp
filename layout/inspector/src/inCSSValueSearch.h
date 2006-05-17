/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
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
  NS_DECL_ISUPPORTS
  NS_DECL_INISEARCHPROCESS
  NS_DECL_INICSSVALUESEARCH

  inCSSValueSearch();
  virtual ~inCSSValueSearch();

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
};

#endif // __inCSSValueSearch_h__
