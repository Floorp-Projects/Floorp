/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTests.h"
#include "DOMSVGStringList.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/SVGSwitchElement.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsStyleUtil.h"
#include "mozilla/Preferences.h"

namespace mozilla::dom {

nsStaticAtom* const SVGTests::sStringListNames[2] = {
    nsGkAtoms::requiredExtensions,
    nsGkAtoms::systemLanguage,
};

SVGTests::SVGTests() {
  mStringListAttributes[LANGUAGE].SetIsCommaSeparated(true);
}

already_AddRefed<DOMSVGStringList> SVGTests::RequiredExtensions() {
  return DOMSVGStringList::GetDOMWrapper(&mStringListAttributes[EXTENSIONS],
                                         AsSVGElement(), true, EXTENSIONS);
}

already_AddRefed<DOMSVGStringList> SVGTests::SystemLanguage() {
  return DOMSVGStringList::GetDOMWrapper(&mStringListAttributes[LANGUAGE],
                                         AsSVGElement(), true, LANGUAGE);
}

bool SVGTests::HasExtension(const nsAString& aExtension) const {
#define SVG_SUPPORTED_EXTENSION(str) \
  if (aExtension.EqualsLiteral(str)) return true;
  SVG_SUPPORTED_EXTENSION("http://www.w3.org/1999/xhtml")
  nsNameSpaceManager* nameSpaceManager = nsNameSpaceManager::GetInstance();
  if (AsSVGElement()->IsInChromeDocument() ||
      !nameSpaceManager->mMathMLDisabled) {
    SVG_SUPPORTED_EXTENSION("http://www.w3.org/1998/Math/MathML")
  }
#undef SVG_SUPPORTED_EXTENSION

  return false;
}

bool SVGTests::IsConditionalProcessingAttribute(
    const nsAtom* aAttribute) const {
  for (uint32_t i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == sStringListNames[i]) {
      return true;
    }
  }
  return false;
}

int32_t SVGTests::GetBestLanguagePreferenceRank(
    const nsAString& aAcceptLangs) const {
  if (!mStringListAttributes[LANGUAGE].IsExplicitlySet()) {
    return -2;
  }

  int32_t lowestRank = -1;

  for (uint32_t i = 0; i < mStringListAttributes[LANGUAGE].Length(); i++) {
    int32_t index = 0;
    for (const nsAString& languageToken :
         nsCharSeparatedTokenizer(aAcceptLangs, ',').ToRange()) {
      bool exactMatch = languageToken.Equals(mStringListAttributes[LANGUAGE][i],
                                             nsCaseInsensitiveStringComparator);
      bool prefixOnlyMatch =
          !exactMatch && nsStyleUtil::DashMatchCompare(
                             mStringListAttributes[LANGUAGE][i], languageToken,
                             nsCaseInsensitiveStringComparator);
      if (index == 0 && exactMatch) {
        // best possible match
        return 0;
      }
      if ((exactMatch || prefixOnlyMatch) &&
          (lowestRank == -1 || 2 * index + prefixOnlyMatch < lowestRank)) {
        lowestRank = 2 * index + prefixOnlyMatch;
      }
      ++index;
    }
  }
  return lowestRank;
}

bool SVGTests::PassesConditionalProcessingTestsIgnoringSystemLanguage() const {
  // Required Extensions
  //
  // The requiredExtensions  attribute defines a list of required language
  // extensions. Language extensions are capabilities within a user agent that
  // go beyond the feature set defined in the SVG specification.
  // Each extension is identified by a URI reference.
  // For now, claim that mozilla's SVG implementation supports XHTML and MathML.
  if (mStringListAttributes[EXTENSIONS].IsExplicitlySet()) {
    if (mStringListAttributes[EXTENSIONS].IsEmpty()) {
      return false;
    }
    for (uint32_t i = 0; i < mStringListAttributes[EXTENSIONS].Length(); i++) {
      if (!HasExtension(mStringListAttributes[EXTENSIONS][i])) {
        return false;
      }
    }
  }
  return true;
}

bool SVGTests::PassesConditionalProcessingTests() const {
  if (!PassesConditionalProcessingTestsIgnoringSystemLanguage()) {
    return false;
  }

  // systemLanguage
  //
  // Evaluates to "true" if one of the languages indicated by user preferences
  // exactly equals one of the languages given in the value of this parameter,
  // or if one of the languages indicated by user preferences exactly equals a
  // prefix of one of the languages given in the value of this parameter such
  // that the first tag character following the prefix is "-".
  if (mStringListAttributes[LANGUAGE].IsExplicitlySet()) {
    if (mStringListAttributes[LANGUAGE].IsEmpty()) {
      return false;
    }

    // Get our language preferences
    nsAutoString acceptLangs;
    Preferences::GetLocalizedString("intl.accept_languages", acceptLangs);

    if (acceptLangs.IsEmpty()) {
      NS_WARNING(
          "no default language specified for systemLanguage conditional test");
      return false;
    }

    for (uint32_t i = 0; i < mStringListAttributes[LANGUAGE].Length(); i++) {
      nsCharSeparatedTokenizer languageTokenizer(acceptLangs, ',');
      while (languageTokenizer.hasMoreTokens()) {
        if (nsStyleUtil::DashMatchCompare(mStringListAttributes[LANGUAGE][i],
                                          languageTokenizer.nextToken(),
                                          nsCaseInsensitiveStringComparator)) {
          return true;
        }
      }
    }
    return false;
  }

  return true;
}

bool SVGTests::ParseConditionalProcessingAttribute(nsAtom* aAttribute,
                                                   const nsAString& aValue,
                                                   nsAttrValue& aResult) {
  for (uint32_t i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == sStringListNames[i]) {
      nsresult rv = mStringListAttributes[i].SetValue(aValue);
      if (NS_FAILED(rv)) {
        mStringListAttributes[i].Clear();
      }
      MaybeInvalidate();
      return true;
    }
  }
  return false;
}

void SVGTests::UnsetAttr(const nsAtom* aAttribute) {
  for (uint32_t i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == sStringListNames[i]) {
      mStringListAttributes[i].Clear();
      MaybeInvalidate();
      return;
    }
  }
}

nsStaticAtom* SVGTests::GetAttrName(uint8_t aAttrEnum) const {
  return sStringListNames[aAttrEnum];
}

void SVGTests::GetAttrValue(uint8_t aAttrEnum, nsAttrValue& aValue) const {
  MOZ_ASSERT(aAttrEnum < ArrayLength(sStringListNames),
             "aAttrEnum out of range");
  aValue.SetTo(mStringListAttributes[aAttrEnum], nullptr);
}

void SVGTests::MaybeInvalidate() {
  nsIContent* parent = AsSVGElement()->GetFlattenedTreeParent();

  if (auto* svgSwitch = SVGSwitchElement::FromNodeOrNull(parent)) {
    svgSwitch->MaybeInvalidate();
  }
}

}  // namespace mozilla::dom
