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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com>
 *
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

#ifndef txMozillaString_h__
#define txMozillaString_h__

#include "nsReadableUtils.h"
#ifndef TX_EXE
#include "nsUnicharUtils.h"
typedef nsCaseInsensitiveStringComparator txCaseInsensitiveStringComparator;
#endif

MOZ_DECL_CTOR_COUNTER(String)

inline String::String()
{
  MOZ_COUNT_CTOR(String);
}

inline String::String(const String& aSource) : mString(aSource.mString)
{
  MOZ_COUNT_CTOR(String);
}

inline String::String(const nsAString& aSource) : mString(aSource)
{
  MOZ_COUNT_CTOR(String);
}

inline String::~String()
{
  MOZ_COUNT_DTOR(String);
}

inline void String::append(UNICODE_CHAR aSource)
{
  mString.Append(aSource);
}

inline void String::append(const String& aSource)
{
  mString.Append(aSource.mString);
}

inline void String::append(const UNICODE_CHAR* aSource, PRUint32 aLength)
{
  mString.Append(aSource, aLength);
}

inline void String::append(const nsAString& aSource)
{
  mString.Append(aSource);
}

inline void String::insert(PRUint32 aOffset,
                           UNICODE_CHAR aSource)
{
  mString.Insert(aSource, aOffset);
}

inline void String::insert(PRUint32 aOffset, const String& aSource)
{
  mString.Insert(aSource.mString, aOffset);
}

inline void String::replace(PRUint32 aOffset,
                            UNICODE_CHAR aSource)
{
  mString.SetCharAt(aSource, aOffset);
}

inline void String::replace(PRUint32 aOffset, const String& aSource)
{
  mString.Replace(aOffset, mString.Length() - aOffset, aSource.mString);
}

inline void String::deleteChars(PRUint32 aOffset,
                                PRUint32 aCount)
{
  mString.Cut(aOffset, aCount);
}

inline UNICODE_CHAR String::charAt(PRUint32 aIndex) const
{
  return mString.CharAt(aIndex);
}

inline void String::clear()
{
  mString.Truncate();
}

inline PRInt32 String::indexOf(UNICODE_CHAR aData,
                               PRInt32 aOffset) const
{
  return mString.FindChar(aData, (PRUint32)aOffset);
}

inline PRInt32 String::indexOf(const String& aData,
                               PRInt32 aOffset) const
{
  return mString.Find(aData.mString, aOffset);
}

inline PRInt32 String::lastIndexOf(UNICODE_CHAR aData,
                                   PRInt32 aOffset) const
{
  return mString.RFindChar(aData, aOffset);
}

inline MBool String::isEqual(const String& aData) const
{
  if (this == &aData)
    return MB_TRUE;
  return mString.Equals(aData.mString);
}

inline MBool String::isEqualIgnoreCase(const String& aData) const
{
  if (this == &aData)
    return MB_TRUE;
  return mString.Equals(aData.mString, txCaseInsensitiveStringComparator());
}

inline MBool String::isEmpty() const
{
  return mString.IsEmpty();
}

inline PRUint32 String::length() const
{
  return mString.Length();
}

inline void String::truncate(PRUint32 aLength)
{
  mString.Truncate(aLength);
}

inline String& String::subString(PRUint32 aStart, String& aDest) const
{
  PRUint32 length = mString.Length();
  if (aStart < length) {
    aDest.mString.Assign(Substring(mString, aStart, length - aStart));
  }
  else {
    NS_ASSERTION(aStart == length,
                 "Bonehead! Calling subString with negative length.");
    aDest.clear();
  }
  return aDest;
}

inline String& String::subString(PRUint32 aStart, PRUint32 aEnd,
                                 String& aDest) const
{
  if (aStart < aEnd) {
    aDest.mString.Assign(Substring(mString, aStart, aEnd - aStart));
  }
  else {
    NS_ASSERTION(aStart == aEnd,
                 "Bonehead! Calling subString with negative length.");
    aDest.clear();
  }
  return aDest;
}

#ifndef TX_EXE
inline void String::toLowerCase()
{
  ToLowerCase(mString);
}

inline void String::toUpperCase()
{
  ToUpperCase(mString);
}
#endif

inline String::operator nsAString&()
{
  return mString;
}

inline String::operator const nsAString&() const
{
  return mString;
}

// XXX DEPRECATED
inline String::String(PRUint32 aSize)
{
  MOZ_COUNT_CTOR(String);
  mString.SetCapacity(aSize);
}

inline String::String(const char* aSource)
{
  MOZ_COUNT_CTOR(String);
  mString.AssignWithConversion(aSource);
}

inline void String::append(const char* aSource)
{
  mString.AppendWithConversion(aSource);
}

inline MBool String::isEqual(const char* aData) const
{
  return mString.EqualsWithConversion(aData);
}

inline nsString& String::getNSString()
{
  return mString;
}

inline const nsString& String::getConstNSString() const
{
  return mString;
}

#endif // txMozillaString_h__
