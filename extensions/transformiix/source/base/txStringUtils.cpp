/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Axel Hecht <axel@pike.org>
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

#include "txStringUtils.h"

int
txCaseInsensitiveStringComparator::operator()(const char_type* lhs,
                                              const char_type* rhs,
                                              PRUint32 aLength ) const
{
  PRUnichar thisChar, otherChar;
  PRUint32 compLoop = 0;
  while (compLoop < aLength) {
    thisChar = lhs[compLoop];
    if ((thisChar >= 'A') && (thisChar <= 'Z')) {
      thisChar += 32;
    }
    otherChar = rhs[compLoop];
    if ((otherChar >= 'A') && (otherChar <= 'Z')) {
      otherChar += 32;
    }
    if (thisChar != otherChar) {
      return thisChar - otherChar;
    }
    ++compLoop;
  }
  return 0;

}

int
txCaseInsensitiveStringComparator::operator()(char_type lhs,
                                              char_type rhs) const
{
  if (lhs >= 'A' && lhs <= 'Z') {
    lhs += 32;
  }
  if (rhs >= 'A' && rhs <= 'Z') {
    rhs += 32;
  }
  return lhs - rhs;
} 

/**
 * A character sink for case conversion.
 */
class ConvertToLowerCase
{
public:
  typedef PRUnichar value_type;

  PRUint32 write( const PRUnichar* aSource, PRUint32 aSourceLength)
  {
    PRUnichar* cp = NS_CONST_CAST(PRUnichar*, aSource);
    const PRUnichar* end = aSource + aSourceLength;
    while (cp != end) {
      PRUnichar ch = *cp;
      if ((ch >= 'A') && (ch <= 'Z'))
        *cp = ch + ('a' - 'A');
      ++cp;
    }
    return aSourceLength;
  }
};

void TX_ToLowerCase(nsAString& aString)
{
  nsAString::iterator fromBegin, fromEnd;
  ConvertToLowerCase converter;
  copy_string(aString.BeginWriting(fromBegin), aString.EndWriting(fromEnd),
              converter);
}

/**
 * A character sink for copying with case conversion.
 */
class CopyToLowerCase
{
public:
  typedef PRUnichar value_type;

  CopyToLowerCase(nsAString::iterator& aDestIter) : mIter(aDestIter)
  {
  }

  PRUint32 write(const PRUnichar* aSource, PRUint32 aSourceLength)
  {
    PRUint32 len = PR_MIN(PRUint32(mIter.size_forward()), aSourceLength);
    PRUnichar* cp = mIter.get();
    const PRUnichar* end = aSource + len;
    while (aSource != end) {
      PRUnichar ch = *aSource;
      if ((ch >= 'A') && (ch <= 'Z'))
        *cp = ch + ('a' - 'A');
      else
        *cp = ch;
      ++aSource;
      ++cp;
    }
    mIter.advance(len);
    return len;
  }

protected:
  nsAString::iterator& mIter;
};

void TX_ToLowerCase(const nsAString& aSource, nsAString& aDest)
{
  nsAString::const_iterator fromBegin, fromEnd;
  nsAString::iterator toBegin;
  aDest.SetLength(aSource.Length());
  CopyToLowerCase converter(aDest.BeginWriting(toBegin));
  copy_string(aSource.BeginReading(fromBegin), aSource.EndReading(fromEnd),
              converter);
}
