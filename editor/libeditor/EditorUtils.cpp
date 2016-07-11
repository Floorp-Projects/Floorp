/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditorUtils.h"

#include "mozilla/OwningNonNull.h"
#include "mozilla/dom/Selection.h"
#include "nsComponentManagerUtils.h"
#include "nsError.h"
#include "nsIClipboardDragDropHookList.h"
// hooks
#include "nsIClipboardDragDropHooks.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMDocument.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINode.h"
#include "nsISimpleEnumerator.h"

class nsISupports;
class nsRange;

namespace mozilla {

using namespace dom;

/******************************************************************************
 * AutoSelectionRestorer
 *****************************************************************************/

AutoSelectionRestorer::AutoSelectionRestorer(
                         Selection* aSelection,
                         EditorBase* aEditorBase
                         MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : mEditorBase(nullptr)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  if (NS_WARN_IF(!aSelection) || NS_WARN_IF(!aEditorBase)) {
    return;
  }
  if (aEditorBase->ArePreservingSelection()) {
    // We already have initialized mSavedSel, so this must be nested call.
    return;
  }
  mSelection = aSelection;
  mEditorBase = aEditorBase;
  mEditorBase->PreserveSelectionAcrossActions(mSelection);
}

AutoSelectionRestorer::~AutoSelectionRestorer()
{
  NS_ASSERTION(!mSelection || mEditorBase,
               "mEditorBase should be non-null when mSelection is");
  // mSelection will be null if this was nested call.
  if (mSelection && mEditorBase->ArePreservingSelection()) {
    mEditorBase->RestorePreservedSelection(mSelection);
  }
}

void
AutoSelectionRestorer::Abort()
{
  NS_ASSERTION(!mSelection || mEditorBase,
               "mEditorBase should be non-null when mSelection is");
  if (mSelection) {
    mEditorBase->StopPreservingSelection();
  }
}

/******************************************************************************
 * some helper classes for iterating the dom tree
 *****************************************************************************/

DOMIterator::DOMIterator(nsINode& aNode MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  mIter = NS_NewContentIterator();
  DebugOnly<nsresult> res = mIter->Init(&aNode);
  MOZ_ASSERT(NS_SUCCEEDED(res));
}

nsresult
DOMIterator::Init(nsRange& aRange)
{
  mIter = NS_NewContentIterator();
  return mIter->Init(&aRange);
}

DOMIterator::DOMIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
}

DOMIterator::~DOMIterator()
{
}

void
DOMIterator::AppendList(const BoolDomIterFunctor& functor,
                        nsTArray<OwningNonNull<nsINode>>& arrayOfNodes) const
{
  // Iterate through dom and build list
  for (; !mIter->IsDone(); mIter->Next()) {
    nsCOMPtr<nsINode> node = mIter->GetCurrentNode();

    if (functor(node)) {
      arrayOfNodes.AppendElement(*node);
    }
  }
}

DOMSubtreeIterator::DOMSubtreeIterator(
                      MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
  : DOMIterator(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_TO_PARENT)
{
}

nsresult
DOMSubtreeIterator::Init(nsRange& aRange)
{
  mIter = NS_NewContentSubtreeIterator();
  return mIter->Init(&aRange);
}

DOMSubtreeIterator::~DOMSubtreeIterator()
{
}

/******************************************************************************
 * some general purpose editor utils
 *****************************************************************************/

bool
EditorUtils::IsDescendantOf(nsINode* aNode,
                            nsINode* aParent,
                            int32_t* aOffset)
{
  MOZ_ASSERT(aNode && aParent);
  if (aNode == aParent) {
    return false;
  }

  for (nsCOMPtr<nsINode> node = aNode; node; node = node->GetParentNode()) {
    if (node->GetParentNode() == aParent) {
      if (aOffset) {
        *aOffset = aParent->IndexOf(node);
      }
      return true;
    }
  }

  return false;
}

bool
EditorUtils::IsDescendantOf(nsIDOMNode* aNode,
                            nsIDOMNode* aParent,
                            int32_t* aOffset)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  NS_ENSURE_TRUE(node && parent, false);
  return IsDescendantOf(node, parent, aOffset);
}

bool
EditorUtils::IsLeafNode(nsIDOMNode* aNode)
{
  bool hasChildren = false;
  if (aNode)
    aNode->HasChildNodes(&hasChildren);
  return !hasChildren;
}

/******************************************************************************
 * utility methods for drag/drop/copy/paste hooks
 *****************************************************************************/

nsresult
EditorHookUtils::GetHookEnumeratorFromDocument(nsIDOMDocument* aDoc,
                                               nsISimpleEnumerator** aResult)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShell> docShell = doc->GetDocShell();
  nsCOMPtr<nsIClipboardDragDropHookList> hookObj = do_GetInterface(docShell);
  NS_ENSURE_TRUE(hookObj, NS_ERROR_FAILURE);

  return hookObj->GetHookEnumerator(aResult);
}

bool
EditorHookUtils::DoInsertionHook(nsIDOMDocument* aDoc,
                                 nsIDOMEvent* aDropEvent,
                                 nsITransferable *aTrans)
{
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  GetHookEnumeratorFromDocument(aDoc, getter_AddRefs(enumerator));
  NS_ENSURE_TRUE(enumerator, true);

  bool hasMoreHooks = false;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreHooks)) && hasMoreHooks)
  {
    nsCOMPtr<nsISupports> isupp;
    if (NS_FAILED(enumerator->GetNext(getter_AddRefs(isupp))))
      break;

    nsCOMPtr<nsIClipboardDragDropHooks> override = do_QueryInterface(isupp);
    if (override)
    {
      bool doInsert = true;
#ifdef DEBUG
      nsresult hookResult =
#endif
      override->OnPasteOrDrop(aDropEvent, aTrans, &doInsert);
      NS_ASSERTION(NS_SUCCEEDED(hookResult), "hook failure in OnPasteOrDrop");
      NS_ENSURE_TRUE(doInsert, false);
    }
  }

  return true;
}

} // namespace mozilla
