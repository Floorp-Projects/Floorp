/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-elements */

#ifndef nsCSSPseudoElements_h___
#define nsCSSPseudoElements_h___

#include "nsIAtom.h"
#include "mozilla/CSSEnabledState.h"

// Is this pseudo-element a CSS2 pseudo-element that can be specified
// with the single colon syntax (in addition to the double-colon syntax,
// which can be used for all pseudo-elements)?
#define CSS_PSEUDO_ELEMENT_IS_CSS2                     (1<<0)
// Is this pseudo-element a pseudo-element that can contain other
// elements?
// (Currently pseudo-elements are either leaves of the tree (relative to
// real elements) or they contain other elements in a non-tree-like
// manner (i.e., like incorrectly-nested start and end tags).  It's
// possible that in the future there might be container pseudo-elements
// that form a properly nested tree structure.  If that happens, we
// should probably split this flag into two.)
#define CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS           (1<<1)
// Flag to add the ability to take into account style attribute set for the
// pseudo element (by default it's ignored).
#define CSS_PSEUDO_ELEMENT_SUPPORTS_STYLE_ATTRIBUTE    (1<<2)
// Flag that indicate the pseudo-element supports a user action pseudo-class
// following it, such as :active or :hover.  This would normally correspond
// to whether the pseudo-element is tree-like, but we don't support these
// pseudo-classes on ::before and ::after generated content yet.  See
// http://dev.w3.org/csswg/selectors4/#pseudo-elements.
#define CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE  (1<<3)
// Is content prevented from parsing selectors containing this pseudo-element?
#define CSS_PSEUDO_ELEMENT_UA_SHEET_ONLY               (1<<4)

namespace mozilla {

// The total count of CSSPseudoElement is less than 256,
// so use uint8_t as its underlying type.
typedef uint8_t CSSPseudoElementTypeBase;
enum class CSSPseudoElementType : CSSPseudoElementTypeBase {
  // If the actual pseudo-elements stop being first here, change
  // GetPseudoType.
#define CSS_PSEUDO_ELEMENT(_name, _value_, _flags) \
  _name,
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT
  Count,
  AnonBox = Count,
#ifdef MOZ_XUL
  XULTree,
#endif
  NotPseudo,
  MAX
};

} // namespace mozilla

// Empty class derived from nsIAtom so that function signatures can
// require an atom from this atom list.
class nsICSSPseudoElement : public nsIAtom {};

class nsCSSPseudoElements
{
  typedef mozilla::CSSPseudoElementType Type;
  typedef mozilla::CSSEnabledState EnabledState;

public:
  static void AddRefAtoms();

  static bool IsPseudoElement(nsIAtom *aAtom);

  static bool IsCSS2PseudoElement(nsIAtom *aAtom);

#define CSS_PSEUDO_ELEMENT(_name, _value, _flags) \
  static nsICSSPseudoElement* _name;
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

  static Type GetPseudoType(nsIAtom* aAtom, EnabledState aEnabledState);

  // Get the atom for a given Type.  aType must be < CSSPseudoElementType::Count
  static nsIAtom* GetPseudoAtom(Type aType);

  static bool PseudoElementContainsElements(const Type aType) {
    return PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS);
  }

  static bool PseudoElementSupportsStyleAttribute(const Type aType) {
    MOZ_ASSERT(aType < Type::Count);
    return PseudoElementHasFlags(aType,
                                 CSS_PSEUDO_ELEMENT_SUPPORTS_STYLE_ATTRIBUTE);
  }

  static bool PseudoElementSupportsUserActionState(const Type aType);

  static bool IsEnabled(Type aType, EnabledState aEnabledState)
  {
    return !PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_UA_SHEET_ONLY) ||
           (aEnabledState & EnabledState::eInUASheets);
  }

private:
  // Does the given pseudo-element have all of the flags given?
  static bool PseudoElementHasFlags(const Type aType, uint32_t aFlags)
  {
    MOZ_ASSERT(aType < Type::Count);
    return (kPseudoElementFlags[size_t(aType)] & aFlags) == aFlags;
  }

  static const uint32_t kPseudoElementFlags[Type::Count];
};

#endif /* nsCSSPseudoElements_h___ */
