/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Robert Longson <longsonr@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
         const nsString *aAcceptLangs = nsnull) const;

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
   * Serialises the conditional processing attribute.
   */
  void GetValue(PRUint8 aAttrEnum, nsAString& aValue) const;

  /**
   * Unsets a conditional processing attribute.
   */
  void UnsetAttr(const nsIAtom* aAttribute);

  void DidChangeStringList(PRUint8 aAttrEnum);

  void MaybeInvalidate();

private:

  struct StringListInfo {
    nsIAtom** mName;
    bool      mIsCommaSeparated;
  };

  enum { FEATURES, EXTENSIONS, LANGUAGE };
  SVGStringList mStringListAttributes[3];
  static StringListInfo sStringListInfo[3];
};

#endif // MOZILLA_DOMSVGTESTS_H__
