/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsROCSSPrimitiveValue_h___
#define nsROCSSPrimitiveValue_h___

#include "nsIDOMCSSPrimitiveValue.h"
#include "nsString.h"
#include "nsCoord.h"
#include "nsUnitConversion.h"

#include "nsCOMPtr.h"
#include "nsDOMError.h"


class nsROCSSPrimitiveValue : public nsIDOMCSSPrimitiveValue
{
public:
  NS_DECL_ISUPPORTS

  // nsIDOMCSSPrimitiveValue
  NS_DECL_NSIDOMCSSPRIMITIVEVALUE

  // nsIDOMCSSValue
  NS_DECL_NSIDOMCSSVALUE

  // nsROCSSPrimitiveValue
  nsROCSSPrimitiveValue(nsISupports *aOwner, float aP2T);
  virtual ~nsROCSSPrimitiveValue();

  void SetTwips(nscoord aValue)
  {
    mTwips = aValue;
  }

  void SetString(const char *aString)
  {
    mString.AssignWithConversion(aString);
    mType = CSS_STRING;
  }

  void SetString(const nsAString& aString)
  {
    mString.Assign(aString);
    mType = CSS_STRING;
  }

private:
  PRUint16 mType;

  nscoord mTwips;
  nsString mString;

  nsISupports *mOwner;

  float mT2P;
};

#endif /* nsROCSSPrimitiveValue_h___ */

