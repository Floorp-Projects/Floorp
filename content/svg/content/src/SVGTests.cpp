/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTests.h"
#include "DOMSVGStringList.h"
#include "nsSVGFeatures.h"
#include "mozilla/dom/SVGSwitchElement.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsStyleUtil.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS0(SVGTests)

nsIAtom** SVGTests::sStringListNames[3] =
{
  &nsGkAtoms::requiredFeatures,
  &nsGkAtoms::requiredExtensions,
  &nsGkAtoms::systemLanguage,
};

SVGTests::SVGTests()
{
  mStringListAttributes[LANGUAGE].SetIsCommaSeparated(true);
}

already_AddRefed<DOMSVGStringList>
SVGTests::RequiredFeatures()
{
  nsCOMPtr<nsIDOMSVGElement> elem = do_QueryInterface(this);
  nsSVGElement* element = static_cast<nsSVGElement*>(elem.get());
  return DOMSVGStringList::GetDOMWrapper(
           &mStringListAttributes[FEATURES], element, true, FEATURES).get();
}

already_AddRefed<DOMSVGStringList>
SVGTests::RequiredExtensions()
{
  nsCOMPtr<nsIDOMSVGElement> elem = do_QueryInterface(this);
  nsSVGElement* element = static_cast<nsSVGElement*>(elem.get());
  return DOMSVGStringList::GetDOMWrapper(
           &mStringListAttributes[EXTENSIONS], element, true, EXTENSIONS).get();
}

already_AddRefed<DOMSVGStringList>
SVGTests::SystemLanguage()
{
  nsCOMPtr<nsIDOMSVGElement> elem = do_QueryInterface(this);
  nsSVGElement* element = static_cast<nsSVGElement*>(elem.get());
  return DOMSVGStringList::GetDOMWrapper(
           &mStringListAttributes[LANGUAGE], element, true, LANGUAGE).get();
}

bool
SVGTests::HasExtension(const nsAString& aExtension)
{
  return nsSVGFeatures::HasExtension(aExtension);
}

bool
SVGTests::IsConditionalProcessingAttribute(const nsIAtom* aAttribute) const
{
  for (uint32_t i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == *sStringListNames[i]) {
      return true;
    }
  }
  return false;
}

int32_t
SVGTests::GetBestLanguagePreferenceRank(const nsSubstring& aAcceptLangs) const
{
  const nsDefaultStringComparator defaultComparator;

  int32_t lowestRank = -1;

  for (uint32_t i = 0; i < mStringListAttributes[LANGUAGE].Length(); i++) {
    nsCharSeparatedTokenizer languageTokenizer(aAcceptLangs, ',');
    int32_t index = 0;
    while (languageTokenizer.hasMoreTokens()) {
      const nsSubstring &languageToken = languageTokenizer.nextToken();
      bool exactMatch = (languageToken == mStringListAttributes[LANGUAGE][i]);
      bool prefixOnlyMatch =
        !exactMatch &&
        nsStyleUtil::DashMatchCompare(mStringListAttributes[LANGUAGE][i],
                                      languageTokenizer.nextToken(),
                                      defaultComparator);
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

const nsString * const SVGTests::kIgnoreSystemLanguage = (nsString *) 0x01;

bool
SVGTests::PassesConditionalProcessingTests(const nsString *aAcceptLangs) const
{
  // Required Features
  if (mStringListAttributes[FEATURES].IsExplicitlySet()) {
    if (mStringListAttributes[FEATURES].IsEmpty()) {
      return false;
    }
    nsCOMPtr<nsIContent> content(
      do_QueryInterface(const_cast<SVGTests*>(this)));

    for (uint32_t i = 0; i < mStringListAttributes[FEATURES].Length(); i++) {
      if (!nsSVGFeatures::HasFeature(content, mStringListAttributes[FEATURES][i])) {
        return false;
      }
    }
  }

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
      if (!nsSVGFeatures::HasExtension(mStringListAttributes[EXTENSIONS][i])) {
        return false;
      }
    }
  }

  if (aAcceptLangs == kIgnoreSystemLanguage) {
    return true;
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
    const nsAutoString acceptLangs(aAcceptLangs ? *aAcceptLangs :
      Preferences::GetLocalizedString("intl.accept_languages"));

    if (acceptLangs.IsEmpty()) {
      NS_WARNING("no default language specified for systemLanguage conditional test");
      return false;
    }

    const nsDefaultStringComparator defaultComparator;

    for (uint32_t i = 0; i < mStringListAttributes[LANGUAGE].Length(); i++) {
      nsCharSeparatedTokenizer languageTokenizer(acceptLangs, ',');
      while (languageTokenizer.hasMoreTokens()) {
        if (nsStyleUtil::DashMatchCompare(mStringListAttributes[LANGUAGE][i],
                                          languageTokenizer.nextToken(),
                                          defaultComparator)) {
          return true;
        }
      }
    }
    return false;
  }

  return true;
}

bool
SVGTests::ParseConditionalProcessingAttribute(nsIAtom* aAttribute,
                                              const nsAString& aValue,
                                              nsAttrValue& aResult)
{
  for (uint32_t i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == *sStringListNames[i]) {
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

void
SVGTests::UnsetAttr(const nsIAtom* aAttribute)
{
  for (uint32_t i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == *sStringListNames[i]) {
      mStringListAttributes[i].Clear();
      MaybeInvalidate();
      return;
    }
  }
}

nsIAtom*
SVGTests::GetAttrName(uint8_t aAttrEnum) const
{
  return *sStringListNames[aAttrEnum];
}

void
SVGTests::GetAttrValue(uint8_t aAttrEnum, nsAttrValue& aValue) const
{
  MOZ_ASSERT(aAttrEnum < ArrayLength(sStringListNames),
             "aAttrEnum out of range");
  aValue.SetTo(mStringListAttributes[aAttrEnum], nullptr);
}

void
SVGTests::MaybeInvalidate()
{
  nsCOMPtr<nsIDOMSVGElement> elem = do_QueryInterface(this);
  nsSVGElement* element = static_cast<nsSVGElement*>(elem.get());

  nsIContent* parent = element->GetFlattenedTreeParent();

  if (parent &&
      parent->NodeInfo()->Equals(nsGkAtoms::svgSwitch, kNameSpaceID_SVG)) {
    static_cast<dom::SVGSwitchElement*>(parent)->MaybeInvalidate();
  }
}

} // namespace dom
} // namespace mozilla
