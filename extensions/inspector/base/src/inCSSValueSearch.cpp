/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "inCSSValueSearch.h"

#include "nsIComponentManager.h"
#include "nsVoidArray.h"
#include "nsReadableUtils.h"

///////////////////////////////////////////////////////////////////////////////

inCSSValueSearch::inCSSValueSearch()
{
  NS_INIT_ISUPPORTS();
  
  mHoldResults = PR_TRUE;
  mReturnRelativeURLs = PR_FALSE;
  mNormalizeChromeURLs = PR_FALSE;
  mResultCount = 0;

  mProperties = new nsCSSProperty[100];
  mPropertyCount = 0;
  nsCSSProps::AddRefTable();
}

inCSSValueSearch::~inCSSValueSearch()
{
  nsCSSProps::ReleaseTable();
  delete mProperties;
  delete mResults;
}

NS_IMPL_ISUPPORTS2(inCSSValueSearch, inISearchProcess, inICSSValueSearch);

///////////////////////////////////////////////////////////////////////////////
// inISearchProcess

NS_IMETHODIMP 
inCSSValueSearch::GetIsActive(PRBool *aIsActive)
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
inCSSValueSearch::GetHoldResults(PRBool *aHoldResults)
{
  *aHoldResults = mHoldResults;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SetHoldResults(PRBool aHoldResults)
{
  mHoldResults = aHoldResults;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SearchSync()
{
  InitSearch();
  
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
  if (doc) {
    PRInt32 count = 0;
    doc->GetNumberOfStyleSheets(&count);
    for (PRInt32 i = 0; i < count; i++) {
      nsCOMPtr<nsIStyleSheet> sheet;
      doc->GetStyleSheetAt(i, getter_AddRefs(sheet));
      SearchStyleSheet(sheet);
    }
  }

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
  KillSearch(inISearchObserver::INTERRUPTED);
  return NS_OK;
}

NS_IMETHODIMP
inCSSValueSearch::SearchStep(PRBool* _retval)
{

  return NS_OK;
}


NS_IMETHODIMP 
inCSSValueSearch::GetStringResultAt(PRInt32 aIndex, PRUnichar **_retval)
{
  if (mHoldResults) {
    nsAutoString* result = (nsAutoString*)mResults->ElementAt(aIndex);
    *_retval = ToNewUnicode(*result);
  } else if (aIndex == mResultCount-1) {
    *_retval = ToNewUnicode(*mLastResult);
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
  *aBaseURL = ToNewUnicode(*mBaseURL);
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SetBaseURL(const PRUnichar* aBaseURL)
{
  nsAutoString url;
  mBaseURL = &url;
  url.Assign(aBaseURL);
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::GetReturnRelativeURLs(PRBool* aReturnRelativeURLs)
{
  *aReturnRelativeURLs = mReturnRelativeURLs;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SetReturnRelativeURLs(PRBool aReturnRelativeURLs)
{
  mReturnRelativeURLs = aReturnRelativeURLs;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::GetNormalizeChromeURLs(PRBool *aNormalizeChromeURLs)
{
  *aNormalizeChromeURLs = mNormalizeChromeURLs;
  return NS_OK;
}

NS_IMETHODIMP
inCSSValueSearch::SetNormalizeChromeURLs(PRBool aNormalizeChromeURLs)
{
  mNormalizeChromeURLs = aNormalizeChromeURLs;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::AddPropertyCriteria(const PRUnichar *aPropName)
{
  nsAutoString propName;
  propName.Assign(aPropName);
  nsCSSProperty prop = nsCSSProps::LookupProperty(propName);
  mProperties[mPropertyCount] = prop;
  mPropertyCount++;
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::GetTextCriteria(PRUnichar** aTextCriteria)
{
  *aTextCriteria = ToNewUnicode(*mTextCriteria);
  return NS_OK;
}

NS_IMETHODIMP 
inCSSValueSearch::SetTextCriteria(const PRUnichar* aTextCriteria)
{
  if (!mTextCriteria) mTextCriteria = new nsAutoString();
  mTextCriteria->Assign(aTextCriteria);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// inCSSValueSearch

nsresult
inCSSValueSearch::InitSearch()
{
  if (mHoldResults) {
    mResults = new nsVoidArray();
  }
  
  mResultCount = 0;

  return NS_OK;
}

nsresult
inCSSValueSearch::KillSearch(PRInt16 aResult)
{
  mIsActive = PR_TRUE;
  mObserver->OnSearchEnd(this, aResult);

  return NS_OK;
}

nsresult
inCSSValueSearch::SearchStyleSheet(nsIStyleSheet* aStyleSheet)
{
  NS_IF_ADDREF(aStyleSheet);
  nsCOMPtr<nsICSSStyleSheet> cssSheet = do_QueryInterface(aStyleSheet);
  if (cssSheet) {
    // recurse downward through the stylesheet tree
    PRInt32 count, i;
    cssSheet->StyleSheetCount(count);
    for (i = 0; i < count; i++) {
      nsICSSStyleSheet* child;
      cssSheet->GetStyleSheetAt(i, child);
      SearchStyleSheet(child);
    }

    cssSheet->StyleRuleCount(count);
    for (i = 0; i < count; i++) {
      nsICSSRule* rule;
      cssSheet->GetStyleRuleAt(i, rule);
      SearchStyleRule(rule);
    }
  }

  NS_IF_RELEASE(aStyleSheet);
  return NS_OK;
}

nsresult
inCSSValueSearch::SearchStyleRule(nsIStyleRule* aStyleRule)
{
  NS_IF_ADDREF(aStyleRule);

  nsCOMPtr<nsICSSStyleRule> cssRule = do_QueryInterface(aStyleRule);
  if (cssRule) {
    nsCOMPtr<nsICSSDeclaration> aDec = cssRule->GetDeclaration();
    for (PRUint32 i = 0; i < mPropertyCount; i++) {
      nsCSSProperty prop = mProperties[i];
      SearchStyleValue(aDec, prop);
    }
  }

  
  NS_IF_RELEASE(aStyleRule);
  return NS_OK;
}

nsresult
inCSSValueSearch::SearchStyleValue(nsICSSDeclaration* aDec, nsCSSProperty aProp)
{
  nsCString cstring = nsCSSProps::GetStringValue(aProp);

  nsCSSValue value;
  aDec->GetValue(aProp, value);

  if (value.GetUnit() == eCSSUnit_URL) {
    nsAutoString* result = new nsAutoString();
    value.GetStringValue(*result);
    if (mReturnRelativeURLs)
        EqualizeURL(result);
    mResults->AppendElement((void*)result);
    mResultCount++;
  }

  return NS_OK;
}

nsresult
inCSSValueSearch::EqualizeURL(nsAutoString* aURL)
{
  if (mNormalizeChromeURLs) {
    if (aURL->Find("chrome://", PR_FALSE, 0, 1) >= 0) {
      PRUint32 len = aURL->Length();
      char* result = new char[len-8];
      char* buffer = ToNewCString(*aURL);
      PRUint32 i = 9;
      PRUint32 milestone = 0;
      PRUint32 s = 0;
      while (i < len) {
        if (buffer[i] == '/') {
          milestone += 1;
        } 
        if (milestone == 0 || milestone > 1) {
          result[i-9-s] = (buffer[i] == '/') ? '/' : buffer[i];
        } else {
          s++;
        }
        i++;
      }
      result[i-9-s] = 0;

      aURL->AssignWithConversion(result);
    }
  } else {
  }

  return NS_OK;
}
