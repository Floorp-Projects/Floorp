/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Text_h
#define mozilla_dom_Text_h

#include "nsGenericDOMDataNode.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

class Text : public nsGenericDOMDataNode
{
public:
  explicit Text(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericDOMDataNode(aNodeInfo)
  {}

  explicit Text(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericDOMDataNode(aNodeInfo)
  {}

  using nsGenericDOMDataNode::GetWholeText;

  // WebIDL API
  already_AddRefed<Text> SplitText(uint32_t aOffset, ErrorResult& rv);
  void GetWholeText(nsAString& aWholeText, ErrorResult& rv)
  {
    rv = GetWholeText(aWholeText);
  }

  static already_AddRefed<Text>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aData, ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

inline mozilla::dom::Text* nsINode::GetAsText()
{
  return IsNodeOfType(eTEXT) ? static_cast<mozilla::dom::Text*>(this)
                             : nullptr;
}

inline const mozilla::dom::Text* nsINode::GetAsText() const
{
  return IsNodeOfType(eTEXT) ? static_cast<const mozilla::dom::Text*>(this)
                             : nullptr;
}

#endif // mozilla_dom_Text_h
