/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_l10n_L10nOverlays_h
#define mozilla_dom_l10n_L10nOverlays_h

#include "mozilla/dom/Element.h"
#include "mozilla/dom/L10nOverlaysBinding.h"
#include "mozilla/dom/LocalizationBinding.h"

namespace mozilla {
namespace dom {

class L10nOverlays {
 public:
  /**
   * Translate an element.
   *
   * Translate the element's text content and attributes. Some HTML markup is
   * allowed in the translation. The element's children with the data-l10n-name
   * attribute will be treated as arguments to the translation. If the
   * translation defines the same children, their attributes and text contents
   * will be used for translating the matching source child.
   */
  static void TranslateElement(const GlobalObject& aGlobal, Element& aElement,
                               const L10nMessage& aTranslation,
                               Nullable<nsTArray<L10nOverlaysError>>& aErrors);
  static void TranslateElement(Element& aElement,
                               const L10nMessage& aTranslation,
                               nsTArray<L10nOverlaysError>& aErrors,
                               ErrorResult& aRv);

 private:
  /**
   * Check if attribute is allowed for the given element.
   *
   * This method is used by the sanitizer when the translation markup contains
   * DOM attributes, or when the translation has traits which map to DOM
   * attributes.
   *
   * `aExplicitlyAllowed` can be passed as a list of attributes explicitly
   * allowed on this element.
   */
  static bool IsAttrNameLocalizable(const nsAtom* nameAtom, Element* aElement,
                                    nsTArray<nsString>* aExplicitlyAllowed);

  /**
   * Create a text node from text content of an Element.
   */
  static already_AddRefed<nsINode> CreateTextNodeFromTextContent(
      Element* aElement, ErrorResult& aRv);

  /**
   * Transplant localizable attributes of an element to another element.
   *
   * Any localizable attributes already set on the target element will be
   * cleared.
   */
  static void OverlayAttributes(
      const Nullable<Sequence<AttributeNameValue>>& aTranslation,
      Element* aToElement, ErrorResult& aRv);
  static void OverlayAttributes(Element* aFromElement, Element* aToElement,
                                ErrorResult& aRv);

  /**
   * Helper to set textContent and localizable attributes on an element.
   */
  static void ShallowPopulateUsing(Element* aFromElement, Element* aToElement,
                                   ErrorResult& aRv);

  /**
   * Sanitize a child element created by the translation.
   *
   * Try to find a corresponding child in sourceElement and use it as the base
   * for the sanitization. This will preserve functional attributes defined on
   * the child element in the source HTML.
   */
  static already_AddRefed<nsINode> GetNodeForNamedElement(
      Element* aSourceElement, Element* aTranslatedChild,
      nsTArray<L10nOverlaysError>& aErrors, ErrorResult& aRv);

  /**
   * Check if element is allowed in the translation.
   *
   * This method is used by the sanitizer when the translation markup contains
   * an element which is not present in the source code.
   */
  static bool IsElementAllowed(Element* aElement);

  /**
   * Sanitize an allowed element.
   *
   * Text-level elements allowed in translations may only use safe attributes
   * and will have any nested markup stripped to text content.
   */
  static already_AddRefed<Element> CreateSanitizedElement(Element* aElement,
                                                          ErrorResult& aRv);

  /**
   * Replace child nodes of an element with child nodes of another element.
   *
   * The contents of the target element will be cleared and fully replaced with
   * sanitized contents of the source element.
   */
  static void OverlayChildNodes(DocumentFragment* aFromFragment,
                                Element* aToElement,
                                nsTArray<L10nOverlaysError>& aErrors,
                                ErrorResult& aRv);

  /**
   * A helper used to test if the string contains HTML markup.
   */
  static bool ContainsMarkup(const nsAString& aStr);
};

}  // namespace dom
}  // namespace mozilla

#endif
