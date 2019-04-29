/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/HTMLIFrameElementBinding.h"
#include "mozilla/dom/FeaturePolicy.h"
#include "mozilla/MappedDeclarations.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/StaticPrefs.h"
#include "nsMappedAttributes.h"
#include "nsAttrValueInlines.h"
#include "nsError.h"
#include "nsStyleConsts.h"
#include "nsContentUtils.h"
#include "nsSandboxFlags.h"
#include "nsNetUtil.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(IFrame)

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLIFrameElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLIFrameElement,
                                                  nsGenericHTMLFrameElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFeaturePolicy)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLIFrameElement,
                                                nsGenericHTMLFrameElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFeaturePolicy)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(HTMLIFrameElement, nsGenericHTMLFrameElement)
NS_IMPL_RELEASE_INHERITED(HTMLIFrameElement, nsGenericHTMLFrameElement)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HTMLIFrameElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLFrameElement)

// static
const DOMTokenListSupportedToken HTMLIFrameElement::sSupportedSandboxTokens[] =
    {
#define SANDBOX_KEYWORD(string, atom, flags) string,
#include "IframeSandboxKeywordList.h"
#undef SANDBOX_KEYWORD
        nullptr};

HTMLIFrameElement::HTMLIFrameElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    FromParser aFromParser)
    : nsGenericHTMLFrameElement(std::move(aNodeInfo), aFromParser) {
  // We always need a featurePolicy, even if not exposed.
  mFeaturePolicy = new FeaturePolicy(this);

  nsCOMPtr<nsIPrincipal> origin = GetFeaturePolicyDefaultOrigin();
  MOZ_ASSERT(origin);
  mFeaturePolicy->SetDefaultOrigin(origin);
}

HTMLIFrameElement::~HTMLIFrameElement() {}

NS_IMPL_ELEMENT_CLONE(HTMLIFrameElement)

nsresult HTMLIFrameElement::BindToTree(Document* aDocument, nsIContent* aParent,
                                       nsIContent* aBindingParent) {
  nsresult rv =
      nsGenericHTMLFrameElement::BindToTree(aDocument, aParent, aBindingParent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (StaticPrefs::dom_security_featurePolicy_enabled()) {
    RefreshFeaturePolicy(true /* parse the feature policy attribute */);
  }
  return NS_OK;
}

bool HTMLIFrameElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       nsAttrValue& aResult) {
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

  return nsGenericHTMLFrameElement::ParseAttribute(
      aNamespaceID, aAttribute, aValue, aMaybeScriptedPrincipal, aResult);
}

void HTMLIFrameElement::MapAttributesIntoRule(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  // frameborder: 0 | 1 (| NO | YES in quirks mode)
  // If frameborder is 0 or No, set border to 0
  // else leave it as the value set in html.css
  const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::frameborder);
  if (value && value->Type() == nsAttrValue::eEnum) {
    int32_t frameborder = value->GetEnumValue();
    if (NS_STYLE_FRAME_0 == frameborder || NS_STYLE_FRAME_NO == frameborder ||
        NS_STYLE_FRAME_OFF == frameborder) {
      aDecls.SetPixelValueIfUnset(eCSSProperty_border_top_width, 0.0f);
      aDecls.SetPixelValueIfUnset(eCSSProperty_border_right_width, 0.0f);
      aDecls.SetPixelValueIfUnset(eCSSProperty_border_bottom_width, 0.0f);
      aDecls.SetPixelValueIfUnset(eCSSProperty_border_left_width, 0.0f);
    }
  }

  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLIFrameElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry attributes[] = {
      {nsGkAtoms::width},
      {nsGkAtoms::height},
      {nsGkAtoms::frameborder},
      {nullptr},
  };

  static const MappedAttributeEntry* const map[] = {
      attributes,
      sImageAlignAttributeMap,
      sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLIFrameElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

nsresult HTMLIFrameElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                         const nsAttrValue* aValue,
                                         const nsAttrValue* aOldValue,
                                         nsIPrincipal* aMaybeScriptedPrincipal,
                                         bool aNotify) {
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

    if (StaticPrefs::dom_security_featurePolicy_enabled()) {
      if (aName == nsGkAtoms::allow || aName == nsGkAtoms::src ||
          aName == nsGkAtoms::srcdoc || aName == nsGkAtoms::sandbox) {
        RefreshFeaturePolicy(true /* parse the feature policy attribute */);
      } else if (aName == nsGkAtoms::allowfullscreen ||
                 aName == nsGkAtoms::allowpaymentrequest) {
        RefreshFeaturePolicy(false /* parse the feature policy attribute */);
      }
    }
  }
  return nsGenericHTMLFrameElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

nsresult HTMLIFrameElement::OnAttrSetButNotChanged(
    int32_t aNamespaceID, nsAtom* aName, const nsAttrValueOrString& aValue,
    bool aNotify) {
  AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);

  return nsGenericHTMLFrameElement::OnAttrSetButNotChanged(aNamespaceID, aName,
                                                           aValue, aNotify);
}

void HTMLIFrameElement::AfterMaybeChangeAttr(int32_t aNamespaceID,
                                             nsAtom* aName, bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::srcdoc) {
      // Don't propagate errors from LoadSrc. The attribute was successfully
      // set/unset, that's what we should reflect.
      LoadSrc();
    }
  }
}

uint32_t HTMLIFrameElement::GetSandboxFlags() const {
  const nsAttrValue* sandboxAttr = GetParsedAttr(nsGkAtoms::sandbox);
  // No sandbox attribute, no sandbox flags.
  if (!sandboxAttr) {
    return SANDBOXED_NONE;
  }
  return nsContentUtils::ParseSandboxAttributeToFlags(sandboxAttr);
}

JSObject* HTMLIFrameElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLIFrameElement_Binding::Wrap(aCx, this, aGivenProto);
}

FeaturePolicy* HTMLIFrameElement::Policy() const { return mFeaturePolicy; }

already_AddRefed<nsIPrincipal>
HTMLIFrameElement::GetFeaturePolicyDefaultOrigin() const {
  nsCOMPtr<nsIPrincipal> principal;

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc)) {
    principal = NodePrincipal();
    return principal.forget();
  }

  nsCOMPtr<nsIURI> nodeURI;
  if (GetURIAttr(nsGkAtoms::src, nullptr, getter_AddRefs(nodeURI)) && nodeURI) {
    principal = BasePrincipal::CreateCodebasePrincipal(
        nodeURI, BasePrincipal::Cast(NodePrincipal())->OriginAttributesRef());
  }

  if (!principal) {
    principal = NodePrincipal();
  }

  return principal.forget();
}

void HTMLIFrameElement::RefreshFeaturePolicy(bool aParseAllowAttribute) {
  MOZ_ASSERT(StaticPrefs::dom_security_featurePolicy_enabled());

  if (aParseAllowAttribute) {
    mFeaturePolicy->ResetDeclaredPolicy();

    // The origin can change if 'src' and 'srcdoc' attributes change.
    nsCOMPtr<nsIPrincipal> origin = GetFeaturePolicyDefaultOrigin();
    MOZ_ASSERT(origin);
    mFeaturePolicy->SetDefaultOrigin(origin);

    nsAutoString allow;
    GetAttr(nsGkAtoms::allow, allow);

    if (!allow.IsEmpty()) {
      // Set or reset the FeaturePolicy directives.
      mFeaturePolicy->SetDeclaredPolicy(OwnerDoc(), allow, NodePrincipal(),
                                        origin);
    }

    mFeaturePolicy->InheritPolicy(OwnerDoc()->Policy());
  }

  if (AllowPaymentRequest()) {
    mFeaturePolicy->MaybeSetAllowedPolicy(NS_LITERAL_STRING("payment"));
  }

  if (AllowFullscreen()) {
    mFeaturePolicy->MaybeSetAllowedPolicy(NS_LITERAL_STRING("fullscreen"));
  }
}

}  // namespace dom
}  // namespace mozilla
