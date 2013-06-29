/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/HTMLIFrameElementBinding.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"
#include "nsError.h"
#include "nsRuleData.h"
#include "nsStyleConsts.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(IFrame)

namespace mozilla {
namespace dom {

HTMLIFrameElement::HTMLIFrameElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                     FromParser aFromParser)
  : nsGenericHTMLFrameElement(aNodeInfo, aFromParser)
{
  SetIsDOMBinding();
}

HTMLIFrameElement::~HTMLIFrameElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLIFrameElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLIFrameElement, Element)

// QueryInterface implementation for HTMLIFrameElement
NS_INTERFACE_TABLE_HEAD(HTMLIFrameElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLFrameElement)
  NS_INTERFACE_TABLE_INHERITED1(HTMLIFrameElement,
                                nsIDOMHTMLIFrameElement)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
NS_ELEMENT_INTERFACE_MAP_END

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
NS_IMPL_STRING_ATTR(HTMLIFrameElement, Sandbox, sandbox)
NS_IMPL_STRING_ATTR(HTMLIFrameElement, Srcdoc, srcdoc)

void
HTMLIFrameElement::GetItemValueText(nsAString& aValue)
{
  GetSrc(aValue);
}

void
HTMLIFrameElement::SetItemValueText(const nsAString& aValue)
{
  SetSrc(aValue);
}

NS_IMETHODIMP
HTMLIFrameElement::GetContentDocument(nsIDOMDocument** aContentDocument)
{
  return nsGenericHTMLFrameElement::GetContentDocument(aContentDocument);
}

NS_IMETHODIMP
HTMLIFrameElement::GetContentWindow(nsIDOMWindow** aContentWindow)
{
  return nsGenericHTMLFrameElement::GetContentWindow(aContentWindow);
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
  }

  return nsGenericHTMLFrameElement::ParseAttribute(aNamespaceID, aAttribute,
                                                   aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Border)) {
    // frameborder: 0 | 1 (| NO | YES in quirks mode)
    // If frameborder is 0 or No, set border to 0
    // else leave it as the value set in html.css
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::frameborder);
    if (value && value->Type() == nsAttrValue::eEnum) {
      int32_t frameborder = value->GetEnumValue();
      if (NS_STYLE_FRAME_0 == frameborder ||
          NS_STYLE_FRAME_NO == frameborder ||
          NS_STYLE_FRAME_OFF == frameborder) {
        nsCSSValue* borderLeftWidth = aData->ValueForBorderLeftWidthValue();
        if (borderLeftWidth->GetUnit() == eCSSUnit_Null)
          borderLeftWidth->SetFloatValue(0.0f, eCSSUnit_Pixel);
        nsCSSValue* borderRightWidth = aData->ValueForBorderRightWidthValue();
        if (borderRightWidth->GetUnit() == eCSSUnit_Null)
          borderRightWidth->SetFloatValue(0.0f, eCSSUnit_Pixel);
        nsCSSValue* borderTopWidth = aData->ValueForBorderTopWidth();
        if (borderTopWidth->GetUnit() == eCSSUnit_Null)
          borderTopWidth->SetFloatValue(0.0f, eCSSUnit_Pixel);
        nsCSSValue* borderBottomWidth = aData->ValueForBorderBottomWidth();
        if (borderBottomWidth->GetUnit() == eCSSUnit_Null)
          borderBottomWidth->SetFloatValue(0.0f, eCSSUnit_Pixel);
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)) {
    // width: value
    nsCSSValue* width = aData->ValueForWidth();
    if (width->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
      if (value && value->Type() == nsAttrValue::eInteger)
        width->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      else if (value && value->Type() == nsAttrValue::ePercent)
        width->SetPercentValue(value->GetPercentValue());
    }

    // height: value
    nsCSSValue* height = aData->ValueForHeight();
    if (height->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
      if (value && value->Type() == nsAttrValue::eInteger)
        height->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      else if (value && value->Type() == nsAttrValue::ePercent)
        height->SetPercentValue(value->GetPercentValue());
    }
  }

  nsGenericHTMLElement::MapScrollingAttributeInto(aAttributes, aData);
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
    sScrollingAttributeMap,
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
HTMLIFrameElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify)
{
  nsresult rv = nsGenericHTMLFrameElement::SetAttr(aNameSpaceID, aName,
                                                   aPrefix, aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::srcdoc) {
    // Don't propagate error here. The attribute was successfully set, that's
    // what we should reflect.
    LoadSrc();
  }

  return NS_OK;
}

nsresult
HTMLIFrameElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue,
                                bool aNotify)
{
  if (aName == nsGkAtoms::sandbox && aNameSpaceID == kNameSpaceID_None) {
    // Parse the new value of the sandbox attribute, and if we have a docshell
    // set its sandbox flags appropriately.
    if (mFrameLoader) {
      nsCOMPtr<nsIDocShell> docshell = mFrameLoader->GetExistingDocShell();

      if (docshell) {
        uint32_t newFlags = 0;
        // If a nullptr aValue is passed in, we want to clear the sandbox flags
        // which we will do by setting them to 0.
        if (aValue) {
          nsAutoString strValue;
          aValue->ToString(strValue);
          newFlags = nsContentUtils::ParseSandboxAttributeToFlags(
            strValue);
        }   
        docshell->SetSandboxFlags(newFlags);
      }
    }
  }
  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aNotify);
}

nsresult
HTMLIFrameElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify)
{
  // Invoke on the superclass.
  nsresult rv = nsGenericHTMLFrameElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::srcdoc) {
    // Fall back to the src attribute, if any
    LoadSrc();
  }

  return NS_OK;
}

uint32_t
HTMLIFrameElement::GetSandboxFlags()
{
  nsAutoString sandboxAttr;

  if (GetAttr(kNameSpaceID_None, nsGkAtoms::sandbox, sandboxAttr)) {
    return nsContentUtils::ParseSandboxAttributeToFlags(sandboxAttr);
  }

  // No sandbox attribute, no sandbox flags.
  return 0;
}

JSObject*
HTMLIFrameElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLIFrameElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
