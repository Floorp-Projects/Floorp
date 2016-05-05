/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLTableCellElement_h
#define mozilla_dom_HTMLTableCellElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLTableCellElement.h"

namespace mozilla {
namespace dom {

class HTMLTableElement;

class HTMLTableCellElement final : public nsGenericHTMLElement,
                                   public nsIDOMHTMLTableCellElement
{
public:
  explicit HTMLTableCellElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
    SetHasWeirdParserInsertionMode();
  }

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLTableCellElement
  NS_DECL_NSIDOMHTMLTABLECELLELEMENT

  uint32_t ColSpan() const
  {
    return GetIntAttr(nsGkAtoms::colspan, 1);
  }
  void SetColSpan(uint32_t aColSpan, ErrorResult& aError)
  {
    SetHTMLIntAttr(nsGkAtoms::colspan, aColSpan, aError);
  }
  uint32_t RowSpan() const
  {
    return GetIntAttr(nsGkAtoms::rowspan, 1);
  }
  void SetRowSpan(uint32_t aRowSpan, ErrorResult& aError)
  {
    SetHTMLIntAttr(nsGkAtoms::rowspan, aRowSpan, aError);
  }
  //already_AddRefed<nsDOMTokenList> Headers() const;
  void GetHeaders(DOMString& aHeaders)
  {
    GetHTMLAttr(nsGkAtoms::headers, aHeaders);
  }
  void SetHeaders(const nsAString& aHeaders, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::headers, aHeaders, aError);
  }
  int32_t CellIndex() const;

  void GetAbbr(DOMString& aAbbr)
  {
    GetHTMLAttr(nsGkAtoms::abbr, aAbbr);
  }
  void SetAbbr(const nsAString& aAbbr, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::abbr, aAbbr, aError);
  }
  void GetScope(DOMString& aScope);
  void SetScope(const nsAString& aScope, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::scope, aScope, aError);
  }
  void GetAlign(DOMString& aAlign);
  void SetAlign(const nsAString& aAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }
  void GetAxis(DOMString& aAxis)
  {
    GetHTMLAttr(nsGkAtoms::axis, aAxis);
  }
  void SetAxis(const nsAString& aAxis, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::axis, aAxis, aError);
  }
  void GetHeight(DOMString& aHeight)
  {
    GetHTMLAttr(nsGkAtoms::height, aHeight);
  }
  void SetHeight(const nsAString& aHeight, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::height, aHeight, aError);
  }
  void GetWidth(DOMString& aWidth)
  {
    GetHTMLAttr(nsGkAtoms::width, aWidth);
  }
  void SetWidth(const nsAString& aWidth, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::width, aWidth, aError);
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
  bool NoWrap()
  {
    return GetBoolAttr(nsGkAtoms::nowrap);
  }
  void SetNoWrap(bool aNoWrap, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::nowrap, aNoWrap, aError);
  }
  void GetVAlign(DOMString& aVAlign)
  {
    GetHTMLAttr(nsGkAtoms::valign, aVAlign);
  }
  void SetVAlign(const nsAString& aVAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::valign, aVAlign, aError);
  }
  void GetBgColor(DOMString& aBgColor)
  {
    GetHTMLAttr(nsGkAtoms::bgcolor, aBgColor);
  }
  void SetBgColor(const nsAString& aBgColor, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::bgcolor, aBgColor, aError);
  }

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

protected:
  virtual ~HTMLTableCellElement();

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  HTMLTableElement* GetTable() const;

  HTMLTableRowElement* GetRow() const;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData);
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLTableCellElement_h */
