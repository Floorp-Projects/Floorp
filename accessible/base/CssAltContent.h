/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CssAltContent_h_
#define CssAltContent_h_

#include "nsStyleStruct.h"

namespace mozilla::a11y {

/**
 * Queries alternative text specified in the CSS content property.
 */
class MOZ_STACK_CLASS CssAltContent {
 public:
  explicit CssAltContent(nsIContent* aContent);

  /**
   * Checks whether any CSS alt text has been specified. For example:
   * if (CssAltContent(someContentNode)) ...
   */
  explicit operator bool() const { return !mItems.IsEmpty(); }

  /**
   * Append all CSS alt text to a string.
   */
  void AppendToString(nsAString& aOut);

  /**
   * Update accessibility if there is CSS alt content on the given element or a
   * descendant pseudo-element which references the given attribute.
   */
  static bool HandleAttributeChange(nsIContent* aContent, int32_t aNameSpaceID,
                                    nsAtom* aAttribute);

 private:
  bool HandleAttributeChange(int32_t aNameSpaceID, nsAtom* aAttribute);

  dom::Element* mRealElement = nullptr;
  dom::Element* mPseudoElement = nullptr;
  mozilla::Span<const mozilla::StyleContentItem> mItems;
};

}  // namespace mozilla::a11y

#endif
