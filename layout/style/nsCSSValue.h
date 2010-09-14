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

#include "nsCOMPtr.h"
#include "nsCRTGlue.h"
#include "nsCSSKeywords.h"
#include "nsCSSProperty.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsTArray.h"

class imgIRequest;
class nsIDocument;
class nsIPrincipal;
class nsPresContext;
class nsIURI;

// Deletes a linked list iteratively to avoid blowing up the stack (bug 456196).
#define NS_CSS_DELETE_LIST_MEMBER(type_, ptr_, member_)                        \
  {                                                                            \
    type_ *cur = (ptr_)->member_;                                              \
    (ptr_)->member_ = nsnull;                                                  \
    while (cur) {                                                              \
      type_ *next = cur->member_;                                              \
      cur->member_ = nsnull;                                                   \
      delete cur;                                                              \
      cur = next;                                                              \
    }                                                                          \
  }

// Clones a linked list iteratively to avoid blowing up the stack.
// If it fails to clone the entire list then 'to_' is deleted and
// we return null.
#define NS_CSS_CLONE_LIST_MEMBER(type_, from_, member_, to_, args_)            \
  {                                                                            \
    type_ *dest = (to_);                                                       \
    (to_)->member_ = nsnull;                                                   \
    for (const type_ *src = (from_)->member_; src; src = src->member_) {       \
      type_ *clone = src->Clone args_;                                         \
      if (!clone) {                                                            \
        delete (to_);                                                          \
        return nsnull;                                                         \
      }                                                                        \
      dest->member_ = clone;                                                   \
      dest = clone;                                                            \
    }                                                                          \
  }

enum nsCSSUnit {
  eCSSUnit_Null         = 0,      // (n/a) null unit, value is not specified
  eCSSUnit_Auto         = 1,      // (n/a) value is algorithmic
  eCSSUnit_Inherit      = 2,      // (n/a) value is inherited
  eCSSUnit_Initial      = 3,      // (n/a) value is default UA value
  eCSSUnit_None         = 4,      // (n/a) value is none
  eCSSUnit_Normal       = 5,      // (n/a) value is normal (algorithmic, different than auto)
  eCSSUnit_System_Font  = 6,      // (n/a) value is -moz-use-system-font
  eCSSUnit_All          = 7,      // (n/a) value is all
  eCSSUnit_Dummy        = 8,      // (n/a) a fake but specified value, used
                                  //       only in temporary values
  eCSSUnit_DummyInherit = 9,      // (n/a) a fake but specified value, used
                                  //       only in temporary values

  eCSSUnit_String       = 11,     // (PRUnichar*) a string value
  eCSSUnit_Ident        = 12,     // (PRUnichar*) a string value
  eCSSUnit_Families     = 13,     // (PRUnichar*) a string value
  eCSSUnit_Attr         = 14,     // (PRUnichar*) a attr(string) value
  eCSSUnit_Local_Font   = 15,     // (PRUnichar*) a local font name
  eCSSUnit_Font_Format  = 16,     // (PRUnichar*) a font format name
  eCSSUnit_Element      = 17,     // (PRUnichar*) an element id

  eCSSUnit_Array        = 20,     // (nsCSSValue::Array*) a list of values
  eCSSUnit_Counter      = 21,     // (nsCSSValue::Array*) a counter(string,[string]) value
  eCSSUnit_Counters     = 22,     // (nsCSSValue::Array*) a counters(string,string[,string]) value
  eCSSUnit_Cubic_Bezier = 23,     // (nsCSSValue::Array*) a list of float values
  eCSSUnit_Function     = 24,     // (nsCSSValue::Array*) a function with
                                  //  parameters.  First elem of array is name,
                                  //  the rest of the values are arguments.

  // The top level of a calc() expression is eCSSUnit_Calc.  All
  // remaining eCSSUnit_Calc_* units only occur inside these toplevel
  // calc values.

  // eCSSUnit_Calc has an array with exactly 1 element.  eCSSUnit_Calc
  // exists so we can distinguish calc(2em) from 2em as specified values
  // (but we drop this distinction for nsStyleCoord when we store
  // computed values).
  eCSSUnit_Calc         = 25,     // (nsCSSValue::Array*) calc() value
  // Plus, Minus, Times_* and Divided have arrays with exactly 2
  // elements.  a + b + c + d is grouped as ((a + b) + c) + d
  eCSSUnit_Calc_Plus    = 26,     // (nsCSSValue::Array*) + node within calc()
  eCSSUnit_Calc_Minus   = 27,     // (nsCSSValue::Array*) - within calc
  eCSSUnit_Calc_Times_L = 28,     // (nsCSSValue::Array*) num * val within calc
  eCSSUnit_Calc_Times_R = 29,     // (nsCSSValue::Array*) val * num within calc
  eCSSUnit_Calc_Divided = 30,     // (nsCSSValue::Array*) / within calc

  eCSSUnit_URL          = 40,     // (nsCSSValue::URL*) value
  eCSSUnit_Image        = 41,     // (nsCSSValue::Image*) value
  eCSSUnit_Gradient     = 42,     // (nsCSSValueGradient*) value

  eCSSUnit_Pair         = 50,     // (nsCSSValuePair*) pair of values
  eCSSUnit_Rect         = 51,     // (nsCSSRect*) rectangle (four values)
  eCSSUnit_List         = 52,     // (nsCSSValueList*) list of values
  eCSSUnit_ListDep      = 53,     // (nsCSSValueList*) same as List
                                  //   but does not own the list
  eCSSUnit_PairList     = 54,     // (nsCSSValuePairList*) list of value pairs
  eCSSUnit_PairListDep  = 55,     // (nsCSSValuePairList*) same as PairList
                                  //   but does not own the list

  eCSSUnit_Integer      = 70,     // (int) simple value
  eCSSUnit_Enumerated   = 71,     // (int) value has enumerated meaning

  eCSSUnit_EnumColor    = 80,     // (int) enumerated color (kColorKTable)
  eCSSUnit_Color        = 81,     // (nscolor) an RGBA value

  eCSSUnit_Percent      = 90,     // (float) 1.0 == 100%) value is percentage of something
  eCSSUnit_Number       = 91,     // (float) value is numeric (usually multiplier, different behavior that percent)

  // Physical length units
  eCSSUnit_PhysicalMillimeter = 200,   // (float) 1/25.4 inch

  // Length units - relative
  // Font relative measure
  eCSSUnit_EM           = 800,    // (float) == current font size
  eCSSUnit_XHeight      = 801,    // (float) distance from top of lower case x to baseline
  eCSSUnit_Char         = 802,    // (float) number of characters, used for width with monospace font
  eCSSUnit_RootEM       = 803,    // (float) == root element font size

  // Screen relative measure
  eCSSUnit_Point        = 900,    // (float) 4/3 of a CSS pixel
  eCSSUnit_Inch         = 901,    // (float) 96 CSS pixels
  eCSSUnit_Millimeter   = 902,    // (float) 96/25.4 CSS pixels
  eCSSUnit_Centimeter   = 903,    // (float) 96/2.54 CSS pixels
  eCSSUnit_Pica         = 904,    // (float) 12 points == 16 CSS pixls
  eCSSUnit_Pixel        = 905,    // (float) CSS pixel unit

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

struct nsCSSValueGradient;
struct nsCSSValuePair;
struct nsCSSValuePair_heap;
struct nsCSSRect;
struct nsCSSRect_heap;
struct nsCSSValueList;
struct nsCSSValueList_heap;
struct nsCSSValuePairList;
struct nsCSSValuePairList_heap;

class nsCSSValue {
public:
  struct Array;
  friend struct Array;

  struct URL;
  friend struct URL;

  struct Image;
  friend struct Image;

  // for valueless units only (null, auto, inherit, none, all, normal)
  explicit nsCSSValue(nsCSSUnit aUnit = eCSSUnit_Null)
    : mUnit(aUnit)
  {
    NS_ABORT_IF_FALSE(aUnit <= eCSSUnit_DummyInherit, "not a valueless unit");
  }

  nsCSSValue(PRInt32 aValue, nsCSSUnit aUnit);
  nsCSSValue(float aValue, nsCSSUnit aUnit);
  nsCSSValue(const nsString& aValue, nsCSSUnit aUnit);
  nsCSSValue(Array* aArray, nsCSSUnit aUnit);
  explicit nsCSSValue(URL* aValue);
  explicit nsCSSValue(Image* aValue);
  explicit nsCSSValue(nsCSSValueGradient* aValue);
  nsCSSValue(const nsCSSValue& aCopy);
  ~nsCSSValue() { Reset(); }

  nsCSSValue&  operator=(const nsCSSValue& aCopy);
  PRBool      operator==(const nsCSSValue& aOther) const;

  PRBool operator!=(const nsCSSValue& aOther) const
  {
    return !(*this == aOther);
  }

  /**
   * Serialize |this| as a specified value for |aProperty| and append
   * it to |aResult|.
   */
  void AppendToString(nsCSSProperty aProperty, nsAString& aResult) const;

  nsCSSUnit GetUnit() const { return mUnit; }
  PRBool    IsLengthUnit() const
    { return eCSSUnit_PhysicalMillimeter <= mUnit && mUnit <= eCSSUnit_Pixel; }
  /**
   * A "fixed" length unit is one that means a specific physical length
   * which we try to match based on the physical characteristics of an
   * output device.
   */
  PRBool    IsFixedLengthUnit() const  
    { return mUnit == eCSSUnit_PhysicalMillimeter; }
  /**
   * What the spec calls relative length units is, for us, split
   * between relative length units and pixel length units.
   * 
   * A "relative" length unit is a multiple of some derived metric,
   * such as a font em-size, which itself was controlled by an input CSS
   * length. Relative length units should not be scaled by zooming, since
   * the underlying CSS length would already have been scaled.
   */
  PRBool    IsRelativeLengthUnit() const  
    { return eCSSUnit_EM <= mUnit && mUnit <= eCSSUnit_RootEM; }
  /**
   * A "pixel" length unit is a some multiple of CSS pixels.
   */
  PRBool    IsPixelLengthUnit() const
    { return eCSSUnit_Point <= mUnit && mUnit <= eCSSUnit_Pixel; }
  PRBool    IsAngularUnit() const  
    { return eCSSUnit_Degree <= mUnit && mUnit <= eCSSUnit_Radian; }
  PRBool    IsFrequencyUnit() const  
    { return eCSSUnit_Hertz <= mUnit && mUnit <= eCSSUnit_Kilohertz; }
  PRBool    IsTimeUnit() const  
    { return eCSSUnit_Seconds <= mUnit && mUnit <= eCSSUnit_Milliseconds; }
  PRBool    IsCalcUnit() const
    { return eCSSUnit_Calc <= mUnit && mUnit <= eCSSUnit_Calc_Divided; }

  PRBool    UnitHasStringValue() const
    { return eCSSUnit_String <= mUnit && mUnit <= eCSSUnit_Element; }
  PRBool    UnitHasArrayValue() const
    { return eCSSUnit_Array <= mUnit && mUnit <= eCSSUnit_Calc_Divided; }

  PRInt32 GetIntValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Integer ||
                      mUnit == eCSSUnit_Enumerated ||
                      mUnit == eCSSUnit_EnumColor,
                      "not an int value");
    return mValue.mInt;
  }

  float GetPercentValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Percent, "not a percent value");
    return mValue.mFloat;
  }

  float GetFloatValue() const
  {
    NS_ABORT_IF_FALSE(eCSSUnit_Number <= mUnit, "not a float value");
    return mValue.mFloat;
  }

  float GetAngleValue() const
  {
    NS_ABORT_IF_FALSE(eCSSUnit_Degree <= mUnit &&
                 mUnit <= eCSSUnit_Radian, "not an angle value");
    return mValue.mFloat;
  }

  // Converts any angle to radians.
  double GetAngleValueInRadians() const;

  nsAString& GetStringValue(nsAString& aBuffer) const
  {
    NS_ABORT_IF_FALSE(UnitHasStringValue(), "not a string value");
    aBuffer.Truncate();
    PRUint32 len = NS_strlen(GetBufferValue(mValue.mString));
    mValue.mString->ToString(len, aBuffer);
    return aBuffer;
  }

  const PRUnichar* GetStringBufferValue() const
  {
    NS_ABORT_IF_FALSE(UnitHasStringValue(), "not a string value");
    return GetBufferValue(mValue.mString);
  }

  nscolor GetColorValue() const
  {
    NS_ABORT_IF_FALSE((mUnit == eCSSUnit_Color), "not a color value");
    return mValue.mColor;
  }

  PRBool IsNonTransparentColor() const;

  Array* GetArrayValue() const
  {
    NS_ABORT_IF_FALSE(UnitHasArrayValue(), "not an array value");
    return mValue.mArray;
  }

  nsIURI* GetURLValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_URL || mUnit == eCSSUnit_Image,
                 "not a URL value");
    return mUnit == eCSSUnit_URL ?
      mValue.mURL->mURI : mValue.mImage->mURI;
  }

  nsCSSValueGradient* GetGradientValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Gradient, "not a gradient value");
    return mValue.mGradient;
  }

  // bodies of these are below
  inline nsCSSValuePair& GetPairValue();
  inline const nsCSSValuePair& GetPairValue() const;

  inline nsCSSRect& GetRectValue();
  inline const nsCSSRect& GetRectValue() const;

  inline nsCSSValueList* GetListValue();
  inline const nsCSSValueList* GetListValue() const;

  inline nsCSSValuePairList* GetPairListValue();
  inline const nsCSSValuePairList* GetPairListValue() const;

  URL* GetURLStructValue() const
  {
    // Not allowing this for Image values, because if the caller takes
    // a ref to them they won't be able to delete them properly.
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_URL, "not a URL value");
    return mValue.mURL;
  }

  const PRUnichar* GetOriginalURLValue() const
  {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_URL || mUnit == eCSSUnit_Image,
                      "not a URL value");
    return GetBufferValue(mUnit == eCSSUnit_URL ?
                            mValue.mURL->mString :
                            mValue.mImage->mString);
  }

  // Not making this inline because that would force us to include
  // imgIRequest.h, which leads to REQUIRES hell, since this header is included
  // all over.
  imgIRequest* GetImageValue() const;

  nscoord GetFixedLength(nsPresContext* aPresContext) const;
  nscoord GetPixelLength() const;

  void Reset()  // sets to null
  {
    if (mUnit != eCSSUnit_Null)
      DoReset();
  }
private:
  void DoReset();

public:
  void SetIntValue(PRInt32 aValue, nsCSSUnit aUnit);
  void SetPercentValue(float aValue);
  void SetFloatValue(float aValue, nsCSSUnit aUnit);
  void SetStringValue(const nsString& aValue, nsCSSUnit aUnit);
  void SetColorValue(nscolor aValue);
  void SetArrayValue(nsCSSValue::Array* aArray, nsCSSUnit aUnit);
  void SetURLValue(nsCSSValue::URL* aURI);
  void SetImageValue(nsCSSValue::Image* aImage);
  void SetGradientValue(nsCSSValueGradient* aGradient);
  void SetPairValue(const nsCSSValuePair* aPair);
  void SetPairValue(const nsCSSValue& xValue, const nsCSSValue& yValue);
  void SetDependentListValue(nsCSSValueList* aList);
  void SetDependentPairListValue(nsCSSValuePairList* aList);
  void SetAutoValue();
  void SetInheritValue();
  void SetInitialValue();
  void SetNoneValue();
  void SetAllValue();
  void SetNormalValue();
  void SetSystemFontValue();
  void SetDummyValue();
  void SetDummyInheritValue();

  // These are a little different - they allocate storage for you and
  // return a handle.
  nsCSSRect& SetRectValue();
  nsCSSValueList* SetListValue();
  nsCSSValuePairList* SetPairListValue();

  void StartImageLoad(nsIDocument* aDocument) const;  // Only pretend const

  // Initializes as a function value with the specified function id.
  Array* InitFunction(nsCSSKeyword aFunctionId, PRUint32 aNumArgs);
  // Checks if this is a function value with the specified function id.
  PRBool EqualsFunction(nsCSSKeyword aFunctionId) const;

  // Returns an already addrefed buffer.  Can return null on allocation
  // failure.
  static nsStringBuffer* BufferFromString(const nsString& aValue);

  struct URL {
    // Methods are not inline because using an nsIPrincipal means requiring
    // caps, which leads to REQUIRES hell, since this header is included all
    // over.

    // aString must not be null.
    // aOriginPrincipal must not be null.
    URL(nsIURI* aURI, nsStringBuffer* aString, nsIURI* aReferrer,
        nsIPrincipal* aOriginPrincipal);

    ~URL();

    PRBool operator==(const URL& aOther) const;

    // URIEquals only compares URIs and principals (unlike operator==, which
    // also compares the original strings).  URIEquals also assumes that the
    // mURI member of both URL objects is non-null.  Do NOT call this method
    // unless you're sure this is the case.
    PRBool URIEquals(const URL& aOther) const;

    nsCOMPtr<nsIURI> mURI; // null == invalid URL
    nsStringBuffer* mString; // Could use nsRefPtr, but it'd add useless
                             // null-checks; this is never null.
    nsCOMPtr<nsIURI> mReferrer;
    nsCOMPtr<nsIPrincipal> mOriginPrincipal;

    NS_INLINE_DECL_REFCOUNTING(nsCSSValue::URL)

  protected:

    // not to be implemented
    URL(const URL& aOther);
    URL& operator=(const URL& aOther);
  };

  struct Image : public URL {
    // Not making the constructor and destructor inline because that would
    // force us to include imgIRequest.h, which leads to REQUIRES hell, since
    // this header is included all over.
    // aString must not be null.
    Image(nsIURI* aURI, nsStringBuffer* aString, nsIURI* aReferrer,
          nsIPrincipal* aOriginPrincipal, nsIDocument* aDocument);
    ~Image();

    // Inherit operator== from nsCSSValue::URL

    nsCOMPtr<imgIRequest> mRequest; // null == image load blocked or somehow failed

    // Override AddRef and Release to not only log ourselves correctly, but
    // also so that we delete correctly without a virtual destructor
    NS_INLINE_DECL_REFCOUNTING(nsCSSValue::Image)
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
    nsCSSValueGradient* mGradient;
    nsCSSValuePair_heap* mPair;
    nsCSSRect_heap* mRect;
    nsCSSValueList_heap* mList;
    nsCSSValueList* mListDependent;
    nsCSSValuePairList_heap* mPairList;
    nsCSSValuePairList* mPairListDependent;
  }         mValue;
};

struct nsCSSValue::Array {

  // return |Array| with reference count of zero
  static Array* Create(size_t aItemCount) {
    return new (aItemCount) Array(aItemCount);
  }

  nsCSSValue& operator[](size_t aIndex) {
    NS_ABORT_IF_FALSE(aIndex < mCount, "out of range");
    return mArray[aIndex];
  }

  const nsCSSValue& operator[](size_t aIndex) const {
    NS_ABORT_IF_FALSE(aIndex < mCount, "out of range");
    return mArray[aIndex];
  }

  nsCSSValue& Item(size_t aIndex) { return (*this)[aIndex]; }
  const nsCSSValue& Item(size_t aIndex) const { return (*this)[aIndex]; }

  size_t Count() const { return mCount; }

  PRBool operator==(const Array& aOther) const
  {
    if (mCount != aOther.mCount)
      return PR_FALSE;
    for (size_t i = 0; i < mCount; ++i)
      if ((*this)[i] != aOther[i])
        return PR_FALSE;
    return PR_TRUE;
  }

  // XXXdholbert This uses a size_t ref count. Should we use a variant
  // of NS_INLINE_DECL_REFCOUNTING that takes a type as an argument?
  void AddRef() {
    if (mRefCnt == size_t(-1)) { // really want SIZE_MAX
      NS_WARNING("refcount overflow, leaking nsCSSValue::Array");
      return;
    }
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "nsCSSValue::Array", sizeof(*this));
  }
  void Release() {
    if (mRefCnt == size_t(-1)) { // really want SIZE_MAX
      NS_WARNING("refcount overflow, leaking nsCSSValue::Array");
      return;
    }
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "nsCSSValue::Array");
    if (mRefCnt == 0)
      delete this;
  }

private:

  size_t mRefCnt;
  const size_t mCount;
  // This must be the last sub-object, since we extend this array to
  // be of size mCount; it needs to be a sub-object so it gets proper
  // alignment.
  nsCSSValue mArray[1];

  void* operator new(size_t aSelfSize, size_t aItemCount) CPP_THROW_NEW {
    NS_ABORT_IF_FALSE(aItemCount > 0, "cannot have a 0 item count");
    return ::operator new(aSelfSize + sizeof(nsCSSValue) * (aItemCount - 1));
  }

  void operator delete(void* aPtr) { ::operator delete(aPtr); }

  nsCSSValue* First() { return mArray; }

  const nsCSSValue* First() const { return mArray; }

#define CSSVALUE_LIST_FOR_EXTRA_VALUES(var)                                   \
  for (nsCSSValue *var = First() + 1, *var##_end = First() + mCount;          \
       var != var##_end; ++var)

  Array(size_t aItemCount)
    : mRefCnt(0)
    , mCount(aItemCount)
  {
    MOZ_COUNT_CTOR(nsCSSValue::Array);
    CSSVALUE_LIST_FOR_EXTRA_VALUES(val) {
      new (val) nsCSSValue();
    }
  }

  ~Array()
  {
    MOZ_COUNT_DTOR(nsCSSValue::Array);
    CSSVALUE_LIST_FOR_EXTRA_VALUES(val) {
      val->~nsCSSValue();
    }
  }

#undef CSSVALUE_LIST_FOR_EXTRA_VALUES

private:
  // not to be implemented
  Array(const Array& aOther);
  Array& operator=(const Array& aOther);
};

// Prefer nsCSSValue::Array for lists of fixed size.
struct nsCSSValueList {
  nsCSSValueList() : mNext(nsnull) { MOZ_COUNT_CTOR(nsCSSValueList); }
  ~nsCSSValueList();

  nsCSSValueList* Clone() const;  // makes a deep copy
  void AppendToString(nsCSSProperty aProperty, nsAString& aResult) const;

  bool operator==(nsCSSValueList const& aOther) const;
  bool operator!=(const nsCSSValueList& aOther) const
  { return !(*this == aOther); }

  nsCSSValue      mValue;
  nsCSSValueList* mNext;

private:
  nsCSSValueList(const nsCSSValueList& aCopy) // makes a shallow copy
    : mValue(aCopy.mValue), mNext(nsnull)
  {
    MOZ_COUNT_CTOR(nsCSSValueList);
  }
};

// nsCSSValueList_heap differs from nsCSSValueList only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSValueList_heap : public nsCSSValueList {
  NS_INLINE_DECL_REFCOUNTING(nsCSSValueList_heap)
};

// This has to be here so that the relationship between nsCSSValueList
// and nsCSSValueList_heap is visible.
inline nsCSSValueList*
nsCSSValue::GetListValue()
{
  if (mUnit == eCSSUnit_List)
    return mValue.mList;
  else {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_ListDep, "not a pairlist value");
    return mValue.mListDependent;
  }
}

inline const nsCSSValueList*
nsCSSValue::GetListValue() const
{
  if (mUnit == eCSSUnit_List)
    return mValue.mList;
  else {
    NS_ABORT_IF_FALSE(mUnit == eCSSUnit_ListDep, "not a pairlist value");
    return mValue.mListDependent;
  }
}

struct nsCSSRect {
  nsCSSRect(void);
  nsCSSRect(const nsCSSRect& aCopy);
  ~nsCSSRect();

  void AppendToString(nsCSSProperty aProperty, nsAString& aResult) const;

  PRBool operator==(const nsCSSRect& aOther) const {
    return mTop == aOther.mTop &&
           mRight == aOther.mRight &&
           mBottom == aOther.mBottom &&
           mLeft == aOther.mLeft;
  }

  PRBool operator!=(const nsCSSRect& aOther) const {
    return mTop != aOther.mTop ||
           mRight != aOther.mRight ||
           mBottom != aOther.mBottom ||
           mLeft != aOther.mLeft;
  }

  void SetAllSidesTo(const nsCSSValue& aValue);

  void Reset() {
    mTop.Reset();
    mRight.Reset();
    mBottom.Reset();
    mLeft.Reset();
  }

  PRBool HasValue() const {
    return
      mTop.GetUnit() != eCSSUnit_Null ||
      mRight.GetUnit() != eCSSUnit_Null ||
      mBottom.GetUnit() != eCSSUnit_Null ||
      mLeft.GetUnit() != eCSSUnit_Null;
  }

  nsCSSValue mTop;
  nsCSSValue mRight;
  nsCSSValue mBottom;
  nsCSSValue mLeft;

  typedef nsCSSValue nsCSSRect::*side_type;
  static const side_type sides[4];
};

// nsCSSRect_heap differs from nsCSSRect only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSRect_heap : public nsCSSRect {
  NS_INLINE_DECL_REFCOUNTING(nsCSSRect_heap)
};

// This has to be here so that the relationship between nsCSSRect
// and nsCSSRect_heap is visible.
inline nsCSSRect&
nsCSSValue::GetRectValue()
{
  NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Rect, "not a pair value");
  return *mValue.mRect;
}

inline const nsCSSRect&
nsCSSValue::GetRectValue() const
{
  NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Rect, "not a pair value");
  return *mValue.mRect;
}

struct nsCSSValuePair {
  nsCSSValuePair()
  {
    MOZ_COUNT_CTOR(nsCSSValuePair);
  }
  nsCSSValuePair(nsCSSUnit aUnit)
    : mXValue(aUnit), mYValue(aUnit)
  {
    MOZ_COUNT_CTOR(nsCSSValuePair);
  }
  nsCSSValuePair(const nsCSSValue& aXValue, const nsCSSValue& aYValue)
    : mXValue(aXValue), mYValue(aYValue)
  {
    MOZ_COUNT_CTOR(nsCSSValuePair);
  }
  nsCSSValuePair(const nsCSSValuePair& aCopy)
    : mXValue(aCopy.mXValue), mYValue(aCopy.mYValue)
  {
    MOZ_COUNT_CTOR(nsCSSValuePair);
  }
  ~nsCSSValuePair()
  {
    MOZ_COUNT_DTOR(nsCSSValuePair);
  }

  PRBool operator==(const nsCSSValuePair& aOther) const {
    return mXValue == aOther.mXValue &&
           mYValue == aOther.mYValue;
  }

  PRBool operator!=(const nsCSSValuePair& aOther) const {
    return mXValue != aOther.mXValue ||
           mYValue != aOther.mYValue;
  }

  void SetBothValuesTo(const nsCSSValue& aValue) {
    mXValue = aValue;
    mYValue = aValue;
  }

  void Reset() {
    mXValue.Reset();
    mYValue.Reset();
  }

  PRBool HasValue() const {
    return mXValue.GetUnit() != eCSSUnit_Null ||
           mYValue.GetUnit() != eCSSUnit_Null;
  }

  void AppendToString(nsCSSProperty aProperty, nsAString& aResult) const;

  nsCSSValue mXValue;
  nsCSSValue mYValue;
};

// nsCSSValuePair_heap differs from nsCSSValuePair only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSValuePair_heap : public nsCSSValuePair {
  // forward constructor
  nsCSSValuePair_heap(const nsCSSValue& aXValue, const nsCSSValue& aYValue)
    : nsCSSValuePair(aXValue, aYValue)
  {}

  NS_INLINE_DECL_REFCOUNTING(nsCSSValuePair_heap)
};

// This has to be here so that the relationship between nsCSSValuePair
// and nsCSSValuePair_heap is visible.
inline nsCSSValuePair&
nsCSSValue::GetPairValue()
{
  NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Pair, "not a pair value");
  return *mValue.mPair;
}

inline const nsCSSValuePair&
nsCSSValue::GetPairValue() const
{
  NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Pair, "not a pair value");
  return *mValue.mPair;
}

// Maybe should be replaced with nsCSSValueList and nsCSSValue::Array?
struct nsCSSValuePairList {
  nsCSSValuePairList() : mNext(nsnull) { MOZ_COUNT_CTOR(nsCSSValuePairList); }
  ~nsCSSValuePairList();

  nsCSSValuePairList* Clone() const; // makes a deep copy
  void AppendToString(nsCSSProperty aProperty, nsAString& aResult) const;

  bool operator==(const nsCSSValuePairList& aOther) const;
  bool operator!=(const nsCSSValuePairList& aOther) const
  { return !(*this == aOther); }

  nsCSSValue          mXValue;
  nsCSSValue          mYValue;
  nsCSSValuePairList* mNext;

private:
  nsCSSValuePairList(const nsCSSValuePairList& aCopy) // makes a shallow copy
    : mXValue(aCopy.mXValue), mYValue(aCopy.mYValue), mNext(nsnull)
  {
    MOZ_COUNT_CTOR(nsCSSValuePairList);
  }
};

// nsCSSValuePairList_heap differs from nsCSSValuePairList only in being
// refcounted.  It should not be necessary to use this class directly;
// it's an implementation detail of nsCSSValue.
struct nsCSSValuePairList_heap : public nsCSSValuePairList {
  NS_INLINE_DECL_REFCOUNTING(nsCSSValuePairList_heap)
};

// This has to be here so that the relationship between nsCSSValuePairList
// and nsCSSValuePairList_heap is visible.
inline nsCSSValuePairList*
nsCSSValue::GetPairListValue()
{
  if (mUnit == eCSSUnit_PairList)
    return mValue.mPairList;
  else {
    NS_ABORT_IF_FALSE (mUnit == eCSSUnit_PairListDep, "not a pairlist value");
    return mValue.mPairListDependent;
  }
}

inline const nsCSSValuePairList*
nsCSSValue::GetPairListValue() const
{
  if (mUnit == eCSSUnit_PairList)
    return mValue.mPairList;
  else {
    NS_ABORT_IF_FALSE (mUnit == eCSSUnit_PairListDep, "not a pairlist value");
    return mValue.mPairListDependent;
  }
}

struct nsCSSValueGradientStop {
public:
  nsCSSValueGradientStop();
  // needed to keep bloat logs happy when we use the nsTArray in nsCSSValueGradient
  nsCSSValueGradientStop(const nsCSSValueGradientStop& aOther);
  ~nsCSSValueGradientStop();

  nsCSSValue mLocation;
  nsCSSValue mColor;

  PRBool operator==(const nsCSSValueGradientStop& aOther) const
  {
    return (mLocation == aOther.mLocation &&
            mColor == aOther.mColor);
  }

  PRBool operator!=(const nsCSSValueGradientStop& aOther) const
  {
    return !(*this == aOther);
  }
};

struct nsCSSValueGradient {
  nsCSSValueGradient(PRBool aIsRadial, PRBool aIsRepeating);

  // true if gradient is radial, false if it is linear
  PRPackedBool mIsRadial;
  PRPackedBool mIsRepeating;
  // line position and angle
  nsCSSValuePair mBgPos;
  nsCSSValue mAngle;

  // Only meaningful if mIsRadial is true
  nsCSSValue mRadialShape;
  nsCSSValue mRadialSize;

  nsTArray<nsCSSValueGradientStop> mStops;

  PRBool operator==(const nsCSSValueGradient& aOther) const
  {
    if (mIsRadial != aOther.mIsRadial ||
        mIsRepeating != aOther.mIsRepeating ||
        mBgPos != aOther.mBgPos ||
        mAngle != aOther.mAngle ||
        mRadialShape != aOther.mRadialShape ||
        mRadialSize != aOther.mRadialSize)
      return PR_FALSE;

    if (mStops.Length() != aOther.mStops.Length())
      return PR_FALSE;

    for (PRUint32 i = 0; i < mStops.Length(); i++) {
      if (mStops[i] != aOther.mStops[i])
        return PR_FALSE;
    }

    return PR_TRUE;
  }

  PRBool operator!=(const nsCSSValueGradient& aOther) const
  {
    return !(*this == aOther);
  }

  NS_INLINE_DECL_REFCOUNTING(nsCSSValueGradient)

private:
  // not to be implemented
  nsCSSValueGradient(const nsCSSValueGradient& aOther);
  nsCSSValueGradient& operator=(const nsCSSValueGradient& aOther);
};

#endif /* nsCSSValue_h___ */

