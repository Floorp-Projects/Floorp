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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2004
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

// 57eb81d1-a607-4429-926b-802519d43aad
#define NS_ICSSDECLARATION_IID \
 { 0x57eb81d1, 0xa607, 0x4429, \
    {0x92, 0x6b, 0x80, 0x25, 0x19, 0xd4, 0x3a, 0xad } }


class nsICSSDeclaration : public nsIDOMCSSStyleDeclaration
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICSSDECLARATION_IID)

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
};

#define NS_DECL_NSICSSDECLARATION                               \
  NS_IMETHOD GetPropertyValue(const nsCSSProperty aPropID,    \
                              nsAString& aValue);               \
  NS_IMETHOD SetPropertyValue(const nsCSSProperty aPropID,    \
                              const nsAString& aValue);

  

#endif // nsICSSDeclaration_h__
