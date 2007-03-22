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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

/* DOM object representing values in DOM computed style */

#ifndef nsROCSSPrimitiveValue_h___
#define nsROCSSPrimitiveValue_h___

#include "nsIDOMCSSPrimitiveValue.h"
#include "nsString.h"
#include "nsCoord.h"
#include "nsUnitConversion.h"
#include "nsReadableUtils.h"
#include "nsIURI.h"
#include "nsIAtom.h"

#include "nsCOMPtr.h"
#include "nsDOMError.h"
#include "nsDOMCSSRect.h"
#include "nsDOMCSSRGBColor.h"

class nsROCSSPrimitiveValue : public nsIDOMCSSPrimitiveValue
{
public:
  NS_DECL_ISUPPORTS

  // nsIDOMCSSPrimitiveValue
  NS_DECL_NSIDOMCSSPRIMITIVEVALUE

  // nsIDOMCSSValue
  NS_DECL_NSIDOMCSSVALUE

  // nsROCSSPrimitiveValue
  nsROCSSPrimitiveValue(PRInt32 aAppUnitsPerInch);
  virtual ~nsROCSSPrimitiveValue();

  void SetNumber(float aValue)
  {
    Reset();
    mValue.mFloat = aValue;
    mType = CSS_NUMBER;
  }

  void SetNumber(PRInt32 aValue)
  {
    Reset();
    mValue.mFloat = float(aValue);
    mType = CSS_NUMBER;
  }

  void SetNumber(PRUint32 aValue)
  {
    Reset();
    mValue.mFloat = float(aValue);
    mType = CSS_NUMBER;
  }

  void SetPercent(float aValue)
  {
    Reset();
    mValue.mFloat = aValue;
    mType = CSS_PERCENTAGE;
  }

  void SetAppUnits(nscoord aValue)
  {
    Reset();
    mValue.mAppUnits = aValue;
    mType = CSS_PX;
  }

  void SetAppUnits(float aValue)
  {
    SetAppUnits(NSToCoordRound(aValue));
  }

  void SetIdent(nsIAtom* aAtom)
  {
    NS_PRECONDITION(aAtom, "Don't pass in a null atom");
    Reset();
    NS_ADDREF(mValue.mAtom = aAtom);
    mType = CSS_IDENT;
  }

  void SetIdent(const nsACString& aString)
  {
    Reset();
    mValue.mAtom = NS_NewAtom(aString);
    if (mValue.mAtom) {
      mType = CSS_IDENT;
    } else {
      // XXXcaa We should probably let the caller know we are out of memory
      mType = CSS_UNKNOWN;
    }
  }

  void SetString(const nsACString& aString)
  {
    Reset();
    mValue.mString = ToNewUnicode(aString);
    if (mValue.mString) {
      mType = CSS_STRING;
    } else {
      // XXXcaa We should probably let the caller know we are out of memory
      mType = CSS_UNKNOWN;
    }
  }

  void SetString(const nsAString& aString)
  {
    Reset();
    mValue.mString = ToNewUnicode(aString);
    if (mValue.mString) {
      mType = CSS_STRING;
    } else {
      // XXXcaa We should probably let the caller know we are out of memory
      mType = CSS_UNKNOWN;
    }
  }

  void SetURI(nsIURI *aURI)
  {
    Reset();
    mValue.mURI = aURI;
    NS_IF_ADDREF(mValue.mURI);
    mType = CSS_URI;
  }

  void SetColor(nsDOMCSSRGBColor* aColor)
  {
    NS_PRECONDITION(aColor, "Null RGBColor being set!");
    Reset();
    mValue.mColor = aColor;
    if (mValue.mColor) {
      NS_ADDREF(mValue.mColor);
      mType = CSS_RGBCOLOR;
    }
    else {
      mType = CSS_UNKNOWN;
    }
  }

  void SetRect(nsIDOMRect* aRect)
  {
    NS_PRECONDITION(aRect, "Null rect being set!");
    Reset();
    mValue.mRect = aRect;
    if (mValue.mRect) {
      NS_ADDREF(mValue.mRect);
      mType = CSS_RECT;
    }
    else {
      mType = CSS_UNKNOWN;
    }
  }

  void Reset(void)
  {
    switch (mType) {
      case CSS_IDENT:
        NS_ASSERTION(mValue.mAtom, "Null atom should never happen");
        NS_RELEASE(mValue.mAtom);
        break;
      case CSS_STRING:
        NS_ASSERTION(mValue.mString, "Null string should never happen");
        nsMemory::Free(mValue.mString);
        mValue.mString = nsnull;
        break;
      case CSS_URI:
        NS_IF_RELEASE(mValue.mURI);
        break;
      case CSS_RECT:
        NS_ASSERTION(mValue.mRect, "Null Rect should never happen");
        NS_RELEASE(mValue.mRect);
        break;
      case CSS_RGBCOLOR:
        NS_ASSERTION(mValue.mColor, "Null RGBColor should never happen");
        NS_RELEASE(mValue.mColor);
        break;
    }
  }

private:
  void GetEscapedURI(nsIURI *aURI, PRUnichar **aReturn);

  PRUint16 mType;

  union {
    nscoord         mAppUnits;
    float           mFloat;
    nsDOMCSSRGBColor* mColor;
    nsIDOMRect*     mRect;
    PRUnichar*      mString;
    nsIURI*         mURI;
    nsIAtom*        mAtom;
  } mValue;
  
  PRInt32 mAppUnitsPerInch;
};

#endif /* nsROCSSPrimitiveValue_h___ */

