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
#ifndef nsCSSValue_h___
#define nsCSSValue_h___

#include "nsColor.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsCoord.h"
#include "nsCSSProperty.h"
#include "nsUnitConversion.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

class imgIRequest;
class nsIDocument;

enum nsCSSUnit {
  eCSSUnit_Null         = 0,      // (n/a) null unit, value is not specified
  eCSSUnit_Auto         = 1,      // (n/a) value is algorithmic
  eCSSUnit_Inherit      = 2,      // (n/a) value is inherited
  eCSSUnit_Initial      = 3,      // (n/a) value is default UA value
  eCSSUnit_None         = 4,      // (n/a) value is none
  eCSSUnit_Normal       = 5,      // (n/a) value is normal (algorithmic, different than auto)
  eCSSUnit_String       = 10,     // (PRUnichar*) a string value
  eCSSUnit_Attr         = 11,     // (PRUnichar*) a attr(string) value
  eCSSUnit_Counter      = 12,     // (PRUnichar*) a counter(string,[string]) value
  eCSSUnit_Counters     = 13,     // (PRUnichar*) a counters(string,string[,string]) value
  eCSSUnit_URL          = 14,     // (nsCSSValue::URL*) value
  eCSSUnit_Image        = 15,     // (nsCSSValue::Image*) value
  eCSSUnit_Integer      = 50,     // (int) simple value
  eCSSUnit_Enumerated   = 51,     // (int) value has enumerated meaning
  eCSSUnit_Color        = 80,     // (color) an RGBA value
  eCSSUnit_Percent      = 90,     // (float) 1.0 == 100%) value is percentage of something
  eCSSUnit_Number       = 91,     // (float) value is numeric (usually multiplier, different behavior that percent)

  // Length units - fixed
  // US English
  eCSSUnit_Inch         = 100,    // (float) Standard length
  eCSSUnit_Foot         = 101,    // (float) 12 inches
  eCSSUnit_Mile         = 102,    // (float) 5280 feet

  // Metric
  eCSSUnit_Millimeter   = 207,    // (float) 1/1000 meter
  eCSSUnit_Centimeter   = 208,    // (float) 1/100 meter
  eCSSUnit_Meter        = 210,    // (float) Standard length
  eCSSUnit_Kilometer    = 213,    // (float) 1000 meters

  // US Typographic
  eCSSUnit_Point        = 300,    // (float) 1/72 inch
  eCSSUnit_Pica         = 301,    // (float) 12 points == 1/6 inch

  // European Typographic
  eCSSUnit_Didot        = 400,    // (float) 15 didots == 16 points
  eCSSUnit_Cicero       = 401,    // (float) 12 didots

  // Length units - relative
  // Font relative measure
  eCSSUnit_EM           = 800,    // (float) == current font size
  eCSSUnit_EN           = 801,    // (float) .5 em
  eCSSUnit_XHeight      = 802,    // (float) distance from top of lower case x to baseline
  eCSSUnit_CapHeight    = 803,    // (float) distance from top of uppercase case H to baseline
  eCSSUnit_Char         = 804,    // (float) number of characters, used for width with monospace font

  // Screen relative measure
  eCSSUnit_Pixel        = 900,    // (float)

  // Proportional Unit (for columns in tables)
  eCSSUnit_Proportional = 950, 

  // Angular units
  eCSSUnit_Degree       = 1000,    // (float) 360 per circle
  eCSSUnit_Grad         = 1001,    // (float) 400 per circle
  eCSSUnit_Radian       = 1002,    // (float) 2pi per circle

  // Frequency units
  eCSSUnit_Hertz        = 2000,    // (float)
  eCSSUnit_Kilohertz    = 2001,    // (float)

  // Time units
  eCSSUnit_Seconds      = 3000,    // (float)
  eCSSUnit_Milliseconds = 3001     // (float)
};

class nsCSSValue {
public:
  struct URL;
  friend struct URL;

  struct Image;
  friend struct Image;
  
  // for valueless units only (null, auto, inherit, none, normal)
  nsCSSValue(nsCSSUnit aUnit = eCSSUnit_Null)
    : mUnit(aUnit)
  {
    NS_ASSERTION(aUnit <= eCSSUnit_Normal, "not a valueless unit");
    if (aUnit > eCSSUnit_Normal) {
      mUnit = eCSSUnit_Null;
    }
    mValue.mInt = 0;
  }

  nsCSSValue(PRInt32 aValue, nsCSSUnit aUnit) NS_HIDDEN;
  nsCSSValue(float aValue, nsCSSUnit aUnit) NS_HIDDEN;
  nsCSSValue(const nsAString& aValue, nsCSSUnit aUnit) NS_HIDDEN;
  nsCSSValue(nscolor aValue) NS_HIDDEN;
  nsCSSValue(URL* aValue) NS_HIDDEN;
  nsCSSValue(Image* aValue) NS_HIDDEN;
  nsCSSValue(const nsCSSValue& aCopy) NS_HIDDEN;
  ~nsCSSValue() NS_HIDDEN;

  NS_HIDDEN_(nsCSSValue&)  operator=(const nsCSSValue& aCopy);
  NS_HIDDEN_(PRBool)      operator==(const nsCSSValue& aOther) const;

  PRBool operator!=(const nsCSSValue& aOther) const
  {
    return !(*this == aOther);
  }

  nsCSSUnit GetUnit() const { return mUnit; };
  PRBool    IsLengthUnit() const
    { return PRBool((eCSSUnit_Inch <= mUnit) && (mUnit <= eCSSUnit_Proportional)); }
  PRBool    IsFixedLengthUnit() const  
    { return PRBool((eCSSUnit_Inch <= mUnit) && (mUnit <= eCSSUnit_Cicero)); }
  PRBool    IsRelativeLengthUnit() const  
    { return PRBool((eCSSUnit_EM <= mUnit) && (mUnit <= eCSSUnit_Proportional)); }
  PRBool    IsAngularUnit() const  
    { return PRBool((eCSSUnit_Degree <= mUnit) && (mUnit <= eCSSUnit_Radian)); }
  PRBool    IsFrequencyUnit() const  
    { return PRBool((eCSSUnit_Hertz <= mUnit) && (mUnit <= eCSSUnit_Kilohertz)); }
  PRBool    IsTimeUnit() const  
    { return PRBool((eCSSUnit_Seconds <= mUnit) && (mUnit <= eCSSUnit_Milliseconds)); }

  PRInt32 GetIntValue() const
  {
    NS_ASSERTION(mUnit == eCSSUnit_Integer || mUnit == eCSSUnit_Enumerated,
                 "not an int value");
    return mValue.mInt;
  }

  float GetPercentValue() const
  {
    NS_ASSERTION(mUnit == eCSSUnit_Percent, "not a percent value");
    return mValue.mFloat;
  }

  float GetFloatValue() const
  {
    NS_ASSERTION(eCSSUnit_Number <= mUnit, "not a float value");
    return mValue.mFloat;
  }

  nsAString& GetStringValue(nsAString& aBuffer) const
  {
    NS_ASSERTION(eCSSUnit_String <= mUnit && mUnit <= eCSSUnit_Counters,
                 "not a string value");
    aBuffer.Truncate();
    if (nsnull != mValue.mString) {
      aBuffer.Append(mValue.mString);
    }
    return aBuffer;
  }

  const PRUnichar* GetStringBufferValue() const
  {
    NS_ASSERTION(eCSSUnit_String <= mUnit && mUnit <= eCSSUnit_Counters,
                 "not a string value");
    return mValue.mString;
  }

  nscolor GetColorValue() const
  {
    NS_ASSERTION((mUnit == eCSSUnit_Color), "not a color value");
    return mValue.mColor;
  }

  nsIURI* GetURLValue() const
  {
    NS_ASSERTION(mUnit == eCSSUnit_URL || mUnit == eCSSUnit_Image,
                 "not a URL value");
    return mUnit == eCSSUnit_URL ?
      mValue.mURL->mURI : mValue.mImage->mURI;
  }

  const PRUnichar* GetOriginalURLValue() const
  {
    NS_ASSERTION(mUnit == eCSSUnit_URL || mUnit == eCSSUnit_Image,
                 "not a URL value");
    return mUnit == eCSSUnit_URL ?
      mValue.mURL->mString : mValue.mImage->mString;
  }

  // Not making this inline because that would force us to include
  // imgIRequest.h, which leads to REQUIRES hell, since this header is included
  // all over.
  NS_HIDDEN_(imgIRequest*) GetImageValue() const;
  
  NS_HIDDEN_(nscoord)   GetLengthTwips() const;

  NS_HIDDEN_(void)  Reset()  // sets to null
  {
    if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters) &&
        (nsnull != mValue.mString)) {
      nsCRT::free(mValue.mString);
    } else if (eCSSUnit_URL == mUnit) {
      mValue.mURL->Release();
    } else if (eCSSUnit_Image == mUnit) {
      mValue.mImage->Release();
    }
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }

  NS_HIDDEN_(void)  SetIntValue(PRInt32 aValue, nsCSSUnit aUnit);
  NS_HIDDEN_(void)  SetPercentValue(float aValue);
  NS_HIDDEN_(void)  SetFloatValue(float aValue, nsCSSUnit aUnit);
  NS_HIDDEN_(void)  SetStringValue(const nsAString& aValue, nsCSSUnit aUnit);
  NS_HIDDEN_(void)  SetColorValue(nscolor aValue);
  NS_HIDDEN_(void)  SetURLValue(nsCSSValue::URL* aURI);
  NS_HIDDEN_(void)  SetImageValue(nsCSSValue::Image* aImage);
  NS_HIDDEN_(void)  SetAutoValue();
  NS_HIDDEN_(void)  SetInheritValue();
  NS_HIDDEN_(void)  SetInitialValue();
  NS_HIDDEN_(void)  SetNoneValue();
  NS_HIDDEN_(void)  SetNormalValue();
  NS_HIDDEN_(void)  StartImageLoad(nsIDocument* aDocument) const;  // Not really const, but pretending

#ifdef DEBUG
  NS_HIDDEN_(void)
    AppendToString(nsAString& aBuffer,
                   nsCSSProperty aPropID = eCSSProperty_UNKNOWN) const;
  NS_HIDDEN_(void)
    ToString(nsAString& aBuffer,
             nsCSSProperty aPropID = eCSSProperty_UNKNOWN) const;
#endif

  MOZ_DECL_CTOR_COUNTER(nsCSSValue::URL)

  struct URL {
    // Caller must delete this object immediately if the allocation of
    // |mString| fails.
    URL(nsIURI* aURI, const PRUnichar* aString, nsIURI* aReferrer)
      : mURI(aURI),
        mString(nsCRT::strdup(aString)),
        mReferrer(aReferrer),
        mRefCnt(0)
    {
      MOZ_COUNT_CTOR(nsCSSValue::URL);
    }

    ~URL()
    {
      // null |mString| isn't valid normally, but is checked by callers
      // of the constructor
      if (mString)
        nsCRT::free(mString);
      MOZ_COUNT_DTOR(nsCSSValue::URL);
    }

    PRBool operator==(const URL& aOther)
    {
      PRBool eq;
      return nsCRT::strcmp(mString, aOther.mString) == 0 &&
             (mURI == aOther.mURI || // handles null == null
              (mURI && aOther.mURI &&
               NS_SUCCEEDED(mURI->Equals(aOther.mURI, &eq)) &&
               eq));
    }

    nsCOMPtr<nsIURI> mURI; // null == invalid URL
    PRUnichar* mString;
    nsCOMPtr<nsIURI> mReferrer;

    void AddRef() { ++mRefCnt; }
    void Release() { if (--mRefCnt == 0) delete this; }
  protected:
    nsrefcnt mRefCnt;
  };

  MOZ_DECL_CTOR_COUNTER(nsCSSValue::Image)

  struct Image : public URL {
    // Not making the constructor and destructor inline because that would
    // force us to include imgIRequest.h, which leads to REQUIRES hell, since
    // this header is included all over.
    Image(nsIURI* aURI, const PRUnichar* aString, nsIURI* aReferrer,
          nsIDocument* aDocument) NS_HIDDEN;
    ~Image() NS_HIDDEN;

    // Inherit operator== from nsCSSValue::URL

    nsCOMPtr<imgIRequest> mRequest; // null == image load blocked or somehow failed

    // Override AddRef/Release so we delete ourselves via the right pointer.
    void AddRef() { ++mRefCnt; }
    void Release() { if (--mRefCnt == 0) delete this; }
  };

protected:
  nsCSSUnit mUnit;
  union {
    PRInt32    mInt;
    float      mFloat;
    PRUnichar* mString;
    nscolor    mColor;
    URL*       mURL;
    Image*     mImage;
  }         mValue;
};

#endif /* nsCSSValue_h___ */

