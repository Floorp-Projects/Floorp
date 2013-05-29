/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLImageElement_h
#define mozilla_dom_HTMLImageElement_h

#include "nsGenericHTMLElement.h"
#include "nsImageLoadingContent.h"
#include "nsIDOMHTMLImageElement.h"
#include "imgRequestProxy.h"

namespace mozilla {
namespace dom {

class HTMLImageElement MOZ_FINAL : public nsGenericHTMLElement,
                                   public nsImageLoadingContent,
                                   public nsIDOMHTMLImageElement
{
public:
  explicit HTMLImageElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLImageElement();

  static already_AddRefed<HTMLImageElement>
    Image(const GlobalObject& aGlobal, const Optional<uint32_t>& aWidth,
          const Optional<uint32_t>& aHeight, ErrorResult& aError);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual bool Draggable() const MOZ_OVERRIDE;

  // nsIDOMHTMLImageElement
  NS_DECL_NSIDOMHTMLIMAGEELEMENT

  // override from nsImageLoadingContent
  CORSMode GetCORSMode();

  // nsIContent
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);

  bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex);

  // SetAttr override.  C++ is stupid, so have to override both
  // overloaded methods.
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep, bool aNullParent);

  virtual nsEventStates IntrinsicState() const;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  nsresult CopyInnerTo(Element* aDest);

  void MaybeLoadImage();
  virtual nsIDOMNode* AsDOMNode() { return this; }

  bool IsMap()
  {
    return GetBoolAttr(nsGkAtoms::ismap);
  }
  void SetIsMap(bool aIsMap, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::ismap, aIsMap, aError);
  }
  uint32_t Width()
  {
    return GetWidthHeightForImage(mCurrentRequest).width;
  }
  void SetWidth(uint32_t aWidth, ErrorResult& aError)
  {
    SetUnsignedIntAttr(nsGkAtoms::width, aWidth, aError);
  }
  uint32_t Height()
  {
    return GetWidthHeightForImage(mCurrentRequest).height;
  }
  void SetHeight(uint32_t aHeight, ErrorResult& aError)
  {
    SetUnsignedIntAttr(nsGkAtoms::height, aHeight, aError);
  }
  uint32_t NaturalWidth();
  uint32_t NaturalHeight();
  bool Complete();
  int32_t Hspace()
  {
    return GetIntAttr(nsGkAtoms::hspace, 0);
  }
  void SetHspace(int32_t aHspace, ErrorResult& aError)
  {
    SetHTMLIntAttr(nsGkAtoms::hspace, aHspace, aError);
  }
  int32_t Vspace()
  {
    return GetIntAttr(nsGkAtoms::vspace, 0);
  }
  void SetVspace(int32_t aVspace, ErrorResult& aError)
  {
    SetHTMLIntAttr(nsGkAtoms::vspace, aVspace, aError);
  }

  // The XPCOM versions of the following getters work for Web IDL bindings as well
  void SetAlt(const nsAString& aAlt, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::alt, aAlt, aError);
  }
  void SetSrc(const nsAString& aSrc, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::src, aSrc, aError);
  }
  void SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::crossorigin, aCrossOrigin, aError);
  }
  void SetUseMap(const nsAString& aUseMap, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::usemap, aUseMap, aError);
  }
  void SetName(const nsAString& aName, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aError);
  }
  void SetAlign(const nsAString& aAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }
  void SetLongDesc(const nsAString& aLongDesc, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::longdesc, aLongDesc, aError);
  }
  void SetBorder(const nsAString& aBorder, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::border, aBorder, aError);
  }

protected:
  nsIntPoint GetXY();
  virtual void GetItemValueText(nsAString& text);
  virtual void SetItemValueText(const nsAString& text);
  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLImageElement_h */
