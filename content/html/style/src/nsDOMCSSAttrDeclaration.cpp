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
  nsCSSDeclaration* decl;
  nsresult rv = GetCSSDeclaration(&decl, PR_TRUE);

  if (NS_SUCCEEDED(rv) && decl && mContent) {
    nsCOMPtr<nsIDocument> doc;
    mContent->GetDocument(*getter_AddRefs(doc));

    if (doc) {
      doc->BeginUpdate();

      doc->AttributeWillChange(mContent, kNameSpaceID_None,
                               nsHTMLAtoms::style);
    }

    nsChangeHint hint = NS_STYLE_HINT_NONE;

    nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName);
    nsCSSValue val;

    rv = decl->RemoveProperty(prop, val);

    if (NS_SUCCEEDED(rv)) {
      // We pass in eCSSProperty_UNKNOWN here so that we don't get the
      // property name in the return string.
      val.ToString(aReturn, eCSSProperty_UNKNOWN);
      hint = nsCSSProps::kHintTable[prop];
    } else {
      // If we tried to remove an invalid property or a property that wasn't
      //  set we simply return success and an empty string
      rv = NS_OK;
    }

    if (doc) {
      doc->AttributeChanged(mContent, kNameSpaceID_None, nsHTMLAtoms::style,
                            nsIDOMMutationEvent::MODIFICATION, 
                            hint);
      doc->EndUpdate();
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
nsDOMCSSAttributeDeclaration::GetCSSDeclaration(nsCSSDeclaration **aDecl,
                                                PRBool aAllocate)
{
  nsresult result = NS_OK;

  *aDecl = nsnull;
  if (mContent) {
    nsHTMLValue val;
    mContent->GetHTMLAttribute(nsHTMLAtoms::style, val);
    if (eHTMLUnit_ISupports == val.GetUnit()) {
      nsCOMPtr<nsISupports> rule = val.GetISupportsValue();
      nsCOMPtr<nsICSSStyleRule> cssRule = do_QueryInterface(rule, &result);
      if (cssRule) {
        *aDecl = cssRule->GetDeclaration();
      }
    }
    else if (aAllocate) {
      result = NS_NewCSSDeclaration(aDecl);
      if (NS_OK == result) {
        nsCOMPtr<nsICSSStyleRule> cssRule;
        result = NS_NewCSSStyleRule(getter_AddRefs(cssRule), nsCSSSelector());
        if (NS_OK == result) {
          cssRule->SetDeclaration(*aDecl);
          cssRule->SetWeight(0x7fffffff);
          result = mContent->SetHTMLAttribute(nsHTMLAtoms::style,
                                              nsHTMLValue(cssRule),
                                              PR_FALSE);
        }
        else {
          (*aDecl)->RuleAbort();
          *aDecl = nsnull;
        }
      }
    }
  }

  return result;
}

nsresult
nsDOMCSSAttributeDeclaration::SetCSSDeclaration(nsCSSDeclaration *aDecl)
{
  nsresult result = NS_OK;

  if (mContent) {
    nsHTMLValue val;
    mContent->GetHTMLAttribute(nsHTMLAtoms::style, val);
    if (eHTMLUnit_ISupports == val.GetUnit()) {
      nsCOMPtr<nsISupports> rule = val.GetISupportsValue();
      nsCOMPtr<nsICSSStyleRule> cssRule = do_QueryInterface(rule, &result);
      if (cssRule) {
        cssRule->SetDeclaration(aDecl);
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
  nsCOMPtr<nsIDocument> doc;

  result = mContent->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(result)) {
    return result;
  }
  
  result = GetCSSParsingEnvironment(mContent,
                                    getter_AddRefs(baseURI),
                                    getter_AddRefs(cssLoader),
                                    getter_AddRefs(cssParser));
  if (NS_FAILED(result)) {
    return result;
  }

  nsChangeHint hint = NS_STYLE_HINT_NONE;
  if (doc) {
    doc->BeginUpdate();
    doc->AttributeWillChange(mContent, kNameSpaceID_None, nsHTMLAtoms::style);
  }
  
  result = cssParser->ParseProperty(aPropName, aPropValue, baseURI, decl, &hint);
  if (doc) {
    doc->AttributeChanged(mContent, kNameSpaceID_None, nsHTMLAtoms::style,
                          nsIDOMMutationEvent::MODIFICATION, hint);
    doc->EndUpdate();
  }

  if (cssLoader) {
    cssLoader->RecycleParser(cssParser);
  }

  return NS_OK;
}

nsresult
nsDOMCSSAttributeDeclaration::ParseDeclaration(const nsAString& aDecl,
                                               PRBool aParseOnlyOneDecl,
                                               PRBool aClearOldDecl)
{
  nsCSSDeclaration* decl;
  nsresult result = GetCSSDeclaration(&decl, PR_TRUE);

  if (decl) {
    nsCOMPtr<nsICSSLoader> cssLoader;
    nsCOMPtr<nsICSSParser> cssParser;
    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIDocument> doc;

    result = mContent->GetDocument(*getter_AddRefs(doc));
    if (NS_FAILED(result)) {
      return result;
    }
    result = GetCSSParsingEnvironment(mContent,
                                      getter_AddRefs(baseURI),
                                      getter_AddRefs(cssLoader),
                                      getter_AddRefs(cssParser));

    if (NS_SUCCEEDED(result)) {
      nsChangeHint hint = NS_STYLE_HINT_NONE;
      if (doc) {
        doc->BeginUpdate();

        doc->AttributeWillChange(mContent, kNameSpaceID_None,
                                 nsHTMLAtoms::style);
      }
      nsCSSDeclaration* declClone = decl->Clone();

      if (aClearOldDecl) {
        hint = decl->GetStyleImpact();
        // This should be done with decl->Clear() once such a method exists.
        nsAutoString propName;
        PRUint32 count, i;

        count = decl->Count();

        for (i = 0; i < count; i++) {
          decl->GetNthProperty(0, propName);

          nsCSSProperty prop = nsCSSProps::LookupProperty(propName);
          nsCSSValue val;

          decl->RemoveProperty(prop, val);
        }
      }
  
      nsChangeHint newHint = NS_STYLE_HINT_NONE;
      result = cssParser->ParseAndAppendDeclaration(aDecl, baseURI, decl,
                                                    aParseOnlyOneDecl, &newHint);
      NS_UpdateHint(hint, newHint);
      
      if (result == NS_CSS_PARSER_DROP_DECLARATION) {
        SetCSSDeclaration(declClone);
        result = NS_OK;
      }
      if (doc) {
        if (NS_SUCCEEDED(result) && result != NS_CSS_PARSER_DROP_DECLARATION) {
          doc->AttributeChanged(mContent, kNameSpaceID_None,
                                nsHTMLAtoms::style,
                                nsIDOMMutationEvent::MODIFICATION, hint);
        }
        doc->EndUpdate();
      }
      if (cssLoader) {
        cssLoader->RecycleParser(cssParser);
      }
    }
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

