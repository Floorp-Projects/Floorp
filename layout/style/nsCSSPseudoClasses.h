/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-classes */

#ifndef nsCSSPseudoClasses_h___
#define nsCSSPseudoClasses_h___

#include "nsStringFwd.h"
#include "mozilla/CSSEnabledState.h"
#include "mozilla/EventStates.h"
#include "mozilla/Maybe.h"

// The following two flags along with the pref defines where this pseudo
// class can be used:
// * If none of the two flags is presented, the pref completely controls
//   the availability of this pseudo class. And in that case, if it has
//   no pref, this property is usable everywhere.
// * If any of the flags is set, this pseudo class is always enabled in
//   the specific contexts regardless of the value of the pref. If there
//   is no pref for this pseudo class at all in this case, it is an
//   internal-only pseudo class, which cannot be used anywhere else.
#define CSS_PSEUDO_CLASS_ENABLED_MASK                  (3<<0)
#define CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS          (1<<0)
#define CSS_PSEUDO_CLASS_ENABLED_IN_CHROME             (1<<1)
#define CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS_AND_CHROME \
  (CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS | CSS_PSEUDO_CLASS_ENABLED_IN_CHROME)

class nsAtom;
class nsIDocument;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom

// The total count of CSSPseudoClassType is less than 256,
// so use uint8_t as its underlying type.
typedef uint8_t CSSPseudoClassTypeBase;
enum class CSSPseudoClassType : CSSPseudoClassTypeBase
{
#define CSS_PSEUDO_CLASS(_name, _value, _flags, _pref) \
  _name,
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS
  Count,
  NotPseudo,  // This value MUST be second last! SelectorMatches depends on it.
  MAX
};

} // namespace mozilla

class nsCSSPseudoClasses
{
  typedef mozilla::CSSPseudoClassType Type;
  typedef mozilla::CSSEnabledState EnabledState;

public:
  static void RegisterStaticAtoms();

  static Type GetPseudoType(nsAtom* aAtom, EnabledState aEnabledState);
  static bool HasStringArg(Type aType);
  static bool HasNthPairArg(Type aType);
  static bool HasSelectorListArg(Type aType) {
    return aType == Type::any;
  }
  static bool IsUserActionPseudoClass(Type aType);

  // Should only be used on types other than Count and NotPseudoClass
  static void PseudoTypeToString(Type aType, nsAString& aString);

  static bool IsEnabled(Type aType, EnabledState aEnabledState)
  {
    auto index = static_cast<size_t>(aType);
    MOZ_ASSERT(index < static_cast<size_t>(Type::Count));
    if (sPseudoClassEnabled[index] ||
        aEnabledState == EnabledState::eIgnoreEnabledState) {
      return true;
    }
    auto flags = kPseudoClassFlags[index];
    if (((aEnabledState & EnabledState::eInChrome) &&
         (flags & CSS_PSEUDO_CLASS_ENABLED_IN_CHROME)) ||
        ((aEnabledState & EnabledState::eInUASheets) &&
         (flags & CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS))) {
      return true;
    }
    return false;
  }

  // Checks whether the given pseudo class matches the element.
  // It returns Some(result) if this function is able to check
  // the pseudo-class, Nothing() otherwise.
  static mozilla::Maybe<bool>
    MatchesElement(Type aType, const mozilla::dom::Element* aElement);

  /**
   * Checks if a function-like ident-containing pseudo (:pseudo(ident))
   * matches a given element.
   *
   * Returns true if it parses and matches, Some(false) if it
   * parses but does not match. Asserts if it fails to parse; only
   * call this when you're sure it's a string-like pseudo.
   *
   * In Servo mode, please ensure that UpdatePossiblyStaleDocumentState()
   * has been called first.
   *
   * @param aElement The element we are trying to match
   * @param aPseudo The name of the pseudoselector
   * @param aString The identifier inside the pseudoselector (cannot be null)
   * @param aDocument The document
   * @param aStateMask Mask containing states which we should exclude.
   *                   Ignored if aDependence is null
   * @param aDependence Pointer to be set to true if we ignored a state due to
   *                    aStateMask. Can be null.
   */
  static bool StringPseudoMatches(const mozilla::dom::Element* aElement,
                                  mozilla::CSSPseudoClassType aPseudo,
                                  const char16_t* aString,
                                  const nsIDocument* aDocument,
                                  mozilla::EventStates aStateMask,
                                  bool* const aDependence = nullptr);

  static bool LangPseudoMatches(const mozilla::dom::Element* aElement,
                                const nsAtom* aOverrideLang,
                                bool aHasOverrideLang,
                                const char16_t* aString,
                                const nsIDocument* aDocument);

  static const mozilla::EventStates sPseudoClassStateDependences[size_t(Type::Count) + 2];

private:
  static const uint32_t kPseudoClassFlags[size_t(Type::Count)];
  static bool sPseudoClassEnabled[size_t(Type::Count)];
};

#endif /* nsCSSPseudoClasses_h___ */
