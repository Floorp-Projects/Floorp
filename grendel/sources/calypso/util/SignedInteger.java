/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package calypso.util;

/**
 * Utility class that represents the parse state from parsing a
 * signed integer. The sign of the integer is maintained so that "10"
 * can be distinguished from "+10". In addition, we can optionally
 * handle numbers expressed as percentages.
 *
 * @author Kipp E.B. Hickman
 * @see Integer.parseInt
 */
public class SignedInteger {
  static public final int PLUS = 1;
  static public final int MINUS = -1;
  static public final int NONE = 0;

  protected int fValue;
  protected int fSign;
  protected boolean fPct;

  /**
    The integer part works (almost) the same  as atoi():
      <ul>
      <li>Skip leading white spaces.
      <li>Non-digit char terminates convertion.
      <li>If got a valid integer, don't throw NumberFormatException.
      <li>Throw exception if first nonspace char is not digit(atoi returns 0).
    Example: " +4z" works as 4.
  */
  // Nav 4.0 #define XP_ATOI(ss) atoi(ss)
  // Typical usage: ns\lib\layout\layutil.c  lo_ValueOrPercent();
  public SignedInteger(String aString, boolean aAllowPct)
       throws NumberFormatException
  {
    if( aString == null)
      throw new NumberFormatException("1");   // Null string

    int value = 0;
    int sign = NONE;
    int pp;

    int len = aString.length();
    if (len == 0)
      throw new NumberFormatException("2");   // Empty string

    //eat leading whitespace
    for(pp=0; pp<len ; pp++) {
      if( ! java.lang.Character.isWhitespace(aString.charAt(pp) ) )
        break;
    }

    if( pp >= len )
      throw new NumberFormatException("3");   // Blank string

    if (pp > 0 ) {
      aString = aString.substring( pp );
      len -= pp;
    }

    if (aString.charAt(0) == '+') {
      sign = PLUS;
      aString = aString.substring(1);
      len--;
    } else if (aString.charAt(0) == '-') {
      sign = MINUS;
      aString = aString.substring(1);
      len--;
    }

    // no white space allowed after sign.
    if( ! java.lang.Character.isDigit(aString.charAt(0)) )
      throw new NumberFormatException("4");     // start with none digit

    // cut off nondigit at end, otherwise Integer.parseInt() will throw exception.
    for(pp=0;  pp < len ; pp++) {
      if( ! java.lang.Character.isDigit(aString.charAt(pp)) )
        break;
    }

    if( pp < len ) {
      // it has nondigit at end.
      if (aAllowPct && aString.charAt(pp) == '%' )
        fPct = true;
      // cut it off
      aString = aString.substring(0, pp );
    }

    value = Integer.parseInt(aString, 10);

    fValue = value;
    fSign = sign;
    if (fPct) {
      if ((fSign == MINUS) && (fValue != 0)) {
        // Negative percentages are not allowed
        throw new NumberFormatException("5");   // negative percentage
      } else if (fValue > 100) {
        // Percentages larger than 100 are quietly clamped
        fValue = 100;
      }
    }
  }

  public SignedInteger(int aValue, int aSign) {
    Assert.Assertion(aValue >= 0);
    Assert.Assertion((aSign == PLUS) || (aSign == MINUS) || (aSign == NONE));
    fValue = aValue;
    fSign = aSign;
  }

  public int intValue() {
    return fValue;
  }

  public boolean isPct() {
    return fPct;
  }

  /**
   * Compute a value based on "aBaseValue". Apply the sign of this
   * SignedInteger to determine the value to return.
   */
  public int computeValue(int aBaseValue) {
    if (fPct) {
      return aBaseValue * fValue;
    }
    switch (fSign) {
    case PLUS:
      return aBaseValue + fValue;
    case MINUS:
      return aBaseValue - fValue;
    }
    return fValue;
  }

  /**
   * Return the sign of the value.
   */
  public int getSign() {
    return fSign;
  }

  public String toString() {
    if (fPct) {
      return Integer.toString(fValue, 10) + "%";
    }
    switch (fSign) {
    case PLUS:
      return "+" + Integer.toString(fValue, 10);
    case MINUS:
      return "-" + Integer.toString(fValue, 10);
    }
    return Integer.toString(fValue, 10);
  }
  // XXX equals
  // XXX hashcode
}
