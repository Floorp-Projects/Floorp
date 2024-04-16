/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTests.h"

#include "DOMSVGStringList.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/SVGSwitchElement.h"
#include "mozilla/intl/oxilangtag_ffi_generated.h"
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

// Find the best match from aAvailLangs for the users accept-languages,
// returning the index in the aAvailLangs list, or -1 if no match.
int32_t FindBestLanguage(const nsTArray<nsCString>& aAvailLangs) {
  AutoTArray<nsCString, 16> reqLangs;
  nsCString acceptLangs;
  Preferences::GetLocalizedCString("intl.accept_languages", acceptLangs);
  nsCCharSeparatedTokenizer languageTokenizer(acceptLangs, ',');
  while (languageTokenizer.hasMoreTokens()) {
    reqLangs.AppendElement(languageTokenizer.nextToken());
  }

  for (const auto& req : reqLangs) {
    for (const auto& avail : aAvailLangs) {
      if (avail.Length() > req.Length()) {
        // Ensure that en does not match en-us, i.e. you need to have en in
        // intl.accept_languages to match en in markup.
        continue;
      }
      using namespace intl::ffi;
      struct LangTagDelete {
        void operator()(LangTag* aLangTag) const { lang_tag_destroy(aLangTag); }
      };
      UniquePtr<LangTag, LangTagDelete> langTag(lang_tag_new(&avail));
      if (langTag && lang_tag_matches(langTag.get(), &req)) {
        return &avail - &aAvailLangs[0];
      }
    }
  }
  return -1;
}

nsIContent* SVGTests::FindActiveSwitchChild(
    const dom::SVGSwitchElement* aSwitch) {
  AutoTArray<nsCString, 16> availLocales;
  AutoTArray<nsIContent*, 16> children;
  nsIContent* defaultChild = nullptr;
  for (auto* child = aSwitch->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (!child->IsElement()) {
      continue;
    }
    nsCOMPtr<SVGTests> tests(do_QueryInterface(child));
    if (tests) {
      if (!tests->mPassesConditionalProcessingTests.valueOr(true) ||
          !tests->PassesRequiredExtensionsTests()) {
        continue;
      }
      const auto& languages = tests->mStringListAttributes[LANGUAGE];
      if (!languages.IsExplicitlySet()) {
        if (!defaultChild) {
          defaultChild = child;
        }
        continue;
      }
      for (uint32_t i = 0; i < languages.Length(); i++) {
        children.AppendElement(child);
        availLocales.AppendElement(NS_ConvertUTF16toUTF8(languages[i]));
      }
    }
  }

  // For each entry in availLocales, we expect to have a corresponding entry
  // in children that provides the child node associated with that locale.
  MOZ_ASSERT(children.Length() == availLocales.Length());

  if (availLocales.IsEmpty()) {
    return defaultChild;
  }

  int32_t index = FindBestLanguage(availLocales);
  if (index >= 0) {
    return children[index];
  }

  return defaultChild;
}

bool SVGTests::PassesRequiredExtensionsTests() const {
  // Required Extensions
  //
  // The requiredExtensions  attribute defines a list of required language
  // extensions. Language extensions are capabilities within a user agent that
  // go beyond the feature set defined in the SVG specification.
  // Each extension is identified by a URI reference.
  // For now, claim that mozilla's SVG implementation supports XHTML and MathML.
  const auto& extensions = mStringListAttributes[EXTENSIONS];
  if (extensions.IsExplicitlySet()) {
    if (extensions.IsEmpty()) {
      mPassesConditionalProcessingTests = Some(false);
      return false;
    }
    for (uint32_t i = 0; i < extensions.Length(); i++) {
      if (!HasExtension(extensions[i])) {
        mPassesConditionalProcessingTests = Some(false);
        return false;
      }
    }
  }
  return true;
}

bool SVGTests::PassesConditionalProcessingTests() const {
  if (mPassesConditionalProcessingTests) {
    return mPassesConditionalProcessingTests.value();
  }
  if (!PassesRequiredExtensionsTests()) {
    return false;
  }

  // systemLanguage
  //
  // Evaluates to true if there's a BCP 47 match for the one of the user
  // preference languages with one of the languages given in the value of
  // this parameter.
  const auto& languages = mStringListAttributes[LANGUAGE];
  if (languages.IsExplicitlySet()) {
    if (languages.IsEmpty()) {
      mPassesConditionalProcessingTests = Some(false);
      return false;
    }

    AutoTArray<nsCString, 4> availLocales;
    for (uint32_t i = 0; i < languages.Length(); i++) {
      availLocales.AppendElement(NS_ConvertUTF16toUTF8(languages[i]));
    }

    mPassesConditionalProcessingTests =
        Some(FindBestLanguage(availLocales) >= 0);
    return mPassesConditionalProcessingTests.value();
  }

  mPassesConditionalProcessingTests = Some(true);
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
      mPassesConditionalProcessingTests = Nothing();
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
      mPassesConditionalProcessingTests = Nothing();
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
