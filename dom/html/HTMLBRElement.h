/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLBRElement_h
#define mozilla_dom_HTMLBRElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"

namespace mozilla {
namespace dom {

#define BR_ELEMENT_FLAG_BIT(n_) \
  NODE_FLAG_BIT(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// BR element specific bits
enum {
  // NS_PADDING_FOR_EMPTY_EDITOR is set if the <br> element is created by
  // editor for placing caret at proper position in empty editor.
  NS_PADDING_FOR_EMPTY_EDITOR = BR_ELEMENT_FLAG_BIT(0),

  // NS_PADDING_FOR_EMPTY_LAST_LINE is set if the <br> element is created by
  // editor for placing caret at proper position for making empty last line
  // in a block or <textarea> element visible.
  NS_PADDING_FOR_EMPTY_LAST_LINE = BR_ELEMENT_FLAG_BIT(1),
};

ASSERT_NODE_FLAGS_SPACE(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET + 2);

class HTMLBRElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLBRElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLBRElement, br)

  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction()
      const override;
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  bool Clear() { return GetBoolAttr(nsGkAtoms::clear); }
  void SetClear(const nsAString& aClear, ErrorResult& aError) {
    return SetHTMLAttr(nsGkAtoms::clear, aClear, aError);
  }
  void GetClear(DOMString& aClear) const {
    return GetHTMLAttr(nsGkAtoms::clear, aClear);
  }

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  bool IsPaddingForEmptyEditor() const {
    return HasFlag(NS_PADDING_FOR_EMPTY_EDITOR);
  }
  bool IsPaddingForEmptyLastLine() const {
    return HasFlag(NS_PADDING_FOR_EMPTY_LAST_LINE);
  }

 private:
  virtual ~HTMLBRElement();

  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    MappedDeclarations&);
};

}  // namespace dom
}  // namespace mozilla

#endif
