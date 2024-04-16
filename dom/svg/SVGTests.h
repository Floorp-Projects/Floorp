/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGTESTS_H_
#define DOM_SVG_SVGTESTS_H_

#include "nsStringFwd.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/SVGStringList.h"

class nsAttrValue;
class nsAtom;
class nsIContent;
class nsStaticAtom;

namespace mozilla {

namespace dom {
class DOMSVGStringList;
class SVGSwitchElement;
}  // namespace dom

#define MOZILLA_DOMSVGTESTS_IID                      \
  {                                                  \
    0x92370da8, 0xda28, 0x4895, {                    \
      0x9b, 0x1b, 0xe0, 0x06, 0x0d, 0xb7, 0x3f, 0xc3 \
    }                                                \
  }

namespace dom {

class SVGElement;

class SVGTests : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOMSVGTESTS_IID)

  SVGTests();

  friend class dom::DOMSVGStringList;
  using SVGStringList = mozilla::SVGStringList;

  /**
   * Find the active switch child using BCP 47 rules.
   */
  static nsIContent* FindActiveSwitchChild(
      const dom::SVGSwitchElement* aSwitch);

  /**
   * Check whether the conditional processing attributes requiredExtensions
   * and systemLanguage both "return true" if they apply to
   * and are specified on the given element. Returns true if this element
   * should be rendered, false if it should not.
   */
  bool PassesConditionalProcessingTests() const;

  /**
   * Returns true if the attribute is one of the conditional processing
   * attributes.
   */
  bool IsConditionalProcessingAttribute(const nsAtom* aAttribute) const;

  bool ParseConditionalProcessingAttribute(nsAtom* aAttribute,
                                           const nsAString& aValue,
                                           nsAttrValue& aResult);

  /**
   * Unsets a conditional processing attribute.
   */
  void UnsetAttr(const nsAtom* aAttribute);

  nsStaticAtom* GetAttrName(uint8_t aAttrEnum) const;
  void GetAttrValue(uint8_t aAttrEnum, nsAttrValue& aValue) const;

  void MaybeInvalidate();

  // WebIDL
  already_AddRefed<DOMSVGStringList> RequiredExtensions();
  already_AddRefed<DOMSVGStringList> SystemLanguage();

  bool HasExtension(const nsAString& aExtension) const;

  virtual SVGElement* AsSVGElement() = 0;

  const SVGElement* AsSVGElement() const {
    return const_cast<SVGTests*>(this)->AsSVGElement();
  }

 protected:
  virtual ~SVGTests() = default;

 private:
  /**
   * Check whether the extensions processing attribute applies to and is
   * specified on the given element. Returns true if this element should be
   * rendered, false if it should not.
   */
  bool PassesRequiredExtensionsTests() const;

  enum { EXTENSIONS, LANGUAGE };
  SVGStringList mStringListAttributes[2];
  static nsStaticAtom* const sStringListNames[2];
  mutable Maybe<bool> mPassesConditionalProcessingTests = Some(true);
};

NS_DEFINE_STATIC_IID_ACCESSOR(SVGTests, MOZILLA_DOMSVGTESTS_IID)

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGTESTS_H_
