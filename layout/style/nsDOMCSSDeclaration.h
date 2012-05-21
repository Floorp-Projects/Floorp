/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for DOM objects for element.style and cssStyleRule.style */

#ifndef nsDOMCSSDeclaration_h___
#define nsDOMCSSDeclaration_h___

#include "nsICSSDeclaration.h"
#include "nsIDOMCSS2Properties.h"
#include "nsCOMPtr.h"

class nsCSSParser;
class nsIURI;
class nsIPrincipal;
class nsIDocument;

namespace mozilla {
namespace css {
class Declaration;
class Loader;
class Rule;
}
}

class nsDOMCSSDeclaration : public nsICSSDeclaration,
                            public nsIDOMCSS2Properties
{
public:
  // Only implement QueryInterface; subclasses have the responsibility
  // of implementing AddRef/Release.
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_NSICSSDECLARATION

  // Require subclasses to implement |GetParentRule|.
  //NS_DECL_NSIDOMCSSSTYLEDECLARATION
  NS_IMETHOD GetCssText(nsAString & aCssText);
  NS_IMETHOD SetCssText(const nsAString & aCssText);
  NS_IMETHOD GetPropertyValue(const nsAString & propertyName,
                              nsAString & _retval);
  NS_IMETHOD GetPropertyCSSValue(const nsAString & propertyName,
                                 nsIDOMCSSValue **_retval);
  NS_IMETHOD RemoveProperty(const nsAString & propertyName,
                            nsAString & _retval);
  NS_IMETHOD GetPropertyPriority(const nsAString & propertyName,
                                 nsAString & _retval);
  NS_IMETHOD SetProperty(const nsAString & propertyName,
                         const nsAString & value, const nsAString & priority);
  NS_IMETHOD GetLength(PRUint32 *aLength);
  NS_IMETHOD Item(PRUint32 index, nsAString & _retval);
  NS_IMETHOD GetParentRule(nsIDOMCSSRule * *aParentRule) = 0;

  // We implement this as a shim which forwards to GetPropertyValue
  // and SetPropertyValue; subclasses need not.
  NS_DECL_NSIDOMCSS2PROPERTIES

protected:
  // This method can return null regardless of the value of aAllocate;
  // however, a null return should only be considered a failure
  // if aAllocate is true.
  virtual mozilla::css::Declaration* GetCSSDeclaration(bool aAllocate) = 0;
  virtual nsresult SetCSSDeclaration(mozilla::css::Declaration* aDecl) = 0;
  // Document that we must call BeginUpdate/EndUpdate on around the
  // calls to SetCSSDeclaration and the style rule mutation that leads
  // to it.
  virtual nsIDocument* DocToUpdate() = 0;

  // Information neded to parse a declaration.  We need the mSheetURI
  // for error reporting, mBaseURI to resolve relative URIs,
  // mPrincipal for subresource loads, and mCSSLoader for determining
  // whether we're in quirks mode.  mBaseURI needs to be a strong
  // pointer because of xml:base possibly creating base URIs on the
  // fly.  This is why we don't use CSSParsingEnvironment as a return
  // value, to avoid multiple-refcounting of mBaseURI.
  struct CSSParsingEnvironment {
    nsIURI* mSheetURI;
    nsCOMPtr<nsIURI> mBaseURI;
    nsIPrincipal* mPrincipal;
    mozilla::css::Loader* mCSSLoader;
  };
  
  // On failure, mPrincipal should be set to null in aCSSParseEnv.
  // If mPrincipal is null, the other members may not be set to
  // anything meaningful.
  virtual void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv) = 0;

  // An implementation for GetCSSParsingEnvironment for callers wrapping
  // an css::Rule.
  static void GetCSSParsingEnvironmentForRule(mozilla::css::Rule* aRule,
                                              CSSParsingEnvironment& aCSSParseEnv);

  nsresult ParsePropertyValue(const nsCSSProperty aPropID,
                              const nsAString& aPropValue,
                              bool aIsImportant);

  // Prop-id based version of RemoveProperty.  Note that this does not
  // return the old value; it just does a straight removal.
  nsresult RemoveProperty(const nsCSSProperty aPropID);

protected:
  virtual ~nsDOMCSSDeclaration();
};

#endif // nsDOMCSSDeclaration_h___
