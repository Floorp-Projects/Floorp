/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-classes */

#include "mozilla/ArrayUtils.h"

#include "nsCSSPseudoClasses.h"
#include "nsCSSPseudoElements.h"
#include "nsStaticAtom.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Element.h"
#include "nsString.h"
#include "nsAttrValueInlines.h"
#include "nsIMozBrowserFrame.h"

using namespace mozilla;

#define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_) \
  static_assert(!((flags_) & CSS_PSEUDO_CLASS_ENABLED_IN_CHROME) || \
                ((flags_) & CSS_PSEUDO_CLASS_ENABLED_IN_UA_SHEETS), \
                "Pseudo-class '" #name_ "' is enabled in chrome, so it " \
                "should also be enabled in UA sheets");
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS

class CSSPseudoClassAtoms
{
public:
  #define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_) \
    NS_STATIC_ATOM_DECL(name_)
  #include "nsCSSPseudoClassList.h"
  #undef CSS_PSEUDO_CLASS
};

#define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_) \
  NS_STATIC_ATOM_DEFN(CSSPseudoClassAtoms, name_)
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS

#define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_) \
  NS_STATIC_ATOM_BUFFER(name_, value_)
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS

static const nsStaticAtomSetup sCSSPseudoClassAtomSetup[] = {
  #define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_) \
    NS_STATIC_ATOM_SETUP(CSSPseudoClassAtoms, name_)
  #include "nsCSSPseudoClassList.h"
  #undef CSS_PSEUDO_CLASS
};

// Flags data for each of the pseudo-classes, which must be separate
// from the previous array since there's no place for it in
// nsStaticAtomSetup.
/* static */ const uint32_t
nsCSSPseudoClasses::kPseudoClassFlags[] = {
  #define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_) \
    flags_,
  #include "nsCSSPseudoClassList.h"
  #undef CSS_PSEUDO_CLASS
};

/* static */ bool
nsCSSPseudoClasses::sPseudoClassEnabled[] = {
  // If the pseudo class has any "ENABLED_IN" flag set, it is disabled by
  // default. Note that, if a pseudo class has pref, whatever its default
  // value is, it'll later be changed in nsCSSPseudoClasses::AddRefAtoms()
  // If the pseudo class has "ENABLED_IN" flags but doesn't have a pref,
  // it is an internal pseudo class which is disabled elsewhere.
  #define IS_ENABLED_BY_DEFAULT(flags_) \
    (!((flags_) & CSS_PSEUDO_CLASS_ENABLED_MASK))
  #define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_) \
    IS_ENABLED_BY_DEFAULT(flags_),
  #include "nsCSSPseudoClassList.h"
  #undef CSS_PSEUDO_CLASS
  #undef IS_ENABLED_BY_DEFAULT
};

void nsCSSPseudoClasses::AddRefAtoms()
{
  NS_RegisterStaticAtoms(sCSSPseudoClassAtomSetup);

  #define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_)                      \
    if (pref_[0]) {                                                           \
      auto idx = static_cast<CSSPseudoElementTypeBase>(Type::name_);          \
      Preferences::AddBoolVarCache(&sPseudoClassEnabled[idx], pref_);         \
    }
  #include "nsCSSPseudoClassList.h"
  #undef CSS_PSEUDO_CLASS
}

bool
nsCSSPseudoClasses::HasStringArg(Type aType)
{
  return aType == Type::lang ||
         aType == Type::mozLocaleDir ||
         aType == Type::dir;
}

bool
nsCSSPseudoClasses::HasNthPairArg(Type aType)
{
  return aType == Type::nthChild ||
         aType == Type::nthLastChild ||
         aType == Type::nthOfType ||
         aType == Type::nthLastOfType;
}

void
nsCSSPseudoClasses::PseudoTypeToString(Type aType, nsAString& aString)
{
  MOZ_ASSERT(aType < Type::Count, "Unexpected type");
  auto idx = static_cast<CSSPseudoClassTypeBase>(aType);
  (*sCSSPseudoClassAtomSetup[idx].mAtom)->ToString(aString);
}

/* static */ CSSPseudoClassType
nsCSSPseudoClasses::GetPseudoType(nsAtom* aAtom, EnabledState aEnabledState)
{
  for (uint32_t i = 0; i < ArrayLength(sCSSPseudoClassAtomSetup); ++i) {
    if (*sCSSPseudoClassAtomSetup[i].mAtom == aAtom) {
      Type type = Type(i);
      return IsEnabled(type, aEnabledState) ? type : Type::NotPseudo;
    }
  }
  return Type::NotPseudo;
}

/* static */ bool
nsCSSPseudoClasses::IsUserActionPseudoClass(Type aType)
{
  // See http://dev.w3.org/csswg/selectors4/#useraction-pseudos
  return aType == Type::hover ||
         aType == Type::active ||
         aType == Type::focus;
}

/* static */ bool
nsCSSPseudoClasses::LangPseudoMatches(const mozilla::dom::Element* aElement,
                                      const nsAtom* aOverrideLang,
                                      bool aHasOverrideLang,
                                      const char16_t* aString,
                                      const nsIDocument* aDocument)
{
  NS_ASSERTION(aString, "null lang parameter");
  if (!aString || !*aString) {
    return false;
  }

  // We have to determine the language of the current element.  Since
  // this is currently no property and since the language is inherited
  // from the parent we have to be prepared to look at all parent
  // nodes.  The language itself is encoded in the LANG attribute.
  if (auto* language = aHasOverrideLang ? aOverrideLang : aElement->GetLang()) {
    return nsStyleUtil::DashMatchCompare(nsDependentAtomString(language),
                                         nsDependentString(aString),
                                         nsASCIICaseInsensitiveStringComparator());
  }

  if (!aDocument) {
    return false;
  }

  // Try to get the language from the HTTP header or if this
  // is missing as well from the preferences.
  // The content language can be a comma-separated list of
  // language codes.
  nsAutoString language;
  aDocument->GetContentLanguage(language);

  nsDependentString langString(aString);
  language.StripWhitespace();
  for (auto const& lang : language.Split(char16_t(','))) {
    if (nsStyleUtil::DashMatchCompare(lang,
                                      langString,
                                      nsASCIICaseInsensitiveStringComparator())) {
      return true;
    }
  }
  return false;
}

/* static */ bool
nsCSSPseudoClasses::StringPseudoMatches(const mozilla::dom::Element* aElement,
                                        CSSPseudoClassType aPseudo,
                                        const char16_t* aString,
                                        const nsIDocument* aDocument,
                                        EventStates aStateMask,
                                        bool* const aDependence)
{

  switch (aPseudo) {
    case CSSPseudoClassType::mozLocaleDir:
      {
        const bool docIsRTL =
          aDocument->GetDocumentState().HasState(NS_DOCUMENT_STATE_RTL_LOCALE);
        nsDependentString dirString(aString);

        if (dirString.EqualsLiteral("rtl")) {
          if (!docIsRTL) {
            return false;
          }
        } else if (dirString.EqualsLiteral("ltr")) {
          if (docIsRTL) {
            return false;
          }
        } else {
          // Selectors specifying other directions never match.
          return false;
        }
      }
      break;

    case CSSPseudoClassType::dir:
      {
        if (aDependence) {
          EventStates states = sPseudoClassStateDependences[
            static_cast<CSSPseudoClassTypeBase>(aPseudo)];
          if (aStateMask.HasAtLeastOneOfStates(states)) {
            *aDependence = true;
            return false;
          }
        }

        // If we only had to consider HTML, directionality would be
        // exclusively LTR or RTL.
        //
        // However, in markup languages where there is no direction attribute
        // we have to consider the possibility that neither dir(rtl) nor
        // dir(ltr) matches.
        EventStates state = aElement->StyleState();
        nsDependentString dirString(aString);

        if (dirString.EqualsLiteral("rtl")) {
          if (!state.HasState(NS_EVENT_STATE_RTL)) {
            return false;
          }
        } else if (dirString.EqualsLiteral("ltr")) {
          if (!state.HasState(NS_EVENT_STATE_LTR)) {
            return false;
          }
        } else {
          // Selectors specifying other directions never match.
          return false;
        }
      }
      break;

    case CSSPseudoClassType::lang:
      if (LangPseudoMatches(aElement, nullptr, false, aString, aDocument)) {
        break;
      }
      return false;

    default: MOZ_ASSERT_UNREACHABLE("Called StringPseudoMatches() with unknown string-like pseudo");
  }
  return true;
}

/* static */ Maybe<bool>
nsCSSPseudoClasses::MatchesElement(Type aType, const dom::Element* aElement)
{
  switch (aType) {
    case CSSPseudoClassType::mozNativeAnonymous:
      return Some(aElement->IsInNativeAnonymousSubtree());
    case CSSPseudoClassType::mozUseShadowTreeRoot:
      return Some(aElement->IsRootOfUseElementShadowTree());
    case CSSPseudoClassType::mozTableBorderNonzero: {
      if (!aElement->IsHTMLElement(nsGkAtoms::table)) {
        return Some(false);
      }
      const nsAttrValue *val = aElement->GetParsedAttr(nsGkAtoms::border);
      return Some(val && (val->Type() != nsAttrValue::eInteger ||
                          val->GetIntegerValue() != 0));
    }
    case CSSPseudoClassType::mozBrowserFrame: {
      nsIMozBrowserFrame* browserFrame =
        const_cast<Element*>(aElement)->GetAsMozBrowserFrame();
      return Some(browserFrame && browserFrame->GetReallyIsBrowser());
    }
    default:
      return Nothing();
  }
}

// The dependencies for all state dependent pseudo-classes (i.e. those declared
// using CSS_STATE_DEPENDENT_PSEUDO_CLASS, the only one of which is :dir(...)).
const EventStates
nsCSSPseudoClasses::sPseudoClassStateDependences[size_t(CSSPseudoClassType::Count) + 2] = {
#define CSS_PSEUDO_CLASS(_name, _value, _flags, _pref) \
  EventStates(),
#define CSS_STATE_DEPENDENT_PSEUDO_CLASS(_name, _value, _flags, _pref, _states) \
  _states,
#include "nsCSSPseudoClassList.h"
#undef CSS_STATE_DEPENDENT_PSEUDO_CLASS
#undef CSS_PSEUDO_CLASS
  // Add more entries for our fake values to make sure we can't
  // index out of bounds into this array no matter what.
  EventStates(),
  EventStates()
};
