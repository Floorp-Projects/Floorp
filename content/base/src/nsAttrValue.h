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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

typedef PRUptrdiff PtrBits;
class nsAString;
class nsIAtom;
class nsISVGValue;
class nsIDocument;
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
  nsAttrValue(mozilla::css::StyleRule* aValue, const nsAString* aSerialized);
  explicit nsAttrValue(nsISVGValue* aValue);
  explicit nsAttrValue(const nsIntMargin& aValue);
  ~nsAttrValue();

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
    eCSSStyleRule = 0x10,
    eAtomArray =    0x11 
    ,eSVGValue =    0x12
    ,eDoubleValue  = 0x13
    ,eIntMarginValue = 0x14
  };

  ValueType Type() const;

  void Reset();

  void SetTo(const nsAttrValue& aOther);
  void SetTo(const nsAString& aValue);
  void SetTo(PRInt16 aInt);
  void SetTo(mozilla::css::StyleRule* aValue, const nsAString* aSerialized);
  void SetTo(nsISVGValue* aValue);
  void SetTo(const nsIntMargin& aValue);

  void SwapValueWith(nsAttrValue& aOther);

  void ToString(nsAString& aResult) const;

  // Methods to get value. These methods do not convert so only use them
  // to retrieve the datatype that this nsAttrValue has.
  inline PRBool IsEmptyString() const;
  const nsCheapString GetStringValue() const;
  inline nsIAtom* GetAtomValue() const;
  inline PRInt32 GetIntegerValue() const;
  PRBool GetColorValue(nscolor& aColor) const;
  inline PRInt16 GetEnumValue() const;
  inline float GetPercentValue() const;
  inline AtomArray* GetAtomArrayValue() const;
  inline mozilla::css::StyleRule* GetCSSStyleRuleValue() const;
  inline nsISVGValue* GetSVGValue() const;
  inline double GetDoubleValue() const;
  PRBool GetIntMarginValue(nsIntMargin& aMargin) const;

  /**
   * Returns the string corresponding to the stored enum value.
   *
   * @param aResult   the string representing the enum tag
   * @param aRealTag  wheter we want to have the real tag or the saved one
   */
  void GetEnumString(nsAString& aResult, PRBool aRealTag) const;

  // Methods to get access to atoms we may have
  // Returns the number of atoms we have; 0 if we have none.  It's OK
  // to call this without checking the type first; it handles that.
  PRUint32 GetAtomCount() const;
  // Returns the atom at aIndex (0-based).  Do not call this with
  // aIndex >= GetAtomCount().
  nsIAtom* AtomAt(PRInt32 aIndex) const;

  PRUint32 HashValue() const;
  PRBool Equals(const nsAttrValue& aOther) const;
  PRBool Equals(const nsAString& aValue, nsCaseTreatment aCaseSensitive) const;
  PRBool Equals(nsIAtom* aValue, nsCaseTreatment aCaseSensitive) const;

  /**
   * Returns true if this AttrValue is equal to the given atom, or is an
   * array which contains the given atom.
   */
  PRBool Contains(nsIAtom* aValue, nsCaseTreatment aCaseSensitive) const;

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
   * @return whether the enum value was found or not
   */
  PRBool ParseEnumValue(const nsAString& aValue,
                        const EnumTable* aTable,
                        PRBool aCaseSensitive);

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
  PRBool ParseSpecialIntValue(const nsAString& aString);


  /**
   * Parse a string value into an integer.
   *
   * @param aString the string to parse
   * @return whether the value could be parsed
   */
  PRBool ParseIntValue(const nsAString& aString) {
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
  PRBool ParseIntWithBounds(const nsAString& aString, PRInt32 aMin,
                            PRInt32 aMax = PR_INT32_MAX);

  /**
   * Parse a string value into a non-negative integer.
   * This method follows the rules for parsing non-negative integer from:
   * http://dev.w3.org/html5/spec/infrastructure.html#rules-for-parsing-non-negative-integers
   *
   * @param  aString the string to parse
   * @return whether the value is valid
   */
  PRBool ParseNonNegativeIntValue(const nsAString& aString);

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
  PRBool ParsePositiveIntValue(const nsAString& aString);

  /**
   * Parse a string into a color.  This implements what HTML5 calls the
   * "rules for parsing a legacy color value".
   *
   * @param aString the string to parse
   * @return whether the value could be parsed
   */
  PRBool ParseColor(const nsAString& aString);

  /**
   * Parse a string value into a double-precision floating point value.
   *
   * @param aString the string to parse
   * @return whether the value could be parsed
   */
  PRBool ParseDoubleValue(const nsAString& aString);

  /**
   * Parse a lazy URI.  This just sets up the storage for the URI; it
   * doesn't actually allocate it.
   */
  PRBool ParseLazyURIValue(const nsAString& aString);

  /**
   * Parse a margin string of format 'top, right, bottom, left' into
   * an nsIntMargin.
   *
   * @param aString the string to parse
   * @return whether the value could be parsed
   */
  PRBool ParseIntMarginValue(const nsAString& aString);

  PRInt64 SizeOf() const;

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
    // mType isn't mCSSStyleRule or eSVGValue.
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
      nsISVGValue* mSVGValue;
      double mDoubleValue;
      nsIntMargin* mIntMargin;
    };
  };

  inline ValueBaseType BaseType() const;

  /**
   * Get the index of an EnumTable in the sEnumTableArray.
   * If the EnumTable is not in the sEnumTableArray, it is added.
   * If there is no more space in sEnumTableArray, it returns PR_FALSE.
   *
   * @param aTable   the EnumTable to get the index of.
   * @param aResult  the index of the EnumTable.
   * @return         whether the index has been found or inserted.
   */
  PRBool GetEnumTableIndex(const EnumTable* aTable, PRInt16& aResult);

  inline void SetPtrValueAndType(void* aValue, ValueBaseType aType);
  void SetIntValueAndType(PRInt32 aValue, ValueType aType,
                          const nsAString* aStringValue);
  void SetColorValue(nscolor aColor, const nsAString& aString);
  void SetMiscAtomOrString(const nsAString* aValue);
  void ResetMiscAtomOrString();
  inline void ResetIfSet();

  inline void* GetPtr() const;
  inline MiscContainer* GetMiscContainer() const;
  inline PRInt32 GetIntInternal() const;

  PRBool EnsureEmptyMiscContainer();
  PRBool EnsureEmptyAtomArray();
  nsStringBuffer* GetStringBuffer(const nsAString& aValue) const;
  // aStrict is set PR_TRUE if stringifying the return value equals with
  // aValue.
  PRInt32 StringToInteger(const nsAString& aValue,
                          PRBool* aStrict,
                          PRInt32* aErrorCode,
                          PRBool aCanBePercent = PR_FALSE,
                          PRBool* aIsPercent = nsnull) const;

  static nsTArray<const EnumTable*, nsTArrayDefaultAllocator>* sEnumTableArray;

  PtrBits mBits;
};

/**
 * Implementation of inline methods
 */

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

inline nsISVGValue*
nsAttrValue::GetSVGValue() const
{
  NS_PRECONDITION(Type() == eSVGValue, "wrong type");
  return GetMiscContainer()->mSVGValue;
}

inline double
nsAttrValue::GetDoubleValue() const
{
  NS_PRECONDITION(Type() == eDoubleValue, "wrong type");
  return GetMiscContainer()->mDoubleValue;
}

inline PRBool
nsAttrValue::GetIntMarginValue(nsIntMargin& aMargin) const
{
  NS_PRECONDITION(Type() == eIntMarginValue, "wrong type");
  nsIntMargin* m = GetMiscContainer()->mIntMargin;
  if (!m)
    return PR_FALSE;
  aMargin = *m;
  return PR_TRUE;
}

inline nsAttrValue::ValueBaseType
nsAttrValue::BaseType() const
{
  return static_cast<ValueBaseType>(mBits & NS_ATTRVALUE_BASETYPE_MASK);
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

inline PRBool
nsAttrValue::IsEmptyString() const
{
  return !mBits;
}

#endif
