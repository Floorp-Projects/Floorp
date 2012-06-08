/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGTests.h"
#include "DOMSVGStringList.h"
#include "nsContentErrors.h" // For NS_PROPTABLE_PROP_OVERWRITTEN
#include "nsSVGFeatures.h"
#include "nsSVGSwitchElement.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsStyleUtil.h"
#include "nsSVGUtils.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(DOMSVGTests, nsIDOMSVGTests)

nsIAtom** DOMSVGTests::sStringListNames[3] =
{
  &nsGkAtoms::requiredFeatures,
  &nsGkAtoms::requiredExtensions,
  &nsGkAtoms::systemLanguage,
};

/* readonly attribute nsIDOMSVGStringList requiredFeatures; */
NS_IMETHODIMP
DOMSVGTests::GetRequiredFeatures(nsIDOMSVGStringList * *aRequiredFeatures)
{
  nsCOMPtr<nsSVGElement> element = do_QueryInterface(this);
  *aRequiredFeatures = DOMSVGStringList::GetDOMWrapper(
                         GetOrCreateStringListAttribute(FEATURES), element, true, FEATURES).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGStringList requiredExtensions; */
NS_IMETHODIMP
DOMSVGTests::GetRequiredExtensions(nsIDOMSVGStringList * *aRequiredExtensions)
{
  nsCOMPtr<nsSVGElement> element = do_QueryInterface(this);
  *aRequiredExtensions = DOMSVGStringList::GetDOMWrapper(
                           GetOrCreateStringListAttribute(EXTENSIONS), element, true, EXTENSIONS).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGStringList systemLanguage; */
NS_IMETHODIMP
DOMSVGTests::GetSystemLanguage(nsIDOMSVGStringList * *aSystemLanguage)
{
  nsCOMPtr<nsSVGElement> element = do_QueryInterface(this);
  *aSystemLanguage = DOMSVGStringList::GetDOMWrapper(
                       GetOrCreateStringListAttribute(LANGUAGE), element, true, LANGUAGE).get();
  return NS_OK;
}

/* boolean hasExtension (in DOMString extension); */
NS_IMETHODIMP
DOMSVGTests::HasExtension(const nsAString & extension, bool *_retval)
{
  *_retval = nsSVGFeatures::HasExtension(extension);
  return NS_OK;
}

bool
DOMSVGTests::IsConditionalProcessingAttribute(const nsIAtom* aAttribute) const
{
  for (PRUint32 i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == *sStringListNames[i]) {
      return true;
    }
  }
  return false;
}

PRInt32
DOMSVGTests::GetBestLanguagePreferenceRank(const nsSubstring& aAcceptLangs) const
{
  const nsDefaultStringComparator defaultComparator;

  PRInt32 lowestRank = -1;

  const SVGStringList *languageStringList = GetStringListAttribute(LANGUAGE);
  if (!languageStringList) {
    return lowestRank;
  }
  for (PRUint32 i = 0; i < languageStringList->Length(); i++) {
    nsCharSeparatedTokenizer languageTokenizer(aAcceptLangs, ',');
    PRInt32 index = 0;
    while (languageTokenizer.hasMoreTokens()) {
      const nsSubstring &languageToken = languageTokenizer.nextToken();
      bool exactMatch = (languageToken == (*languageStringList)[i]);
      bool prefixOnlyMatch =
        !exactMatch &&
        nsStyleUtil::DashMatchCompare((*languageStringList)[i],
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

const nsString * const DOMSVGTests::kIgnoreSystemLanguage = (nsString *) 0x01;

bool
DOMSVGTests::PassesConditionalProcessingTests(const nsString *aAcceptLangs) const
{
  // Required Features
  const SVGStringList *featuresStringList = GetStringListAttribute(FEATURES);
  if (featuresStringList && featuresStringList->IsExplicitlySet()) {
    if (featuresStringList->IsEmpty()) {
      return false;
    }
    nsCOMPtr<nsIContent> content(
      do_QueryInterface(const_cast<DOMSVGTests*>(this)));

    for (PRUint32 i = 0; i < featuresStringList->Length(); i++) {
      if (!nsSVGFeatures::HasFeature(content, (*featuresStringList)[i])) {
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
  const SVGStringList *extensionsStringList = GetStringListAttribute(EXTENSIONS);
  if (extensionsStringList && extensionsStringList->IsExplicitlySet()) {
    if (extensionsStringList->IsEmpty()) {
      return false;
    }
    for (PRUint32 i = 0; i < extensionsStringList->Length(); i++) {
      if (!nsSVGFeatures::HasExtension((*extensionsStringList)[i])) {
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
  const SVGStringList *languageStringList = GetStringListAttribute(LANGUAGE);
  if (languageStringList && languageStringList->IsExplicitlySet()) {
    if (languageStringList->IsEmpty()) {
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

    for (PRUint32 i = 0; i < languageStringList->Length(); i++) {
      nsCharSeparatedTokenizer languageTokenizer(acceptLangs, ',');
      while (languageTokenizer.hasMoreTokens()) {
        if (nsStyleUtil::DashMatchCompare((*languageStringList)[i],
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
DOMSVGTests::ParseConditionalProcessingAttribute(nsIAtom* aAttribute,
                                                 const nsAString& aValue,
                                                 nsAttrValue& aResult)
{
  for (PRUint32 i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == *sStringListNames[i]) {
      SVGStringList *stringList = GetOrCreateStringListAttribute(i);
      if (stringList) {
        nsresult rv = stringList->SetValue(aValue);
        if (NS_FAILED(rv)) {
          stringList->Clear();
        }
      }
      MaybeInvalidate();
      return true;
    }
  }
  return false;
}

void
DOMSVGTests::UnsetAttr(const nsIAtom* aAttribute)
{
  for (PRUint32 i = 0; i < ArrayLength(sStringListNames); i++) {
    if (aAttribute == *sStringListNames[i]) {
      SVGStringList *stringList = GetStringListAttribute(i);
      if (stringList) {
        // don't destroy the property in case there are tear-offs
        // referring to it
        stringList->Clear();
        MaybeInvalidate();
      }
      return;
    }
  }
}

// Callback function, for freeing PRUint64 values stored in property table
// when the element goes away
static void
ReleaseStringListPropertyValue(void*    aObject,       /* unused */
                               nsIAtom* aPropertyName, /* unused */
                               void*    aPropertyValue,
                               void*    aData          /* unused */)
{
  SVGStringList* valPtr =
    static_cast<SVGStringList*>(aPropertyValue);
  delete valPtr;
}

SVGStringList*
DOMSVGTests::GetStringListAttribute(PRUint8 aAttrEnum) const
{
  nsIAtom *attrName = GetAttrName(aAttrEnum);
  const nsCOMPtr<nsSVGElement> element =
    do_QueryInterface(const_cast<DOMSVGTests*>(this));

  return static_cast<SVGStringList*>(element->GetProperty(attrName));
}

SVGStringList*
DOMSVGTests::GetOrCreateStringListAttribute(PRUint8 aAttrEnum) const
{
  SVGStringList* stringListPtr = GetStringListAttribute(aAttrEnum);
  if (stringListPtr) {
    return stringListPtr;
  }
  nsIAtom *attrName = GetAttrName(aAttrEnum);
  const nsCOMPtr<nsSVGElement> element =
    do_QueryInterface(const_cast<DOMSVGTests*>(this));

  stringListPtr = new SVGStringList();
  stringListPtr->SetIsCommaSeparated(aAttrEnum == LANGUAGE);
  nsresult rv = element->SetProperty(attrName,
                                     stringListPtr,
                                     ReleaseStringListPropertyValue,
                                     true);
  NS_ABORT_IF_FALSE(rv != NS_PROPTABLE_PROP_OVERWRITTEN,
                    "Setting property value when it's already set...?"); 

  if (NS_LIKELY(NS_SUCCEEDED(rv))) {
    return stringListPtr;
  }
  // property-insertion failed (e.g. OOM in property-table code)
  delete stringListPtr;
  return nsnull;
}

nsIAtom*
DOMSVGTests::GetAttrName(PRUint8 aAttrEnum) const
{
  return *sStringListNames[aAttrEnum];
}

void
DOMSVGTests::GetAttrValue(PRUint8 aAttrEnum, nsAttrValue& aValue) const
{
  MOZ_ASSERT(aAttrEnum < ArrayLength(sStringListNames),
             "aAttrEnum out of range");
  aValue.SetTo(*GetOrCreateStringListAttribute(aAttrEnum), nsnull);
}

void
DOMSVGTests::MaybeInvalidate()
{
  nsCOMPtr<nsSVGElement> element = do_QueryInterface(this);

  nsIContent* parent = element->GetFlattenedTreeParent();

  if (parent &&
      parent->NodeInfo()->Equals(nsGkAtoms::svgSwitch, kNameSpaceID_SVG)) {
    static_cast<nsSVGSwitchElement*>(parent)->MaybeInvalidate();
  }
}
