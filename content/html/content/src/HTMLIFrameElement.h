/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLIFrameElement_h
#define mozilla_dom_HTMLIFrameElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsDOMSettableTokenList.h"

namespace mozilla {
namespace dom {

class HTMLIFrameElement MOZ_FINAL : public nsGenericHTMLFrameElement
                                  , public nsIDOMHTMLIFrameElement
{
public:
  HTMLIFrameElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                    FromParser aFromParser = NOT_FROM_PARSER);

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLIFrameElement, iframe)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLIFrameElement
  NS_DECL_NSIDOMHTMLIFRAMEELEMENT

  // nsIContent
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult) MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify) MOZ_OVERRIDE;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                bool aNotify) MOZ_OVERRIDE;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) MOZ_OVERRIDE;

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
  nsDOMSettableTokenList* Sandbox()
  {
    return GetTokenList(nsGkAtoms::sandbox);
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

protected:
  virtual ~HTMLIFrameElement();

  virtual void GetItemValueText(nsAString& text) MOZ_OVERRIDE;
  virtual void SetItemValueText(const nsAString& text) MOZ_OVERRIDE;

  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData);
};

} // namespace dom
} // namespace mozilla

#endif
