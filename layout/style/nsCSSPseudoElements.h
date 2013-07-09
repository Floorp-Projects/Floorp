/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-elements */

#ifndef nsCSSPseudoElements_h___
#define nsCSSPseudoElements_h___

#include "nsIAtom.h"

// Is this pseudo-element a CSS2 pseudo-element that can be specified
// with the single colon syntax (in addition to the double-colon syntax,
// which can be used for all pseudo-elements)?
#define CSS_PSEUDO_ELEMENT_IS_CSS2                  (1<<0)
// Is this pseudo-element a pseudo-element that can contain other
// elements?
// (Currently pseudo-elements are either leaves of the tree (relative to
// real elements) or they contain other elements in a non-tree-like
// manner (i.e., like incorrectly-nested start and end tags).  It's
// possible that in the future there might be container pseudo-elements
// that form a properly nested tree structure.  If that happens, we
// should probably split this flag into two.)
#define CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS        (1<<1)
// Flag to add the ability to take into account style attribute set for the
// pseudo element (by default it's ignored).
#define CSS_PSEUDO_ELEMENT_SUPPORTS_STYLE_ATTRIBUTE (1<<2)

// Empty class derived from nsIAtom so that function signatures can
// require an atom from this atom list.
class nsICSSPseudoElement : public nsIAtom {};

class nsCSSPseudoElements {
public:

  static void AddRefAtoms();

  static bool IsPseudoElement(nsIAtom *aAtom);

  static bool IsCSS2PseudoElement(nsIAtom *aAtom);

  static bool PseudoElementContainsElements(nsIAtom *aAtom) {
    return PseudoElementHasFlags(aAtom, CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS);
  }

#define CSS_PSEUDO_ELEMENT(_name, _value, _flags) \
  static nsICSSPseudoElement* _name;
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

  enum Type {
    // If the actual pseudo-elements stop being first here, change
    // GetPseudoType.
#define CSS_PSEUDO_ELEMENT(_name, _value_, _flags) \
    ePseudo_##_name,
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT
    ePseudo_PseudoElementCount,
    ePseudo_AnonBox = ePseudo_PseudoElementCount,
#ifdef MOZ_XUL
    ePseudo_XULTree,
#endif
    ePseudo_NotPseudoElement,
    ePseudo_MAX
  };

  static Type GetPseudoType(nsIAtom* aAtom);

  // Get the atom for a given Type.  aType must be < ePseudo_PseudoElementCount
  static nsIAtom* GetPseudoAtom(Type aType);

  static bool PseudoElementSupportsStyleAttribute(nsIAtom *aAtom) {
    return PseudoElementHasFlags(aAtom, CSS_PSEUDO_ELEMENT_SUPPORTS_STYLE_ATTRIBUTE);
  }

  static bool PseudoElementSupportsStyleAttribute(const Type aType) {
    MOZ_ASSERT(aType < ePseudo_PseudoElementCount);
    return PseudoElementHasFlags(GetPseudoAtom(aType), CSS_PSEUDO_ELEMENT_SUPPORTS_STYLE_ATTRIBUTE);
  }

private:
  static uint32_t FlagsForPseudoElement(nsIAtom *aAtom);

  // Does the given pseudo-element have all of the flags given?
  static bool PseudoElementHasFlags(nsIAtom *aAtom, uint32_t aFlags)
  {
    return (FlagsForPseudoElement(aAtom) & aFlags) == aFlags;
  }
};

#endif /* nsCSSPseudoElements_h___ */
