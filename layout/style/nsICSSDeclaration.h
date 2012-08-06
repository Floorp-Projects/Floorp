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

#include "nsIDOMCSSStyleDeclaration.h"
#include "nsCSSProperty.h"
#include "nsWrapperCache.h"

// 57eb81d1-a607-4429-926b-802519d43aad
#define NS_ICSSDECLARATION_IID \
 { 0x57eb81d1, 0xa607, 0x4429, \
    {0x92, 0x6b, 0x80, 0x25, 0x19, 0xd4, 0x3a, 0xad } }

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

  // Also have to declare the nsIDOMCSSStyleDeclaration method, so we
  // don't hide it... very sad, but it stole the good method name
  NS_IMETHOD GetPropertyValue(const nsAString& aPropName,
                              nsAString& aValue) = 0;
  
  /**
   * Method analogous to nsIDOMCSSStyleDeclaration::SetProperty.  This
   * method does NOT allow setting a priority (the priority will
   * always be set to default priority).
   */
  NS_IMETHOD SetPropertyValue(const nsCSSProperty aPropID,
                              const nsAString& aValue) = 0;

  virtual nsINode *GetParentObject() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSDeclaration, NS_ICSSDECLARATION_IID)

#define NS_DECL_NSICSSDECLARATION                               \
  NS_IMETHOD GetPropertyValue(const nsCSSProperty aPropID,    \
                              nsAString& aValue);               \
  NS_IMETHOD SetPropertyValue(const nsCSSProperty aPropID,    \
                              const nsAString& aValue);

#endif // nsICSSDeclaration_h__
