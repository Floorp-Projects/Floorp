/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "L10nOverlays.h"
#include "mozilla/dom/HTMLTemplateElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "HTMLSplitOnSpacesTokenizer.h"
#include "nsHtml5StringParser.h"
#include "nsTextNode.h"

using namespace mozilla::dom;
using namespace mozilla;

bool L10nOverlays::IsAttrNameLocalizable(
    const nsAtom* nameAtom, Element* aElement,
    nsTArray<nsString>* aExplicitlyAllowed) {
  nsAutoString name;
  nameAtom->ToString(name);

  if (aExplicitlyAllowed->Contains(name)) {
    return true;
  }

  nsAtom* elemName = aElement->NodeInfo()->NameAtom();

  uint32_t nameSpace = aElement->NodeInfo()->NamespaceID();

  if (nameSpace == kNameSpaceID_XHTML) {
    // Is it a globally safe attribute?
    if (nameAtom == nsGkAtoms::title || nameAtom == nsGkAtoms::aria_label ||
        nameAtom == nsGkAtoms::aria_valuetext) {
      return true;
    }

    // Is it allowed on this element?
    if (elemName == nsGkAtoms::a) {
      return nameAtom == nsGkAtoms::download;
    }
    if (elemName == nsGkAtoms::area) {
      return nameAtom == nsGkAtoms::download || nameAtom == nsGkAtoms::alt;
    }
    if (elemName == nsGkAtoms::input) {
      // Special case for value on HTML inputs with type button, reset, submit
      if (nameAtom == nsGkAtoms::value) {
        HTMLInputElement* input = HTMLInputElement::FromNode(aElement);
        if (input) {
          uint32_t type = input->ControlType();
          if (type == NS_FORM_INPUT_SUBMIT || type == NS_FORM_INPUT_BUTTON ||
              type == NS_FORM_INPUT_RESET) {
            return true;
          }
        }
      }
      return nameAtom == nsGkAtoms::alt || nameAtom == nsGkAtoms::placeholder;
    }
    if (elemName == nsGkAtoms::menuitem) {
      return nameAtom == nsGkAtoms::label;
    }
    if (elemName == nsGkAtoms::menu) {
      return nameAtom == nsGkAtoms::label;
    }
    if (elemName == nsGkAtoms::optgroup) {
      return nameAtom == nsGkAtoms::label;
    }
    if (elemName == nsGkAtoms::option) {
      return nameAtom == nsGkAtoms::label;
    }
    if (elemName == nsGkAtoms::track) {
      return nameAtom == nsGkAtoms::label;
    }
    if (elemName == nsGkAtoms::img) {
      return nameAtom == nsGkAtoms::alt;
    }
    if (elemName == nsGkAtoms::textarea) {
      return nameAtom == nsGkAtoms::placeholder;
    }
    if (elemName == nsGkAtoms::th) {
      return nameAtom == nsGkAtoms::abbr;
    }

  } else if (nameSpace == kNameSpaceID_XUL) {
    // Is it a globally safe attribute?
    if (nameAtom == nsGkAtoms::accesskey || nameAtom == nsGkAtoms::aria_label ||
        nameAtom == nsGkAtoms::aria_valuetext || nameAtom == nsGkAtoms::label ||
        nameAtom == nsGkAtoms::title || nameAtom == nsGkAtoms::tooltiptext) {
      return true;
    }

    // Is it allowed on this element?
    if (elemName == nsGkAtoms::description) {
      return nameAtom == nsGkAtoms::value;
    }
    if (elemName == nsGkAtoms::key) {
      return nameAtom == nsGkAtoms::key || nameAtom == nsGkAtoms::keycode;
    }
    if (elemName == nsGkAtoms::label) {
      return nameAtom == nsGkAtoms::value;
    }
  }

  return false;
}

already_AddRefed<nsINode> L10nOverlays::CreateTextNodeFromTextContent(
    Element* aElement, ErrorResult& aRv) {
  nsAutoString content;
  aElement->GetTextContent(content, aRv);

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return aElement->OwnerDoc()->CreateTextNode(content);
}

class AttributeNameValueComparator {
 public:
  bool Equals(const AttributeNameValue& aAttribute,
              const nsAttrName* aAttrName) const {
    return aAttrName->Equals(NS_ConvertUTF8toUTF16(aAttribute.mName));
  }
};

void L10nOverlays::OverlayAttributes(
    const Nullable<Sequence<AttributeNameValue>>& aTranslation,
    Element* aToElement, ErrorResult& aRv) {
  nsTArray<nsString> explicitlyAllowed;

  nsAutoString l10nAttrs;
  aToElement->GetAttr(kNameSpaceID_None, nsGkAtoms::datal10nattrs, l10nAttrs);

  HTMLSplitOnSpacesTokenizer tokenizer(l10nAttrs, ',');
  while (tokenizer.hasMoreTokens()) {
    const nsAString& token = tokenizer.nextToken();
    if (!token.IsEmpty() && !explicitlyAllowed.Contains(token)) {
      explicitlyAllowed.AppendElement(token);
    }
  }

  uint32_t i = aToElement->GetAttrCount();
  while (i > 0) {
    const nsAttrName* attrName = aToElement->GetAttrNameAt(i - 1);

    if (IsAttrNameLocalizable(attrName->LocalName(), aToElement,
                              &explicitlyAllowed) &&
        (aTranslation.IsNull() ||
         !aTranslation.Value().Contains(attrName,
                                        AttributeNameValueComparator()))) {
      nsAutoString name;
      attrName->LocalName()->ToString(name);
      aToElement->RemoveAttribute(name, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
    }
    i--;
  }

  if (aTranslation.IsNull()) {
    return;
  }

  for (auto& attribute : aTranslation.Value()) {
    RefPtr<nsAtom> nameAtom = NS_Atomize(attribute.mName);
    if (IsAttrNameLocalizable(nameAtom, aToElement, &explicitlyAllowed)) {
      NS_ConvertUTF8toUTF16 value(attribute.mValue);
      if (!aToElement->AttrValueIs(kNameSpaceID_None, nameAtom, value,
                                   eCaseMatters)) {
        aToElement->SetAttr(nameAtom, value, aRv);
        if (NS_WARN_IF(aRv.Failed())) {
          return;
        }
      }
    }
  }
}

void L10nOverlays::OverlayAttributes(Element* aFromElement, Element* aToElement,
                                     ErrorResult& aRv) {
  Nullable<Sequence<AttributeNameValue>> attributes;
  uint32_t attrCount = aFromElement->GetAttrCount();

  if (attrCount == 0) {
    attributes.SetNull();
  } else {
    Sequence<AttributeNameValue> sequence;

    uint32_t i = 0;
    while (BorrowedAttrInfo info = aFromElement->GetAttrInfoAt(i++)) {
      AttributeNameValue* attr = sequence.AppendElement(fallible);
      MOZ_ASSERT(info.mName->NamespaceEquals(kNameSpaceID_None),
                 "No namespaced attributes allowed.");
      info.mName->LocalName()->ToUTF8String(attr->mName);

      nsAutoString value;
      info.mValue->ToString(value);
      attr->mValue.Assign(NS_ConvertUTF16toUTF8(value));
    }

    attributes.SetValue(sequence);
  }

  return OverlayAttributes(attributes, aToElement, aRv);
}

void L10nOverlays::ShallowPopulateUsing(Element* aFromElement,
                                        Element* aToElement, ErrorResult& aRv) {
  nsAutoString content;
  aFromElement->GetTextContent(content, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  aToElement->SetTextContent(content, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  OverlayAttributes(aFromElement, aToElement, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

already_AddRefed<nsINode> L10nOverlays::GetNodeForNamedElement(
    Element* aSourceElement, Element* aTranslatedChild,
    nsTArray<L10nOverlaysError>& aErrors, ErrorResult& aRv) {
  nsAutoString childName;
  aTranslatedChild->GetAttr(kNameSpaceID_None, nsGkAtoms::datal10nname,
                            childName);
  RefPtr<Element> sourceChild = nullptr;

  nsINodeList* childNodes = aSourceElement->ChildNodes();
  for (uint32_t i = 0; i < childNodes->Length(); i++) {
    nsINode* childNode = childNodes->Item(i);

    if (!childNode->IsElement()) {
      continue;
    }
    Element* childElement = childNode->AsElement();

    if (childElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::datal10nname,
                                  childName, eCaseMatters)) {
      sourceChild = childElement;
      break;
    }
  }

  if (!sourceChild) {
    L10nOverlaysError error;
    error.mCode.Construct(L10nOverlays_Binding::ERROR_NAMED_ELEMENT_MISSING);
    error.mL10nName.Construct(childName);
    aErrors.AppendElement(error);
    return CreateTextNodeFromTextContent(aTranslatedChild, aRv);
  }

  nsAtom* sourceChildName = sourceChild->NodeInfo()->NameAtom();
  nsAtom* translatedChildName = aTranslatedChild->NodeInfo()->NameAtom();
  if (sourceChildName != translatedChildName &&
      // Create a specific exception for img vs. image mismatches,
      // see bug 1543493
      !(translatedChildName == nsGkAtoms::img &&
        sourceChildName == nsGkAtoms::image)) {
    L10nOverlaysError error;
    error.mCode.Construct(
        L10nOverlays_Binding::ERROR_NAMED_ELEMENT_TYPE_MISMATCH);
    error.mL10nName.Construct(childName);
    error.mTranslatedElementName.Construct(
        aTranslatedChild->NodeInfo()->LocalName());
    error.mSourceElementName.Construct(sourceChild->NodeInfo()->LocalName());
    aErrors.AppendElement(error);
    return CreateTextNodeFromTextContent(aTranslatedChild, aRv);
  }

  aSourceElement->RemoveChild(*sourceChild, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  RefPtr<nsINode> clone = sourceChild->CloneNode(false, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  ShallowPopulateUsing(aTranslatedChild, clone->AsElement(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  return clone.forget();
}

bool L10nOverlays::IsElementAllowed(Element* aElement) {
  uint32_t nameSpace = aElement->NodeInfo()->NamespaceID();
  if (nameSpace != kNameSpaceID_XHTML) {
    return false;
  }

  nsAtom* nameAtom = aElement->NodeInfo()->NameAtom();

  return nameAtom == nsGkAtoms::em || nameAtom == nsGkAtoms::strong ||
         nameAtom == nsGkAtoms::small || nameAtom == nsGkAtoms::s ||
         nameAtom == nsGkAtoms::cite || nameAtom == nsGkAtoms::q ||
         nameAtom == nsGkAtoms::dfn || nameAtom == nsGkAtoms::abbr ||
         nameAtom == nsGkAtoms::data || nameAtom == nsGkAtoms::time ||
         nameAtom == nsGkAtoms::code || nameAtom == nsGkAtoms::var ||
         nameAtom == nsGkAtoms::samp || nameAtom == nsGkAtoms::kbd ||
         nameAtom == nsGkAtoms::sub || nameAtom == nsGkAtoms::sup ||
         nameAtom == nsGkAtoms::i || nameAtom == nsGkAtoms::b ||
         nameAtom == nsGkAtoms::u || nameAtom == nsGkAtoms::mark ||
         nameAtom == nsGkAtoms::bdi || nameAtom == nsGkAtoms::bdo ||
         nameAtom == nsGkAtoms::span || nameAtom == nsGkAtoms::br ||
         nameAtom == nsGkAtoms::wbr;
}

already_AddRefed<Element> L10nOverlays::CreateSanitizedElement(
    Element* aElement, ErrorResult& aRv) {
  // Start with an empty element of the same type to remove nested children
  // and non-localizable attributes defined by the translation.

  nsAutoString nameSpaceURI;
  aElement->NodeInfo()->GetNamespaceURI(nameSpaceURI);
  ElementCreationOptionsOrString options;
  RefPtr<Element> clone = aElement->OwnerDoc()->CreateElementNS(
      nameSpaceURI, aElement->NodeInfo()->LocalName(), options, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  ShallowPopulateUsing(aElement, clone, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  return clone.forget();
}

void L10nOverlays::OverlayChildNodes(DocumentFragment* aFromFragment,
                                     Element* aToElement,
                                     nsTArray<L10nOverlaysError>& aErrors,
                                     ErrorResult& aRv) {
  nsINodeList* childNodes = aFromFragment->ChildNodes();
  for (uint32_t i = 0; i < childNodes->Length(); i++) {
    nsINode* childNode = childNodes->Item(i);

    if (!childNode->IsElement()) {
      // Keep the translated text node.
      continue;
    }

    RefPtr<Element> childElement = childNode->AsElement();

    if (childElement->HasAttr(kNameSpaceID_None, nsGkAtoms::datal10nname)) {
      RefPtr<nsINode> sanitized =
          GetNodeForNamedElement(aToElement, childElement, aErrors, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
      aFromFragment->ReplaceChild(*sanitized, *childNode, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
      continue;
    }

    if (IsElementAllowed(childElement)) {
      RefPtr<Element> sanitized = CreateSanitizedElement(childElement, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
      aFromFragment->ReplaceChild(*sanitized, *childNode, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
      continue;
    }

    L10nOverlaysError error;
    error.mCode.Construct(L10nOverlays_Binding::ERROR_FORBIDDEN_TYPE);
    error.mTranslatedElementName.Construct(
        childElement->NodeInfo()->LocalName());
    aErrors.AppendElement(error);

    // If all else fails, replace the element with its text content.
    RefPtr<nsINode> textNode = CreateTextNodeFromTextContent(childElement, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    aFromFragment->ReplaceChild(*textNode, *childNode, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  while (aToElement->HasChildren()) {
    aToElement->RemoveChildNode(aToElement->GetLastChild(), true);
  }
  aToElement->AppendChild(*aFromFragment, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

void L10nOverlays::TranslateElement(
    const GlobalObject& aGlobal, Element& aElement,
    const L10nMessage& aTranslation,
    Nullable<nsTArray<L10nOverlaysError>>& aErrors) {
  nsTArray<L10nOverlaysError> errors;

  ErrorResult rv;

  TranslateElement(aElement, aTranslation, errors, rv);
  if (NS_WARN_IF(rv.Failed())) {
    L10nOverlaysError error;
    error.mCode.Construct(L10nOverlays_Binding::ERROR_UNKNOWN);
    errors.AppendElement(error);
  }
  if (!errors.IsEmpty()) {
    aErrors.SetValue(std::move(errors));
  }
}

bool L10nOverlays::ContainsMarkup(const nsACString& aStr) {
  // We use our custom ContainsMarkup rather than the
  // one from FragmentOrElement.cpp, because we don't
  // want to trigger HTML parsing on every `Preferences & Options`
  // type of string.
  const char* start = aStr.BeginReading();
  const char* end = aStr.EndReading();

  while (start != end) {
    char c = *start;
    if (c == '<') {
      return true;
    }
    ++start;

    if (c == '&' && start != end) {
      c = *start;
      if (c == '#' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z')) {
        return true;
      }
      ++start;
    }
  }

  return false;
}

void L10nOverlays::TranslateElement(Element& aElement,
                                    const L10nMessage& aTranslation,
                                    nsTArray<L10nOverlaysError>& aErrors,
                                    ErrorResult& aRv) {
  if (!aTranslation.mValue.IsVoid()) {
    NodeInfo* nodeInfo = aElement.NodeInfo();
    if (nodeInfo->NameAtom() == nsGkAtoms::title &&
        nodeInfo->NamespaceID() == kNameSpaceID_XHTML) {
      // A special case for the HTML title element whose content must be text.
      aElement.SetTextContent(NS_ConvertUTF8toUTF16(aTranslation.mValue), aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
    } else if (!ContainsMarkup(aTranslation.mValue)) {
      // If the translation doesn't contain any markup skip the overlay logic.
      aElement.SetTextContent(NS_ConvertUTF8toUTF16(aTranslation.mValue), aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
    } else {
      // Else parse the translation's HTML into a DocumentFragment,
      // sanitize it and replace the element's content.
      RefPtr<DocumentFragment> fragment =
          new (aElement.OwnerDoc()->NodeInfoManager())
              DocumentFragment(aElement.OwnerDoc()->NodeInfoManager());
      nsContentUtils::ParseFragmentHTML(
          NS_ConvertUTF8toUTF16(aTranslation.mValue), fragment,
          nsGkAtoms::_template, kNameSpaceID_XHTML, false, true);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }

      OverlayChildNodes(fragment, &aElement, aErrors, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
    }
  }

  // Even if the translation doesn't define any localizable attributes, run
  // overlayAttributes to remove any localizable attributes set by previous
  // translations.
  OverlayAttributes(aTranslation.mAttributes, &aElement, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}
