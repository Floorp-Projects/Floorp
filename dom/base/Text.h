/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Text_h
#define mozilla_dom_Text_h

#include "mozilla/dom/CharacterData.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class Text : public CharacterData {
 public:
  NS_IMPL_FROMNODE_HELPER(Text, IsText())

  explicit Text(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : CharacterData(std::move(aNodeInfo)) {}

  // WebIDL API
  already_AddRefed<Text> SplitText(uint32_t aOffset, ErrorResult& rv);
  void GetWholeText(nsAString& aWholeText);

  static already_AddRefed<Text> Constructor(const GlobalObject& aGlobal,
                                            const nsAString& aData,
                                            ErrorResult& aRv);

  /**
   * Method to see if the text node contains data that is useful
   * for a translation: i.e., it consists of more than just whitespace,
   * digits and punctuation.
   */
  bool HasTextForTranslation();
};

}  // namespace dom
}  // namespace mozilla

inline mozilla::dom::Text* nsINode::GetAsText() {
  return IsText() ? AsText() : nullptr;
}

inline const mozilla::dom::Text* nsINode::GetAsText() const {
  return IsText() ? AsText() : nullptr;
}

inline mozilla::dom::Text* nsINode::AsText() {
  MOZ_ASSERT(IsText());
  return static_cast<mozilla::dom::Text*>(this);
}

inline const mozilla::dom::Text* nsINode::AsText() const {
  MOZ_ASSERT(IsText());
  return static_cast<const mozilla::dom::Text*>(this);
}

#endif  // mozilla_dom_Text_h
