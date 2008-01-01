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

/* representation of simple property values within CSS declarations */

#ifndef nsCSSValue_h___
#define nsCSSValue_h___

#include "nsColor.h"
#include "nsString.h"
#include "nsCoord.h"
#include "nsCSSProperty.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCRTGlue.h"
#include "nsStringBuffer.h"

class imgIRequest;
class nsIDocument;
class nsIPrincipal;

enum nsCSSUnit {
  eCSSUnit_Null         = 0,      // (n/a) null unit, value is not specified
  eCSSUnit_Auto         = 1,      // (n/a) value is algorithmic
  eCSSUnit_Inherit      = 2,      // (n/a) value is inherited
  eCSSUnit_Initial      = 3,      // (n/a) value is default UA value
  eCSSUnit_None         = 4,      // (n/a) value is none
  eCSSUnit_Normal       = 5,      // (n/a) value is normal (algorithmic, different than auto)
  eCSSUnit_System_Font  = 6,      // (n/a) value is -moz-use-system-font
  eCSSUnit_Dummy        = 7,      // (n/a) a fake but specified value, used
                                  //       only in temporary values
  eCSSUnit_String       = 10,     // (PRUnichar*) a string value
  eCSSUnit_Attr         = 11,     // (PRUnichar*) a attr(string) value
  eCSSUnit_Array        = 20,     // (nsCSSValue::Array*) a list of values
  eCSSUnit_Counter      = 21,     // (nsCSSValue::Array*) a counter(string,[string]) value
  eCSSUnit_Counters     = 22,     // (nsCSSValue::Array*) a counters(string,string[,string]) value
  eCSSUnit_URL          = 30,     // (nsCSSValue::URL*) value
  eCSSUnit_Image        = 31,     // (nsCSSValue::Image*) value
  eCSSUnit_Integer      = 50,     // (int) simple value
  eCSSUnit_Enumerated   = 51,     // (int) value has enumerated meaning
  eCSSUnit_Color        = 80,     // (color) an RGBA value
  eCSSUnit_Percent      = 90,     // (float) 1.0 == 100%) value is percentage of something
  eCSSUnit_Number       = 91,     // (float) value is numeric (usually multiplier, different behavior that percent)

  // Length units - fixed
  // US English
  eCSSUnit_Inch         = 100,    // (float) 0.0254 meters
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
  eCSSUnit_Pixel        = 900,    // (float) CSS pixel unit

  // Angular units
  eCSSUnit_Degree       = 1000,    // (float) 360 per circle
  eCSSUnit_Grad         = 1001,    // (float) 400 per circle
  eCSSUnit_Radian       = 1002,    // (float) 2*pi per circle

  // Frequency units
  eCSSUnit_Hertz        = 2000,    // (float) 1/seconds
  eCSSUnit_Kilohertz    = 2001,    // (float) 1000 Hertz

  // Time units
  eCSSUnit_Seconds      = 3000,    // (float) Standard time
  eCSSUnit_Milliseconds = 3001     // (float) 1/1000 second
};

class nsCSSValue {
public:
  struct Array;
  friend struct Array;

  struct URL;
  friend struct URL;

  struct Image;
  friend struct Image;
  
  // for valueless units only (null, auto, inherit, none, normal)
  explicit nsCSSValue(nsCSSUnit aUnit = eCSSUnit_Null)
    : mUnit(aUnit)
  {
    NS_ASSERTION(aUnit <= eCSSUnit_System_Font, "not a valueless unit");
  }

  nsCSSValue(PRInt32 aValue, nsCSSUnit aUnit) NS_HIDDEN;
  nsCSSValue(float aValue, nsCSSUnit aUnit) NS_HIDDEN;
  nsCSSValue(const nsString& aValue, nsCSSUnit aUnit) NS_HIDDEN;
  explicit nsCSSValue(nscolor aValue) NS_HIDDEN;
  nsCSSValue(Array* aArray, nsCSSUnit aUnit) NS_HIDDEN;
  explicit nsCSSValue(URL* aValue) NS_HIDDEN;
  explicit nsCSSValue(Image* aValue) NS_HIDDEN;
  nsCSSValue(const nsCSSValue& aCopy) NS_HIDDEN;
  ~nsCSSValue() { Reset(); }

  NS_HIDDEN_(nsCSSValue&)  operator=(const nsCSSValue& aCopy);
  NS_HIDDEN_(PRBool)      operator==(const nsCSSValue& aOther) const;

  PRBool operator!=(const nsCSSValue& aOther) const
  {
    return !(*this == aOther);
  }

  nsCSSUnit GetUnit() const { return mUnit; }
  PRBool    IsLengthUnit() const
    { return PRBool((eCSSUnit_Inch <= mUnit) && (mUnit <= eCSSUnit_Pixel)); }
  PRBool    IsFixedLengthUnit() const  
    { return PRBool((eCSSUnit_Inch <= mUnit) && (mUnit <= eCSSUnit_Cicero)); }
  PRBool    IsRelativeLengthUnit() const  
    { return PRBool((eCSSUnit_EM <= mUnit) && (mUnit <= eCSSUnit_Pixel)); }
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
    NS_ASSERTION(eCSSUnit_String <= mUnit && mUnit <= eCSSUnit_Attr,
                 "not a string value");
    aBuffer.Truncate();
    PRUint32 len = NS_strlen(GetBufferValue(mValue.mString));
    mValue.mString->ToString(len, aBuffer);
    return aBuffer;
  }

  const PRUnichar* GetStringBufferValue() const
  {
    NS_ASSERTION(eCSSUnit_String <= mUnit && mUnit <= eCSSUnit_Attr,
                 "not a string value");
    return GetBufferValue(mValue.mString);
  }

  nscolor GetColorValue() const
  {
    NS_ASSERTION((mUnit == eCSSUnit_Color), "not a color value");
    return mValue.mColor;
  }

  Array* GetArrayValue() const
  {
    NS_ASSERTION(eCSSUnit_Array <= mUnit && mUnit <= eCSSUnit_Counters,
                 "not an array value");
    return mValue.mArray;
  }

  nsIURI* GetURLValue() const
  {
    NS_ASSERTION(mUnit == eCSSUnit_URL || mUnit == eCSSUnit_Image,
                 "not a URL value");
    return mUnit == eCSSUnit_URL ?
      mValue.mURL->mURI : mValue.mImage->mURI;
  }

  URL* GetURLStructValue() const
  {
    // Not allowing this for Image values, because if the caller takes
    // a ref to them they won't be able to delete them properly.
    NS_ASSERTION(mUnit == eCSSUnit_URL, "not a URL value");
    return mValue.mURL;
  }

  const PRUnichar* GetOriginalURLValue() const
  {
    NS_ASSERTION(mUnit == eCSSUnit_URL || mUnit == eCSSUnit_Image,
                 "not a URL value");
    return GetBufferValue(mUnit == eCSSUnit_URL ?
                            mValue.mURL->mString :
                            mValue.mImage->mString);
  }

  // Not making this inline because that would force us to include
  // imgIRequest.h, which leads to REQUIRES hell, since this header is included
  // all over.
  NS_HIDDEN_(imgIRequest*) GetImageValue() const;

  NS_HIDDEN_(nscoord)   GetLengthTwips() const;

  NS_HIDDEN_(void)  Reset()  // sets to null
  {
    if (mUnit != eCSSUnit_Null)
      DoReset();
  }
private:
  NS_HIDDEN_(void)  DoReset();

public:
  NS_HIDDEN_(void)  SetIntValue(PRInt32 aValue, nsCSSUnit aUnit);
  NS_HIDDEN_(void)  SetPercentValue(float aValue);
  NS_HIDDEN_(void)  SetFloatValue(float aValue, nsCSSUnit aUnit);
  NS_HIDDEN_(void)  SetStringValue(const nsString& aValue, nsCSSUnit aUnit);
  NS_HIDDEN_(void)  SetColorValue(nscolor aValue);
  NS_HIDDEN_(void)  SetArrayValue(nsCSSValue::Array* aArray, nsCSSUnit aUnit);
  NS_HIDDEN_(void)  SetURLValue(nsCSSValue::URL* aURI);
  NS_HIDDEN_(void)  SetImageValue(nsCSSValue::Image* aImage);
  NS_HIDDEN_(void)  SetAutoValue();
  NS_HIDDEN_(void)  SetInheritValue();
  NS_HIDDEN_(void)  SetInitialValue();
  NS_HIDDEN_(void)  SetNoneValue();
  NS_HIDDEN_(void)  SetNormalValue();
  NS_HIDDEN_(void)  SetSystemFontValue();
  NS_HIDDEN_(void)  SetDummyValue();
  NS_HIDDEN_(void)  StartImageLoad(nsIDocument* aDocument)
                                   const;  // Not really const, but pretending

  // Returns an already addrefed buffer.  Can return null on allocation
  // failure.
  static nsStringBuffer* BufferFromString(const nsString& aValue);
  
  struct Array {

    // return |Array| with reference count of zero
    static Array* Create(PRUint16 aItemCount) {
      return new (aItemCount) Array(aItemCount);
    }

    nsCSSValue& operator[](PRUint16 aIndex) {
      NS_ASSERTION(aIndex < mCount, "out of range");
      return *(First() + aIndex);
    }

    const nsCSSValue& operator[](PRUint16 aIndex) const {
      NS_ASSERTION(aIndex < mCount, "out of range");
      return *(First() + aIndex);
    }

    nsCSSValue& Item(PRUint16 aIndex) { return (*this)[aIndex]; }
    const nsCSSValue& Item(PRUint16 aIndex) const { return (*this)[aIndex]; }

    PRUint16 Count() const { return mCount; }

    PRBool operator==(const Array& aOther) const
    {
      if (mCount != aOther.mCount)
        return PR_FALSE;
      for (PRUint16 i = 0; i < mCount; ++i)
        if ((*this)[i] != aOther[i])
          return PR_FALSE;
      return PR_TRUE;
    }

    void AddRef() {
      ++mRefCnt;
      NS_LOG_ADDREF(this, mRefCnt, "nsCSSValue::Array", sizeof(*this));
    }
    void Release() {
      --mRefCnt;
      NS_LOG_RELEASE(this, mRefCnt, "nsCSSValue::Array");
      if (mRefCnt == 0)
        delete this;
    }

  private:

    PRUint16 mRefCnt;
    PRUint16 mCount;

    void* operator new(size_t aSelfSize, PRUint16 aItemCount) CPP_THROW_NEW {
      return ::operator new(aSelfSize + sizeof(nsCSSValue)*aItemCount);
    }

    void operator delete(void* aPtr) { ::operator delete(aPtr); }

    nsCSSValue* First() {
      return (nsCSSValue*) (((char*)this) + sizeof(*this));
    }

    const nsCSSValue* First() const {
      return (const nsCSSValue*) (((const char*)this) + sizeof(*this));
    }

#define CSSVALUE_LIST_FOR_VALUES(var)                                         \
  for (nsCSSValue *var = First(), *var##_end = var + mCount;                  \
       var != var##_end; ++var)

    Array(PRUint16 aItemCount)
      : mRefCnt(0)
      , mCount(aItemCount)
    {
      MOZ_COUNT_CTOR(nsCSSValue::Array);
      CSSVALUE_LIST_FOR_VALUES(val) {
        new (val) nsCSSValue();
      }
    }

    ~Array()
    {
      MOZ_COUNT_DTOR(nsCSSValue::Array);
      CSSVALUE_LIST_FOR_VALUES(val) {
        val->~nsCSSValue();
      }
    }

#undef CSSVALUE_LIST_FOR_VALUES

  private:
    Array(const Array& aOther); // not to be implemented
  };

  struct URL {
    // Methods are not inline because using an nsIPrincipal means requiring
    // caps, which leads to REQUIRES hell, since this header is included all
    // over.    

    // aString must not be null.
    // aOriginPrincipal must not be null.
    URL(nsIURI* aURI, nsStringBuffer* aString, nsIURI* aReferrer,
        nsIPrincipal* aOriginPrincipal) NS_HIDDEN;

    ~URL() NS_HIDDEN;

    NS_HIDDEN_(PRBool) operator==(const URL& aOther) const;

    // URIEquals only compares URIs and principals (unlike operator==, which
    // also compares the original strings).  URIEquals also assumes that the
    // mURI member of both URL objects is non-null.  Do NOT call this method
    // unless you're sure this is the case.
    NS_HIDDEN_(PRBool) URIEquals(const URL& aOther) const;

    nsCOMPtr<nsIURI> mURI; // null == invalid URL
    nsStringBuffer* mString; // Could use nsRefPtr, but it'd add useless
                             // null-checks; this is never null.
    nsCOMPtr<nsIURI> mReferrer;
    nsCOMPtr<nsIPrincipal> mOriginPrincipal;

    void AddRef() { ++mRefCnt; }
    void Release() { if (--mRefCnt == 0) delete this; }
  protected:
    nsrefcnt mRefCnt;
  };

  struct Image : public URL {
    // Not making the constructor and destructor inline because that would
    // force us to include imgIRequest.h, which leads to REQUIRES hell, since
    // this header is included all over.
    // aString must not be null.
    Image(nsIURI* aURI, nsStringBuffer* aString, nsIURI* aReferrer,
          nsIPrincipal* aOriginPrincipal, nsIDocument* aDocument) NS_HIDDEN;
    ~Image() NS_HIDDEN;

    // Inherit operator== from nsCSSValue::URL

    nsCOMPtr<imgIRequest> mRequest; // null == image load blocked or somehow failed

    // Override AddRef/Release so we delete ourselves via the right pointer.
    void AddRef() { ++mRefCnt; }
    void Release() { if (--mRefCnt == 0) delete this; }
  };

private:
  static const PRUnichar* GetBufferValue(nsStringBuffer* aBuffer) {
    return static_cast<PRUnichar*>(aBuffer->Data());
  }

protected:
  nsCSSUnit mUnit;
  union {
    PRInt32    mInt;
    float      mFloat;
    // Note: the capacity of the buffer may exceed the length of the string.
    // If we're of a string type, mString is not null.
    nsStringBuffer* mString;
    nscolor    mColor;
    Array*     mArray;
    URL*       mURL;
    Image*     mImage;
  }         mValue;
};

#endif /* nsCSSValue_h___ */

