/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLIFrameElement_h
#define mozilla_dom_HTMLIFrameElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsDOMTokenList.h"

namespace mozilla {
namespace dom {

class HTMLIFrameElement final : public nsGenericHTMLFrameElement
                              , public nsIDOMHTMLIFrameElement
{
public:
  explicit HTMLIFrameElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                             FromParser aFromParser = NOT_FROM_PARSER);

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLIFrameElement, iframe)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // Element
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const override
  {
    return true;
  }

  // nsIDOMHTMLIFrameElement
  NS_DECL_NSIDOMHTMLIFRAMEELEMENT

  // nsIContent
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) override;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                bool aNotify) override;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) override;

  uint32_t GetSandboxFlags();

  // Web IDL binding methods
  // The XPCOM GetSrc is fine for our purposes
  void SetSrc(const nsAString& aSrc, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::src, aSrc, aError);
  }
  void GetSrcdoc(DOMString& aSrcdoc)
  {
    GetHTMLAttr(nsGkAtoms::srcdoc, aSrcdoc);
  }
  void SetSrcdoc(const nsAString& aSrcdoc, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::srcdoc, aSrcdoc, aError);
  }
  void GetName(DOMString& aName)
  {
    GetHTMLAttr(nsGkAtoms::name, aName);
  }
  void SetName(const nsAString& aName, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aError);
  }
  nsDOMTokenList* Sandbox()
  {
    return GetTokenList(nsGkAtoms::sandbox, sSupportedSandboxTokens);
  }
  bool AllowFullscreen() const
  {
    return GetBoolAttr(nsGkAtoms::allowfullscreen);
  }
  void SetAllowFullscreen(bool aAllow, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::allowfullscreen, aAllow, aError);
  }
  void GetWidth(DOMString& aWidth)
  {
    GetHTMLAttr(nsGkAtoms::width, aWidth);
  }
  void SetWidth(const nsAString& aWidth, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::width, aWidth, aError);
  }
  void GetHeight(DOMString& aHeight)
  {
    GetHTMLAttr(nsGkAtoms::height, aHeight);
  }
  void SetHeight(const nsAString& aHeight, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::height, aHeight, aError);
  }
  using nsGenericHTMLFrameElement::GetContentDocument;
  using nsGenericHTMLFrameElement::GetContentWindow;
  void GetAlign(DOMString& aAlign)
  {
    GetHTMLAttr(nsGkAtoms::align, aAlign);
  }
  void SetAlign(const nsAString& aAlign, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::align, aAlign, aError);
  }
  void GetScrolling(DOMString& aScrolling)
  {
    GetHTMLAttr(nsGkAtoms::scrolling, aScrolling);
  }
  void SetScrolling(const nsAString& aScrolling, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::scrolling, aScrolling, aError);
  }
  void GetFrameBorder(DOMString& aFrameBorder)
  {
    GetHTMLAttr(nsGkAtoms::frameborder, aFrameBorder);
  }
  void SetFrameBorder(const nsAString& aFrameBorder, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::frameborder, aFrameBorder, aError);
  }
  // The XPCOM GetLongDesc is fine
  void SetLongDesc(const nsAString& aLongDesc, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::longdesc, aLongDesc, aError);
  }
  void GetMarginWidth(DOMString& aMarginWidth)
  {
    GetHTMLAttr(nsGkAtoms::marginwidth, aMarginWidth);
  }
  void SetMarginWidth(const nsAString& aMarginWidth, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::marginwidth, aMarginWidth, aError);
  }
  void GetMarginHeight(DOMString& aMarginHeight)
  {
    GetHTMLAttr(nsGkAtoms::marginheight, aMarginHeight);
  }
  void SetMarginHeight(const nsAString& aMarginHeight, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::marginheight, aMarginHeight, aError);
  }
  void SetReferrerPolicy(const nsAString& aReferrer, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::referrerpolicy, aReferrer, aError);
  }
  void GetReferrerPolicy(nsAString& aReferrer)
  {
    GetEnumAttr(nsGkAtoms::referrerpolicy, EmptyCString().get(), aReferrer);
  }
  nsIDocument* GetSVGDocument()
  {
    return GetContentDocument();
  }
  bool Mozbrowser() const
  {
    return GetBoolAttr(nsGkAtoms::mozbrowser);
  }
  void SetMozbrowser(bool aAllow, ErrorResult& aError)
  {
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

protected:
  virtual ~HTMLIFrameElement();

  virtual void GetItemValueText(DOMString& text) override;
  virtual void SetItemValueText(const nsAString& text) override;

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData);

  static const DOMTokenListSupportedToken sSupportedSandboxTokens[];
};

} // namespace dom
} // namespace mozilla

#endif
