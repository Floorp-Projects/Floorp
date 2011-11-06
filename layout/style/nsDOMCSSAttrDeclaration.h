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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ms2ger <ms2ger@gmail.com>
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
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMCSSAttributeDeclaration,
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
