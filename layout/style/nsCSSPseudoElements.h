/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-elements */

#ifndef nsCSSPseudoElements_h___
#define nsCSSPseudoElements_h___

#include "nsAtom.h"
#include "nsStaticAtom.h"
#include "mozilla/CSSEnabledState.h"
#include "mozilla/Compiler.h"

// Is this pseudo-element a CSS2 pseudo-element that can be specified
// with the single colon syntax (in addition to the double-colon syntax,
// which can be used for all pseudo-elements)?
//
// Note: We also rely on this for IsEagerlyCascadedInServo.
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
// Should this pseudo-element be enabled only for UA sheets?
#define CSS_PSEUDO_ELEMENT_ENABLED_IN_UA_SHEETS (1<<4)
// Should this pseudo-element be enabled only for UA sheets and chrome
// stylesheets?
#define CSS_PSEUDO_ELEMENT_ENABLED_IN_CHROME (1<<5)

#define CSS_PSEUDO_ELEMENT_ENABLED_IN_UA_SHEETS_AND_CHROME \
  (CSS_PSEUDO_ELEMENT_ENABLED_IN_UA_SHEETS |               \
   CSS_PSEUDO_ELEMENT_ENABLED_IN_CHROME)

// Can we use the ChromeOnly document.createElement(..., { pseudo: "::foo" })
// API for creating pseudo-implementing native anonymous content in JS with this
// pseudo-element?
#define CSS_PSEUDO_ELEMENT_IS_JS_CREATED_NAC           (1<<6)
// Does this pseudo-element act like an item for containers (such as flex and
// grid containers) and thus needs parent display-based style fixup?
#define CSS_PSEUDO_ELEMENT_IS_FLEX_OR_GRID_ITEM        (1<<7)

// Trivial subclass of nsStaticAtom so that function signatures can require an
// atom from this atom list.
class nsICSSPseudoElement : public nsStaticAtom
{
public:
  constexpr nsICSSPseudoElement(const char16_t* aStr, uint32_t aLength,
                                uint32_t aStringOffset)
    : nsStaticAtom(aStr, aLength, aStringOffset)
  {}
};

namespace mozilla {

// The total count of CSSPseudoElement is less than 256,
// so use uint8_t as its underlying type.
typedef uint8_t CSSPseudoElementTypeBase;
enum class CSSPseudoElementType : CSSPseudoElementTypeBase {
  // If the actual pseudo-elements stop being first here, change
  // GetPseudoType.
#define CSS_PSEUDO_ELEMENT(_name, _value, _flags) \
  _name,
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT
  Count,
  InheritingAnonBox = Count, // pseudo from nsCSSAnonBoxes,
                             // IsNonInheritingAnonBox false.
  NonInheritingAnonBox, // from nsCSSAnonBoxes, IsNonInheritingAnonBox true.
#ifdef MOZ_XUL
  XULTree,
#endif
  NotPseudo,
  MAX
};

namespace detail {

struct CSSPseudoElementAtoms
{
  #define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
    NS_STATIC_ATOM_DECL_STRING(name_, value_)
  #include "nsCSSPseudoElementList.h"
  #undef CSS_PSEUDO_ELEMENT

  enum class Atoms {
    #define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
      NS_STATIC_ATOM_ENUM(name_)
    #include "nsCSSPseudoElementList.h"
    #undef CSS_PSEUDO_ELEMENT
    AtomsCount
  };

  const nsICSSPseudoElement mAtoms[static_cast<size_t>(Atoms::AtomsCount)];
};

} // namespace detail

} // namespace mozilla

class nsCSSPseudoElements
{
  typedef mozilla::CSSPseudoElementType Type;
  typedef mozilla::CSSEnabledState EnabledState;

public:
  static void RegisterStaticAtoms();

  static bool IsPseudoElement(nsAtom *aAtom);

  static bool IsCSS2PseudoElement(nsAtom *aAtom);

  // This must match EAGER_PSEUDO_COUNT in Rust code.
  static const size_t kEagerPseudoCount = 4;

  static bool IsEagerlyCascadedInServo(const Type aType)
  {
    return PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_IS_CSS2);
  }

private:
  static const nsStaticAtom* const sAtoms;
  static constexpr size_t sAtomsLen =
    static_cast<size_t>(mozilla::detail::CSSPseudoElementAtoms::Atoms::AtomsCount);

public:
  #define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
    NS_STATIC_ATOM_DECL_PTR(nsICSSPseudoElement, name_);
  #include "nsCSSPseudoElementList.h"
  #undef CSS_PSEUDO_ELEMENT

  static Type GetPseudoType(nsAtom* aAtom, EnabledState aEnabledState);

  // Get the atom for a given Type. aType must be < CSSPseudoElementType::Count.
  // This only ever returns static atoms, so it's fine to return a raw pointer.
  static nsAtom* GetPseudoAtom(Type aType);

  // Get the atom for a given pseudo-element string (e.g. "::before").  This can
  // return dynamic atoms, for unrecognized pseudo-elements.
  static already_AddRefed<nsAtom> GetPseudoAtom(const nsAString& aPseudoElement);

  static bool PseudoElementContainsElements(const Type aType) {
    return PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_CONTAINS_ELEMENTS);
  }

  static bool PseudoElementSupportsStyleAttribute(const Type aType) {
    MOZ_ASSERT(aType < Type::Count);
    return PseudoElementHasFlags(aType,
                                 CSS_PSEUDO_ELEMENT_SUPPORTS_STYLE_ATTRIBUTE);
  }

  static bool PseudoElementSupportsUserActionState(const Type aType);

  static bool PseudoElementIsJSCreatedNAC(Type aType)
  {
    return PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_IS_JS_CREATED_NAC);
  }

  static bool PseudoElementIsFlexOrGridItem(const Type aType)
  {
    return PseudoElementHasFlags(aType,
                                 CSS_PSEUDO_ELEMENT_IS_FLEX_OR_GRID_ITEM);
  }

  static bool IsEnabled(Type aType, EnabledState aEnabledState)
  {
    if (!PseudoElementHasAnyFlag(
      aType, CSS_PSEUDO_ELEMENT_ENABLED_IN_UA_SHEETS_AND_CHROME)) {
      return true;
    }

    if ((aEnabledState & EnabledState::eInUASheets) &&
        PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_ENABLED_IN_UA_SHEETS)) {
      return true;
    }

    if ((aEnabledState & EnabledState::eInChrome) &&
        PseudoElementHasFlags(aType, CSS_PSEUDO_ELEMENT_ENABLED_IN_CHROME)) {
      return true;
    }

    return false;
  }

  static nsString PseudoTypeAsString(Type aPseudoType);

private:
  // Does the given pseudo-element have all of the flags given?

  // Work around https://gcc.gnu.org/bugzilla/show_bug.cgi?id=64037 ,
  // which is a general gcc bug that we seem to have hit only on Android/x86.
#if defined(ANDROID) && defined(__i386__) && defined(__GNUC__) && \
    !defined(__clang__)
#if (MOZ_GCC_VERSION_AT_LEAST(4,8,0) && MOZ_GCC_VERSION_AT_MOST(4,8,4)) || \
    (MOZ_GCC_VERSION_AT_LEAST(4,9,0) && MOZ_GCC_VERSION_AT_MOST(4,9,2))
   __attribute__((noinline))
#endif
#endif
  static bool PseudoElementHasFlags(const Type aType, uint32_t aFlags)
  {
    MOZ_ASSERT(aType < Type::Count);
    return (kPseudoElementFlags[size_t(aType)] & aFlags) == aFlags;
  }

  static bool PseudoElementHasAnyFlag(const Type aType, uint32_t aFlags)
  {
    MOZ_ASSERT(aType < Type::Count);
    return (kPseudoElementFlags[size_t(aType)] & aFlags) != 0;
  }

  static const uint32_t kPseudoElementFlags[size_t(Type::Count)];
};

#endif /* nsCSSPseudoElements_h___ */
