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

void
nsDOMCSSAttributeDeclaration::DropReference()
{
  mContent = nsnull;
}

nsresult
nsDOMCSSAttributeDeclaration::DeclarationChanged()
{
  NS_ASSERTION(mContent, "Must have content node to set the decl!");
  nsHTMLValue val;
  nsresult rv = mContent->GetHTMLAttribute(nsHTMLAtoms::style, val);
  NS_ASSERTION(rv == NS_CONTENT_ATTR_HAS_VALUE &&
               eHTMLUnit_ISupports == val.GetUnit(),
               "content must have rule");
  nsCOMPtr<nsICSSStyleRule> oldRule =
    do_QueryInterface(nsCOMPtr<nsISupports>(val.GetISupportsValue()));

  nsCOMPtr<nsICSSStyleRule> newRule = oldRule->DeclarationChanged(PR_FALSE);
  if (!newRule) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
    
  return mContent->SetHTMLAttribute(nsHTMLAtoms::style,
                                    nsHTMLValue(newRule),
                                    PR_TRUE);
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

      nsCOMPtr<nsICSSStyleRule> cssRule;
      result = NS_NewCSSStyleRule(getter_AddRefs(cssRule), nsnull, decl);
      if (NS_FAILED(result)) {
        decl->RuleAbort();
        return result;
      }
        
      result = mContent->SetHTMLAttribute(nsHTMLAtoms::style,
                                          nsHTMLValue(cssRule),
                                          PR_FALSE);
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
 * being initialized.
 */
nsresult
nsDOMCSSAttributeDeclaration::GetCSSParsingEnvironment(nsIURI** aBaseURI,
                                                       nsICSSLoader** aCSSLoader,
                                                       nsICSSParser** aCSSParser)
{
  NS_ASSERTION(mContent, "Something is severely broken -- there should be an nsIContent here!");
  // null out the out params since some of them may not get initialized below
  *aBaseURI = nsnull;
  *aCSSLoader = nsnull;
  *aCSSParser = nsnull;
  
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult result = mContent->GetNodeInfo(getter_AddRefs(nodeInfo));
  if (NS_FAILED(result)) {
    return result;
  }
  // XXXbz GetOwnerDocument
  nsIDocument* doc = nodeInfo->GetDocument();

  mContent->GetBaseURL(aBaseURI);
  
  nsCOMPtr<nsIHTMLContentContainer> htmlContainer(do_QueryInterface(doc));
  if (htmlContainer) {
    htmlContainer->GetCSSLoader(*aCSSLoader);
  }
  NS_ASSERTION(!doc || *aCSSLoader, "Document with no CSS loader!");
  
  if (*aCSSLoader) {
    result = (*aCSSLoader)->GetParserFor(nsnull, aCSSParser);
  } else {
    result = NS_NewCSSParser(aCSSParser);
  }
  if (NS_FAILED(result)) {
    return result;
  }
  
  // If we are not HTML, we need to be case-sensitive.  Otherwise, Look up our
  // namespace.  If we're XHTML, we need to be case-sensitive Otherwise, we
  // should not be
  (*aCSSParser)->SetCaseSensitive(!mContent->IsContentOfType(nsIContent::eHTML) ||
                                  nodeInfo->NamespaceEquals(kNameSpaceID_XHTML));

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSAttributeDeclaration::GetParentRule(nsIDOMCSSRule **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  *aParent = nsnull;
  return NS_OK;
}

