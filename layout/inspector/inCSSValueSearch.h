/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __inCSSValueSearch_h__
#define __inCSSValueSearch_h__

#include "inICSSValueSearch.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDOMDocument.h"
#include "inISearchObserver.h"
#include "nsTArray.h"
#include "nsCSSProps.h"

class nsIDOMCSSStyleSheet;
class nsIDOMCSSRuleList;
class nsIDOMCSSStyleRule;
class nsIURI;

class inCSSValueSearch final : public inICSSValueSearch
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INISEARCHPROCESS
  NS_DECL_INICSSVALUESEARCH

  inCSSValueSearch();

protected:
  virtual ~inCSSValueSearch();
  nsCOMPtr<inISearchObserver> mObserver;
  nsCOMPtr<nsIDOMDocument> mDocument;
  nsTArray<nsAutoString *>* mResults;
  nsCSSPropertyID* mProperties;
  nsString mLastResult;
  nsString mBaseURL;
  nsString mTextCriteria;
  int32_t mResultCount;
  uint32_t mPropertyCount;
  bool mIsActive;
  bool mHoldResults;
  bool mReturnRelativeURLs;
  bool mNormalizeChromeURLs;

  nsresult InitSearch();
  nsresult KillSearch(int16_t aResult);
  nsresult SearchStyleSheet(nsIDOMCSSStyleSheet* aStyleSheet, nsIURI* aBaseURI);
  nsresult SearchRuleList(nsIDOMCSSRuleList* aRuleList, nsIURI* aBaseURI);
  nsresult SearchStyleRule(nsIDOMCSSStyleRule* aStyleRule, nsIURI* aBaseURI);
  nsresult SearchStyleValue(const nsString& aValue, nsIURI* aBaseURI);
  nsresult EqualizeURL(nsAutoString* aURL);
};

// {4D977F60-FBE7-4583-8CB7-F5ED882293EF}
#define IN_CSSVALUESEARCH_CID \
{ 0x4d977f60, 0xfbe7, 0x4583, { 0x8c, 0xb7, 0xf5, 0xed, 0x88, 0x22, 0x93, 0xef } }

#endif // __inCSSValueSearch_h__
