/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface for accessing style declarations using enums instead of strings,
 * for internal use
 */

#ifndef nsICSSDeclaration_h__
#define nsICSSDeclaration_h__

/**
 * This interface provides access to methods analogous to those of
 * CSSStyleDeclaration; the difference is that these use nsCSSPropertyID
 * enums for the prop names instead of using strings.
 */

#include "mozilla/Attributes.h"
#include "nsCSSPropertyID.h"
#include "mozilla/dom/CSSValue.h"
#include "nsWrapperCache.h"
#include "nsStringFwd.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"

class nsINode;
class nsIPrincipal;
namespace mozilla {
namespace css {
class Rule;
} // namespace css
namespace dom {
class DocGroup;
} // namespace dom
} // namespace mozilla

// dbeabbfa-6cb3-4f5c-aec2-dd558d9d681f
#define NS_ICSSDECLARATION_IID \
{ 0xdbeabbfa, 0x6cb3, 0x4f5c, \
 { 0xae, 0xc2, 0xdd, 0x55, 0x8d, 0x9d, 0x68, 0x1f } }

class nsICSSDeclaration : public nsISupports
                        , public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSSDECLARATION_IID)

  virtual nsINode *GetParentObject() = 0;
  mozilla::dom::DocGroup* GetDocGroup();

  NS_IMETHOD GetPropertyValue(const nsAString& aPropName,
                              nsAString& aValue) = 0;
  NS_IMETHOD RemoveProperty(const nsAString& aPropertyName,
                            nsAString& aReturn) = 0;
  NS_IMETHOD SetProperty(const nsAString& aPropertyName,
                         const nsAString& aValue,
                         const nsAString& aPriority,
                         nsIPrincipal* aSubjectPrincipal = nullptr) = 0;
  void Item(uint32_t aIndex, nsAString& aReturn)
  {
    bool found;
    IndexedGetter(aIndex, found, aReturn);
    if (!found) {
      aReturn.Truncate();
    }
  }

  virtual void
  GetCSSImageURLs(const nsAString& aPropertyName,
                  nsTArray<nsString>& aImageURLs,
                  mozilla::ErrorResult& aRv)
  {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

  // WebIDL interface for CSSStyleDeclaration
  virtual void SetCssText(const nsAString& aString,
                          nsIPrincipal* aSubjectPrincipal,
                          mozilla::ErrorResult& rv) = 0;
  virtual void GetCssText(nsAString& aString) = 0;
  virtual uint32_t Length() = 0;

  // The actual implementation of the Item method and the WebIDL indexed getter
  virtual void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName) = 0;

  void GetPropertyValue(const nsAString& aPropName, nsString& aValue,
                        mozilla::ErrorResult& rv) {
    rv = GetPropertyValue(aPropName, aValue);
  }
  virtual void GetPropertyPriority(const nsAString& aPropName,
                                   nsAString& aPriority) = 0;
  void SetProperty(const nsAString& aPropName, const nsAString& aValue,
                   const nsAString& aPriority, nsIPrincipal* aSubjectPrincipal,
                   mozilla::ErrorResult& rv) {
    rv = SetProperty(aPropName, aValue, aPriority, aSubjectPrincipal);
  }
  void RemoveProperty(const nsAString& aPropName, nsString& aRetval,
                      mozilla::ErrorResult& rv) {
    rv = RemoveProperty(aPropName, aRetval);
  }
  virtual mozilla::css::Rule* GetParentRule() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSDeclaration, NS_ICSSDECLARATION_IID)

#define NS_DECL_NSIDOMCSSSTYLEDECLARATION_HELPER                        \
  void GetCssText(nsAString& aCssText) override;                        \
  void SetCssText(const nsAString& aCssText,                            \
                  nsIPrincipal* aSubjectPrincipal,                      \
                  mozilla::ErrorResult& aRv) override;                  \
  NS_IMETHOD GetPropertyValue(const nsAString & propertyName, nsAString & _retval) override; \
  NS_IMETHOD RemoveProperty(const nsAString & propertyName, nsAString & _retval) override; \
  void GetPropertyPriority(const nsAString & propertyName,              \
                           nsAString & aPriority) override;             \
  NS_IMETHOD SetProperty(const nsAString& propertyName,                       \
                         const nsAString& value,                              \
                         const nsAString& priority,                           \
                         nsIPrincipal* aSubjectPrincipal = nullptr) override; \
  uint32_t Length() override;                                           \
  mozilla::css::Rule* GetParentRule() override;

#endif // nsICSSDeclaration_h__
