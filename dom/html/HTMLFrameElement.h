/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLFrameElement_h
#define mozilla_dom_HTMLFrameElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLFrameElement.h"
#include "nsGkAtoms.h"

namespace mozilla {
namespace dom {

class HTMLFrameElement final : public nsGenericHTMLFrameElement
{
public:
  using nsGenericHTMLFrameElement::SwapFrameLoaders;

  explicit HTMLFrameElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                            FromParser aFromParser = NOT_FROM_PARSER);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLFrameElement, frame)

  // nsIContent
  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  // WebIDL API
  void GetFrameBorder(DOMString& aFrameBorder) const
  {
    GetHTMLAttr(nsGkAtoms::frameborder, aFrameBorder);
  }
  void SetFrameBorder(const nsAString& aFrameBorder, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::frameborder, aFrameBorder, aError);
  }

  void GetLongDesc(nsAString& aLongDesc) const
  {
    GetURIAttr(nsGkAtoms::longdesc, nullptr, aLongDesc);
  }
  void SetLongDesc(const nsAString& aLongDesc, ErrorResult& aError)
  {
    SetAttrHelper(nsGkAtoms::longdesc, aLongDesc);
  }

  void GetMarginHeight(DOMString& aMarginHeight) const
  {
    GetHTMLAttr(nsGkAtoms::marginheight, aMarginHeight);
  }
  void SetMarginHeight(const nsAString& aMarginHeight, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::marginheight, aMarginHeight, aError);
  }

  void GetMarginWidth(DOMString& aMarginWidth) const
  {
    GetHTMLAttr(nsGkAtoms::marginwidth, aMarginWidth);
  }
  void SetMarginWidth(const nsAString& aMarginWidth, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::marginwidth, aMarginWidth, aError);
  }

  void GetName(DOMString& aName) const
  {
    GetHTMLAttr(nsGkAtoms::name, aName);
  }
  void SetName(const nsAString& aName, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aError);
  }

  bool NoResize() const
  {
   return GetBoolAttr(nsGkAtoms::noresize);
  }
  void SetNoResize(bool& aNoResize, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::noresize, aNoResize, aError);
  }

  void GetScrolling(DOMString& aScrolling) const
  {
    GetHTMLAttr(nsGkAtoms::scrolling, aScrolling);
  }
  void SetScrolling(const nsAString& aScrolling, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::scrolling, aScrolling, aError);
  }

  void GetSrc(nsString& aSrc)
  {
    GetURIAttr(nsGkAtoms::src, nullptr, aSrc);
  }
  void SetSrc(const nsAString& aSrc, nsIPrincipal& aTriggeringPrincipal, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::src, aSrc, aTriggeringPrincipal, aError);
  }

  using nsGenericHTMLFrameElement::GetContentDocument;
  using nsGenericHTMLFrameElement::GetContentWindow;

protected:
  virtual ~HTMLFrameElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    GenericSpecifiedValues* aGenericData);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLFrameElement_h
