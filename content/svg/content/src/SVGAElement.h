/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAElement_h
#define mozilla_dom_SVGAElement_h

#include "Link.h"
#include "nsILink.h"
#include "nsSVGString.h"
#include "mozilla/dom/SVGGraphicsElement.h"

nsresult NS_NewSVGAElement(nsIContent **aResult,
                           already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGGraphicsElement SVGAElementBase;

class SVGAElement MOZ_FINAL : public SVGAElementBase,
                              public nsILink,
                              public Link
{
protected:
  SVGAElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  friend nsresult (::NS_NewSVGAElement(nsIContent **aResult,
                                       already_AddRefed<nsINodeInfo> aNodeInfo));
  virtual JSObject* WrapNode(JSContext *cx,
                             JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsINode interface methods
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor) MOZ_OVERRIDE;
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor) MOZ_OVERRIDE;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // nsILink
  NS_IMETHOD LinkAdded() MOZ_OVERRIDE { return NS_OK; }
  NS_IMETHOD LinkRemoved() MOZ_OVERRIDE { return NS_OK; }

  // nsIContent
  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual bool IsFocusable(int32_t *aTabIndex = nullptr, bool aWithMouse = false) MOZ_OVERRIDE;
  virtual bool IsLink(nsIURI** aURI) const MOZ_OVERRIDE;
  virtual void GetLinkTarget(nsAString& aTarget) MOZ_OVERRIDE;
  virtual nsLinkState GetLinkState() const MOZ_OVERRIDE;
  virtual already_AddRefed<nsIURI> GetHrefURI() const MOZ_OVERRIDE;
  virtual nsEventStates IntrinsicState() const MOZ_OVERRIDE;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedString> Href();
  already_AddRefed<nsIDOMSVGAnimatedString> Target();
  void GetDownload(nsAString & aDownload);
  void SetDownload(const nsAString & aDownload, ErrorResult& rv);

protected:

  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { HREF, TARGET };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGAElement_h
