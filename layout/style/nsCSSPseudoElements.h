/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-elements */

#ifndef nsCSSPseudoElements_h___
#define nsCSSPseudoElements_h___

#include "nsGkAtoms.h"
#include "mozilla/CSSEnabledState.h"
#include "mozilla/Compiler.h"
#include "mozilla/PseudoStyleType.h"

// Is this pseudo-element a CSS2 pseudo-element that can be specified
// with the single colon syntax (in addition to the double-colon syntax,
// which can be used for all pseudo-elements)?
//
// Note: We also rely on this for IsEagerlyCascadedInServo.
#define CSS_PSEUDO_ELEMENT_IS_CSS2 (1 << 0)
// Is this pseudo-element a pseudo-element that can contain other
// elements?
// (Currently pseudo-elements are either leaves of the tree (relative to
// real elements) or they contain other elements in a non-tree-like
// manner (i.e., like incorrectly-nested start and end tags).  It's
// possible that in the future there might be container pseudo-elements
// that form a properly nested tree structure.  If that happens, we
// should probably split this flag into two.)
#define CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS (1 << 1)
// Flag to add the ability to take into account style attribute set for the
// pseudo element (by default it's ignored).
#define CSS_PSEUDO_ELEMENT_SUPPORTS_STYLE_ATTRIBUTE (1 << 2)
// Flag that indicate the pseudo-element supports a user action pseudo-class
// following it, such as :active or :hover.  This would normally correspond
// to whether the pseudo-element is tree-like, but we don't support these
// pseudo-classes on ::before and ::after generated content yet.  See
// http://dev.w3.org/csswg/selectors4/#pseudo-elements.
#define CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE (1 << 3)
// Should this pseudo-element be enabled only for UA sheets?
#define CSS_PSEUDO_ELEMENT_ENABLED_IN_UA_SHEETS (1 << 4)
// Should this pseudo-element be enabled only for UA sheets and chrome
// stylesheets?
#define CSS_PSEUDO_ELEMENT_ENABLED_IN_CHROME (1 << 5)

#define CSS_PSEUDO_ELEMENT_ENABLED_IN_UA_SHEETS_AND_CHROME \
  (CSS_PSEUDO_ELEMENT_ENABLED_IN_UA_SHEETS |               \
   CSS_PSEUDO_ELEMENT_ENABLED_IN_CHROME)

// Can we use the ChromeOnly document.createElement(..., { pseudo: "::foo" })
// API for creating pseudo-implementing native anonymous content in JS with this
// pseudo-element?
#define CSS_PSEUDO_ELEMENT_IS_JS_CREATED_NAC (1 << 6)
// Does this pseudo-element act like an item for containers (such as flex and
// grid containers) and thus needs parent display-based style fixup?
#define CSS_PSEUDO_ELEMENT_IS_FLEX_OR_GRID_ITEM (1 << 7)

class nsCSSPseudoElements {
  typedef mozilla::PseudoStyleType Type;
  typedef mozilla::CSSEnabledState EnabledState;

 public:
  static bool IsPseudoElement(nsAtom* aAtom);

  static bool IsCSS2PseudoElement(nsAtom* aAtom);

  static bool IsEagerlyCascadedInServo(const Type aType) {
    return PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_IS_CSS2);
  }

 public:
#ifdef DEBUG
  static void AssertAtoms();
#endif

// Alias nsCSSPseudoElements::foo() to nsGkAtoms::foo.
#define CSS_PSEUDO_ELEMENT(name_, value_, flags_)         \
  static nsCSSPseudoElementStaticAtom* name_() {          \
    return const_cast<nsCSSPseudoElementStaticAtom*>(     \
        static_cast<const nsCSSPseudoElementStaticAtom*>( \
            nsGkAtoms::PseudoElement_##name_));           \
  }
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

  static Type GetPseudoType(nsAtom* aAtom, EnabledState aEnabledState);

  // Get the atom for a given Type. aType must be <
  // PseudoType::CSSPseudoElementsEnd.
  // This only ever returns static atoms, so it's fine to return a raw pointer.
  static nsAtom* GetPseudoAtom(Type aType);

  // Get the atom for a given pseudo-element string (e.g. "::before").  This can
  // return dynamic atoms, for unrecognized pseudo-elements.
  static already_AddRefed<nsAtom> GetPseudoAtom(
      const nsAString& aPseudoElement);

  static bool PseudoElementContainsElements(const Type aType) {
    return PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS);
  }

  static bool PseudoElementSupportsStyleAttribute(const Type aType) {
    MOZ_ASSERT(aType < Type::CSSPseudoElementsEnd);
    return PseudoElementHasFlags(aType,
                                 CSS_PSEUDO_ELEMENT_SUPPORTS_STYLE_ATTRIBUTE);
  }

  static bool PseudoElementSupportsUserActionState(const Type aType);

  static bool PseudoElementIsJSCreatedNAC(Type aType) {
    return PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_IS_JS_CREATED_NAC);
  }

  static bool PseudoElementIsFlexOrGridItem(const Type aType) {
    return PseudoElementHasFlags(aType,
                                 CSS_PSEUDO_ELEMENT_IS_FLEX_OR_GRID_ITEM);
  }

  static bool IsEnabled(Type aType, EnabledState aEnabledState) {
    if (!PseudoElementHasAnyFlag(
            aType, CSS_PSEUDO_ELEMENT_ENABLED_IN_UA_SHEETS_AND_CHROME)) {
      return true;
    }

    if ((aEnabledState & EnabledState::InUASheets) &&
        PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_ENABLED_IN_UA_SHEETS)) {
      return true;
    }

    if ((aEnabledState & EnabledState::InChrome) &&
        PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_ENABLED_IN_CHROME)) {
      return true;
    }

    return false;
  }

  static nsString PseudoTypeAsString(Type aPseudoType);

 private:
  // Does the given pseudo-element have all of the flags given?
  static bool PseudoElementHasFlags(const Type aType, uint32_t aFlags) {
    MOZ_ASSERT(aType < Type::CSSPseudoElementsEnd);
    return (kPseudoElementFlags[size_t(aType)] & aFlags) == aFlags;
  }

  static bool PseudoElementHasAnyFlag(const Type aType, uint32_t aFlags) {
    MOZ_ASSERT(aType < Type::CSSPseudoElementsEnd);
    return (kPseudoElementFlags[size_t(aType)] & aFlags) != 0;
  }

  static nsStaticAtom* GetAtomBase();

  static const uint32_t kPseudoElementFlags[size_t(Type::CSSPseudoElementsEnd)];
};

#endif /* nsCSSPseudoElements_h___ */
