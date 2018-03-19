/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Text_h
#define mozilla_dom_Text_h

#include "mozilla/dom/CharacterData.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

class Text : public CharacterData
{
public:
  explicit Text(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : CharacterData(aNodeInfo)
  {}

  explicit Text(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : CharacterData(aNodeInfo)
  {}

  // WebIDL API
  already_AddRefed<Text> SplitText(uint32_t aOffset, ErrorResult& rv);
  void GetWholeText(nsAString& aWholeText, ErrorResult& rv);

  static already_AddRefed<Text>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aData, ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

inline mozilla::dom::Text* nsINode::GetAsText()
{
  return IsText()  ? static_cast<mozilla::dom::Text*>(this)
                   : nullptr;
}

inline const mozilla::dom::Text* nsINode::GetAsText() const
{
  return IsText() ? static_cast<const mozilla::dom::Text*>(this)
                  : nullptr;
}

#endif // mozilla_dom_Text_h
