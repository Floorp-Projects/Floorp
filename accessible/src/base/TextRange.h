/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TextRange_h__
#define mozilla_a11y_TextRange_h__

#include "mozilla/Move.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace a11y {

class Accessible;
class HyperTextAccessible;

/**
 * Represents a text range within the text control or document.
 */
class TextRange MOZ_FINAL
{
public:
  TextRange(HyperTextAccessible* aRoot,
            Accessible* aStartContainer, int32_t aStartOffset,
            Accessible* aEndContainer, int32_t aEndOffset);
  TextRange() {}
  TextRange(TextRange&& aRange) :
    mRoot(Move(aRange.mRoot)), mStartContainer(Move(aRange.mStartContainer)),
    mEndContainer(Move(aRange.mEndContainer)),
    mStartOffset(aRange.mStartOffset), mEndOffset(aRange.mEndOffset) {}

  Accessible* StartContainer() const { return mStartContainer; }
  int32_t StartOffset() const { return mStartOffset; }
  Accessible* EndContainer() const { return mEndContainer; }
  int32_t EndOffset() const { return mEndOffset; }

  /**
   * Return text enclosed by the range.
   */
  void Text(nsAString& aText) const;

  /**
   * Return true if this TextRange object represents an actual range of text.
   */
  bool IsValid() const { return mRoot; }

private:
  friend class HyperTextAccessible;

  TextRange(const TextRange&) MOZ_DELETE;
  TextRange& operator=(const TextRange&) MOZ_DELETE;

  const nsRefPtr<HyperTextAccessible> mRoot;
  nsRefPtr<Accessible> mStartContainer;
  nsRefPtr<Accessible> mEndContainer;
  int32_t mStartOffset;
  int32_t mEndOffset;
};


} // namespace a11y
} // namespace mozilla

#endif
