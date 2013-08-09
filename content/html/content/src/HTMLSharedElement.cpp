/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSharedElement.h"
#include "mozilla/dom/HTMLBaseElementBinding.h"
#include "mozilla/dom/HTMLDirectoryElementBinding.h"
#include "mozilla/dom/HTMLHeadElementBinding.h"
#include "mozilla/dom/HTMLHtmlElementBinding.h"
#include "mozilla/dom/HTMLParamElementBinding.h"
#include "mozilla/dom/HTMLQuoteElementBinding.h"

#include "nsAttrValueInlines.h"
#include "nsStyleConsts.h"
#include "nsRuleData.h"
#include "nsMappedAttributes.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Shared)

namespace mozilla {
namespace dom {

extern nsAttrValue::EnumTable kListTypeTable[];

HTMLSharedElement::~HTMLSharedElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLSharedElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLSharedElement, Element)

// QueryInterface implementation for HTMLSharedElement
NS_INTERFACE_MAP_BEGIN(HTMLSharedElement)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLBaseElement, base)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLDirectoryElement, dir)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLQuoteElement, q)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLQuoteElement, blockquote)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLHeadElement, head)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLHtmlElement, html)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)


NS_IMPL_ELEMENT_CLONE(HTMLSharedElement)

// nsIDOMHTMLQuoteElement
NS_IMPL_URI_ATTR(HTMLSharedElement, Cite, cite)

// nsIDOMHTMLHeadElement
// Empty

// nsIDOMHTMLHtmlElement
NS_IMPL_STRING_ATTR(HTMLSharedElement, Version, version)

// nsIDOMHTMLBaseElement
NS_IMPL_STRING_ATTR(HTMLSharedElement, Target, target)

NS_IMETHODIMP
HTMLSharedElement::GetHref(nsAString& aValue)
{
  nsAutoString href;
  GetAttr(kNameSpaceID_None, nsGkAtoms::href, href);

  nsCOMPtr<nsIURI> uri;
  nsIDocument* doc = OwnerDoc();
  nsContentUtils::NewURIWithDocumentCharset(
    getter_AddRefs(uri), href, doc, doc->GetDocumentURI());

  if (!uri) {
    aValue = href;
    return NS_OK;
  }
  
  nsAutoCString spec;
  uri->GetSpec(spec);
  CopyUTF8toUTF16(spec, aValue);

  return NS_OK;
}

NS_IMETHODIMP
HTMLSharedElement::SetHref(const nsAString& aValue)
{
  return SetAttrHelper(nsGkAtoms::href, aValue);
}


bool
HTMLSharedElement::ParseAttribute(int32_t aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None &&
      mNodeInfo->Equals(nsGkAtoms::dir)) {
    if (aAttribute == nsGkAtoms::type) {
      return aResult.ParseEnumValue(aValue, mozilla::dom::kListTypeTable, false);
    }
    if (aAttribute == nsGkAtoms::start) {
      return aResult.ParseIntWithBounds(aValue, 1);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
DirectoryMapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                               nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(List)) {
    nsCSSValue* listStyleType = aData->ValueForListStyleType();
    if (listStyleType->GetUnit() == eCSSUnit_Null) {
      // type: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
      if (value) {
        if (value->Type() == nsAttrValue::eEnum) {
          listStyleType->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
        } else {
          listStyleType->SetIntValue(NS_STYLE_LIST_STYLE_DISC, eCSSUnit_Enumerated);
        }
      }
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLSharedElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  if (mNodeInfo->Equals(nsGkAtoms::dir)) {
    static const MappedAttributeEntry attributes[] = {
      { &nsGkAtoms::type },
      // { &nsGkAtoms::compact }, // XXX
      { nullptr} 
    };
  
    static const MappedAttributeEntry* const map[] = {
      attributes,
      sCommonAttributeMap,
    };

    return FindAttributeDependence(aAttribute, map);
  }

  return nsGenericHTMLElement::IsAttributeMapped(aAttribute);
}

static void
SetBaseURIUsingFirstBaseWithHref(nsIDocument* aDocument, nsIContent* aMustMatch)
{
  NS_PRECONDITION(aDocument, "Need a document!");

  for (nsIContent* child = aDocument->GetFirstChild(); child;
       child = child->GetNextNode()) {
    if (child->IsHTML(nsGkAtoms::base) &&
        child->HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
      if (aMustMatch && child != aMustMatch) {
        return;
      }

      // Resolve the <base> element's href relative to our document URI
      nsAutoString href;
      child->GetAttr(kNameSpaceID_None, nsGkAtoms::href, href);

      nsCOMPtr<nsIURI> newBaseURI;
      nsContentUtils::NewURIWithDocumentCharset(
        getter_AddRefs(newBaseURI), href, aDocument,
        aDocument->GetDocumentURI());

      // Try to set our base URI.  If that fails, try to set base URI to null
      nsresult rv = aDocument->SetBaseURI(newBaseURI);
      if (NS_FAILED(rv)) {
        aDocument->SetBaseURI(nullptr);
      }
      return;
    }
  }

  aDocument->SetBaseURI(nullptr);
}

static void
SetBaseTargetUsingFirstBaseWithTarget(nsIDocument* aDocument,
                                      nsIContent* aMustMatch)
{
  NS_PRECONDITION(aDocument, "Need a document!");

  for (nsIContent* child = aDocument->GetFirstChild(); child;
       child = child->GetNextNode()) {
    if (child->IsHTML(nsGkAtoms::base) &&
        child->HasAttr(kNameSpaceID_None, nsGkAtoms::target)) {
      if (aMustMatch && child != aMustMatch) {
        return;
      }

      nsString target;
      child->GetAttr(kNameSpaceID_None, nsGkAtoms::target, target);
      aDocument->SetBaseTarget(target);
      return;
    }
  }

  aDocument->SetBaseTarget(EmptyString());
}

nsresult
HTMLSharedElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the href attribute of a <base> tag is changing, we may need to update
  // the document's base URI, which will cause all the links on the page to be
  // re-resolved given the new base.  If the target attribute is changing, we
  // similarly need to change the base target.
  if (mNodeInfo->Equals(nsGkAtoms::base) &&
      aNameSpaceID == kNameSpaceID_None &&
      IsInDoc()) {
    if (aName == nsGkAtoms::href) {
      SetBaseURIUsingFirstBaseWithHref(GetCurrentDoc(), this);
    } else if (aName == nsGkAtoms::target) {
      SetBaseTargetUsingFirstBaseWithTarget(GetCurrentDoc(), this);
    }
  }

  return NS_OK;
}

nsresult
HTMLSharedElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                             bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aName, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we're the first <base> with an href and our href attribute is being
  // unset, then we're no longer the first <base> with an href, and we need to
  // find the new one.  Similar for target.
  if (mNodeInfo->Equals(nsGkAtoms::base) &&
      aNameSpaceID == kNameSpaceID_None &&
      IsInDoc()) {
    if (aName == nsGkAtoms::href) {
      SetBaseURIUsingFirstBaseWithHref(GetCurrentDoc(), nullptr);
    } else if (aName == nsGkAtoms::target) {
      SetBaseTargetUsingFirstBaseWithTarget(GetCurrentDoc(), nullptr);
    }
  }

  return NS_OK;
}

nsresult
HTMLSharedElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // The document stores a pointer to its base URI and base target, which we may
  // need to update here.
  if (mNodeInfo->Equals(nsGkAtoms::base) &&
      aDocument) {
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::href)) {
      SetBaseURIUsingFirstBaseWithHref(aDocument, this);
    }
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::target)) {
      SetBaseTargetUsingFirstBaseWithTarget(aDocument, this);
    }
  }

  return NS_OK;
}

void
HTMLSharedElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsIDocument* doc = GetCurrentDoc();

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

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

nsMapRuleToAttributesFunc
HTMLSharedElement::GetAttributeMappingFunction() const
{
  if (mNodeInfo->Equals(nsGkAtoms::dir)) {
    return &DirectoryMapAttributesIntoRule;
  }

  return nsGenericHTMLElement::GetAttributeMappingFunction();
}

JSObject*
HTMLSharedElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  if (mNodeInfo->Equals(nsGkAtoms::param)) {
    return HTMLParamElementBinding::Wrap(aCx, aScope, this);
  }
  if (mNodeInfo->Equals(nsGkAtoms::base)) {
    return HTMLBaseElementBinding::Wrap(aCx, aScope, this);
  }
  if (mNodeInfo->Equals(nsGkAtoms::dir)) {
    return HTMLDirectoryElementBinding::Wrap(aCx, aScope, this);
  }
  if (mNodeInfo->Equals(nsGkAtoms::q) ||
      mNodeInfo->Equals(nsGkAtoms::blockquote)) {
    return HTMLQuoteElementBinding::Wrap(aCx, aScope, this);
  }
  if (mNodeInfo->Equals(nsGkAtoms::head)) {
    return HTMLHeadElementBinding::Wrap(aCx, aScope, this);
  }
  MOZ_ASSERT(mNodeInfo->Equals(nsGkAtoms::html));
  return HTMLHtmlElementBinding::Wrap(aCx, aScope, this);
}

} // namespace mozilla
} // namespace dom
