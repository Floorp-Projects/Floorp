/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * faster version of nsIDOMCSSStyleDeclaration using enums instead of
 * strings, for internal use
 */

#ifndef nsICSSDeclaration_h__
#define nsICSSDeclaration_h__

/**
 * This interface provides access to methods analogous to those of
 * nsIDOMCSSStyleDeclaration; the difference is that these use
 * nsCSSProperty enums for the prop names instead of using strings.
 * This is meant for use in performance-sensitive code only!  Most
 * consumers should continue to use nsIDOMCSSStyleDeclaration.
 */

#include "mozilla/Attributes.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsCSSProperty.h"
#include "CSSValue.h"
#include "nsWrapperCache.h"
#include "nsString.h"
#include "nsIDOMCSSRule.h"
#include "nsIDOMCSSValue.h"
#include "mozilla/ErrorResult.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"

// dbeabbfa-6cb3-4f5c-aec2-dd558d9d681f
#define NS_ICSSDECLARATION_IID \
{ 0xdbeabbfa, 0x6cb3, 0x4f5c, \
 { 0xae, 0xc2, 0xdd, 0x55, 0x8d, 0x9d, 0x68, 0x1f } }

class nsINode;

class nsICSSDeclaration : public nsIDOMCSSStyleDeclaration,
                          public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSSDECLARATION_IID)

  /**
   * Method analogous to nsIDOMCSSStyleDeclaration::GetPropertyValue,
   * which obeys all the same restrictions.
   */
  NS_IMETHOD GetPropertyValue(const nsCSSProperty aPropID,
                              nsAString& aValue) = 0;

  /**
   * Method analogous to nsIDOMCSSStyleDeclaration::SetProperty.  This
   * method does NOT allow setting a priority (the priority will
   * always be set to default priority).
   */
  NS_IMETHOD SetPropertyValue(const nsCSSProperty aPropID,
                              const nsAString& aValue) = 0;

  virtual nsINode *GetParentObject() = 0;

  // Also have to declare all the nsIDOMCSSStyleDeclaration methods,
  // since we want to be able to call them from the WebIDL versions.
  NS_IMETHOD GetCssText(nsAString& aCssText) MOZ_OVERRIDE = 0;
  NS_IMETHOD SetCssText(const nsAString& aCssText) MOZ_OVERRIDE = 0;
  NS_IMETHOD GetPropertyValue(const nsAString& aPropName,
                              nsAString& aValue) MOZ_OVERRIDE = 0;
  virtual already_AddRefed<mozilla::dom::CSSValue>
    GetPropertyCSSValue(const nsAString& aPropertyName,
                        mozilla::ErrorResult& aRv) = 0;
  NS_IMETHOD GetPropertyCSSValue(const nsAString& aProp, nsIDOMCSSValue** aVal) MOZ_OVERRIDE
  {
    mozilla::ErrorResult error;
    nsRefPtr<mozilla::dom::CSSValue> val = GetPropertyCSSValue(aProp, error);
    if (error.Failed()) {
      return error.ErrorCode();
    }

    nsCOMPtr<nsIDOMCSSValue> xpVal = do_QueryInterface(val);
    xpVal.forget(aVal);
    return NS_OK;
  }
  NS_IMETHOD RemoveProperty(const nsAString& aPropertyName,
                            nsAString& aReturn) MOZ_OVERRIDE = 0;
  NS_IMETHOD GetPropertyPriority(const nsAString& aPropertyName,
                                 nsAString& aReturn) MOZ_OVERRIDE = 0;
  NS_IMETHOD SetProperty(const nsAString& aPropertyName,
                         const nsAString& aValue,
                         const nsAString& aPriority) MOZ_OVERRIDE = 0;
  NS_IMETHOD GetLength(uint32_t* aLength) MOZ_OVERRIDE = 0;
  NS_IMETHOD Item(uint32_t aIndex, nsAString& aReturn) MOZ_OVERRIDE
  {
    bool found;
    IndexedGetter(aIndex, found, aReturn);
    if (!found) {
      aReturn.Truncate();
    }
    return NS_OK;
  }
  NS_IMETHOD GetParentRule(nsIDOMCSSRule * *aParentRule) MOZ_OVERRIDE = 0;

  // WebIDL interface for CSSStyleDeclaration
  void SetCssText(const nsAString& aString, mozilla::ErrorResult& rv) {
    rv = SetCssText(aString);
  }
  void GetCssText(nsString& aString) {
    // Cast to nsAString& so we end up calling our virtual
    // |GetCssText(nsAString& aCssText)| overload, which does the real work.
    GetCssText(static_cast<nsAString&>(aString));
  }
  uint32_t Length() {
    uint32_t length;
    GetLength(&length);
    return length;
  }
  void Item(uint32_t aIndex, nsString& aPropName) {
    Item(aIndex, static_cast<nsAString&>(aPropName));
  }

  // The actual implementation of the Item method and the WebIDL indexed getter
  virtual void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName) = 0;

  void GetPropertyValue(const nsAString& aPropName, nsString& aValue,
                        mozilla::ErrorResult& rv) {
    rv = GetPropertyValue(aPropName, aValue);
  }
  void GetPropertyPriority(const nsAString& aPropName, nsString& aPriority) {
    GetPropertyPriority(aPropName, static_cast<nsAString&>(aPriority));
  }
  void SetProperty(const nsAString& aPropName, const nsAString& aValue,
                   const nsAString& aPriority, mozilla::ErrorResult& rv) {
    rv = SetProperty(aPropName, aValue, aPriority);
  }
  void RemoveProperty(const nsAString& aPropName, nsString& aRetval,
                      mozilla::ErrorResult& rv) {
    rv = RemoveProperty(aPropName, aRetval);
  }
  already_AddRefed<nsIDOMCSSRule> GetParentRule() {
    nsCOMPtr<nsIDOMCSSRule> rule;
    GetParentRule(getter_AddRefs(rule));
    return rule.forget();
  }
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSDeclaration, NS_ICSSDECLARATION_IID)

#define NS_DECL_NSICSSDECLARATION                               \
  NS_IMETHOD GetPropertyValue(const nsCSSProperty aPropID,    \
                              nsAString& aValue);               \
  NS_IMETHOD SetPropertyValue(const nsCSSProperty aPropID,    \
                              const nsAString& aValue);

#define NS_DECL_NSIDOMCSSSTYLEDECLARATION_HELPER \
  NS_IMETHOD GetCssText(nsAString & aCssText); \
  NS_IMETHOD SetCssText(const nsAString & aCssText); \
  NS_IMETHOD GetPropertyValue(const nsAString & propertyName, nsAString & _retval); \
  NS_IMETHOD RemoveProperty(const nsAString & propertyName, nsAString & _retval); \
  NS_IMETHOD GetPropertyPriority(const nsAString & propertyName, nsAString & _retval); \
  NS_IMETHOD SetProperty(const nsAString & propertyName, const nsAString & value, const nsAString & priority); \
  NS_IMETHOD GetLength(uint32_t *aLength); \
  NS_IMETHOD Item(uint32_t index, nsAString & _retval); \
  NS_IMETHOD GetParentRule(nsIDOMCSSRule * *aParentRule);

#endif // nsICSSDeclaration_h__
