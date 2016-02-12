/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTests_h
#define mozilla_dom_SVGTests_h

#include "nsStringFwd.h"
#include "SVGStringList.h"
#include "nsCOMPtr.h"

class nsAttrValue;
class nsIAtom;
class nsString;

namespace mozilla {
class DOMSVGStringList;

#define MOZILLA_DOMSVGTESTS_IID \
   { 0x92370da8, 0xda28, 0x4895, \
     {0x9b, 0x1b, 0xe0, 0x06, 0x0d, 0xb7, 0x3f, 0xc3 } }

namespace dom {

class SVGTests : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOMSVGTESTS_IID)

  SVGTests();

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
   * if only the prefix matches, -2 if there's no systemLanguage attribute,
   * or -1 if no indices match.
   * XXX This algorithm is O(M*N).
   */
  int32_t GetBestLanguagePreferenceRank(const nsSubstring& aAcceptLangs) const;

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

  nsIAtom* GetAttrName(uint8_t aAttrEnum) const;
  void GetAttrValue(uint8_t aAttrEnum, nsAttrValue &aValue) const;

  void MaybeInvalidate();

  // WebIDL
  already_AddRefed<DOMSVGStringList> RequiredFeatures();
  already_AddRefed<DOMSVGStringList> RequiredExtensions();
  already_AddRefed<DOMSVGStringList> SystemLanguage();
  bool HasExtension(const nsAString& aExtension);

protected:
  virtual ~SVGTests() {}

private:
  enum { FEATURES, EXTENSIONS, LANGUAGE };
  SVGStringList mStringListAttributes[3];
  static nsIAtom** sStringListNames[3];
};

NS_DEFINE_STATIC_IID_ACCESSOR(SVGTests, MOZILLA_DOMSVGTESTS_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGTests_h
