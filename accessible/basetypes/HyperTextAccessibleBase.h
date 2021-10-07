/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _HyperTextAccessibleBase_H_
#define _HyperTextAccessibleBase_H_

namespace mozilla::a11y {
class Accessible;

// This character marks where in the text returned via Text interface,
// that embedded object characters exist
const char16_t kEmbeddedObjectChar = 0xfffc;
const char16_t kImaginaryEmbeddedObjectChar = ' ';
const char16_t kForcedNewLineChar = '\n';

/**
 * An index type. Assert if out of range value was attempted to be used.
 */
class index_t {
 public:
  MOZ_IMPLICIT index_t(int32_t aVal) : mVal(aVal) {}

  operator uint32_t() const {
    MOZ_ASSERT(mVal >= 0, "Attempt to use wrong index!");
    return mVal;
  }

  bool IsValid() const { return mVal >= 0; }

 private:
  int32_t mVal;
};

class HyperTextAccessibleBase {
 public:
  /**
   * Return child accessible at the given text offset.
   *
   * @param  aOffset  [in] the given text offset
   */
  virtual int32_t GetChildIndexAtOffset(uint32_t aOffset) const;

  /**
   * Return child accessible at the given text offset.
   *
   * @param  aOffset  [in] the given text offset
   */
  virtual Accessible* GetChildAtOffset(uint32_t aOffset) const;

  /**
   * Return text offset of the given child accessible within hypertext
   * accessible.
   *
   * @param  aChild           [in] accessible child to get text offset for
   * @param  aInvalidateAfter [in, optional] indicates whether invalidate
   *                           cached offsets for next siblings of the child
   */
  int32_t GetChildOffset(const Accessible* aChild,
                         bool aInvalidateAfter = false) const;

  /**
   * Return text offset for the child accessible index.
   */
  virtual int32_t GetChildOffset(uint32_t aChildIndex,
                                 bool aInvalidateAfter = false) const;

  /**
   * Return character count within the hypertext accessible.
   */
  uint32_t CharacterCount() const;

  /**
   * Get caret offset, if no caret then -1.
   */
  virtual int32_t CaretOffset() const = 0;

  /**
   * Transform magic offset into text offset.
   */
  index_t ConvertMagicOffset(int32_t aOffset) const;

  /**
   * Return text between given offsets.
   */
  virtual void TextSubstring(int32_t aStartOffset, int32_t aEndOffset,
                             nsAString& aText) const;

 protected:
  virtual const Accessible* Acc() const = 0;
  Accessible* Acc() {
    const Accessible* acc =
        const_cast<const HyperTextAccessibleBase*>(this)->Acc();
    return const_cast<Accessible*>(acc);
  }
};

}  // namespace mozilla::a11y

#endif
