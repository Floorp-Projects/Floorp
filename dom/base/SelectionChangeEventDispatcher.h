/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SelectionChangeEventDispatcher_h
#define mozilla_SelectionChangeEventDispatcher_h

#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"

class nsINode;
class nsRange;

namespace mozilla {

namespace dom {
class Document;
class Selection;
}  // namespace dom

class SelectionChangeEventDispatcher final {
 public:
  // SelectionChangeEventDispatcher has to participate in cycle collection
  // because it holds strong references to nsINodes in its mOldRanges array.
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(
      SelectionChangeEventDispatcher)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(SelectionChangeEventDispatcher)

  MOZ_CAN_RUN_SCRIPT
  void OnSelectionChange(dom::Document* aDocument, dom::Selection* aSelection,
                         int16_t aReason);

  // This field is used to keep track of the ranges which were present in the
  // selection when the selectionchange event was previously fired. This allows
  // for the selectionchange event to only be fired when a selection is actually
  // changed.
  struct RawRangeData {
    // These properties are not void*s to avoid the potential situation where
    // the nsINode is freed, and a new nsINode is allocated with the same
    // address, which could potentially break the comparison logic. In reality,
    // this is extremely unlikely to occur (potentially impossible), but these
    // nsCOMPtrs are safer. They are never dereferenced.
    nsCOMPtr<nsINode> mStartContainer;
    nsCOMPtr<nsINode> mEndContainer;

    // XXX These are int32_ts on nsRange, but uint32_ts in the return value
    // of GetStart_, so I use uint32_ts here. See bug 1194256.
    uint32_t mStartOffset;
    uint32_t mEndOffset;

    explicit RawRangeData(const nsRange* aRange);
    bool Equals(const nsRange* aRange);
  };

 private:
  nsTArray<RawRangeData> mOldRanges;

  ~SelectionChangeEventDispatcher() {}
};

}  // namespace mozilla

#endif  // mozilla_SelectionChangeEventDispatcher_h
