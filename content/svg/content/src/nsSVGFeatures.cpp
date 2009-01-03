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

#include "nsString.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsContentUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "nsCommaSeparatedTokenizer.h"
#include "nsStyleUtil.h"
#include "nsSVGUtils.h"

/**
 * Check whether we support the given feature string.
 *
 * @param aFeature one of the feature strings specified at
 *    http://www.w3.org/TR/SVG11/feature.html
 */
PRBool
NS_SVG_HaveFeature(const nsAString& aFeature)
{
  if (!NS_SVGEnabled()) {
    return PR_FALSE;
  }

#define SVG_SUPPORTED_FEATURE(str) if (aFeature.Equals(NS_LITERAL_STRING(str).get())) return PR_TRUE;
#define SVG_UNSUPPORTED_FEATURE(str)
#include "nsSVGFeaturesList.h"
#undef SVG_SUPPORTED_FEATURE
#undef SVG_UNSUPPORTED_FEATURE
  return PR_FALSE;
}

/**
 * Check whether we support the given list of feature strings.
 *
 * @param aFeatures a whitespace separated list containing one or more of the
 *   feature strings specified at http://www.w3.org/TR/SVG11/feature.html
 */
static PRBool
HaveFeatures(const nsSubstring& aFeatures)
{
  nsWhitespaceTokenizer tokenizer(aFeatures);
  while (tokenizer.hasMoreTokens()) {
    if (!NS_SVG_HaveFeature(tokenizer.nextToken())) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

/**
 * Check whether we support the given extension string.
 *
 * @param aExtension the URI of an extension. Known extensions are
 *   MathML and XHTML.
 */
static PRBool
HaveExtension(const nsAString& aExtension)
{
#define SVG_SUPPORTED_EXTENSION(str) if (aExtension.Equals(NS_LITERAL_STRING(str).get())) return PR_TRUE;
  SVG_SUPPORTED_EXTENSION("http://www.w3.org/1999/xhtml")
#ifdef MOZ_MATHML
  SVG_SUPPORTED_EXTENSION("http://www.w3.org/1998/Math/MathML")
#endif
#undef SVG_SUPPORTED_EXTENSION

  return PR_FALSE;
}

/**
 * Check whether we support the given list of extension strings.
 *
 * @param aExtension a whitespace separated list containing one or more
 *   extension strings
 */
static PRBool
HaveExtensions(const nsSubstring& aExtensions)
{
  nsWhitespaceTokenizer tokenizer(aExtensions);
  while (tokenizer.hasMoreTokens()) {
    if (!HaveExtension(tokenizer.nextToken())) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

/**
 * Compare the language name(s) in a systemLanguage attribute to the
 * user's language preferences, as defined in
 * http://www.w3.org/TR/SVG11/struct.html#SystemLanguageAttribute
 * We have a match if a language name in the users language preferences
 * exactly equals one of the language names or exactly equals a prefix of
 * one of the language names in the systemLanguage attribute.
 * XXX This algorithm is O(M*N).
 */
static PRBool
MatchesLanguagePreferences(const nsSubstring& aAttribute, const nsSubstring& aLanguagePreferences) 
{
  const nsDefaultStringComparator defaultComparator;

  nsCommaSeparatedTokenizer attributeTokenizer(aAttribute);

  while (attributeTokenizer.hasMoreTokens()) {
    const nsSubstring &attributeToken = attributeTokenizer.nextToken();
    nsCommaSeparatedTokenizer languageTokenizer(aLanguagePreferences);
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

/**
 * Check whether this element supports the specified attributes
 * (i.e. whether the SVG specification defines the attributes
 * for the specified element).
 *
 * @param aTagName the tag for the element
 * @param aAttr the conditional to test for, either
 *    ATTRS_TEST or ATTRS_EXTERNAL
 */
static PRBool
ElementSupportsAttributes(const nsIAtom *aTagName, PRUint16 aAttr)
{
#define SVG_ELEMENT(_atom, _supports) if (aTagName == nsGkAtoms::_atom) return (_supports & aAttr) != 0;
#include "nsSVGElementList.h"
#undef SVG_ELEMENT
  return PR_FALSE;
}

/**
 * Check whether the conditional processing attributes requiredFeatures,
 * requiredExtensions and systemLanguage all "return true" if they apply to
 * and are specified on the given element. Returns true if this element
 * should be rendered, false if it should not.
 *
 * @param aContent the element to test
 */
PRBool
NS_SVG_PassesConditionalProcessingTests(nsIContent *aContent)
{
  if (!aContent->IsNodeOfType(nsINode::eELEMENT)) {
    return PR_FALSE;
  }

  if (!ElementSupportsAttributes(aContent->Tag(), ATTRS_CONDITIONAL)) {
    return PR_TRUE;
  }

  // Required Features
  nsAutoString value;
  if (aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::requiredFeatures, value)) {
    if (value.IsEmpty() || !HaveFeatures(value)) {
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

  // systemLanguage
  //
  // Evaluates to "true" if one of the languages indicated by user preferences
  // exactly equals one of the languages given in the value of this parameter,
  // or if one of the languages indicated by user preferences exactly equals a
  // prefix of one of the languages given in the value of this parameter such
  // that the first tag character following the prefix is "-".
  if (aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::systemLanguage,
                        value)) {
    // Get our language preferences
    nsAutoString langPrefs(nsContentUtils::GetLocalizedStringPref("intl.accept_languages"));
    if (!langPrefs.IsEmpty()) {
      langPrefs.StripWhitespace();
      value.StripWhitespace();
      return MatchesLanguagePreferences(value, langPrefs);
    } else {
      // For now, evaluate to true.
      NS_WARNING("no default language specified for systemLanguage conditional test");
      return !value.IsEmpty();
    }
  }

  return PR_TRUE;
}
