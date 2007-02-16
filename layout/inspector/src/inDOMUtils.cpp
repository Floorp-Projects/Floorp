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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *   Christopher A. Aillon <christopher@aillon.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "inDOMUtils.h"
#include "inLayoutUtils.h"

#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMCharacterData.h"
#include "nsRuleNode.h"
#include "nsIStyleRule.h"
#include "nsICSSStyleRule.h"
#include "nsICSSStyleRuleDOMWrapper.h"
#include "nsIDOMWindowInternal.h"

static NS_DEFINE_CID(kInspectorCSSUtilsCID, NS_INSPECTORCSSUTILS_CID);

///////////////////////////////////////////////////////////////////////////////

inDOMUtils::inDOMUtils()
{
  mCSSUtils = do_GetService(kInspectorCSSUtilsCID);
}

inDOMUtils::~inDOMUtils()
{
}

NS_IMPL_ISUPPORTS1(inDOMUtils, inIDOMUtils)

///////////////////////////////////////////////////////////////////////////////
// inIDOMUtils

NS_IMETHODIMP
inDOMUtils::IsIgnorableWhitespace(nsIDOMCharacterData *aDataNode,
                                  PRBool *aReturn)
{
  NS_PRECONDITION(aDataNode, "Must have a character data node");
  NS_PRECONDITION(aReturn, "Must have an out parameter");

  *aReturn = PR_FALSE;

  nsCOMPtr<nsIContent> content = do_QueryInterface(aDataNode);
  NS_ASSERTION(content, "Does not implement nsIContent!");

  if (!content->TextIsOnlyWhitespace()) {
    return NS_OK;
  }

  // Okay.  We have only white space.  Let's check the white-space
  // property now and make sure that this isn't preformatted text...

  nsCOMPtr<nsIDOMWindowInternal> win = inLayoutUtils::GetWindowFor(aDataNode);
  if (!win) {
    // Hmm.  Things are screwy if we have no window...
    NS_ERROR("No window!");
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell = inLayoutUtils::GetPresShellFor(win);
  if (!presShell) {
    // Display:none iframe or something... Bail out
    return NS_OK;
  }

  nsIFrame* frame = presShell->GetPrimaryFrameFor(content);
  if (frame) {
    const nsStyleText* text = frame->GetStyleText();
    *aReturn = text->mWhiteSpace != NS_STYLE_WHITESPACE_PRE &&
               text->mWhiteSpace != NS_STYLE_WHITESPACE_MOZ_PRE_WRAP;
  }
  else {
    // empty inter-tag text node without frame, e.g., in between <table>\n<tr>
    *aReturn = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetParentForNode(nsIDOMNode* aNode,
                             PRBool aShowingAnonymousContent,
                             nsIDOMNode** aParent)
{
    // First do the special cases -- document nodes and anonymous content
  nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(aNode));
  nsCOMPtr<nsIDOMNode> parent;

  if (doc) {
    parent = inLayoutUtils::GetContainerFor(doc);
  } else if (aShowingAnonymousContent) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
    if (content) {
      nsCOMPtr<nsIContent> bparent;
      nsRefPtr<nsBindingManager> bindingManager = inLayoutUtils::GetBindingManagerFor(aNode);
      if (bindingManager) {
        bindingManager->GetInsertionParent(content, getter_AddRefs(bparent));
      }
    
      parent = do_QueryInterface(bparent);
    }
  }
  
  if (!parent) {
    // Ok, just get the normal DOM parent node
    aNode->GetParentNode(getter_AddRefs(parent));
  }

  NS_IF_ADDREF(*aParent = parent);
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetCSSStyleRules(nsIDOMElement *aElement,
                             nsISupportsArray **_retval)
{
  if (!aElement) return NS_ERROR_NULL_POINTER;

  *_retval = nsnull;

  nsRuleNode* ruleNode = nsnull;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  mCSSUtils->GetRuleNodeForContent(content, &ruleNode);
  if (!ruleNode) {
    // This can fail for content nodes that are not in the document or
    // if the document they're in doesn't have a presshell.  Bail out.
    return NS_OK;
  }

  nsCOMPtr<nsISupportsArray> rules;
  NS_NewISupportsArray(getter_AddRefs(rules));
  if (!rules) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIStyleRule> srule;
  nsCOMPtr<nsICSSStyleRule> cssRule;
  nsCOMPtr<nsIDOMCSSRule> domRule;
  for (PRBool isRoot;
       mCSSUtils->IsRuleNodeRoot(ruleNode, &isRoot), !isRoot;
       mCSSUtils->GetRuleNodeParent(ruleNode, &ruleNode))
  {
    mCSSUtils->GetRuleNodeRule(ruleNode, getter_AddRefs(srule));
    cssRule = do_QueryInterface(srule);
    if (cssRule) {
      cssRule->GetDOMRule(getter_AddRefs(domRule));
      if (domRule)
        rules->InsertElementAt(domRule, 0);
    }
  }

  *_retval = rules;
  NS_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetRuleLine(nsIDOMCSSStyleRule *aRule, PRUint32 *_retval)
{
  *_retval = 0;
  if (!aRule)
    return NS_OK;
  nsCOMPtr<nsICSSStyleRuleDOMWrapper> rule = do_QueryInterface(aRule);
  nsCOMPtr<nsICSSStyleRule> cssrule;
  rule->GetCSSStyleRule(getter_AddRefs(cssrule));
  if (cssrule)
    *_retval = cssrule->GetLineNumber();
  return NS_OK;
}

NS_IMETHODIMP 
inDOMUtils::GetBindingURLs(nsIDOMElement *aElement, nsIArray **_retval)
{
  return mCSSUtils->GetBindingURLs(aElement, _retval);
}

NS_IMETHODIMP
inDOMUtils::SetContentState(nsIDOMElement *aElement, PRInt32 aState)
{
  if (!aElement)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIEventStateManager> esm = inLayoutUtils::GetEventStateManagerFor(aElement);
  if (esm) {
    nsCOMPtr<nsIContent> content;
    content = do_QueryInterface(aElement);
  
    return esm->SetContentState(content, aState);
  }
  
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inDOMUtils::GetContentState(nsIDOMElement *aElement, PRInt32* aState)
{
  *aState = 0;

  if (!aElement)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIEventStateManager> esm = inLayoutUtils::GetEventStateManagerFor(aElement);
  if (esm) {
    nsCOMPtr<nsIContent> content;
    content = do_QueryInterface(aElement);
  
    return esm->GetContentState(content, *aState);
  }

  return NS_ERROR_FAILURE;
}

