/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAElement_h
#define mozilla_dom_SVGAElement_h

#include "Link.h"
#include "nsSVGString.h"
#include "mozilla/dom/SVGGraphicsElement.h"

nsresult NS_NewSVGAElement(nsIContent **aResult,
                           already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {

class EventChainPostVisitor;
class EventChainPreVisitor;

namespace dom {

typedef SVGGraphicsElement SVGAElementBase;

class SVGAElement final : public SVGAElementBase,
                          public Link
{
protected:
  explicit SVGAElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  friend nsresult (::NS_NewSVGAElement(nsIContent **aResult,
                                       already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGAElement, SVGAElementBase)

  // nsINode interface methods
  virtual nsresult GetEventTargetParent(
                     EventChainPreVisitor& aVisitor) override;
  virtual nsresult PostHandleEvent(
                     EventChainPostVisitor& aVisitor) override;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  // nsIContent
  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;
  virtual bool IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) override;
  virtual bool IsLink(nsIURI** aURI) const override;
  virtual void GetLinkTarget(nsAString& aTarget) override;
  virtual already_AddRefed<nsIURI> GetHrefURI() const override;
  virtual EventStates IntrinsicState() const override;
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) override;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) override;

  // WebIDL
  already_AddRefed<SVGAnimatedString> Href();
  already_AddRefed<SVGAnimatedString> Target();
  void GetDownload(nsAString & aDownload);
  void SetDownload(const nsAString & aDownload, ErrorResult& rv);

protected:
  virtual ~SVGAElement();

  virtual StringAttributesInfo GetStringInfo() override;

  enum { HREF, XLINK_HREF, TARGET };
  nsSVGString mStringAttributes[3];
  static StringInfo sStringInfo[3];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGAElement_h
