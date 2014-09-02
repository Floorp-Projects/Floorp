/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLTableRowElement_h
#define mozilla_dom_HTMLTableRowElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

class nsIDOMHTMLTableElement;
class nsContentList;

namespace mozilla {
namespace dom {

class HTMLTableSectionElement;

class HTMLTableRowElement MOZ_FINAL : public nsGenericHTMLElement
{
public:
  explicit HTMLTableRowElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
    SetHasWeirdParserInsertionMode();
  }

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLTableRowElement, tr)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  int32_t RowIndex() const;
  int32_t SectionRowIndex() const;
  nsIHTMLCollection* Cells();
  already_AddRefed<nsGenericHTMLElement>
    InsertCell(int32_t aIndex, ErrorResult& aError);
  void DeleteCell(int32_t aValue, ErrorResult& aError);

  void GetAlign(nsString& aAlign)
  {
    GetHTMLAttr(nsGkAtoms::align, aAlign);
  }
  void SetAlign(const nsAString& aAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }
  void GetCh(nsString& aCh)
  {
    GetHTMLAttr(nsGkAtoms::_char, aCh);
  }
  void SetCh(const nsAString& aCh, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::_char, aCh, aError);
  }
  void GetChOff(nsString& aChOff)
  {
    GetHTMLAttr(nsGkAtoms::charoff, aChOff);
  }
  void SetChOff(const nsAString& aChOff, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::charoff, aChOff, aError);
  }
  void GetVAlign(nsString& aVAlign)
  {
    GetHTMLAttr(nsGkAtoms::valign, aVAlign);
  }
  void SetVAlign(const nsAString& aVAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::valign, aVAlign, aError);
  }
  void GetBgColor(nsString& aBgColor)
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
                                nsAttrValue& aResult) MOZ_OVERRIDE;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(HTMLTableRowElement,
                                                     nsGenericHTMLElement)

protected:
  virtual ~HTMLTableRowElement();

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

  HTMLTableSectionElement* GetSection() const;
  HTMLTableElement* GetTable() const;
  nsRefPtr<nsContentList> mCells;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData);
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLTableRowElement_h */
