/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HyperTextAccessibleWrap_h__
#define mozilla_a11y_HyperTextAccessibleWrap_h__

#include "HyperTextAccessible.h"

namespace mozilla {
namespace a11y {

struct TextPoint;

class HyperTextAccessibleWrap : public HyperTextAccessible {
 public:
  HyperTextAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc)
      : HyperTextAccessible(aContent, aDoc) {}

  void TextForRange(nsAString& aText, int32_t aStartOffset,
                    HyperTextAccessible* aEndContainer, int32_t aEndOffset);

  nsIntRect BoundsForRange(int32_t aStartOffset,
                           HyperTextAccessible* aEndContainer,
                           int32_t aEndOffset);

  void LeftWordAt(int32_t aOffset, HyperTextAccessible** aStartContainer,
                  int32_t* aStartOffset, HyperTextAccessible** aEndContainer,
                  int32_t* aEndOffset);

  void RightWordAt(int32_t aOffset, HyperTextAccessible** aStartContainer,
                   int32_t* aStartOffset, HyperTextAccessible** aEndContainer,
                   int32_t* aEndOffset);

  void NextClusterAt(int32_t aOffset, HyperTextAccessible** aNextContainer,
                     int32_t* aNextOffset);

  void PreviousClusterAt(int32_t aOffset, HyperTextAccessible** aPrevContainer,
                         int32_t* aPrevOffset);

 protected:
  ~HyperTextAccessibleWrap() {}

  TextPoint FindTextPoint(int32_t aOffset, nsDirection aDirection,
                          nsSelectionAmount aAmount,
                          EWordMovementType aWordMovementType);

  HyperTextAccessibleWrap* EditableRoot();
};

}  // namespace a11y
}  // namespace mozilla

#endif
