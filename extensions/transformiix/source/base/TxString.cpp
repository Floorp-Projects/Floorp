/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Contributor(s):
 *
 * Tom Kneeland
 *    -- original author.
 *
 * Keith Visco <kvisco@ziplink.net>
 * Larry Fitzpatrick
 *
 */

#include "TxString.h"
#include <stdlib.h>
#include <string.h>

String::String(const UNICODE_CHAR* aSource, PRUint32 aLength)
{
  if (aLength) {
    mString = Substring(aSource, aSource + aLength);
  }
  else {
    mString = nsDependentString(aSource);
  }
}

int
txCaseInsensitiveStringComparator::operator()(const char_type* lhs,
                                              const char_type* rhs,
                                              PRUint32 aLength ) const
{
  UNICODE_CHAR thisChar, otherChar;
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

void String::toLowerCase()
{
  nsAFlatString::char_iterator c, e;
  mString.BeginWriting(c);
  mString.EndWriting(e);
  while (c != e) {
    if (*c >= 'A' && *c <= 'Z')
      *c += 32;
    ++c;
  }
}

void String::toUpperCase()
{
  nsAFlatString::char_iterator c, e;
  mString.BeginWriting(c);
  mString.EndWriting(e);
  while (c != e) {
    if (*c >= 'a' && *c <= 'z')
      *c -= 32;
    ++c;
  }
}

ostream& operator<<(ostream& aOutput, const String& aSource)
{
  aOutput << NS_LossyConvertUCS2toASCII(aSource.mString).get();
  return aOutput;
}
