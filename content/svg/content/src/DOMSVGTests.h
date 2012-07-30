/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMSVGTESTS_H__
#define MOZILLA_DOMSVGTESTS_H__

#include "nsIDOMSVGTests.h"
#include "nsStringFwd.h"
#include "SVGStringList.h"

class nsAttrValue;
class nsIAtom;
class nsString;

namespace mozilla {
class DOMSVGStringList;
}

class DOMSVGTests : public nsIDOMSVGTests
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGTESTS

  virtual ~DOMSVGTests() {}

  friend class mozilla::DOMSVGStringList;
  typedef mozilla::SVGStringList SVGStringList;

  /**
   * Compare the language name(s) in a systemLanguage attribute to the
   * user's language preferences, as defined in
   * http://www.w3.org/TR/SVG11/struct.html#SystemLanguageAttribute
   * We have a match if a language name in the users language preferences
   * exactly equals one of the language names or exactly equals a prefix of
   * one of the language names in the systemLanguage attribute.
   * @returns 2 * the lowest index in the aAcceptLangs that matches + 1
   * if only the prefix matches, or -1 if no indices match.
   * XXX This algorithm is O(M*N).
   */
  PRInt32 GetBestLanguagePreferenceRank(const nsSubstring& aAcceptLangs) const;

  /**
   * Special value to pass to PassesConditionalProcessingTests to ignore systemLanguage
   * attributes
   */
  static const nsString * const kIgnoreSystemLanguage;

  /**
   * Check whether the conditional processing attributes requiredFeatures,
   * requiredExtensions and systemLanguage all "return true" if they apply to
   * and are specified on the given element. Returns true if this element
   * should be rendered, false if it should not.
   *
   * @param aAcceptLangs Optional parameter to pass in the value of the
   *   intl.accept_languages preference if the caller has it cached.
   *   Alternatively, pass in kIgnoreSystemLanguage to skip the systemLanguage
   *   check if the caller is giving that special treatment.
   */
  bool PassesConditionalProcessingTests(
         const nsString *aAcceptLangs = nullptr) const;

  /**
   * Returns true if the attribute is one of the conditional processing
   * attributes.
   */
  bool IsConditionalProcessingAttribute(const nsIAtom* aAttribute) const;

  bool ParseConditionalProcessingAttribute(
         nsIAtom* aAttribute,
         const nsAString& aValue,
         nsAttrValue& aResult);

  /**
   * Unsets a conditional processing attribute.
   */
  void UnsetAttr(const nsIAtom* aAttribute);

  nsIAtom* GetAttrName(PRUint8 aAttrEnum) const;
  SVGStringList* GetStringListAttribute(PRUint8 aAttrEnum) const;
  SVGStringList* GetOrCreateStringListAttribute(PRUint8 aAttrEnum) const;
  void GetAttrValue(PRUint8 aAttrEnum, nsAttrValue &aValue) const;

  void MaybeInvalidate();

private:
  enum { FEATURES, EXTENSIONS, LANGUAGE };
  static nsIAtom** sStringListNames[3];
};

#endif // MOZILLA_DOMSVGTESTS_H__
