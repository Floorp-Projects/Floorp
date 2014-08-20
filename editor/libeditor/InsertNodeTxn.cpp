/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>                      // for printf

#include "mozilla/dom/Selection.h"      // for Selection

#include "InsertNodeTxn.h"
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsEditor.h"                   // for nsEditor
#include "nsError.h"                    // for NS_ERROR_NULL_POINTER, etc
#include "nsIContent.h"                 // for nsIContent
#include "nsMemory.h"                   // for nsMemory
#include "nsReadableUtils.h"            // for ToNewCString
#include "nsString.h"                   // for nsString

using namespace mozilla;
using namespace mozilla::dom;

InsertNodeTxn::InsertNodeTxn(nsIContent& aNode, nsINode& aParent,
                             int32_t aOffset, nsEditor& aEditor)
  : EditTxn()
  , mNode(&aNode)
  , mParent(&aParent)
  , mOffset(aOffset)
  , mEditor(aEditor)
{
}

InsertNodeTxn::~InsertNodeTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(InsertNodeTxn, EditTxn,
                                   mNode,
                                   mParent)

NS_IMPL_ADDREF_INHERITED(InsertNodeTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(InsertNodeTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertNodeTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP
InsertNodeTxn::DoTransaction()
{
  MOZ_ASSERT(mNode && mParent);

  uint32_t count = mParent->GetChildCount();
  if (mOffset > static_cast<int32_t>(count) || mOffset == -1) {
    // -1 is sentinel value meaning "append at end"
    mOffset = count;
  }

  // Note, it's ok for ref to be null. That means append.
  nsCOMPtr<nsIContent> ref = mParent->GetChildAt(mOffset);

  mEditor.MarkNodeDirty(GetAsDOMNode(mNode));

  ErrorResult rv;
  mParent->InsertBefore(*mNode, ref, rv);
  NS_ENSURE_SUCCESS(rv.ErrorCode(), rv.ErrorCode());

  // Only set selection to insertion point if editor gives permission
  if (mEditor.GetShouldTxnSetSelection()) {
    nsRefPtr<Selection> selection = mEditor.GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    // Place the selection just after the inserted element
    selection->Collapse(mParent, mOffset + 1);
  } else {
    // Do nothing - DOM Range gravity will adjust selection
  }
  return NS_OK;
}

NS_IMETHODIMP
InsertNodeTxn::UndoTransaction()
{
  MOZ_ASSERT(mNode && mParent);

  ErrorResult rv;
  mParent->RemoveChild(*mNode, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
InsertNodeTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("InsertNodeTxn");
  return NS_OK;
}
