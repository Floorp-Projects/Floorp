/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for DOM objects for element.style and cssStyleRule.style */

#ifndef nsDOMCSSDeclaration_h___
#define nsDOMCSSDeclaration_h___

#include "mozilla/Attributes.h"
#include "nsICSSDeclaration.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/CSS2PropertiesBinding.h"

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

class nsDOMCSSDeclaration : public nsICSSDeclaration
{
public:
  // Only implement QueryInterface; subclasses have the responsibility
  // of implementing AddRef/Release.
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  // Declare addref and release so they can be called on us, but don't
  // implement them.  Our subclasses must handle their own
  // refcounting.
  NS_IMETHOD_(nsrefcnt) AddRef() = 0;
  NS_IMETHOD_(nsrefcnt) Release() = 0;

  NS_DECL_NSICSSDECLARATION
  using nsICSSDeclaration::GetLength;

  // Require subclasses to implement |GetParentRule|.
  //NS_DECL_NSIDOMCSSSTYLEDECLARATION
  NS_IMETHOD GetCssText(nsAString & aCssText);
  NS_IMETHOD SetCssText(const nsAString & aCssText) MOZ_OVERRIDE;
  NS_IMETHOD GetPropertyValue(const nsAString & propertyName,
                              nsAString & _retval) MOZ_OVERRIDE;
  virtual already_AddRefed<mozilla::dom::CSSValue>
    GetPropertyCSSValue(const nsAString & propertyName,
                        mozilla::ErrorResult& aRv) MOZ_OVERRIDE;
  using nsICSSDeclaration::GetPropertyCSSValue;
  NS_IMETHOD RemoveProperty(const nsAString & propertyName,
                            nsAString & _retval);
  NS_IMETHOD GetPropertyPriority(const nsAString & propertyName,
                                 nsAString & _retval) MOZ_OVERRIDE;
  NS_IMETHOD SetProperty(const nsAString & propertyName,
                         const nsAString & value, const nsAString & priority) MOZ_OVERRIDE;
  NS_IMETHOD GetLength(uint32_t *aLength) MOZ_OVERRIDE;
  NS_IMETHOD GetParentRule(nsIDOMCSSRule * *aParentRule) MOZ_OVERRIDE = 0;

  // WebIDL interface for CSS2Properties
#define CSS_PROP_DOMPROP_PREFIXED(prop_) Moz ## prop_
#define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_,          \
                 kwtable_, stylestruct_, stylestructoffset_, animtype_)      \
  void                                                                       \
  Get##method_(nsAString& aValue, mozilla::ErrorResult& rv)                  \
  {                                                                          \
    rv = GetPropertyValue(eCSSProperty_##id_, aValue);                       \
  }                                                                          \
                                                                             \
  void                                                                       \
  Set##method_(const nsAString& aValue, mozilla::ErrorResult& rv)            \
  {                                                                          \
    rv = SetPropertyValue(eCSSProperty_##id_, aValue);                       \
  }

#define CSS_PROP_LIST_EXCLUDE_INTERNAL
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_)  \
  CSS_PROP(name_, id_, method_, flags_, pref_, X, X, X, X, X)
#include "nsCSSPropList.h"

#define CSS_PROP_ALIAS(aliasname_, propid_, aliasmethod_, pref_)  \
  CSS_PROP(X, propid_, aliasmethod_, X, pref_, X, X, X, X, X)
#include "nsCSSPropAliasList.h"
#undef CSS_PROP_ALIAS

#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LIST_EXCLUDE_INTERNAL
#undef CSS_PROP
#undef CSS_PROP_DOMPROP_PREFIXED

  virtual void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName);

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap)
  {
    return mozilla::dom::CSS2PropertiesBinding::Wrap(cx, scope, this,
                                                     triedToWrap);
  }

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
  nsDOMCSSDeclaration()
  {
    SetIsDOMBinding();
  }
};

#endif // nsDOMCSSDeclaration_h___
