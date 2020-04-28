/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLIFrameElement_h
#define mozilla_dom_HTMLIFrameElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/FeaturePolicy.h"
#include "nsGenericHTMLFrameElement.h"
#include "nsDOMTokenList.h"

namespace mozilla {
namespace dom {

class HTMLIFrameElement final : public nsGenericHTMLFrameElement {
 public:
  explicit HTMLIFrameElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
      FromParser aFromParser = NOT_FROM_PARSER);

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLIFrameElement, iframe)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLIFrameElement,
                                           nsGenericHTMLFrameElement)

  // Element
  virtual bool IsInteractiveHTMLContent() const override { return true; }

  // nsIContent
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction()
      const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  void BindToBrowsingContext(BrowsingContext* aBrowsingContext);

  uint32_t GetSandboxFlags() const;

  // Web IDL binding methods
  void GetSrc(nsString& aSrc) const {
    GetURIAttr(nsGkAtoms::src, nullptr, aSrc);
  }
  void SetSrc(const nsAString& aSrc, nsIPrincipal* aTriggeringPrincipal,
              ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::src, aSrc, aTriggeringPrincipal, aError);
  }
  void GetSrcdoc(DOMString& aSrcdoc) {
    GetHTMLAttr(nsGkAtoms::srcdoc, aSrcdoc);
  }
  void SetSrcdoc(const nsAString& aSrcdoc, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::srcdoc, aSrcdoc, aError);
  }
  void GetName(DOMString& aName) { GetHTMLAttr(nsGkAtoms::name, aName); }
  void SetName(const nsAString& aName, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::name, aName, aError);
  }
  nsDOMTokenList* Sandbox() {
    if (!mSandbox) {
      mSandbox =
          new nsDOMTokenList(this, nsGkAtoms::sandbox, sSupportedSandboxTokens);
    }
    return mSandbox;
  }
  bool AllowFullscreen() const {
    return GetBoolAttr(nsGkAtoms::allowfullscreen);
  }
  void SetAllowFullscreen(bool aAllow, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::allowfullscreen, aAllow, aError);
  }
  bool AllowPaymentRequest() const {
    return GetBoolAttr(nsGkAtoms::allowpaymentrequest);
  }
  void SetAllowPaymentRequest(bool aAllow, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::allowpaymentrequest, aAllow, aError);
  }
  void GetWidth(DOMString& aWidth) { GetHTMLAttr(nsGkAtoms::width, aWidth); }
  void SetWidth(const nsAString& aWidth, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::width, aWidth, aError);
  }
  void GetHeight(DOMString& aHeight) {
    GetHTMLAttr(nsGkAtoms::height, aHeight);
  }
  void SetHeight(const nsAString& aHeight, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::height, aHeight, aError);
  }
  using nsGenericHTMLFrameElement::GetContentDocument;
  using nsGenericHTMLFrameElement::GetContentWindow;
  void GetAlign(DOMString& aAlign) { GetHTMLAttr(nsGkAtoms::align, aAlign); }
  void SetAlign(const nsAString& aAlign, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }
  void GetAllow(DOMString& aAllow) { GetHTMLAttr(nsGkAtoms::allow, aAllow); }
  void SetAllow(const nsAString& aAllow, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::allow, aAllow, aError);
  }
  void GetScrolling(DOMString& aScrolling) {
    GetHTMLAttr(nsGkAtoms::scrolling, aScrolling);
  }
  void SetScrolling(const nsAString& aScrolling, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::scrolling, aScrolling, aError);
  }
  void GetFrameBorder(DOMString& aFrameBorder) {
    GetHTMLAttr(nsGkAtoms::frameborder, aFrameBorder);
  }
  void SetFrameBorder(const nsAString& aFrameBorder, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::frameborder, aFrameBorder, aError);
  }
  void GetLongDesc(nsAString& aLongDesc) const {
    GetURIAttr(nsGkAtoms::longdesc, nullptr, aLongDesc);
  }
  void SetLongDesc(const nsAString& aLongDesc, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::longdesc, aLongDesc, aError);
  }
  void GetMarginWidth(DOMString& aMarginWidth) {
    GetHTMLAttr(nsGkAtoms::marginwidth, aMarginWidth);
  }
  void SetMarginWidth(const nsAString& aMarginWidth, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::marginwidth, aMarginWidth, aError);
  }
  void GetMarginHeight(DOMString& aMarginHeight) {
    GetHTMLAttr(nsGkAtoms::marginheight, aMarginHeight);
  }
  void SetMarginHeight(const nsAString& aMarginHeight, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::marginheight, aMarginHeight, aError);
  }
  void SetReferrerPolicy(const nsAString& aReferrer, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::referrerpolicy, aReferrer, aError);
  }
  void GetReferrerPolicy(nsAString& aReferrer) {
    GetEnumAttr(nsGkAtoms::referrerpolicy, EmptyCString().get(), aReferrer);
  }
  Document* GetSVGDocument(nsIPrincipal& aSubjectPrincipal) {
    return GetContentDocument(aSubjectPrincipal);
  }
  bool Mozbrowser() const { return GetBoolAttr(nsGkAtoms::mozbrowser); }
  void SetMozbrowser(bool aAllow, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::mozbrowser, aAllow, aError);
  }
  using nsGenericHTMLFrameElement::SetMozbrowser;
  // nsGenericHTMLFrameElement::GetFrameLoader is fine
  // nsGenericHTMLFrameElement::GetAppManifestURL is fine

  // The fullscreen flag is set to true only when requestFullscreen is
  // explicitly called on this <iframe> element. In case this flag is
  // set, the fullscreen state of this element will not be reverted
  // automatically when its subdocument exits fullscreen.
  bool FullscreenFlag() const { return mFullscreenFlag; }
  void SetFullscreenFlag(bool aValue) { mFullscreenFlag = aValue; }

  mozilla::dom::FeaturePolicy* FeaturePolicy() const;

 protected:
  virtual ~HTMLIFrameElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                bool aNotify) override;
  virtual nsresult OnAttrSetButNotChanged(int32_t aNamespaceID, nsAtom* aName,
                                          const nsAttrValueOrString& aValue,
                                          bool aNotify) override;

 private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    MappedDeclarations&);

  static const DOMTokenListSupportedToken sSupportedSandboxTokens[];

  void RefreshFeaturePolicy(bool aParseAllowAttribute);

  // If this iframe has a 'srcdoc' attribute, the document's origin will be
  // returned. Otherwise, if this iframe has a 'src' attribute, the origin will
  // be the parsing of its value as URL. If the URL is invalid, or 'src'
  // attribute doesn't exist, the origin will be the document's origin.
  already_AddRefed<nsIPrincipal> GetFeaturePolicyDefaultOrigin() const;

  /**
   * This function is called by AfterSetAttr and OnAttrSetButNotChanged.
   * This function will be called by AfterSetAttr whether the attribute is being
   * set or unset.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aNotify Whether we plan to notify document observers.
   */
  void AfterMaybeChangeAttr(int32_t aNamespaceID, nsAtom* aName, bool aNotify);

  /**
   * Feature policy inheritance is broken in cross process model, so we may
   * have to store feature policy in browsingContext when neccesary.
   */
  void MaybeStoreCrossOriginFeaturePolicy();

  RefPtr<dom::FeaturePolicy> mFeaturePolicy;
  RefPtr<nsDOMTokenList> mSandbox;
};

}  // namespace dom
}  // namespace mozilla

#endif
