/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleTextRange_h_
#define mozilla_a11y_xpcAccessibleTextRange_h_

#include <utility>

#include "TextRange.h"
#include "nsIAccessibleTextRange.h"
#include "xpcAccessibleHyperText.h"

namespace mozilla {
namespace a11y {

class TextRange;

#define NS_ACCESSIBLETEXTRANGE_IMPL_IID              \
  { /* 133c8bf4-4913-4355-bd50-426bd1d6e1ad */       \
    0xb17652d9, 0x4f54, 0x4c56, {                    \
      0xbb, 0x62, 0x6d, 0x5b, 0xf1, 0xef, 0x91, 0x0c \
    }                                                \
  }

class xpcAccessibleTextRange final : public nsIAccessibleTextRange {
 public:
  explicit xpcAccessibleTextRange(TextRange& aRange) { SetRange(aRange); }

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStartContainer(nsIAccessibleText** aAnchor) final;
  NS_IMETHOD GetStartOffset(int32_t* aOffset) final;
  NS_IMETHOD GetEndContainer(nsIAccessibleText** aAnchor) final;
  NS_IMETHOD GetEndOffset(int32_t* aOffset) final;
  NS_IMETHOD GetContainer(nsIAccessible** aContainer) final;
  NS_IMETHOD GetEmbeddedChildren(nsIArray** aList) final;
  NS_IMETHOD Compare(nsIAccessibleTextRange* aOtherRange, bool* aResult) final;
  NS_IMETHOD CompareEndPoints(uint32_t aEndPoint,
                              nsIAccessibleTextRange* aOtherRange,
                              uint32_t aOtherRangeEndPoint,
                              int32_t* aResult) final;
  NS_IMETHOD GetText(nsAString& aText) final;
  NS_IMETHOD Crop(nsIAccessible* aContainer, bool* aSuccess) final;
  NS_IMETHOD ScrollIntoView(uint32_t aHow) final;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ACCESSIBLETEXTRANGE_IMPL_IID)

 private:
  xpcAccessibleTextRange() {}

  ~xpcAccessibleTextRange() {}

  friend class xpcAccessibleHyperText;

  xpcAccessibleTextRange& operator=(const xpcAccessibleTextRange&) = delete;

  void SetRange(TextRange& aRange);

  TextRange Range();

  // We can't hold a strong reference to an Accessible, but XPCOM needs strong
  // references. Thus, instead of holding a TextRange here, we hold
  // xpcAccessibleHyperText references and create the TextRange for each call
  // using Range().
  RefPtr<xpcAccessibleHyperText> mRoot;
  RefPtr<xpcAccessibleHyperText> mStartContainer;
  int32_t mStartOffset;
  RefPtr<xpcAccessibleHyperText> mEndContainer;
  int32_t mEndOffset;
};

NS_DEFINE_STATIC_IID_ACCESSOR(xpcAccessibleTextRange,
                              NS_ACCESSIBLETEXTRANGE_IMPL_IID)

}  // namespace a11y
}  // namespace mozilla

#endif
