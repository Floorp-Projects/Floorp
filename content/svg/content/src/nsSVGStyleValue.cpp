/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is 
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsSVGValue.h"
#include "nsISVGStyleValue.h"
#include "nsSVGStyleValue.h"
#include "nsIStyleRule.h"
#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsICSSParser.h"
#include "nsIServiceManager.h"
#include "nsLayoutCID.h"

static NS_DEFINE_CID(kCSSParserCID, NS_CSSPARSER_CID);

class nsSVGStyleValue : public nsSVGValue,
                        public nsISVGStyleValue
{
protected:
  friend nsresult
  NS_NewSVGStyleValue(nsISVGStyleValue** aResult);
  
  nsSVGStyleValue();
  
public:
  NS_DECL_ISUPPORTS

  // nsISVGValue interface: 
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

  // nsISVGStyleValue interface:
  NS_IMETHOD GetStyleRule(nsIDocument* baseDoc, nsIStyleRule** rule);
  
protected:
  // Implementation helpers:
  void UpdateStyleRule(nsIDocument* baseDoc);
  
  nsString mValue;
  nsCOMPtr<nsIStyleRule> mRule; // lazily cached
};

//----------------------------------------------------------------------
// Implementation:

nsresult
NS_NewSVGStyleValue(nsISVGStyleValue** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult) return NS_ERROR_NULL_POINTER;
  
  *aResult = (nsISVGStyleValue*) new nsSVGStyleValue();
  if(!*aResult) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsSVGStyleValue::nsSVGStyleValue()
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ISUPPORTS2(nsSVGStyleValue,
                   nsISVGValue,
                   nsISVGStyleValue);


//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGStyleValue::SetValueString(const nsAString& aValue)
{
  WillModify();
  mValue = aValue;
  mRule = nsnull;
  DidModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGStyleValue::GetValueString(nsAString& aValue)
{
  aValue = mValue;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGStyleValue interface:

NS_IMETHODIMP
nsSVGStyleValue::GetStyleRule(nsIDocument* baseDoc, nsIStyleRule** rule)
{
  if (!mRule) {
    UpdateStyleRule(baseDoc);
  }
  
  *rule = mRule;
  NS_IF_ADDREF(*rule);
  
  return NS_OK;
}

//----------------------------------------------------------------------
// Implementation helpers:

void
nsSVGStyleValue::UpdateStyleRule(nsIDocument* baseDoc)
{
  
  if (mValue.IsEmpty()) {
    // XXX: Removing the rule. Is this sufficient?
    mRule = nsnull;
    return;
  }

  NS_ASSERTION(baseDoc,"need base document");
  nsCOMPtr <nsIURI> docURL;
  baseDoc->GetBaseURL(*getter_AddRefs(docURL));
  
  nsCOMPtr<nsICSSParser> css;
  nsComponentManager::CreateInstance(kCSSParserCID,
                                     nsnull,
                                     NS_GET_IID(nsICSSParser),
                                     getter_AddRefs(css));
  NS_ASSERTION(css, "can't get a css parser");
  if (!css) return;    

  css->ParseStyleAttribute(mValue, docURL, getter_AddRefs(mRule)); 
}
