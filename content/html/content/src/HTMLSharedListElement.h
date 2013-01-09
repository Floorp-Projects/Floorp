/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSharedListElement_h
#define mozilla_dom_HTMLSharedListElement_h
#include "mozilla/Util.h"

#include "nsIDOMHTMLOListElement.h"
#include "nsIDOMHTMLDListElement.h"
#include "nsIDOMHTMLUListElement.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

class HTMLSharedListElement : public nsGenericHTMLElement,
                              public nsIDOMHTMLOListElement,
                              public nsIDOMHTMLDListElement,
                              public nsIDOMHTMLUListElement
{
public:
  HTMLSharedListElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
    SetIsDOMBinding();
  }
  virtual ~HTMLSharedListElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLOListElement
  NS_DECL_NSIDOMHTMLOLISTELEMENT

  // nsIDOMHTMLDListElement
  // fully declared by NS_DECL_NSIDOMHTMLOLISTELEMENT

  // nsIDOMHTMLUListElement
  // fully declared by NS_DECL_NSIDOMHTMLOLISTELEMENT

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsXPCClassInfo* GetClassInfo()
  {
    return static_cast<nsXPCClassInfo*>(GetClassInfoInternal());
  }
  nsIClassInfo* GetClassInfoInternal();

  virtual nsIDOMNode* AsDOMNode()
  {
    return static_cast<nsIDOMHTMLOListElement*>(this);
  }

  bool Reversed() const
  {
    return GetBoolAttr(nsGkAtoms::reversed);
  }
  void SetReversed(bool aReversed, mozilla::ErrorResult& rv)
  {
    SetHTMLBoolAttr(nsGkAtoms::reversed, aReversed, rv);
  }
  int32_t Start() const
  {
    return GetIntAttr(nsGkAtoms::start, 1);
  }
  void SetStart(int32_t aStart, mozilla::ErrorResult& rv)
  {
    SetHTMLIntAttr(nsGkAtoms::start, aStart, rv);
  }
  void GetType(nsString& aType)
  {
    GetHTMLAttr(nsGkAtoms::type, aType);
  }
  void SetType(const nsAString& aType, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::type, aType, rv);
  }
  bool Compact() const
  {
    return GetBoolAttr(nsGkAtoms::compact);
  }
  void SetCompact(bool aCompact, mozilla::ErrorResult& rv)
  {
    SetHTMLBoolAttr(nsGkAtoms::compact, aCompact, rv);
  }

protected:
  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope,
                             bool *aTriedToWrap) MOZ_OVERRIDE = 0;
};

class HTMLDListElement MOZ_FINAL : public HTMLSharedListElement
{
public:
  HTMLDListElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : HTMLSharedListElement(aNodeInfo)
  {
  }
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
protected:
  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope,
                             bool *aTriedToWrap) MOZ_OVERRIDE;
};

class HTMLOListElement MOZ_FINAL : public HTMLSharedListElement
{
public:
  HTMLOListElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : HTMLSharedListElement(aNodeInfo)
  {
  }
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
protected:
  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope,
                             bool *aTriedToWrap) MOZ_OVERRIDE;
};

class HTMLUListElement MOZ_FINAL : public HTMLSharedListElement
{
public:
  HTMLUListElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : HTMLSharedListElement(aNodeInfo)
  {
  }
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
protected:
  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope,
                             bool *aTriedToWrap) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLSharedListElement_h
