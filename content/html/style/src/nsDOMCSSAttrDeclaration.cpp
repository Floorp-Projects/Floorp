/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDOMCSSAttrDeclaration.h"
#include "nsCSSDeclaration.h"
#include "nsIDocument.h"
#include "nsHTMLAtoms.h"
#include "nsIStyledContent.h"
#include "nsIDOMMutationEvent.h"
#include "nsHTMLValue.h"
#include "nsICSSStyleRule.h"
#include "nsINodeInfo.h"
#include "nsICSSLoader.h"
#include "nsICSSParser.h"
#include "nsIURI.h"
#include "nsINameSpaceManager.h"
#include "nsStyleConsts.h"
#include "nsContentUtils.h"

MOZ_DECL_CTOR_COUNTER(nsDOMCSSAttributeDeclaration)

nsDOMCSSAttributeDeclaration::nsDOMCSSAttributeDeclaration(nsIStyledContent *aContent)
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

NS_IMPL_ADDREF(nsDOMCSSAttributeDeclaration)
NS_IMPL_RELEASE(nsDOMCSSAttributeDeclaration)

void
nsDOMCSSAttributeDeclaration::DropReference()
{
  mContent = nsnull;
}

nsresult
nsDOMCSSAttributeDeclaration::DeclarationChanged()
{
  NS_ASSERTION(mContent, "Must have content node to set the decl!");
  nsCOMPtr<nsICSSStyleRule> oldRule;
  mContent->GetInlineStyleRule(getter_AddRefs(oldRule));
  NS_ASSERTION(oldRule, "content must have rule");

  nsCOMPtr<nsICSSStyleRule> newRule = oldRule->DeclarationChanged(PR_FALSE);
  if (!newRule) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
    
  return mContent->SetInlineStyleRule(newRule, PR_TRUE);
}

nsresult
nsDOMCSSAttributeDeclaration::GetCSSDeclaration(nsCSSDeclaration **aDecl,
                                                PRBool aAllocate)
{
  nsresult result = NS_OK;

  *aDecl = nsnull;
  if (mContent) {
    nsCOMPtr<nsICSSStyleRule> cssRule;
    mContent->GetInlineStyleRule(getter_AddRefs(cssRule));
    if (cssRule) {
      *aDecl = cssRule->GetDeclaration();
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
        
      result = mContent->SetInlineStyleRule(cssRule, PR_FALSE);
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
nsDOMCSSAttributeDeclaration::GetCSSParsingEnvironment(nsIURI** aSheetURI,
                                                       nsIURI** aBaseURI,
                                                       nsICSSLoader** aCSSLoader,
                                                       nsICSSParser** aCSSParser)
{
  NS_ASSERTION(mContent, "Something is severely broken -- there should be an nsIContent here!");
  // null out the out params since some of them may not get initialized below
  *aSheetURI = nsnull;
  *aBaseURI = nsnull;
  *aCSSLoader = nsnull;
  *aCSSParser = nsnull;

  nsINodeInfo *nodeInfo = mContent->GetNodeInfo();

  // XXXbz GetOwnerDocument
  nsIDocument* doc = nsContentUtils::GetDocument(nodeInfo);

  nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();
  nsCOMPtr<nsIURI> sheetURI = doc->GetDocumentURI();

  if (doc) {
    NS_IF_ADDREF(*aCSSLoader = doc->GetCSSLoader());
    NS_ASSERTION(*aCSSLoader, "Document with no CSS loader!");
  }
  
  nsresult rv = NS_OK;

  if (*aCSSLoader) {
    rv = (*aCSSLoader)->GetParserFor(nsnull, aCSSParser);
  } else {
    rv = NS_NewCSSParser(aCSSParser);
  }
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  // If we are not HTML, we need to be case-sensitive.  Otherwise, Look up our
  // namespace.  If we're XHTML, we need to be case-sensitive Otherwise, we
  // should not be
  (*aCSSParser)->SetCaseSensitive(!mContent->IsContentOfType(nsIContent::eHTML) ||
                                  nodeInfo->NamespaceEquals(kNameSpaceID_XHTML));

  baseURI.swap(*aBaseURI);
  sheetURI.swap(*aSheetURI);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSAttributeDeclaration::GetParentRule(nsIDOMCSSRule **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  *aParent = nsnull;
  return NS_OK;
}

