/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSharedElement.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/HTMLBaseElementBinding.h"
#include "mozilla/dom/HTMLDirectoryElementBinding.h"
#include "mozilla/dom/HTMLHeadElementBinding.h"
#include "mozilla/dom/HTMLHtmlElementBinding.h"
#include "mozilla/dom/HTMLParamElementBinding.h"
#include "mozilla/dom/HTMLQuoteElementBinding.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/MappedDeclarations.h"
#include "nsAttrValueInlines.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsContentUtils.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIURI.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Shared)

namespace mozilla::dom {

extern nsAttrValue::EnumTable kListTypeTable[];

HTMLSharedElement::~HTMLSharedElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLSharedElement)

void HTMLSharedElement::GetHref(nsAString& aValue) {
  MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::base),
             "This should only get called for <base> elements");
  nsAutoString href;
  GetAttr(kNameSpaceID_None, nsGkAtoms::href, href);

  nsCOMPtr<nsIURI> uri;
  Document* doc = OwnerDoc();
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(uri), href, doc,
                                            doc->GetFallbackBaseURI());

  if (!uri) {
    aValue = href;
    return;
  }

  nsAutoCString spec;
  uri->GetSpec(spec);
  CopyUTF8toUTF16(spec, aValue);
}

void HTMLSharedElement::DoneAddingChildren(bool aHaveNotified) {
  if (mNodeInfo->Equals(nsGkAtoms::head)) {
    nsCOMPtr<Document> doc = GetUncomposedDoc();
    if (doc) {
      doc->OnL10nResourceContainerParsed();
    }

    RefPtr<AsyncEventDispatcher> asyncDispatcher =
        new AsyncEventDispatcher(this, u"DOMHeadElementParsed"_ns,
                                 CanBubble::eYes, ChromeOnlyDispatch::eYes);
    // Always run async in order to avoid running script when the content
    // sink isn't expecting it.
    asyncDispatcher->PostDOMEvent();
  }
}

bool HTMLSharedElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None && mNodeInfo->Equals(nsGkAtoms::dir)) {
    if (aAttribute == nsGkAtoms::type) {
      return aResult.ParseEnumValue(aValue, mozilla::dom::kListTypeTable,
                                    false);
    }
    if (aAttribute == nsGkAtoms::start) {
      return aResult.ParseIntWithBounds(aValue, 1);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

static void DirectoryMapAttributesIntoRule(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  if (!aDecls.PropertyIsSet(eCSSProperty_list_style_type)) {
    // type: enum
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
    if (value) {
      if (value->Type() == nsAttrValue::eEnum) {
        aDecls.SetKeywordValue(eCSSProperty_list_style_type,
                               value->GetEnumValue());
      } else {
        aDecls.SetKeywordValue(eCSSProperty_list_style_type,
                               NS_STYLE_LIST_STYLE_DISC);
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLSharedElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  if (mNodeInfo->Equals(nsGkAtoms::dir)) {
    static const MappedAttributeEntry attributes[] = {
        {nsGkAtoms::type},
        // { nsGkAtoms::compact }, // XXX
        {nullptr}};

    static const MappedAttributeEntry* const map[] = {
        attributes,
        sCommonAttributeMap,
    };

    return FindAttributeDependence(aAttribute, map);
  }

  return nsGenericHTMLElement::IsAttributeMapped(aAttribute);
}

static void SetBaseURIUsingFirstBaseWithHref(Document* aDocument,
                                             nsIContent* aMustMatch) {
  MOZ_ASSERT(aDocument, "Need a document!");

  for (nsIContent* child = aDocument->GetFirstChild(); child;
       child = child->GetNextNode()) {
    if (child->IsHTMLElement(nsGkAtoms::base) &&
        child->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
      if (aMustMatch && child != aMustMatch) {
        return;
      }

      // Resolve the <base> element's href relative to our document's
      // fallback base URI.
      nsAutoString href;
      child->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::href, href);

      nsCOMPtr<nsIURI> newBaseURI;
      nsContentUtils::NewURIWithDocumentCharset(
          getter_AddRefs(newBaseURI), href, aDocument,
          aDocument->GetFallbackBaseURI());

      // Check if CSP allows this base-uri
      nsresult rv = NS_OK;
      nsCOMPtr<nsIContentSecurityPolicy> csp = aDocument->GetCsp();
      if (csp && newBaseURI) {
        // base-uri is only enforced if explicitly defined in the
        // policy - do *not* consult default-src, see:
        // http://www.w3.org/TR/CSP2/#directive-default-src
        bool cspPermitsBaseURI = true;
        rv = csp->Permits(child->AsElement(), nullptr /* nsICSPEventListener */,
                          newBaseURI,
                          nsIContentSecurityPolicy::BASE_URI_DIRECTIVE, true,
                          &cspPermitsBaseURI);
        if (NS_FAILED(rv) || !cspPermitsBaseURI) {
          newBaseURI = nullptr;
        }
      }
      aDocument->SetBaseURI(newBaseURI);
      aDocument->SetChromeXHRDocBaseURI(nullptr);
      return;
    }
  }

  aDocument->SetBaseURI(nullptr);
}

static void SetBaseTargetUsingFirstBaseWithTarget(Document* aDocument,
                                                  nsIContent* aMustMatch) {
  MOZ_ASSERT(aDocument, "Need a document!");

  for (nsIContent* child = aDocument->GetFirstChild(); child;
       child = child->GetNextNode()) {
    if (child->IsHTMLElement(nsGkAtoms::base) &&
        child->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::target)) {
      if (aMustMatch && child != aMustMatch) {
        return;
      }

      nsString target;
      child->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::target, target);
      aDocument->SetBaseTarget(target);
      return;
    }
  }

  aDocument->SetBaseTarget(u""_ns);
}

nsresult HTMLSharedElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                         const nsAttrValue* aValue,
                                         const nsAttrValue* aOldValue,
                                         nsIPrincipal* aSubjectPrincipal,
                                         bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::href) {
      // If the href attribute of a <base> tag is changing, we may need to
      // update the document's base URI, which will cause all the links on the
      // page to be re-resolved given the new base.
      // If the href is being unset (aValue is null), we will need to find a new
      // <base>.
      if (mNodeInfo->Equals(nsGkAtoms::base) && IsInUncomposedDoc()) {
        SetBaseURIUsingFirstBaseWithHref(GetUncomposedDoc(),
                                         aValue ? this : nullptr);
      }
    } else if (aName == nsGkAtoms::target) {
      // The target attribute is in pretty much the same situation as the href
      // attribute, above.
      if (mNodeInfo->Equals(nsGkAtoms::base) && IsInUncomposedDoc()) {
        SetBaseTargetUsingFirstBaseWithTarget(GetUncomposedDoc(),
                                              aValue ? this : nullptr);
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNamespaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

nsresult HTMLSharedElement::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  // The document stores a pointer to its base URI and base target, which we may
  // need to update here.
  if (mNodeInfo->Equals(nsGkAtoms::base) && IsInUncomposedDoc()) {
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
      SetBaseURIUsingFirstBaseWithHref(&aContext.OwnerDoc(), this);
    }
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::target)) {
      SetBaseTargetUsingFirstBaseWithTarget(&aContext.OwnerDoc(), this);
    }
  }

  return NS_OK;
}

void HTMLSharedElement::UnbindFromTree(bool aNullParent) {
  Document* doc = GetUncomposedDoc();

  nsGenericHTMLElement::UnbindFromTree(aNullParent);

  // If we're removing a <base> from a document, we may need to update the
  // document's base URI and base target
  if (doc && mNodeInfo->Equals(nsGkAtoms::base)) {
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
      SetBaseURIUsingFirstBaseWithHref(doc, nullptr);
    }
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::target)) {
      SetBaseTargetUsingFirstBaseWithTarget(doc, nullptr);
    }
  }
}

nsMapRuleToAttributesFunc HTMLSharedElement::GetAttributeMappingFunction()
    const {
  if (mNodeInfo->Equals(nsGkAtoms::dir)) {
    return &DirectoryMapAttributesIntoRule;
  }

  return nsGenericHTMLElement::GetAttributeMappingFunction();
}

JSObject* HTMLSharedElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  if (mNodeInfo->Equals(nsGkAtoms::param)) {
    return HTMLParamElement_Binding::Wrap(aCx, this, aGivenProto);
  }
  if (mNodeInfo->Equals(nsGkAtoms::base)) {
    return HTMLBaseElement_Binding::Wrap(aCx, this, aGivenProto);
  }
  if (mNodeInfo->Equals(nsGkAtoms::dir)) {
    return HTMLDirectoryElement_Binding::Wrap(aCx, this, aGivenProto);
  }
  if (mNodeInfo->Equals(nsGkAtoms::q) ||
      mNodeInfo->Equals(nsGkAtoms::blockquote)) {
    return HTMLQuoteElement_Binding::Wrap(aCx, this, aGivenProto);
  }
  if (mNodeInfo->Equals(nsGkAtoms::head)) {
    return HTMLHeadElement_Binding::Wrap(aCx, this, aGivenProto);
  }
  MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::html));
  return HTMLHtmlElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
