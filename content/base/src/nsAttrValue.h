/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A struct that represents the value (type and actual data) of an
 * attribute.
 */

#ifndef nsAttrValue_h___
#define nsAttrValue_h___

#include "nscore.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsColor.h"
#include "nsCaseTreatment.h"
#include "nsMargin.h"
#include "nsCOMPtr.h"
#include "SVGAttrValueWrapper.h"

typedef PRUptrdiff PtrBits;
class nsAString;
class nsIAtom;
template<class E, class A> class nsTArray;
struct nsTArrayDefaultAllocator;

namespace mozilla {
namespace css {
class StyleRule;
}
}

#define NS_ATTRVALUE_MAX_STRINGLENGTH_ATOM 12

#define NS_ATTRVALUE_BASETYPE_MASK (PtrBits(3))
#define NS_ATTRVALUE_POINTERVALUE_MASK (~NS_ATTRVALUE_BASETYPE_MASK)

#define NS_ATTRVALUE_INTEGERTYPE_BITS 4
#define NS_ATTRVALUE_INTEGERTYPE_MASK (PtrBits((1 << NS_ATTRVALUE_INTEGERTYPE_BITS) - 1))
#define NS_ATTRVALUE_INTEGERTYPE_MULTIPLIER (1 << NS_ATTRVALUE_INTEGERTYPE_BITS)
#define NS_ATTRVALUE_INTEGERTYPE_MAXVALUE ((1 << (31 - NS_ATTRVALUE_INTEGERTYPE_BITS)) - 1)
#define NS_ATTRVALUE_INTEGERTYPE_MINVALUE (-NS_ATTRVALUE_INTEGERTYPE_MAXVALUE - 1)

#define NS_ATTRVALUE_ENUMTABLEINDEX_BITS (32 - 16 - NS_ATTRVALUE_INTEGERTYPE_BITS)
#define NS_ATTRVALUE_ENUMTABLE_VALUE_NEEDS_TO_UPPER (1 << (NS_ATTRVALUE_ENUMTABLEINDEX_BITS - 1))
#define NS_ATTRVALUE_ENUMTABLEINDEX_MAXVALUE (NS_ATTRVALUE_ENUMTABLE_VALUE_NEEDS_TO_UPPER - 1)
#define NS_ATTRVALUE_ENUMTABLEINDEX_MASK \
  (PtrBits((((1 << NS_ATTRVALUE_ENUMTABLEINDEX_BITS) - 1) &~ NS_ATTRVALUE_ENUMTABLE_VALUE_NEEDS_TO_UPPER)))

/**
 * A class used to construct a nsString from a nsStringBuffer (we might
 * want to move this to nsString at some point).
 */
class nsCheapString : public nsString {
public:
  nsCheapString(nsStringBuffer* aBuf)
  {
    if (aBuf)
      aBuf->ToString(aBuf->StorageSize()/2 - 1, *this);
  }
};

class nsAttrValue {
public:
  typedef nsTArray< nsCOMPtr<nsIAtom> > AtomArray;

  nsAttrValue();
  nsAttrValue(const nsAttrValue& aOther);
  explicit nsAttrValue(const nsAString& aValue);
  explicit nsAttrValue(nsIAtom* aValue);
  nsAttrValue(mozilla::css::StyleRule* aValue, const nsAString* aSerialized);
  explicit nsAttrValue(const nsIntMargin& aValue);
  ~nsAttrValue();

  inline const nsAttrValue& operator=(const nsAttrValue& aOther);

  static nsresult Init();
  static void Shutdown();

  // This has to be the same as in ValueBaseType
  enum ValueType {
    eString =       0x00, //   00
                          //   01  this value indicates an 'misc' struct
    eAtom =         0x02, //   10
    eInteger =      0x03, // 0011
    eColor =        0x07, // 0111
    eEnum =         0x0B, // 1011  This should eventually die
    ePercent =      0x0F, // 1111
    // Values below here won't matter, they'll be always stored in the 'misc'
    // struct.
    eCSSStyleRule =    0x10
    ,eAtomArray =      0x11
    ,eDoubleValue  =   0x12
    ,eIntMarginValue = 0x13
    ,eSVGTypesBegin =  0x14
    ,eSVGAngle =       eSVGTypesBegin
    ,eSVGIntegerPair = 0x15
    ,eSVGLength =      0x16
    ,eSVGLengthList =  0x17
    ,eSVGNumberList =  0x18
    ,eSVGNumberPair =  0x19
    ,eSVGPathData   =  0x20
    ,eSVGPointList  =  0x21
    ,eSVGPreserveAspectRatio = 0x22
    ,eSVGStringList =  0x23
    ,eSVGTransformList = 0x24
    ,eSVGViewBox =     0x25
    ,eSVGTypesEnd =    0x34
  };

  ValueType Type() const;

  void Reset();

  void SetTo(const nsAttrValue& aOther);
  void SetTo(const nsAString& aValue);
  void SetTo(nsIAtom* aValue);
  void SetTo(PRInt16 aInt);
  void SetTo(PRInt32 aInt, const nsAString* aSerialized);
  void SetTo(double aValue, const nsAString* aSerialized);
  void SetTo(mozilla::css::StyleRule* aValue, const nsAString* aSerialized);
  void SetTo(const nsIntMargin& aValue);
  void SetTo(const nsSVGAngle& aValue, const nsAString* aSerialized);
  void SetTo(const nsSVGIntegerPair& aValue, const nsAString* aSerialized);
  void SetTo(const nsSVGLength2& aValue, const nsAString* aSerialized);
  void SetTo(const mozilla::SVGLengthList& aValue,
             const nsAString* aSerialized);
  void SetTo(const mozilla::SVGNumberList& aValue,
             const nsAString* aSerialized);
  void SetTo(const nsSVGNumberPair& aValue, const nsAString* aSerialized);
  void SetTo(const mozilla::SVGPathData& aValue, const nsAString* aSerialized);
  void SetTo(const mozilla::SVGPointList& aValue, const nsAString* aSerialized);
  void SetTo(const mozilla::SVGAnimatedPreserveAspectRatio& aValue,
             const nsAString* aSerialized);
  void SetTo(const mozilla::SVGStringList& aValue,
             const nsAString* aSerialized);
  void SetTo(const mozilla::SVGTransformList& aValue,
             const nsAString* aSerialized);
  void SetTo(const nsSVGViewBox& aValue, const nsAString* aSerialized);

  /**
   * Sets this object with the string or atom representation of aValue.
   *
   * After calling this method, this object will have type eString unless the
   * type of aValue is eAtom, in which case this object will also have type
   * eAtom.
   */
  void SetToSerialized(const nsAttrValue& aValue);

  void SwapValueWith(nsAttrValue& aOther);

  void ToString(nsAString& aResult) const;
  /**
   * Returns the value of this object as an atom. If necessary, the value will
   * first be serialised using ToString before converting to an atom.
   */
  already_AddRefed<nsIAtom> GetAsAtom() const;

  // Methods to get value. These methods do not convert so only use them
  // to retrieve the datatype that this nsAttrValue has.
  inline bool IsEmptyString() const;
  const nsCheapString GetStringValue() const;
  inline nsIAtom* GetAtomValue() const;
  inline PRInt32 GetIntegerValue() const;
  bool GetColorValue(nscolor& aColor) const;
  inline PRInt16 GetEnumValue() const;
  inline float GetPercentValue() const;
  inline AtomArray* GetAtomArrayValue() const;
  inline mozilla::css::StyleRule* GetCSSStyleRuleValue() const;
  inline double GetDoubleValue() const;
  bool GetIntMarginValue(nsIntMargin& aMargin) const;

  /**
   * Returns the string corresponding to the stored enum value.
   *
   * @param aResult   the string representing the enum tag
   * @param aRealTag  wheter we want to have the real tag or the saved one
   */
  void GetEnumString(nsAString& aResult, bool aRealTag) const;

  // Methods to get access to atoms we may have
  // Returns the number of atoms we have; 0 if we have none.  It's OK
  // to call this without checking the type first; it handles that.
  PRUint32 GetAtomCount() const;
  // Returns the atom at aIndex (0-based).  Do not call this with
  // aIndex >= GetAtomCount().
  nsIAtom* AtomAt(PRInt32 aIndex) const;

  PRUint32 HashValue() const;
  bool Equals(const nsAttrValue& aOther) const;
  bool Equals(const nsAString& aValue, nsCaseTreatment aCaseSensitive) const;
  bool Equals(nsIAtom* aValue, nsCaseTreatment aCaseSensitive) const;

  /**
   * Compares this object with aOther according to their string representation.
   *
   * For example, when called on an object with type eInteger and value 4, and
   * given aOther of type eString and value "4", EqualsAsStrings will return
   * true (while Equals will return false).
   */
  bool EqualsAsStrings(const nsAttrValue& aOther) const;

  /**
   * Returns true if this AttrValue is equal to the given atom, or is an
   * array which contains the given atom.
   */
  bool Contains(nsIAtom* aValue, nsCaseTreatment aCaseSensitive) const;
  /**
   * Returns true if this AttrValue is an atom equal to the given
   * string, or is an array of atoms which contains the given string.
   * This always does a case-sensitive comparison.
   */
  bool Contains(const nsAString& aValue) const;

  void ParseAtom(const nsAString& aValue);
  void ParseAtomArray(const nsAString& aValue);
  void ParseStringOrAtom(const nsAString& aValue);

  /**
   * Structure for a mapping from int (enum) values to strings.  When you use
   * it you generally create an array of them.
   * Instantiate like this:
   * EnumTable myTable[] = {
   *   { "string1", 1 },
   *   { "string2", 2 },
   *   { 0 }
   * }
   */
  struct EnumTable {
    /** The string the value maps to */
    const char* tag;
    /** The enum value that maps to this string */
    PRInt16 value;
  };

  /**
   * Parse into an enum value.
   *
   * @param aValue the string to find the value for
   * @param aTable the enumeration to map with
   * @param aCaseSensitive specify if the parsing has to be case sensitive
   * @param aDefaultValue if non-null, this function will always return true.
   *        Failure to parse aValue as one of the values in aTable will just
   *        cause aDefaultValue->value to be stored as the enumeration value.
   * @return whether the enum value was found or not
   */
  bool ParseEnumValue(const nsAString& aValue,
                        const EnumTable* aTable,
                        bool aCaseSensitive,
                        const EnumTable* aDefaultValue = nsnull);

  /**
   * Parse a string into an integer. Can optionally parse percent (n%).
   * This method explicitly sets a lower bound of zero on the element,
   * whether it be percent or raw integer.
   *
   * @param aString the string to parse
   * @return whether the value could be parsed
   *
   * @see http://www.whatwg.org/html/#rules-for-parsing-dimension-values
   */
  bool ParseSpecialIntValue(const nsAString& aString);


  /**
   * Parse a string value into an integer.
   *
   * @param aString the string to parse
   * @return whether the value could be parsed
   */
  bool ParseIntValue(const nsAString& aString) {
    return ParseIntWithBounds(aString, PR_INT32_MIN, PR_INT32_MAX);
  }

  /**
   * Parse a string value into an integer with minimum value and maximum value.
   *
   * @param aString the string to parse
   * @param aMin the minimum value (if value is less it will be bumped up)
   * @param aMax the maximum value (if value is greater it will be chopped down)
   * @return whether the value could be parsed
   */
  bool ParseIntWithBounds(const nsAString& aString, PRInt32 aMin,
                            PRInt32 aMax = PR_INT32_MAX);

  /**
   * Parse a string value into a non-negative integer.
   * This method follows the rules for parsing non-negative integer from:
   * http://dev.w3.org/html5/spec/infrastructure.html#rules-for-parsing-non-negative-integers
   *
   * @param  aString the string to parse
   * @return whether the value is valid
   */
  bool ParseNonNegativeIntValue(const nsAString& aString);

  /**
   * Parse a string value into a positive integer.
   * This method follows the rules for parsing non-negative integer from:
   * http://dev.w3.org/html5/spec/infrastructure.html#rules-for-parsing-non-negative-integers
   * In addition of these rules, the value has to be greater than zero.
   *
   * This is generally used for parsing content attributes which reflecting IDL
   * attributes are limited to only non-negative numbers greater than zero, see:
   * http://dev.w3.org/html5/spec/common-dom-interfaces.html#limited-to-only-non-negative-numbers-greater-than-zero
   *
   * @param aString       the string to parse
   * @return              whether the value was valid
   */
  bool ParsePositiveIntValue(const nsAString& aString);

  /**
   * Parse a string into a color.  This implements what HTML5 calls the
   * "rules for parsing a legacy color value".
   *
   * @param aString the string to parse
   * @return whether the value could be parsed
   */
  bool ParseColor(const nsAString& aString);

  /**
   * Parse a string value into a double-precision floating point value.
   *
   * @param aString the string to parse
   * @return whether the value could be parsed
   */
  bool ParseDoubleValue(const nsAString& aString);

  /**
   * Parse a lazy URI.  This just sets up the storage for the URI; it
   * doesn't actually allocate it.
   */
  bool ParseLazyURIValue(const nsAString& aString);

  /**
   * Parse a margin string of format 'top, right, bottom, left' into
   * an nsIntMargin.
   *
   * @param aString the string to parse
   * @return whether the value could be parsed
   */
  bool ParseIntMarginValue(const nsAString& aString);

  size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

private:
  // These have to be the same as in ValueType
  enum ValueBaseType {
    eStringBase =    eString,    // 00
    eOtherBase =     0x01,       // 01
    eAtomBase =      eAtom,      // 10
    eIntegerBase =   0x03        // 11
  };

  struct MiscContainer
  {
    ValueType mType;
    // mStringBits points to either nsIAtom* or nsStringBuffer* and is used when
    // mType isn't mCSSStyleRule.
    // Note eStringBase and eAtomBase is used also to handle the type of
    // mStringBits.
    PtrBits mStringBits;
    union {
      PRInt32 mInteger;
      nscolor mColor;
      PRUint32 mEnumValue;
      PRInt32 mPercent;
      mozilla::css::StyleRule* mCSSStyleRule;
      AtomArray* mAtomArray;
      double mDoubleValue;
      nsIntMargin* mIntMargin;
      const nsSVGAngle* mSVGAngle;
      const nsSVGIntegerPair* mSVGIntegerPair;
      const nsSVGLength2* mSVGLength;
      const mozilla::SVGLengthList* mSVGLengthList;
      const mozilla::SVGNumberList* mSVGNumberList;
      const nsSVGNumberPair* mSVGNumberPair;
      const mozilla::SVGPathData* mSVGPathData;
      const mozilla::SVGPointList* mSVGPointList;
      const mozilla::SVGAnimatedPreserveAspectRatio* mSVGPreserveAspectRatio;
      const mozilla::SVGStringList* mSVGStringList;
      const mozilla::SVGTransformList* mSVGTransformList;
      const nsSVGViewBox* mSVGViewBox;
    };
  };

  inline ValueBaseType BaseType() const;
  inline bool IsSVGType(ValueType aType) const;

  /**
   * Get the index of an EnumTable in the sEnumTableArray.
   * If the EnumTable is not in the sEnumTableArray, it is added.
   *
   * @param aTable   the EnumTable to get the index of.
   * @return         the index of the EnumTable.
   */
  PRInt16  GetEnumTableIndex(const EnumTable* aTable);

  inline void SetPtrValueAndType(void* aValue, ValueBaseType aType);
  void SetIntValueAndType(PRInt32 aValue, ValueType aType,
                          const nsAString* aStringValue);
  void SetColorValue(nscolor aColor, const nsAString& aString);
  void SetMiscAtomOrString(const nsAString* aValue);
  void ResetMiscAtomOrString();
  void SetSVGType(ValueType aType, const void* aValue,
                  const nsAString* aSerialized);
  inline void ResetIfSet();

  inline void* GetPtr() const;
  inline MiscContainer* GetMiscContainer() const;
  inline PRInt32 GetIntInternal() const;

  bool EnsureEmptyMiscContainer();
  bool EnsureEmptyAtomArray();
  nsStringBuffer* GetStringBuffer(const nsAString& aValue) const;
  // aStrict is set true if stringifying the return value equals with
  // aValue.
  PRInt32 StringToInteger(const nsAString& aValue,
                          bool* aStrict,
                          PRInt32* aErrorCode,
                          bool aCanBePercent = false,
                          bool* aIsPercent = nsnull) const;
  // Given an enum table and a particular entry in that table, return
  // the actual integer value we should store.
  PRInt32 EnumTableEntryToValue(const EnumTable* aEnumTable,
                                const EnumTable* aTableEntry);  

  static nsTArray<const EnumTable*, nsTArrayDefaultAllocator>* sEnumTableArray;

  PtrBits mBits;
};

/**
 * Implementation of inline methods
 */

inline const nsAttrValue&
nsAttrValue::operator=(const nsAttrValue& aOther)
{
  SetTo(aOther);
  return *this;
}

inline nsIAtom*
nsAttrValue::GetAtomValue() const
{
  NS_PRECONDITION(Type() == eAtom, "wrong type");
  return reinterpret_cast<nsIAtom*>(GetPtr());
}

inline PRInt32
nsAttrValue::GetIntegerValue() const
{
  NS_PRECONDITION(Type() == eInteger, "wrong type");
  return (BaseType() == eIntegerBase)
         ? GetIntInternal()
         : GetMiscContainer()->mInteger;
}

inline PRInt16
nsAttrValue::GetEnumValue() const
{
  NS_PRECONDITION(Type() == eEnum, "wrong type");
  // We don't need to worry about sign extension here since we're
  // returning an PRInt16 which will cut away the top bits.
  return static_cast<PRInt16>((
    (BaseType() == eIntegerBase)
    ? static_cast<PRUint32>(GetIntInternal())
    : GetMiscContainer()->mEnumValue)
      >> NS_ATTRVALUE_ENUMTABLEINDEX_BITS);
}

inline float
nsAttrValue::GetPercentValue() const
{
  NS_PRECONDITION(Type() == ePercent, "wrong type");
  return ((BaseType() == eIntegerBase)
          ? GetIntInternal()
          : GetMiscContainer()->mPercent)
            / 100.0f;
}

inline nsAttrValue::AtomArray*
nsAttrValue::GetAtomArrayValue() const
{
  NS_PRECONDITION(Type() == eAtomArray, "wrong type");
  return GetMiscContainer()->mAtomArray;
}

inline mozilla::css::StyleRule*
nsAttrValue::GetCSSStyleRuleValue() const
{
  NS_PRECONDITION(Type() == eCSSStyleRule, "wrong type");
  return GetMiscContainer()->mCSSStyleRule;
}

inline double
nsAttrValue::GetDoubleValue() const
{
  NS_PRECONDITION(Type() == eDoubleValue, "wrong type");
  return GetMiscContainer()->mDoubleValue;
}

inline bool
nsAttrValue::GetIntMarginValue(nsIntMargin& aMargin) const
{
  NS_PRECONDITION(Type() == eIntMarginValue, "wrong type");
  nsIntMargin* m = GetMiscContainer()->mIntMargin;
  if (!m)
    return false;
  aMargin = *m;
  return true;
}

inline nsAttrValue::ValueBaseType
nsAttrValue::BaseType() const
{
  return static_cast<ValueBaseType>(mBits & NS_ATTRVALUE_BASETYPE_MASK);
}

inline bool
nsAttrValue::IsSVGType(ValueType aType) const
{
  return aType >= eSVGTypesBegin && aType <= eSVGTypesEnd;
}

inline void
nsAttrValue::SetPtrValueAndType(void* aValue, ValueBaseType aType)
{
  NS_ASSERTION(!(NS_PTR_TO_INT32(aValue) & ~NS_ATTRVALUE_POINTERVALUE_MASK),
               "pointer not properly aligned, this will crash");
  mBits = reinterpret_cast<PtrBits>(aValue) | aType;
}

inline void
nsAttrValue::ResetIfSet()
{
  if (mBits) {
    Reset();
  }
}

inline void*
nsAttrValue::GetPtr() const
{
  NS_ASSERTION(BaseType() != eIntegerBase,
               "getting pointer from non-pointer");
  return reinterpret_cast<void*>(mBits & NS_ATTRVALUE_POINTERVALUE_MASK);
}

inline nsAttrValue::MiscContainer*
nsAttrValue::GetMiscContainer() const
{
  NS_ASSERTION(BaseType() == eOtherBase, "wrong type");
  return static_cast<MiscContainer*>(GetPtr());
}

inline PRInt32
nsAttrValue::GetIntInternal() const
{
  NS_ASSERTION(BaseType() == eIntegerBase,
               "getting integer from non-integer");
  // Make sure we get a signed value.
  // Lets hope the optimizer optimizes this into a shift. Unfortunatly signed
  // bitshift right is implementaion dependant.
  return static_cast<PRInt32>(mBits & ~NS_ATTRVALUE_INTEGERTYPE_MASK) /
         NS_ATTRVALUE_INTEGERTYPE_MULTIPLIER;
}

inline bool
nsAttrValue::IsEmptyString() const
{
  return !mBits;
}

#endif
