/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLTableElement_h
#define mozilla_dom_HTMLTableElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "mozilla/dom/HTMLTableCaptionElement.h"
#include "mozilla/dom/HTMLTableSectionElement.h"

namespace mozilla {
namespace dom {

#define TABLE_ATTRS_DIRTY ((nsMappedAttributes*)0x1)

class TableRowsCollection;

class HTMLTableElement MOZ_FINAL : public nsGenericHTMLElement,
                                   public nsIDOMHTMLTableElement
{
public:
  HTMLTableElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLTableElement, table)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  HTMLTableCaptionElement* GetCaption() const
  {
    return static_cast<HTMLTableCaptionElement*>(GetChild(nsGkAtoms::caption));
  }
  void SetCaption(HTMLTableCaptionElement* aCaption)
  {
    DeleteCaption();
    if (aCaption) {
      mozilla::ErrorResult rv;
      nsINode::AppendChild(*aCaption, rv);
    }
  }

  void DeleteTFoot();

  already_AddRefed<nsGenericHTMLElement> CreateCaption();

  void DeleteCaption();

  HTMLTableSectionElement* GetTHead() const
  {
    return static_cast<HTMLTableSectionElement*>(GetChild(nsGkAtoms::thead));
  }
  void SetTHead(HTMLTableSectionElement* aTHead, ErrorResult& aError)
  {
    if (aTHead && !aTHead->IsHTML(nsGkAtoms::thead)) {
      aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
      return;
    }

    DeleteTHead();
    if (aTHead) {
      nsINode::InsertBefore(*aTHead, nsINode::GetFirstChild(), aError);
    }
  }
  already_AddRefed<nsGenericHTMLElement> CreateTHead();

  void DeleteTHead();

  HTMLTableSectionElement* GetTFoot() const
  {
    return static_cast<HTMLTableSectionElement*>(GetChild(nsGkAtoms::tfoot));
  }
  void SetTFoot(HTMLTableSectionElement* aTFoot, ErrorResult& aError)
  {
    if (aTFoot && !aTFoot->IsHTML(nsGkAtoms::tfoot)) {
      aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
      return;
    }

    DeleteTFoot();
    if (aTFoot) {
      nsINode::AppendChild(*aTFoot, aError);
    }
  }
  already_AddRefed<nsGenericHTMLElement> CreateTFoot();

  nsIHTMLCollection* TBodies();

  already_AddRefed<nsGenericHTMLElement> CreateTBody();

  nsIHTMLCollection* Rows();

  already_AddRefed<nsGenericHTMLElement> InsertRow(int32_t aIndex,
                                                   ErrorResult& aError);
  void DeleteRow(int32_t aIndex, ErrorResult& aError);

  void GetAlign(nsString& aAlign)
  {
    GetHTMLAttr(nsGkAtoms::align, aAlign);
  }
  void SetAlign(const nsAString& aAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }
  void GetBorder(nsString& aBorder)
  {
    GetHTMLAttr(nsGkAtoms::border, aBorder);
  }
  void SetBorder(const nsAString& aBorder, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::border, aBorder, aError);
  }
  void GetFrame(nsString& aFrame)
  {
    GetHTMLAttr(nsGkAtoms::frame, aFrame);
  }
  void SetFrame(const nsAString& aFrame, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::frame, aFrame, aError);
  }
  void GetRules(nsString& aRules)
  {
    GetHTMLAttr(nsGkAtoms::rules, aRules);
  }
  void SetRules(const nsAString& aRules, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::rules, aRules, aError);
  }
  void GetSummary(nsString& aSummary)
  {
    GetHTMLAttr(nsGkAtoms::summary, aSummary);
  }
  void SetSummary(const nsAString& aSummary, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::summary, aSummary, aError);
  }
  void GetWidth(nsString& aWidth)
  {
    GetHTMLAttr(nsGkAtoms::width, aWidth);
  }
  void SetWidth(const nsAString& aWidth, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::width, aWidth, aError);
  }
  void GetBgColor(nsString& aBgColor)
  {
    GetHTMLAttr(nsGkAtoms::bgcolor, aBgColor);
  }
  void SetBgColor(const nsAString& aBgColor, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::bgcolor, aBgColor, aError);
  }
  void GetCellPadding(nsString& aCellPadding)
  {
    GetHTMLAttr(nsGkAtoms::cellpadding, aCellPadding);
  }
  void SetCellPadding(const nsAString& aCellPadding, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::cellpadding, aCellPadding, aError);
  }
  void GetCellSpacing(nsString& aCellSpacing)
  {
    GetHTMLAttr(nsGkAtoms::cellspacing, aCellSpacing);
  }
  void SetCellSpacing(const nsAString& aCellSpacing, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::cellspacing, aCellSpacing, aError);
  }

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;
  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) MOZ_OVERRIDE;
  /**
   * Called when an attribute has just been changed
   */
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLTableElement,
                                           nsGenericHTMLElement)
  nsMappedAttributes* GetAttributesMappedForCell();

protected:
  virtual ~HTMLTableElement();

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;

  nsIContent* GetChild(nsIAtom *aTag) const
  {
    for (nsIContent* cur = nsINode::GetFirstChild(); cur;
         cur = cur->GetNextSibling()) {
      if (cur->IsHTML(aTag)) {
        return cur;
      }
    }
    return nullptr;
  }

  nsRefPtr<nsContentList> mTBodies;
  nsRefPtr<TableRowsCollection> mRows;
  // Sentinel value of TABLE_ATTRS_DIRTY indicates that this is dirty and needs
  // to be recalculated.
  nsMappedAttributes *mTableInheritedAttributes;
  void BuildInheritedAttributes();
  void ReleaseInheritedAttributes();

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData);
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLTableElement_h */
