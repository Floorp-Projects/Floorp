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
 *   Christopher A. Aillon <christopher@aillon.com>
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

#include "inDOMUtils.h"
#include "inLayoutUtils.h"

#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMCharacterData.h"
#include "nsITextContent.h"
#include "nsEnumeratorUtils.h"
#include "nsIXBLBinding.h" 
#include "nsIStyleContext.h"
#include "nsRuleNode.h"
#include "nsIStyleRule.h"
#include "nsICSSStyleRule.h"
#include "nsIDOMCSSStyleRule.h"
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

NS_IMPL_ISUPPORTS1(inDOMUtils, inIDOMUtils);

///////////////////////////////////////////////////////////////////////////////
// inIDOMUtils

// Helper to get a style context for the node, recursively resolving if needed.
nsresult
inDOMUtils::GetStyleContextForContent(nsIContent* aContent,
                                      nsIPresShell* aPresShell,
                                      nsIStyleContext** aStyleContext)
{
  NS_PRECONDITION(aContent, "Gotta have a content");
  NS_PRECONDITION(aPresShell, "Need a PresShell");
  NS_PRECONDITION(aStyleContext, "Need somewhere to put the result");
  
  *aStyleContext = nsnull;

  nsIFrame* frame;
  aPresShell->GetPrimaryFrameFor(aContent, &frame);
  if (frame) {
    return mCSSUtils->GetStyleContextForFrame(frame, aStyleContext);
  }

  // No frame.  Do this the hard way.
  // Get the parent style context
  nsCOMPtr<nsIStyleContext> parentContext;
  nsCOMPtr<nsIContent> parent;
  aContent->GetParent(*getter_AddRefs(parent));
  if (parent) {
    nsresult rv = GetStyleContextForContent(parent, aPresShell,
                                            getter_AddRefs(parentContext));
    if (NS_FAILED(rv))
      return rv;
  }

  nsCOMPtr<nsIPresContext> presContext;
  aPresShell->GetPresContext(getter_AddRefs(presContext));
  NS_ENSURE_TRUE(presContext, NS_ERROR_UNEXPECTED);

  if (aContent->IsContentOfType(nsIContent::eELEMENT)) {
    return presContext->ResolveStyleContextFor(aContent, parentContext,
                                               aStyleContext);
  }

  return presContext->ResolveStyleContextForNonElement(parentContext,
                                                       aStyleContext);
}

NS_IMETHODIMP
inDOMUtils::IsIgnorableWhitespace(nsIDOMCharacterData *aDataNode,
                                  PRBool *aReturn)
{
  NS_PRECONDITION(aDataNode, "Must have a character data node");
  NS_PRECONDITION(aReturn, "Must have an out parameter");

  *aReturn = PR_FALSE;

  nsCOMPtr<nsITextContent> textContent = do_QueryInterface(aDataNode);
  NS_ASSERTION(textContent, "Does not implement nsITextContent!");

  PRBool whiteSpaceOnly = PR_FALSE;
  textContent->IsOnlyWhitespace(&whiteSpaceOnly);
  if (!whiteSpaceOnly) {
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
  NS_ASSERTION(presShell, "No pres shell!");

  nsCOMPtr<nsIStyleContext> styleContext;
  GetStyleContextForContent(textContent, presShell,
                            getter_AddRefs(styleContext));

  if (styleContext) {
    PRBool significant = PR_FALSE;
    mCSSUtils->IsWhiteSpaceSignificant(styleContext, &significant);

    *aReturn = !significant;
  }
  else {
    // No style context.  Let's just assume the default value of
    // white-space: normal, which we can safely ignore.
    *aReturn = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetStyleRules(nsIDOMElement *aElement, nsISupportsArray **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsISupportsArray> rules;
  NS_NewISupportsArray(getter_AddRefs(rules));
  if (!rules) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIDOMWindowInternal> win(inLayoutUtils::GetWindowFor(aElement));
  if (!win) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIPresShell> shell(inLayoutUtils::GetPresShellFor(win));
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
  nsCOMPtr<nsIStyleContext> styleContext;

  GetStyleContextForContent(content, shell,
                            getter_AddRefs(styleContext));
  if (!styleContext) {
    NS_ERROR("no StyleContext");
    return NS_ERROR_UNEXPECTED;
  }

  nsRuleNode* ruleNode = nsnull;
  styleContext->GetRuleNode(&ruleNode);

  nsCOMPtr<nsIStyleRule> srule;
  for (PRBool isRoot;
       mCSSUtils->IsRuleNodeRoot(ruleNode, &isRoot), !isRoot;
       mCSSUtils->GetRuleNodeParent(ruleNode, &ruleNode))
  {
    mCSSUtils->GetRuleNodeRule(ruleNode, getter_AddRefs(srule));
    rules->InsertElementAt(srule, 0);
  }

  *_retval = rules;
  NS_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetRuleWeight(nsIDOMCSSStyleRule *aRule, PRUint32 *_retval)
{
  if (!aRule) return NS_OK;
  nsCOMPtr<nsIDOMCSSStyleRule> rule = aRule;
  nsCOMPtr<nsICSSStyleRule> cssrule = do_QueryInterface(rule);
  *_retval = cssrule->GetWeight();
  return NS_OK;
}

NS_IMETHODIMP
inDOMUtils::GetRuleLine(nsIDOMCSSStyleRule *aRule, PRUint32 *_retval)
{
  if (!aRule) return NS_OK;
  nsCOMPtr<nsIDOMCSSStyleRule> rule = aRule;
  nsCOMPtr<nsICSSStyleRule> cssrule = do_QueryInterface(rule);
  *_retval = cssrule->GetLineNumber();
  return NS_OK;
}

NS_IMETHODIMP 
inDOMUtils::GetBindingURLs(nsIDOMElement *aElement, nsISimpleEnumerator **_retval)
{
  nsCOMPtr<nsISupportsArray> urls;
  NS_NewISupportsArray(getter_AddRefs(urls));
  nsCOMPtr<nsISimpleEnumerator> e;
  NS_NewArrayEnumerator(getter_AddRefs(e), urls);

  *_retval = e;
  NS_ADDREF(*_retval);

  nsCOMPtr<nsIDOMDocument> doc1;
  aElement->GetOwnerDocument(getter_AddRefs(doc1));
  if (!doc1) return NS_OK;
  
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(doc1);
  nsCOMPtr<nsIBindingManager> bindingManager;
  doc->GetBindingManager(getter_AddRefs(bindingManager));
  if (!bindingManager) return NS_OK;
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  nsCOMPtr<nsIXBLBinding> binding;
  bindingManager->GetBinding(content, getter_AddRefs(binding));
  
  nsCOMPtr<nsIXBLBinding> tempBinding;
  while (binding) {
    nsCString id;
    binding->GetID(id);
    nsCString uri;
    binding->GetDocURI(uri);
    uri += "#";
    uri += id;
  
    nsCOMPtr<nsIAtom> a = NS_NewAtom(uri.get());
    urls->AppendElement(a);
    
    binding->GetBaseBinding(getter_AddRefs(tempBinding));
    binding = tempBinding;
  }
    
  return NS_OK;
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
  *aState = NS_EVENT_STATE_UNSPECIFIED;

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

