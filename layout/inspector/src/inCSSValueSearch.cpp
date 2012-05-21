/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "inCSSValueSearch.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsVoidArray.h"
#include "nsReadableUtils.h"
#include "nsIDOMDocument.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSRuleList.h"
#include "nsIDOMCSSStyleRule.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSImportRule.h"
#include "nsIDOMCSSMediaRule.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

///////////////////////////////////////////////////////////////////////////////
inCSSValueSearch::inCSSValueSearch()
  : mResults(nsnull),
    mProperties(nsnull),
    mResultCount(0),
    mPropertyCount(0),
    mIsActive(false),
    mHoldResults(true),
    mReturnRelativeURLs(true),
    mNormalizeChromeURLs(false)
{
  nsCSSProps::AddRefTable();
  mProperties = new nsCSSProperty[100];
}

inCSSValueSearch::~inCSSValueSearch()
{
  delete[] mProperties;
  delete mResults;
  nsCSSProps::ReleaseTable();
}

NS_IMPL_ISUPPORTS2(inCSSValueSearch, inISearchProcess, inICSSValueSearch)

///////////////////////////////////////////////////////////////////////////////
// inISearchProcess

NS_IMETHODIMP 
inCSSValueSearch::GetIsActive(bool *aIsActive)
{
  *aIsActive = mIsActive;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::GetResultCount(PRInt32 *aResultCount)
{
  *aResultCount = mResultCount;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::GetHoldResults(bool *aHoldResults)
{
  *aHoldResults = mHoldResults;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SetHoldResults(bool aHoldResults)
{
  mHoldResults = aHoldResults;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SearchSync()
{
  InitSearch();

  if (!mDocument) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> baseURI;
  nsCOMPtr<nsIDocument> idoc = do_QueryInterface(mDocument);
  if (idoc) {
    baseURI = idoc->GetBaseURI();
  }

  nsCOMPtr<nsIDOMStyleSheetList> sheets;
  nsresult rv = mDocument->GetStyleSheets(getter_AddRefs(sheets));
  NS_ENSURE_SUCCESS(rv, NS_OK);

  PRUint32 length;
  sheets->GetLength(&length);
  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<nsIDOMStyleSheet> sheet;
    sheets->Item(i, getter_AddRefs(sheet));
    nsCOMPtr<nsIDOMCSSStyleSheet> cssSheet = do_QueryInterface(sheet);
    if (cssSheet)
      SearchStyleSheet(cssSheet, baseURI);
  }

  // XXX would be nice to search inline style as well.

  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SearchAsync(inISearchObserver *aObserver)
{
  InitSearch();
  mObserver = aObserver;

  return NS_OK;
}


NS_IMETHODIMP
inCSSValueSearch::SearchStop()
{
  KillSearch(inISearchObserver::IN_INTERRUPTED);
  return NS_OK;
}

NS_IMETHODIMP
inCSSValueSearch::SearchStep(bool* _retval)
{

  return NS_OK;
}


NS_IMETHODIMP 
inCSSValueSearch::GetStringResultAt(PRInt32 aIndex, nsAString& _retval)
{
  if (mHoldResults) {
    nsAutoString* result = mResults->ElementAt(aIndex);
    _retval = *result;
  } else if (aIndex == mResultCount-1) {
    _retval = mLastResult;
  } else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::GetIntResultAt(PRInt32 aIndex, PRInt32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
inCSSValueSearch::GetUIntResultAt(PRInt32 aIndex, PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////////////
// inICSSValueSearch

NS_IMETHODIMP 
inCSSValueSearch::GetDocument(nsIDOMDocument** aDocument)
{
  *aDocument = mDocument;
  NS_IF_ADDREF(*aDocument);
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SetDocument(nsIDOMDocument* aDocument)
{
  mDocument = aDocument;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::GetBaseURL(PRUnichar** aBaseURL)
{
  if (!(*aBaseURL = ToNewUnicode(mBaseURL)))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SetBaseURL(const PRUnichar* aBaseURL)
{
  mBaseURL.Assign(aBaseURL);
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::GetReturnRelativeURLs(bool* aReturnRelativeURLs)
{
  *aReturnRelativeURLs = mReturnRelativeURLs;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SetReturnRelativeURLs(bool aReturnRelativeURLs)
{
  mReturnRelativeURLs = aReturnRelativeURLs;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::GetNormalizeChromeURLs(bool *aNormalizeChromeURLs)
{
  *aNormalizeChromeURLs = mNormalizeChromeURLs;
  return NS_OK;
}

NS_IMETHODIMP
inCSSValueSearch::SetNormalizeChromeURLs(bool aNormalizeChromeURLs)
{
  mNormalizeChromeURLs = aNormalizeChromeURLs;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::AddPropertyCriteria(const PRUnichar *aPropName)
{
  nsCSSProperty prop =
    nsCSSProps::LookupProperty(nsDependentString(aPropName));
  mProperties[mPropertyCount] = prop;
  mPropertyCount++;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::GetTextCriteria(PRUnichar** aTextCriteria)
{
  if (!(*aTextCriteria = ToNewUnicode(mTextCriteria)))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SetTextCriteria(const PRUnichar* aTextCriteria)
{
  mTextCriteria.Assign(aTextCriteria);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// inCSSValueSearch

nsresult
inCSSValueSearch::InitSearch()
{
  if (mHoldResults) {
    mResults = new nsTArray<nsAutoString *>();
  }
  
  mResultCount = 0;

  return NS_OK;
}

nsresult
inCSSValueSearch::KillSearch(PRInt16 aResult)
{
  mIsActive = true;
  mObserver->OnSearchEnd(this, aResult);

  return NS_OK;
}

nsresult
inCSSValueSearch::SearchStyleSheet(nsIDOMCSSStyleSheet* aStyleSheet, nsIURI* aBaseURL)
{
  nsCOMPtr<nsIURI> baseURL;
  nsAutoString href;
  aStyleSheet->GetHref(href);
  if (href.IsEmpty())
    baseURL = aBaseURL;
  else
    NS_NewURI(getter_AddRefs(baseURL), href, nsnull, aBaseURL);

  nsCOMPtr<nsIDOMCSSRuleList> rules;
  nsresult rv = aStyleSheet->GetCssRules(getter_AddRefs(rules));
  NS_ENSURE_SUCCESS(rv, rv);

  return SearchRuleList(rules, baseURL);
}

nsresult
inCSSValueSearch::SearchRuleList(nsIDOMCSSRuleList* aRuleList, nsIURI* aBaseURL)
{
  PRUint32 length;
  aRuleList->GetLength(&length);
  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<nsIDOMCSSRule> rule;
    aRuleList->Item(i, getter_AddRefs(rule));
    PRUint16 type;
    rule->GetType(&type);
    switch (type) {
      case nsIDOMCSSRule::STYLE_RULE: {
        nsCOMPtr<nsIDOMCSSStyleRule> styleRule = do_QueryInterface(rule);
        SearchStyleRule(styleRule, aBaseURL);
      } break;
      case nsIDOMCSSRule::IMPORT_RULE: {
        nsCOMPtr<nsIDOMCSSImportRule> importRule = do_QueryInterface(rule);
        nsCOMPtr<nsIDOMCSSStyleSheet> childSheet;
        importRule->GetStyleSheet(getter_AddRefs(childSheet));
        if (childSheet)
          SearchStyleSheet(childSheet, aBaseURL);
      } break;
      case nsIDOMCSSRule::MEDIA_RULE: {
        nsCOMPtr<nsIDOMCSSMediaRule> mediaRule = do_QueryInterface(rule);
        nsCOMPtr<nsIDOMCSSRuleList> childRules;
        mediaRule->GetCssRules(getter_AddRefs(childRules));
        SearchRuleList(childRules, aBaseURL);
      } break;
      default:
        // XXX handle nsIDOMCSSRule::PAGE_RULE if we ever support it
        break;
    }
  }
  return NS_OK;
}

nsresult
inCSSValueSearch::SearchStyleRule(nsIDOMCSSStyleRule* aStyleRule, nsIURI* aBaseURL)
{
  nsCOMPtr<nsIDOMCSSStyleDeclaration> decl;
  nsresult rv = aStyleRule->GetStyle(getter_AddRefs(decl));
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRUint32 length;
  decl->GetLength(&length);
  nsAutoString property, value;
  for (PRUint32 i = 0; i < length; ++i) {
    decl->Item(i, property);
    // XXX This probably ought to use GetPropertyCSSValue if it were
    // implemented.
    decl->GetPropertyValue(property, value);
    SearchStyleValue(value, aBaseURL);
  }
  return NS_OK;
}

nsresult
inCSSValueSearch::SearchStyleValue(const nsAFlatString& aValue, nsIURI* aBaseURL)
{
  if (StringBeginsWith(aValue, NS_LITERAL_STRING("url(")) &&
      StringEndsWith(aValue, NS_LITERAL_STRING(")"))) {
    const nsASingleFragmentString &url =
      Substring(aValue, 4, aValue.Length() - 5);
    // XXXldb Need to do more with |mReturnRelativeURLs|, perhaps?
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), url, nsnull, aBaseURL);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString spec;
    uri->GetSpec(spec);
    nsAutoString *result = new NS_ConvertUTF8toUTF16(spec);
    if (mReturnRelativeURLs)
      EqualizeURL(result);
    mResults->AppendElement(result);
    ++mResultCount;
  }

  return NS_OK;
}

nsresult
inCSSValueSearch::EqualizeURL(nsAutoString* aURL)
{
  if (mNormalizeChromeURLs) {
    if (aURL->Find("chrome://", false, 0, 1) >= 0) {
      PRUint32 len = aURL->Length();
      PRUnichar* result = new PRUnichar[len-8];
      const PRUnichar* src = aURL->get();
      PRUint32 i = 9;
      PRUint32 milestone = 0;
      PRUint32 s = 0;
      while (i < len) {
        if (src[i] == '/') {
          milestone += 1;
        } 
        if (milestone != 1) {
          result[i-9-s] = src[i];
        } else {
          s++;
        }
        i++;
      }
      result[i-9-s] = 0;

      aURL->Assign(result);
      delete [] result;
    }
  } else {
  }

  return NS_OK;
}
