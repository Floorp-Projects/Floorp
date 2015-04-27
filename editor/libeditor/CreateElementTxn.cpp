/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CreateElementTxn.h"

#include <algorithm>
#include <stdio.h>

#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"

#include "mozilla/Casting.h"

#include "nsAlgorithm.h"
#include "nsAString.h"
#include "nsDebug.h"
#include "nsEditor.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIDOMCharacterData.h"
#include "nsIEditor.h"
#include "nsINode.h"
#include "nsISupportsUtils.h"
#include "nsMemory.h"
#include "nsReadableUtils.h"
#include "nsStringFwd.h"
#include "nsString.h"

using namespace mozilla;
using namespace mozilla::dom;

CreateElementTxn::CreateElementTxn(nsEditor& aEditor,
                                   nsIAtom& aTag,
                                   nsINode& aParent,
                                   int32_t aOffsetInParent)
  : EditTxn()
  , mEditor(&aEditor)
  , mTag(&aTag)
  , mParent(&aParent)
  , mOffsetInParent(aOffsetInParent)
{
}

CreateElementTxn::~CreateElementTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(CreateElementTxn, EditTxn,
                                   mParent,
                                   mNewNode,
                                   mRefNode)

NS_IMPL_ADDREF_INHERITED(CreateElementTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(CreateElementTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CreateElementTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)


NS_IMETHODIMP
CreateElementTxn::DoTransaction()
{
  MOZ_ASSERT(mEditor && mTag && mParent);

  mNewNode = mEditor->CreateHTMLContent(mTag);
  NS_ENSURE_STATE(mNewNode);

  // Try to insert formatting whitespace for the new node:
  mEditor->MarkNodeDirty(GetAsDOMNode(mNewNode));

  // Insert the new node
  ErrorResult rv;
  if (mOffsetInParent == -1) {
    mParent->AppendChild(*mNewNode, rv);
    return rv.StealNSResult();
  }

  mOffsetInParent = std::min(mOffsetInParent,
                             static_cast<int32_t>(mParent->GetChildCount()));

  // Note, it's ok for mRefNode to be null. That means append
  mRefNode = mParent->GetChildAt(mOffsetInParent);

  mParent->InsertBefore(*mNewNode, mRefNode, rv);
  NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());

  // Only set selection to insertion point if editor gives permission
  if (!mEditor->GetShouldTxnSetSelection()) {
    // Do nothing - DOM range gravity will adjust selection
    return NS_OK;
  }

  nsRefPtr<Selection> selection = mEditor->GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  rv = selection->CollapseNative(mParent, mParent->IndexOf(mNewNode) + 1);
  NS_ASSERTION(!rv.Failed(),
               "selection could not be collapsed after insert");
  return NS_OK;
}

NS_IMETHODIMP
CreateElementTxn::UndoTransaction()
{
  MOZ_ASSERT(mEditor && mParent);

  ErrorResult rv;
  mParent->RemoveChild(*mNewNode, rv);

  return rv.StealNSResult();
}

NS_IMETHODIMP
CreateElementTxn::RedoTransaction()
{
  MOZ_ASSERT(mEditor && mParent);

  // First, reset mNewNode so it has no attributes or content
  // XXX We never actually did this, we only cleared mNewNode's contents if it
  // was a CharacterData node (which it's not, it's an Element)

  // Now, reinsert mNewNode
  ErrorResult rv;
  mParent->InsertBefore(*mNewNode, mRefNode, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
CreateElementTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("CreateElementTxn: ");
  aString += nsDependentAtomString(mTag);
  return NS_OK;
}

already_AddRefed<Element>
CreateElementTxn::GetNewNode()
{
  return nsCOMPtr<Element>(mNewNode).forget();
}
