/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Scooter Morris <scootermorris@comcast.net>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net>
 *   Frederic Wang <fred.wang@free.fr> - requiredExtensions
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * This file contains code to help implement the Conditional Processing
 * section of the SVG specification (i.e. the <switch> element and the
 * requiredFeatures, requiredExtensions and systemLanguage attributes).
 *
 *   http://www.w3.org/TR/SVG11/struct.html#ConditionalProcessing
 */

#include "nsSVGFeatures.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsContentUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsStyleUtil.h"
#include "nsSVGUtils.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

/*static*/ bool
nsSVGFeatures::HaveFeature(nsISupports* aObject, const nsAString& aFeature)
{
  if (aFeature.EqualsLiteral("http://www.w3.org/TR/SVG11/feature#Script")) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aObject));
    if (content) {
      nsIDocument *doc = content->GetCurrentDoc();
      if (doc && doc->IsResourceDoc()) {
        // no scripting in SVG images or external resource documents
        return PR_FALSE;
      }
    }
    return Preferences::GetBool("javascript.enabled", false);
  }
#define SVG_SUPPORTED_FEATURE(str) if (aFeature.EqualsLiteral(str)) return PR_TRUE;
#define SVG_UNSUPPORTED_FEATURE(str)
#include "nsSVGFeaturesList.h"
#undef SVG_SUPPORTED_FEATURE
#undef SVG_UNSUPPORTED_FEATURE
  return PR_FALSE;
}

/*static*/ bool
nsSVGFeatures::HaveFeatures(nsISupports* aObject, const nsSubstring& aFeatures)
{
  nsWhitespaceTokenizer tokenizer(aFeatures);
  while (tokenizer.hasMoreTokens()) {
    if (!HaveFeature(aObject, tokenizer.nextToken())) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

/*static*/ bool
nsSVGFeatures::HaveExtension(const nsAString& aExtension)
{
#define SVG_SUPPORTED_EXTENSION(str) if (aExtension.EqualsLiteral(str)) return PR_TRUE;
  SVG_SUPPORTED_EXTENSION("http://www.w3.org/1999/xhtml")
  SVG_SUPPORTED_EXTENSION("http://www.w3.org/1998/Math/MathML")
#undef SVG_SUPPORTED_EXTENSION

  return PR_FALSE;
}

/*static*/ bool
nsSVGFeatures::HaveExtensions(const nsSubstring& aExtensions)
{
  nsWhitespaceTokenizer tokenizer(aExtensions);
  while (tokenizer.hasMoreTokens()) {
    if (!HaveExtension(tokenizer.nextToken())) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

/*static*/ bool
nsSVGFeatures::MatchesLanguagePreferences(const nsSubstring& aAttribute,
                                          const nsSubstring& aAcceptLangs) 
{
  const nsDefaultStringComparator defaultComparator;

  nsCharSeparatedTokenizerTemplate<IsSVGWhitespace>
    attributeTokenizer(aAttribute, ',');

  while (attributeTokenizer.hasMoreTokens()) {
    const nsSubstring &attributeToken = attributeTokenizer.nextToken();
    nsCharSeparatedTokenizer languageTokenizer(aAcceptLangs, ',');
    while (languageTokenizer.hasMoreTokens()) {
      if (nsStyleUtil::DashMatchCompare(attributeToken,
                                        languageTokenizer.nextToken(),
                                        defaultComparator)) {
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

/*static*/ PRInt32
nsSVGFeatures::GetBestLanguagePreferenceRank(const nsSubstring& aAttribute,
                                             const nsSubstring& aAcceptLangs) 
{
  const nsDefaultStringComparator defaultComparator;

  nsCharSeparatedTokenizer attributeTokenizer(aAttribute, ',');
  PRInt32 lowestRank = -1;

  while (attributeTokenizer.hasMoreTokens()) {
    const nsSubstring &attributeToken = attributeTokenizer.nextToken();
    nsCharSeparatedTokenizer languageTokenizer(aAcceptLangs, ',');
    PRInt32 index = 0;
    while (languageTokenizer.hasMoreTokens()) {
      const nsSubstring &languageToken = languageTokenizer.nextToken();
      bool exactMatch = (languageToken == attributeToken);
      bool prefixOnlyMatch =
        !exactMatch &&
        nsStyleUtil::DashMatchCompare(attributeToken,
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

/*static*/ bool
nsSVGFeatures::ElementSupportsAttributes(const nsIAtom *aTagName, PRUint16 aAttr)
{
#define SVG_ELEMENT(_atom, _supports) if (aTagName == nsGkAtoms::_atom) return (_supports & aAttr) != 0;
#include "nsSVGElementList.h"
#undef SVG_ELEMENT
  return PR_FALSE;
}

const nsString * const nsSVGFeatures::kIgnoreSystemLanguage = (nsString *) 0x01;

/*static*/ bool
nsSVGFeatures::PassesConditionalProcessingTests(nsIContent *aContent,
                                                const nsString *aAcceptLangs)
{
  if (!aContent->IsElement()) {
    return PR_FALSE;
  }

  if (!ElementSupportsAttributes(aContent->Tag(), ATTRS_CONDITIONAL)) {
    return PR_TRUE;
  }

  // Required Features
  nsAutoString value;
  if (aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::requiredFeatures, value)) {
    if (value.IsEmpty() || !HaveFeatures(aContent, value)) {
      return PR_FALSE;
    }
  }

  // Required Extensions
  //
  // The requiredExtensions  attribute defines a list of required language
  // extensions. Language extensions are capabilities within a user agent that
  // go beyond the feature set defined in the SVG specification.
  // Each extension is identified by a URI reference.
  // For now, claim that mozilla's SVG implementation supports XHTML and MathML.
  if (aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::requiredExtensions, value)) {
    if (value.IsEmpty() || !HaveExtensions(value)) {
      return PR_FALSE;
    }
  }

  if (aAcceptLangs == kIgnoreSystemLanguage) {
    return PR_TRUE;
  }

  // systemLanguage
  //
  // Evaluates to "true" if one of the languages indicated by user preferences
  // exactly equals one of the languages given in the value of this parameter,
  // or if one of the languages indicated by user preferences exactly equals a
  // prefix of one of the languages given in the value of this parameter such
  // that the first tag character following the prefix is "-".
  if (aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::systemLanguage,
                        value)) {

    const nsAutoString acceptLangs(aAcceptLangs ? *aAcceptLangs :
      Preferences::GetLocalizedString("intl.accept_languages"));

    // Get our language preferences
    if (!acceptLangs.IsEmpty()) {
      return MatchesLanguagePreferences(value, acceptLangs);
    } else {
      // For now, evaluate to true.
      NS_WARNING("no default language specified for systemLanguage conditional test");
      return !value.IsEmpty();
    }
  }

  return PR_TRUE;
}
