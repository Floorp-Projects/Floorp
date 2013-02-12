/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessNode.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "RootAccessible.h"

#include "nsIDocShell.h"
#include "nsIDOMWindow.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsFocusManager.h"
#include "nsPresContext.h"
#include "mozilla/Services.h"

using namespace mozilla::a11y;

/* For documentation of the accessibility architecture,
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

/*
 * Class nsAccessNode
 */

////////////////////////////////////////////////////////////////////////////////
// AccessNode. nsISupports

NS_IMPL_CYCLE_COLLECTION_1(nsAccessNode, mContent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsAccessNode)
  NS_INTERFACE_MAP_ENTRY(nsAccessNode)
NS_INTERFACE_MAP_END
 
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAccessNode)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(nsAccessNode, LastRelease())

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode construction/desctruction

nsAccessNode::
  nsAccessNode(nsIContent* aContent, DocAccessible* aDoc) :
  mContent(aContent), mDoc(aDoc)
{
}

nsAccessNode::~nsAccessNode()
{
  NS_ASSERTION(!mDoc, "LastRelease was never called!?!");
}

void nsAccessNode::LastRelease()
{
  // First cleanup if needed...
  if (mDoc) {
    Shutdown();
    NS_ASSERTION(!mDoc, "A Shutdown() impl forgot to call its parent's Shutdown?");
  }
  // ... then die.
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode public


void
nsAccessNode::Shutdown()
{
  mContent = nullptr;
  mDoc = nullptr;
}

RootAccessible*
nsAccessNode::RootAccessible() const
{
  nsCOMPtr<nsIDocShell> docShell = nsCoreUtils::GetDocShellFor(GetNode());
  NS_ASSERTION(docShell, "No docshell for mContent");
  if (!docShell) {
    return nullptr;
  }
  nsCOMPtr<nsIDocShellTreeItem> root;
  docShell->GetRootTreeItem(getter_AddRefs(root));
  NS_ASSERTION(root, "No root content tree item");
  if (!root) {
    return nullptr;
  }

  DocAccessible* docAcc = nsAccUtils::GetDocAccessibleFor(root);
  return docAcc ? docAcc->AsRoot() : nullptr;
}

nsIFrame*
nsAccessNode::GetFrame() const
{
  return mContent ? mContent->GetPrimaryFrame() : nullptr;
}

nsINode*
nsAccessNode::GetNode() const
{
  return mContent;
}

void
nsAccessNode::Language(nsAString& aLanguage)
{
  aLanguage.Truncate();

  if (!mDoc)
    return;

  nsCoreUtils::GetLanguageFor(mContent, nullptr, aLanguage);
  if (aLanguage.IsEmpty()) { // Nothing found, so use document's language
    mDoc->DocumentNode()->GetHeaderData(nsGkAtoms::headerContentLanguage,
                                        aLanguage);
  }
}

