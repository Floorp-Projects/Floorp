/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAElement_h
#define mozilla_dom_SVGAElement_h

#include "Link.h"
#include "nsDOMTokenList.h"
#include "SVGAnimatedString.h"
#include "mozilla/dom/SVGGraphicsElement.h"

nsresult NS_NewSVGAElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {

class EventChainPostVisitor;
class EventChainPreVisitor;

namespace dom {

typedef SVGGraphicsElement SVGAElementBase;

class SVGAElement final : public SVGAElementBase, public Link {
 protected:
  using Element::GetText;

  explicit SVGAElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  friend nsresult(::NS_NewSVGAElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGAElement, SVGAElementBase)

  // nsINode interface methods
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;
  MOZ_CAN_RUN_SCRIPT
  nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) override;
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // nsIContent
  virtual nsresult BindToTree(Document* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  virtual int32_t TabIndexDefault() override;
  bool IsFocusableInternal(int32_t* aTabIndex, bool aWithMouse) override;
  virtual bool IsLink(nsIURI** aURI) const override;
  virtual void GetLinkTarget(nsAString& aTarget) override;
  virtual already_AddRefed<nsIURI> GetHrefURI() const override;
  virtual EventStates IntrinsicState() const override;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                bool aNotify) override;

  // Link
  virtual bool ElementHasHref() const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> Href();
  already_AddRefed<DOMSVGAnimatedString> Target();
  void GetDownload(nsAString& aDownload);
  void SetDownload(const nsAString& aDownload, ErrorResult& rv);
  void GetPing(nsAString& aPing);
  void SetPing(const nsAString& aPing, mozilla::ErrorResult& rv);
  void GetRel(nsAString& aRel);
  void SetRel(const nsAString& aRel, mozilla::ErrorResult& rv);
  void SetReferrerPolicy(const nsAString& aPolicy, mozilla::ErrorResult& rv);
  void GetReferrerPolicy(nsAString& aPolicy);
  nsDOMTokenList* RelList();
  void GetHreflang(nsAString& aHreflang);
  void SetHreflang(const nsAString& aHreflang, mozilla::ErrorResult& rv);
  void GetType(nsAString& aType);
  void SetType(const nsAString& aType, mozilla::ErrorResult& rv);
  void GetText(nsAString& aText, mozilla::ErrorResult& rv);
  void SetText(const nsAString& aText, mozilla::ErrorResult& rv);

  void NodeInfoChanged(Document* aOldDoc) final {
    ClearHasPendingLinkUpdate();
    SVGAElementBase::NodeInfoChanged(aOldDoc);
  }

 protected:
  virtual ~SVGAElement() = default;

  virtual StringAttributesInfo GetStringInfo() override;

  enum { HREF, XLINK_HREF, TARGET };
  SVGAnimatedString mStringAttributes[3];
  static StringInfo sStringInfo[3];

  RefPtr<nsDOMTokenList> mRelList;
  static DOMTokenListSupportedToken sSupportedRelValues[];
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGAElement_h
