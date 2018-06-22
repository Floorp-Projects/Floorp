/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLTableSectionElement_h
#define mozilla_dom_HTMLTableSectionElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsContentList.h" // For ctor.

namespace mozilla {
namespace dom {

class HTMLTableSectionElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLTableSectionElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
    SetHasWeirdParserInsertionMode();
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  nsIHTMLCollection* Rows();
  already_AddRefed<nsGenericHTMLElement>
    InsertRow(int32_t aIndex, ErrorResult& aError);
  void DeleteRow(int32_t aValue, ErrorResult& aError);

  void GetAlign(DOMString& aAlign)
  {
    GetHTMLAttr(nsGkAtoms::align, aAlign);
  }
  void SetAlign(const nsAString& aAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }
  void GetCh(DOMString& aCh)
  {
    GetHTMLAttr(nsGkAtoms::_char, aCh);
  }
  void SetCh(const nsAString& aCh, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::_char, aCh, aError);
  }
  void GetChOff(DOMString& aChOff)
  {
    GetHTMLAttr(nsGkAtoms::charoff, aChOff);
  }
  void SetChOff(const nsAString& aChOff, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::charoff, aChOff, aError);
  }
  void GetVAlign(DOMString& aVAlign)
  {
    GetHTMLAttr(nsGkAtoms::valign, aVAlign);
  }
  void SetVAlign(const nsAString& aVAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::valign, aVAlign, aError);
  }

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(HTMLTableSectionElement,
                                                     nsGenericHTMLElement)
protected:
  virtual ~HTMLTableSectionElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  RefPtr<nsContentList> mRows;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    MappedDeclarations&);
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLTableSectionElement_h */
