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

public class StringUtils {
  /**
   * Uppercase the characters in aString.
   *
   * @see: java.lang.Character
   */
  static public String UpperCase(String aString) {
    StringBuf buf = StringBuf.Alloc();
    buf.append(aString);
    buf.toUpperCase();
    aString = buf.toString();
    StringBuf.Recycle(buf);
    return aString;
  }

  /**
   * Lowercase the characters in aString.
   *
   * @see: java.lang.Character
   */
  static public String LowerCase(String aString) {
    StringBuf buf = StringBuf.Alloc();
    buf.append(aString);
    buf.toLowerCase();
    aString = buf.toString();
    StringBuf.Recycle(buf);
    return aString;
  }

  /**
   * Return true if the string buffer contains nothing but whitespace
   * as defined by Character.isWhitespace()
   *
   * @see: java.lang.Character
   */
  static public boolean IsWhitespace(String aString) {
    // XXX this should just loop right here instead of copying
    // the string because the loop will be inlined with a good
    // compiler
    StringBuf buf = StringBuf.Alloc();
    buf.append(aString);
    boolean rv = buf.isWhitespace();
    StringBuf.Recycle(buf);
    return rv;
  }

  /**
   * Translate an integer into a string that is at least aDigits
   * wide. Pad with zeros if necessary.
   */
  static public String ToHex(int i, int aDigits) {
    String rv = Integer.toString(i, 16);
    int len = rv.length();
    if (len < aDigits) {
      StringBuf buf = StringBuf.Alloc();
      len = aDigits - len;
      while (--len >= 0) {
        buf.append('0');
      }
      buf.append(rv);
      rv = buf.toString();
      StringBuf.Recycle(buf);
    }
    return rv;
  }

  /**
   * Compress the whitespace out of a string and return a new string
   */
  static public String CompressWhitespace(String aString, boolean aLeading) {
    StringBuf buf = StringBuf.Alloc();
    buf.append(aString);
    buf.compressWhitespace(aLeading);
    String rv = buf.toString();
    StringBuf.Recycle(buf);
    return rv;
  }

  /**
   * Quote a string using java source file rules. The result is a new
   * string with all the appropriate data quoted. The outer quotes that
   * would be required in a java source file are not provided by this
   * routine unless aProvideOuterQuotes is true.
   */
  static char[] hex;
  static {
    hex = new char[16];
    "0123456789abcdef".getChars(0, 16, hex, 0);
  }
  static public String JavaQuote(String aString, boolean aProvideOuterQuotes)
  {
    StringBuf buf = StringBuf.Alloc();
    if (aProvideOuterQuotes) {
      buf.append('"');
    }
    for (int i = 0, n = aString.length(); i < n; i++) {
      char ch = aString.charAt(i);
      switch (ch) {
      case '\n':
        buf.append("\\n");
        break;
      case '\r':
        buf.append("\\r");
        break;
      case '\t':
        buf.append("\\t");
        break;
      case '"':
        buf.append("\\\"");
        break;
      default:
        if ((ch < 32) || (ch >= 127)) {
          buf.append("\\u");
          buf.append(hex[(ch >> 12) & 0xf]);
          buf.append(hex[(ch >> 8) & 0xf]);
          buf.append(hex[(ch >> 4) & 0xf]);
          buf.append(hex[ch & 0xf]);
        } else {
          buf.append(ch);
        }
      }
    }
    if (aProvideOuterQuotes) {
      buf.append('"');
    }
    String rv = buf.toString();
    StringBuf.Recycle(buf);
    return rv;
  }
}
