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
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsILink
  NS_IMETHOD LinkAdded() { return NS_OK; }
  NS_IMETHOD LinkRemoved() { return NS_OK; }

  // nsIContent
  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual bool IsFocusable(int32_t *aTabIndex = nullptr, bool aWithMouse = false);
  virtual bool IsLink(nsIURI** aURI) const;
  virtual void GetLinkTarget(nsAString& aTarget);
  virtual nsLinkState GetLinkState() const;
  virtual already_AddRefed<nsIURI> GetHrefURI() const;
  virtual nsEventStates IntrinsicState() const;
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

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedString> Href();
  already_AddRefed<nsIDOMSVGAnimatedString> Target();
  void GetDownload(nsAString & aDownload);
  void SetDownload(const nsAString & aDownload, ErrorResult& rv);

protected:

  virtual StringAttributesInfo GetStringInfo();

  enum { HREF, TARGET };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGAElement_h
