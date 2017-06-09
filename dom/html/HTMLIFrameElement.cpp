/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/HTMLIFrameElementBinding.h"
#include "mozilla/GenericSpecifiedValuesInlines.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"
#include "nsError.h"
#include "nsStyleConsts.h"
#include "nsContentUtils.h"
#include "nsSandboxFlags.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(IFrame)

namespace mozilla {
namespace dom {

// static
const DOMTokenListSupportedToken HTMLIFrameElement::sSupportedSandboxTokens[] = {
#define SANDBOX_KEYWORD(string, atom, flags) string,
#include "IframeSandboxKeywordList.h"
#undef SANDBOX_KEYWORD
  nullptr
};

HTMLIFrameElement::HTMLIFrameElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                     FromParser aFromParser)
  : nsGenericHTMLFrameElement(aNodeInfo, aFromParser)
{
}

HTMLIFrameElement::~HTMLIFrameElement()
{
}

NS_IMPL_ISUPPORTS_INHERITED(HTMLIFrameElement, nsGenericHTMLFrameElement,
                            nsIDOMHTMLIFrameElement)

NS_IMPL_ELEMENT_CLONE(HTMLIFrameElement)

NS_IMPL_STRING_ATTR(HTMLIFrameElement, Align, align)
NS_IMPL_STRING_ATTR(HTMLIFrameElement, FrameBorder, frameborder)
NS_IMPL_STRING_ATTR(HTMLIFrameElement, Height, height)
NS_IMPL_URI_ATTR(HTMLIFrameElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(HTMLIFrameElement, MarginHeight, marginheight)
NS_IMPL_STRING_ATTR(HTMLIFrameElement, MarginWidth, marginwidth)
NS_IMPL_STRING_ATTR(HTMLIFrameElement, Name, name)
NS_IMPL_STRING_ATTR(HTMLIFrameElement, Scrolling, scrolling)
NS_IMPL_URI_ATTR(HTMLIFrameElement, Src, src)
NS_IMPL_STRING_ATTR(HTMLIFrameElement, Width, width)
NS_IMPL_BOOL_ATTR(HTMLIFrameElement, AllowFullscreen, allowfullscreen)
NS_IMPL_STRING_ATTR(HTMLIFrameElement, Srcdoc, srcdoc)

NS_IMETHODIMP
HTMLIFrameElement::GetContentDocument(nsIDOMDocument** aContentDocument)
{
  return nsGenericHTMLFrameElement::GetContentDocument(aContentDocument);
}

bool
HTMLIFrameElement::ParseAttribute(int32_t aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::marginwidth) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::marginheight) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::height) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::frameborder) {
      return ParseFrameborderValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::scrolling) {
      return ParseScrollingValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::sandbox) {
      aResult.ParseAtomArray(aValue);
      return true;
    }
  }

  return nsGenericHTMLFrameElement::ParseAttribute(aNamespaceID, aAttribute,
                                                   aValue, aResult);
}

void
HTMLIFrameElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                         GenericSpecifiedValues* aData)
{
  if (aData->ShouldComputeStyleStruct(NS_STYLE_INHERIT_BIT(Border))) {
    // frameborder: 0 | 1 (| NO | YES in quirks mode)
    // If frameborder is 0 or No, set border to 0
    // else leave it as the value set in html.css
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::frameborder);
    if (value && value->Type() == nsAttrValue::eEnum) {
      int32_t frameborder = value->GetEnumValue();
      if (NS_STYLE_FRAME_0 == frameborder ||
          NS_STYLE_FRAME_NO == frameborder ||
          NS_STYLE_FRAME_OFF == frameborder) {
        aData->SetPixelValueIfUnset(eCSSProperty_border_top_width, 0.0f);
        aData->SetPixelValueIfUnset(eCSSProperty_border_right_width, 0.0f);
        aData->SetPixelValueIfUnset(eCSSProperty_border_bottom_width, 0.0f);
        aData->SetPixelValueIfUnset(eCSSProperty_border_left_width, 0.0f);
      }
    }
  }

  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLIFrameElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::width },
    { &nsGkAtoms::height },
    { &nsGkAtoms::frameborder },
    { nullptr },
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sImageAlignAttributeMap,
    sCommonAttributeMap,
  };
  
  return FindAttributeDependence(aAttribute, map);
}



nsMapRuleToAttributesFunc
HTMLIFrameElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

nsresult
HTMLIFrameElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue, bool aNotify)
{
  AfterMaybeChangeAttr(aNameSpaceID, aName, aNotify);

  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::sandbox) {
      if (mFrameLoader) {
        // If we have an nsFrameLoader, apply the new sandbox flags.
        // Since this is called after the setter, the sandbox flags have
        // alreay been updated.
        mFrameLoader->ApplySandboxFlags(GetSandboxFlags());
      }
    }
  }
  return nsGenericHTMLFrameElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                                 aOldValue, aNotify);
}

nsresult
HTMLIFrameElement::OnAttrSetButNotChanged(int32_t aNamespaceID, nsIAtom* aName,
                                          const nsAttrValueOrString& aValue,
                                          bool aNotify)
{
  AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);

  return nsGenericHTMLFrameElement::OnAttrSetButNotChanged(aNamespaceID, aName,
                                                           aValue, aNotify);
}

void
HTMLIFrameElement::AfterMaybeChangeAttr(int32_t aNamespaceID,
                                        nsIAtom* aName,
                                        bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::srcdoc) {
      // Don't propagate errors from LoadSrc. The attribute was successfully
      // set/unset, that's what we should reflect.
      LoadSrc();
    }
  }
}

uint32_t
HTMLIFrameElement::GetSandboxFlags()
{
  const nsAttrValue* sandboxAttr = GetParsedAttr(nsGkAtoms::sandbox);
  // No sandbox attribute, no sandbox flags.
  if (!sandboxAttr) {
    return SANDBOXED_NONE;
  }
  return nsContentUtils::ParseSandboxAttributeToFlags(sandboxAttr);
}

JSObject*
HTMLIFrameElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLIFrameElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
