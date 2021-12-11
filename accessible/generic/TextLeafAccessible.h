/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TextLeafAccessible_h__
#define mozilla_a11y_TextLeafAccessible_h__

#include "BaseAccessibles.h"

namespace mozilla {
namespace a11y {

/**
 * Generic class used for text nodes.
 */
class TextLeafAccessible : public LinkableAccessible {
 public:
  TextLeafAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~TextLeafAccessible();

  // LocalAccessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual void AppendTextTo(nsAString& aText, uint32_t aStartOffset = 0,
                            uint32_t aLength = UINT32_MAX) override;
  virtual ENameValueFlag Name(nsString& aName) const override;

  // TextLeafAccessible
  void SetText(const nsAString& aText) { mText = aText; }
  const nsString& Text() const { return mText; }

 protected:
  nsString mText;
};

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible downcast method

inline TextLeafAccessible* LocalAccessible::AsTextLeaf() {
  return IsTextLeaf() ? static_cast<TextLeafAccessible*>(this) : nullptr;
}

}  // namespace a11y
}  // namespace mozilla

#endif
