/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FilteredContentIterator_h
#define FilteredContentIterator_h

#include "nsComposeTxtSrvFilter.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "nscore.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/UniquePtr.h"

class nsAtom;
class nsINode;
class nsRange;

namespace mozilla {

namespace dom {
class AbstractRange;
}

class FilteredContentIterator final {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(FilteredContentIterator)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(FilteredContentIterator)

  explicit FilteredContentIterator(UniquePtr<nsComposeTxtSrvFilter> aFilter);

  nsresult Init(nsINode* aRoot);
  nsresult Init(const dom::AbstractRange* aAbstractRange);
  nsresult Init(nsINode* aStartContainer, uint32_t aStartOffset,
                nsINode* aEndContainer, uint32_t aEndOffset);
  nsresult Init(const RawRangeBoundary& aStartBoundary,
                const RawRangeBoundary& aEndBoundary);
  void First();
  void Last();
  void Next();
  void Prev();
  nsINode* GetCurrentNode();
  bool IsDone();
  nsresult PositionAt(nsINode* aCurNode);

  /* Helpers */
  bool DidSkip() { return mDidSkip; }
  void ClearDidSkip() { mDidSkip = false; }

 protected:
  FilteredContentIterator()
      : mDidSkip(false), mIsOutOfRange(false), mDirection{eDirNotSet} {}

  virtual ~FilteredContentIterator();

  /**
   * Callers must guarantee that mRange isn't nullptr and it's positioned.
   */
  nsresult InitWithRange();

  // enum to give us the direction
  typedef enum { eDirNotSet, eForward, eBackward } eDirectionType;
  nsresult AdvanceNode(nsINode* aNode, nsINode*& aNewNode, eDirectionType aDir);
  void CheckAdvNode(nsINode* aNode, bool& aDidSkip, eDirectionType aDir);
  nsresult SwitchDirections(bool aChangeToForward);

  SafeContentIteratorBase* MOZ_NON_OWNING_REF mCurrentIterator;
  PostContentIterator mPostIterator;
  PreContentIterator mPreIterator;

  UniquePtr<nsComposeTxtSrvFilter> mFilter;
  RefPtr<nsRange> mRange;
  bool mDidSkip;
  bool mIsOutOfRange;
  eDirectionType mDirection;
};

}  // namespace mozilla

#endif  // #ifndef FilteredContentIterator_h
