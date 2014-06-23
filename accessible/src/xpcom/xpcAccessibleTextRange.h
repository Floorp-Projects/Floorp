/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleTextRange_h_
#define mozilla_a11y_xpcAccessibleTextRange_h_

#include "nsIAccessibleTextRange.h"
#include "TextRange.h"

#include "mozilla/Move.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace a11y {

class TextRange;

#define NS_ACCESSIBLETEXTRANGE_IMPL_IID                 \
{  /* 133c8bf4-4913-4355-bd50-426bd1d6e1ad */           \
  0xb17652d9,                                           \
  0x4f54,                                               \
  0x4c56,                                               \
  { 0xbb, 0x62, 0x6d, 0x5b, 0xf1, 0xef, 0x91, 0x0c }    \
}

class xpcAccessibleTextRange MOZ_FINAL : public nsIAccessibleTextRange
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(xpcAccessibleTextRange)

  NS_IMETHOD GetStartContainer(nsIAccessibleText** aAnchor) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD GetStartOffset(int32_t* aOffset) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD GetEndContainer(nsIAccessibleText** aAnchor) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD GetEndOffset(int32_t* aOffset) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD GetContainer(nsIAccessible** aContainer) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD GetEmbeddedChildren(nsIArray** aList) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD Compare(nsIAccessibleTextRange* aOtherRange, bool* aResult) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD CompareEndPoints(uint32_t aEndPoint,
                              nsIAccessibleTextRange* aOtherRange,
                              uint32_t aOtherRangeEndPoint,
                              int32_t* aResult) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD GetText(nsAString& aText) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD GetBounds(nsIArray** aRectList) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD Move(uint32_t aUnit, int32_t aCount) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD MoveStart(uint32_t aUnit, int32_t aCount) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD MoveEnd(uint32_t aUnit, int32_t aCount) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD Normalize(uint32_t aUnit) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD FindText(const nsAString& aText, bool aIsBackward, bool aIsIgnoreCase,
                      nsIAccessibleTextRange** aRange) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD FindAttr(uint32_t aAttr, nsIVariant* aVal, bool aIsBackward,
                      nsIAccessibleTextRange** aRange) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD AddToSelection() MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD RemoveFromSelection() MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD Select() MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD ScrollIntoView(uint32_t aHow) MOZ_FINAL MOZ_OVERRIDE;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ACCESSIBLETEXTRANGE_IMPL_IID)

private:
  xpcAccessibleTextRange(TextRange&& aRange) :
    mRange(Forward<TextRange>(aRange)) {}
  xpcAccessibleTextRange() {}

  ~xpcAccessibleTextRange() {}

  friend class xpcAccessibleHyperText;

  xpcAccessibleTextRange(const xpcAccessibleTextRange&) MOZ_DELETE;
  xpcAccessibleTextRange& operator =(const xpcAccessibleTextRange&) MOZ_DELETE;

  TextRange mRange;
};

NS_DEFINE_STATIC_IID_ACCESSOR(xpcAccessibleTextRange,
                              NS_ACCESSIBLETEXTRANGE_IMPL_IID)

} // namespace a11y
} // namespace mozilla

#endif
