/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsDOMCSSAttrDeclaration.h"
#include "nsCSSDeclaration.h"
#include "nsIDocument.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsIDOMMutationEvent.h"
#include "nsHTMLValue.h"
#include "nsICSSStyleRule.h"
#include "nsINodeInfo.h"
#include "nsICSSLoader.h"
#include "nsICSSParser.h"
#include "nsIURI.h"
#include "nsINameSpaceManager.h"
#include "nsIHTMLContentContainer.h"
#include "nsStyleConsts.h"

MOZ_DECL_CTOR_COUNTER(nsDOMCSSAttributeDeclaration)

nsDOMCSSAttributeDeclaration::nsDOMCSSAttributeDeclaration(nsIHTMLContent *aContent)
{
  MOZ_COUNT_CTOR(nsDOMCSSAttributeDeclaration);

  // This reference is not reference-counted. The content
  // object tells us when its about to go away.
  mContent = aContent;
}

nsDOMCSSAttributeDeclaration::~nsDOMCSSAttributeDeclaration()
{
  MOZ_COUNT_DTOR(nsDOMCSSAttributeDeclaration);
}

NS_IMETHODIMP
nsDOMCSSAttributeDeclaration::RemoveProperty(const nsAString& aPropertyName,
                                             nsAString& aReturn)
{
  aReturn.Truncate();

  nsCSSDeclaration* decl;
  nsresult rv = GetCSSDeclaration(&decl, PR_FALSE);

  if (NS_SUCCEEDED(rv) && decl) {
    nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName);

    decl->GetValue(prop, aReturn);

    rv = decl->RemoveProperty(prop);

    if (NS_SUCCEEDED(rv)) {
      rv = SetCSSDeclaration(decl, PR_TRUE, PR_TRUE);
    } else {
      // RemoveProperty will throw in all sorts of situations -- eg if
      // the property is a shorthand one.  Do not propagate its return
      // value to callers.
      rv = NS_OK;
    }
  }

  return rv;
}

void
nsDOMCSSAttributeDeclaration::DropReference()
{
  mContent = nsnull;
}

nsresult
nsDOMCSSAttributeDeclaration::SetCSSDeclaration(nsCSSDeclaration* aDecl,
                                                PRBool aNotify,
                                                PRBool aDeclOwnedByRule)
{
  NS_ASSERTION(mContent, "Must have content node to set the decl!");
  NS_PRECONDITION(aDecl, "Null decl!");
    
  nsCOMPtr<nsICSSStyleRule> cssRule;
  nsresult rv = NS_NewCSSStyleRule(getter_AddRefs(cssRule), nsCSSSelector());
  if (NS_FAILED(rv)) {
    if (!aDeclOwnedByRule) {
      aDecl->RuleAbort();
    }
    return rv;
  }
    
  cssRule->SetDeclaration(aDecl);
  cssRule->SetWeight(PR_INT32_MAX);
  return mContent->SetHTMLAttribute(nsHTMLAtoms::style,
                                    nsHTMLValue(cssRule),
                                    aNotify);
}

nsresult
nsDOMCSSAttributeDeclaration::GetCSSDeclaration(nsCSSDeclaration **aDecl,
                                                PRBool aAllocate)
{
  nsresult result = NS_OK;

  *aDecl = nsnull;
  if (mContent) {
    nsHTMLValue val;
    result = mContent->GetHTMLAttribute(nsHTMLAtoms::style, val);
    if (result == NS_CONTENT_ATTR_HAS_VALUE &&
        eHTMLUnit_ISupports == val.GetUnit()) {
      nsCOMPtr<nsISupports> rule = val.GetISupportsValue();
      nsCOMPtr<nsICSSStyleRule> cssRule = do_QueryInterface(rule, &result);
      if (cssRule) {
        *aDecl = cssRule->GetDeclaration();
      }
    }
    else if (aAllocate) {
      nsCSSDeclaration *decl = new nsCSSDeclaration();
      if (!decl)
        return NS_ERROR_OUT_OF_MEMORY;
      if (!decl->InitializeEmpty()) {
        decl->RuleAbort();
        return NS_ERROR_OUT_OF_MEMORY;
      }
      result = SetCSSDeclaration(*aDecl, PR_FALSE, PR_FALSE);
      if (NS_SUCCEEDED(result)) {
        *aDecl = decl;
      }
    }
  }

  return result;
}

/*
 * This is a utility function.  It will only fail if it can't get a
 * parser.  This means it can return NS_OK without aURI or aCSSLoader
 * being initialized
 */
nsresult
nsDOMCSSAttributeDeclaration::GetCSSParsingEnvironment(nsIContent* aContent,
                                                       nsIURI** aBaseURI,
                                                       nsICSSLoader** aCSSLoader,
                                                       nsICSSParser** aCSSParser)
{
  NS_ASSERTION(aContent, "Something is severely broken -- there should be an nsIContent here!");
  // null out the out params since some of them may not get initialized below
  *aBaseURI = nsnull;
  *aCSSLoader = nsnull;
  *aCSSParser = nsnull;
  
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult result = aContent->GetNodeInfo(*getter_AddRefs(nodeInfo));
  if (NS_FAILED(result)) {
    return result;
  }
  nsCOMPtr<nsIDocument> doc;
  result = nodeInfo->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(result)) {
    return result;
  }
  
  if (doc) {
    doc->GetBaseURL(*aBaseURI);
    nsCOMPtr<nsIHTMLContentContainer> htmlContainer(do_QueryInterface(doc));
    if (htmlContainer) {
      htmlContainer->GetCSSLoader(*aCSSLoader);
    }
    NS_ASSERTION(*aCSSLoader, "Document with no CSS loader!");
  }
  if (*aCSSLoader) {
    result = (*aCSSLoader)->GetParserFor(nsnull, aCSSParser);
  } else {
    result = NS_NewCSSParser(aCSSParser);
  }
  if (NS_FAILED(result)) {
    return result;
  }
  
  // look up our namespace.  If we're XHTML, we need to be case-sensitive
  // Otherwise, we should not be
  (*aCSSParser)->SetCaseSensitive(nodeInfo->NamespaceEquals(kNameSpaceID_XHTML));

  return NS_OK;
}

nsresult
nsDOMCSSAttributeDeclaration::ParsePropertyValue(const nsAString& aPropName,
                                                 const nsAString& aPropValue)
{
  nsCSSDeclaration* decl;
  nsresult result = GetCSSDeclaration(&decl, PR_TRUE);

  if (!decl) {
    return result;
  }
  
  nsCOMPtr<nsICSSLoader> cssLoader;
  nsCOMPtr<nsICSSParser> cssParser;
  nsCOMPtr<nsIURI> baseURI;
  
  result = GetCSSParsingEnvironment(mContent,
                                    getter_AddRefs(baseURI),
                                    getter_AddRefs(cssLoader),
                                    getter_AddRefs(cssParser));
  if (NS_FAILED(result)) {
    return result;
  }

  nsChangeHint uselessHint = NS_STYLE_HINT_NONE;
  result = cssParser->ParseProperty(aPropName, aPropValue, baseURI, decl,
                                    &uselessHint);
  if (NS_SUCCEEDED(result)) {
    result = SetCSSDeclaration(decl, PR_TRUE, PR_TRUE);
  }

  if (cssLoader) {
    cssLoader->RecycleParser(cssParser);
  }

  return result;
}

nsresult
nsDOMCSSAttributeDeclaration::ParseDeclaration(const nsAString& aDecl,
                                               PRBool aParseOnlyOneDecl,
                                               PRBool aClearOldDecl)
{
  nsCSSDeclaration* decl;
  nsresult result = GetCSSDeclaration(&decl, PR_TRUE);

  if (!decl) {
    return result;
  }

  nsCOMPtr<nsICSSLoader> cssLoader;
  nsCOMPtr<nsICSSParser> cssParser;
  nsCOMPtr<nsIURI> baseURI;

  result = GetCSSParsingEnvironment(mContent,
                                    getter_AddRefs(baseURI),
                                    getter_AddRefs(cssLoader),
                                    getter_AddRefs(cssParser));

  if (NS_FAILED(result)) {
    return result;
  }

  nsChangeHint uselessHint = NS_STYLE_HINT_NONE;
  result = cssParser->ParseAndAppendDeclaration(aDecl, baseURI, decl,
                                                aParseOnlyOneDecl,
                                                &uselessHint,
                                                aClearOldDecl);
  
  if (NS_SUCCEEDED(result)) {
    result = SetCSSDeclaration(decl, PR_TRUE, PR_TRUE);
  }

  if (cssLoader) {
    cssLoader->RecycleParser(cssParser);
  }

  return result;
}

nsresult
nsDOMCSSAttributeDeclaration::GetParent(nsISupports **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  *aParent = mContent;
  NS_IF_ADDREF(*aParent);

  return NS_OK;
}

