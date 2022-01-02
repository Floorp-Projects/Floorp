/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLTableElement_h
#define mozilla_dom_HTMLTableElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/dom/HTMLTableCaptionElement.h"
#include "mozilla/dom/HTMLTableSectionElement.h"

namespace mozilla {
namespace dom {

class TableRowsCollection;

class HTMLTableElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLTableElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLTableElement, table)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  HTMLTableCaptionElement* GetCaption() const {
    return static_cast<HTMLTableCaptionElement*>(GetChild(nsGkAtoms::caption));
  }
  void SetCaption(HTMLTableCaptionElement* aCaption, ErrorResult& aError) {
    DeleteCaption();
    if (aCaption) {
      nsCOMPtr<nsINode> firstChild = nsINode::GetFirstChild();
      nsINode::InsertBefore(*aCaption, firstChild, aError);
    }
  }

  void DeleteTFoot();

  already_AddRefed<nsGenericHTMLElement> CreateCaption();

  void DeleteCaption();

  HTMLTableSectionElement* GetTHead() const {
    return static_cast<HTMLTableSectionElement*>(GetChild(nsGkAtoms::thead));
  }
  void SetTHead(HTMLTableSectionElement* aTHead, ErrorResult& aError) {
    if (aTHead && !aTHead->IsHTMLElement(nsGkAtoms::thead)) {
      aError.ThrowHierarchyRequestError("New value must be a thead element.");
      return;
    }

    DeleteTHead();
    if (aTHead) {
      nsCOMPtr<nsIContent> refNode = nullptr;
      for (refNode = nsINode::GetFirstChild(); refNode;
           refNode = refNode->GetNextSibling()) {
        if (refNode->IsHTMLElement() &&
            !refNode->IsHTMLElement(nsGkAtoms::caption) &&
            !refNode->IsHTMLElement(nsGkAtoms::colgroup)) {
          break;
        }
      }

      nsINode::InsertBefore(*aTHead, refNode, aError);
    }
  }
  already_AddRefed<nsGenericHTMLElement> CreateTHead();

  void DeleteTHead();

  HTMLTableSectionElement* GetTFoot() const {
    return static_cast<HTMLTableSectionElement*>(GetChild(nsGkAtoms::tfoot));
  }
  void SetTFoot(HTMLTableSectionElement* aTFoot, ErrorResult& aError) {
    if (aTFoot && !aTFoot->IsHTMLElement(nsGkAtoms::tfoot)) {
      aError.ThrowHierarchyRequestError("New value must be a tfoot element.");
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

  void GetAlign(DOMString& aAlign) { GetHTMLAttr(nsGkAtoms::align, aAlign); }
  void SetAlign(const nsAString& aAlign, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }
  void GetBorder(DOMString& aBorder) {
    GetHTMLAttr(nsGkAtoms::border, aBorder);
  }
  void SetBorder(const nsAString& aBorder, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::border, aBorder, aError);
  }
  void GetFrame(DOMString& aFrame) { GetHTMLAttr(nsGkAtoms::frame, aFrame); }
  void SetFrame(const nsAString& aFrame, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::frame, aFrame, aError);
  }
  void GetRules(DOMString& aRules) { GetHTMLAttr(nsGkAtoms::rules, aRules); }
  void SetRules(const nsAString& aRules, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::rules, aRules, aError);
  }
  void GetSummary(nsString& aSummary) {
    GetHTMLAttr(nsGkAtoms::summary, aSummary);
  }
  void GetSummary(DOMString& aSummary) {
    GetHTMLAttr(nsGkAtoms::summary, aSummary);
  }
  void SetSummary(const nsAString& aSummary, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::summary, aSummary, aError);
  }
  void GetWidth(DOMString& aWidth) { GetHTMLAttr(nsGkAtoms::width, aWidth); }
  void SetWidth(const nsAString& aWidth, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::width, aWidth, aError);
  }
  void GetBgColor(DOMString& aBgColor) {
    GetHTMLAttr(nsGkAtoms::bgcolor, aBgColor);
  }
  void SetBgColor(const nsAString& aBgColor, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::bgcolor, aBgColor, aError);
  }
  void GetCellPadding(DOMString& aCellPadding) {
    GetHTMLAttr(nsGkAtoms::cellpadding, aCellPadding);
  }
  void SetCellPadding(const nsAString& aCellPadding, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::cellpadding, aCellPadding, aError);
  }
  void GetCellSpacing(DOMString& aCellSpacing) {
    GetHTMLAttr(nsGkAtoms::cellspacing, aCellSpacing);
  }
  void SetCellSpacing(const nsAString& aCellSpacing, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::cellspacing, aCellSpacing, aError);
  }

  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction()
      const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent = true) override;
  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;
  /**
   * Called when an attribute has just been changed
   */
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLTableElement,
                                           nsGenericHTMLElement)
  nsMappedAttributes* GetAttributesMappedForCell();

 protected:
  virtual ~HTMLTableElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  nsIContent* GetChild(nsAtom* aTag) const {
    for (nsIContent* cur = nsINode::GetFirstChild(); cur;
         cur = cur->GetNextSibling()) {
      if (cur->IsHTMLElement(aTag)) {
        return cur;
      }
    }
    return nullptr;
  }

  RefPtr<nsContentList> mTBodies;
  RefPtr<TableRowsCollection> mRows;
  nsMappedAttributes* mTableInheritedAttributes;
  void BuildInheritedAttributes();
  void ReleaseInheritedAttributes();

 private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    MappedDeclarations&);
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_HTMLTableElement_h */
