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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
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

#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsIStyleRule.h"
#include "nsFixedSizeAllocator.h"
#include "nsIRuleNode.h"
#include "nsIRuleWalker.h"

class nsRuleWalker: public nsIRuleWalker {
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetCurrentNode(nsIRuleNode** aResult);
  NS_IMETHOD SetCurrentNode(nsIRuleNode* aNode);

  NS_IMETHOD Forward(nsIStyleRule* aRule);
  NS_IMETHOD Back();

  NS_IMETHOD Reset();

  NS_IMETHOD AtRoot(PRBool* aResult);

private:
  nsCOMPtr<nsIRuleNode> mCurrent; // Our current position.
  nsCOMPtr<nsIRuleNode> mRoot; // The root of the tree we're walking.

protected:
public:
  nsRuleWalker(nsIRuleNode* aRoot) :mRoot(aRoot), mCurrent(aRoot) { NS_INIT_REFCNT(); };
  virtual ~nsRuleWalker() {};
};

NS_IMPL_ISUPPORTS1(nsRuleWalker, nsIRuleWalker)

NS_IMETHODIMP
nsRuleWalker::GetCurrentNode(nsIRuleNode** aResult)
{
  *aResult = mCurrent;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsRuleWalker::SetCurrentNode(nsIRuleNode* aNode)
{
  mCurrent = aNode;
  return NS_OK;
}

NS_IMETHODIMP
nsRuleWalker::Forward(nsIStyleRule* aRule)
{
  if (!mCurrent)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRuleNode> next;
  mCurrent->Transition(aRule, getter_AddRefs(next));
  mCurrent = next;

  return NS_OK;
}

NS_IMETHODIMP
nsRuleWalker::Back()
{
  if (mCurrent == mRoot)
    return NS_OK;

  nsCOMPtr<nsIRuleNode> prev;
  mCurrent->GetParent(getter_AddRefs(prev));
  mCurrent = prev;

  return NS_OK;
}

NS_IMETHODIMP
nsRuleWalker::Reset()
{
  mCurrent = mRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsRuleWalker::AtRoot(PRBool* aResult)
{
  *aResult = (mCurrent == mRoot);
  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewRuleWalker(nsIRuleNode* aRoot, nsIRuleWalker** aResult)
{
  *aResult = new nsRuleWalker(aRoot);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

