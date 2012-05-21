/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object for element.style */

#ifndef nsDOMCSSAttributeDeclaration_h
#define nsDOMCSSAttributeDeclaration_h

#include "nsDOMCSSDeclaration.h"

#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace css {
class Loader;
}

namespace dom {
class Element;
}
}

class nsDOMCSSAttributeDeclaration : public nsDOMCSSDeclaration,
                                     public nsWrapperCache
{
public:
  typedef mozilla::dom::Element Element;
  nsDOMCSSAttributeDeclaration(Element* aContent, bool aIsSMILOverride);
  ~nsDOMCSSAttributeDeclaration();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS_AMBIGUOUS(nsDOMCSSAttributeDeclaration,
                                                     nsICSSDeclaration)

  // If GetCSSDeclaration returns non-null, then the decl it returns
  // is owned by our current style rule.
  virtual mozilla::css::Declaration* GetCSSDeclaration(bool aAllocate);
  virtual void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv);
  NS_IMETHOD GetParentRule(nsIDOMCSSRule **aParent);

  virtual nsINode* GetParentObject();

protected:
  virtual nsresult SetCSSDeclaration(mozilla::css::Declaration* aDecl);
  virtual nsIDocument* DocToUpdate();

  nsRefPtr<Element> mElement;

  /* If true, this indicates that this nsDOMCSSAttributeDeclaration
   * should interact with mContent's SMIL override style rule (rather
   * than the inline style rule).
   */
  const bool mIsSMILOverride;
};

#endif /* nsDOMCSSAttributeDeclaration_h */
