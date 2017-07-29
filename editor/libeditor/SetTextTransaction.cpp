/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SetTextTransaction.h"

#include "mozilla/DebugOnly.h"          // DebugOnly
#include "mozilla/EditorBase.h"         // mEditorBase
#include "mozilla/SelectionState.h"     // RangeUpdater
#include "mozilla/dom/Selection.h"      // Selection local var
#include "mozilla/dom/Text.h"           // mTextNode
#include "nsAString.h"                  // nsAString parameter
#include "nsDebug.h"                    // for NS_ASSERTION, etc.
#include "nsError.h"                    // for NS_OK, etc.

namespace mozilla {

using namespace dom;

SetTextTransaction::SetTextTransaction(Text& aTextNode,
                                       const nsAString& aStringToSet,
                                       EditorBase& aEditorBase,
                                       RangeUpdater* aRangeUpdater)
  : mTextNode(&aTextNode)
  , mStringToSet(aStringToSet)
  , mEditorBase(&aEditorBase)
  , mRangeUpdater(aRangeUpdater)
{
}

nsresult
SetTextTransaction::DoTransaction()
{
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mTextNode)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = mTextNode->GetData(mPreviousData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mTextNode->SetData(mStringToSet);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Only set selection to insertion point if editor gives permission
  if (mEditorBase->GetShouldTxnSetSelection()) {
    RefPtr<Selection> selection = mEditorBase->GetSelection();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_NULL_POINTER;
    }
    DebugOnly<nsresult> rv =
      selection->Collapse(mTextNode, mStringToSet.Length());
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Selection could not be collapsed after insert");
  }
  mRangeUpdater->SelAdjDeleteText(mTextNode, 0, mPreviousData.Length());
  mRangeUpdater->SelAdjInsertText(*mTextNode, 0, mStringToSet);

  return NS_OK;
}

} // namespace mozilla
