/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLTableElement_h
#define mozilla_dom_HTMLTableElement_h

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsMappedAttributes.h"

namespace mozilla {
namespace dom {

#define TABLE_ATTRS_DIRTY ((nsMappedAttributes*)0x1)

class TableRowsCollection;

class HTMLTableElement : public nsGenericHTMLElement,
                         public nsIDOMHTMLTableElement
{
public:
  HTMLTableElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLTableElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLTableElement
  NS_DECL_NSIDOMHTMLTABLEELEMENT

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
  virtual nsIDOMNode* AsDOMNode() { return this; }
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify);
  /**
   * Called when an attribute has just been changed
   */
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLTableElement,
                                           nsGenericHTMLElement)
  nsMappedAttributes* GetAttributesMappedForCell();
  already_AddRefed<nsIDOMHTMLTableSectionElement> GetTHead() {
    return GetSection(nsGkAtoms::thead);
  }
  already_AddRefed<nsIDOMHTMLTableSectionElement> GetTFoot() {
    return GetSection(nsGkAtoms::tfoot);
  }
  already_AddRefed<nsIDOMHTMLTableCaptionElement> GetCaption();
  nsContentList* TBodies();
protected:
  already_AddRefed<nsIDOMHTMLTableSectionElement> GetSection(nsIAtom *aTag);

  nsRefPtr<nsContentList> mTBodies;
  nsRefPtr<TableRowsCollection> mRows;
  // Sentinel value of TABLE_ATTRS_DIRTY indicates that this is dirty and needs
  // to be recalculated.
  nsMappedAttributes *mTableInheritedAttributes;
  void BuildInheritedAttributes();
  void ReleaseInheritedAttributes() {
    if (mTableInheritedAttributes &&
        mTableInheritedAttributes != TABLE_ATTRS_DIRTY)
      NS_RELEASE(mTableInheritedAttributes);
      mTableInheritedAttributes = TABLE_ATTRS_DIRTY;
  }
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLTableElement_h */
