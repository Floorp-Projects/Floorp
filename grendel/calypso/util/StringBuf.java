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

import java.util.*;

/**
 * This class is similar to java/lang/StringBuffer with the following changes:
 * <UL><LI>None of the methods are synchronized
 * <LI>There is no sharing of the character array
 * <LI>Converting to a String requires a copy of the character data
 * <LI>Alloc and Recycle are provided to speed up allocation
 * <LI>A bunch of useful operations are provided (comparisons, etc.)
 * </UL>
 */
public final class StringBuf {
  private char value[];
  private int count;

  static StringBufRecycler gRecycler;

  /**
   * Constructs an empty String buffer, reusing one from the
   * recycler.
   */
  static synchronized public StringBuf Alloc() {
    if (gRecycler == null) {
      gRecycler = new StringBufRecycler();
    }
    return gRecycler.alloc();
  }

  /**
   * Release a StringBuf to the recycler. It is up to the caller
   * to ensure that buffer is no longer being used otherwise
   * unpredicatable program behavior will result.
   */
  static synchronized public void Recycle(StringBuf aBuf) {
    if (gRecycler == null) {
      gRecycler = new StringBufRecycler();
    }
    gRecycler.recycle(aBuf);
  }

  /**
   * Empty the recycler discarding any cached StringBuf objects
   */
  static synchronized public void EmptyRecycler() {
    if (null != gRecycler) {
      gRecycler = null;
    }
  }

  static void classFinalize() throws Throwable {
    // Poof! Now we are unloadable even though we have a static
    // variable.
  }

  /**
   * Constructs an empty String buffer.
   */
  public StringBuf() {
    this(16);
  }

  /**
   * Constructs an empty String buffer with the specified initial length.
   * @param length  the initial length
   */
  public StringBuf(int length) {
    value = new char[length];
  }

  /**
   * Constructs a String buffer with the specified initial value.
   * @param str the initial value of the buffer
   */
  public StringBuf(String str) {
    this(str.length() + 16);
    append(str);
  }

  public StringBuf(char chars[], int offset, int length) {
    value = new char[length];
    System.arraycopy(chars, offset, value, 0, length);
    count = length;
  }

  /**
   * Returns the length (character count) of the buffer.
   */
  public int length() {
    return count;
  }

  /**
   * Returns the current capacity of the String buffer. The capacity
   * is the amount of storage available for newly inserted
   * characters; beyond which an allocation will occur.
   */
  public int capacity() {
    return value.length;
  }

  /**
   * Ensures that the capacity of the buffer is at least equal to the
   * specified minimum.
   * @param minimumCapacity the minimum desired capacity
   */
  public void ensureCapacity(int minimumCapacity) {
    int maxCapacity = value.length;

    if (minimumCapacity > maxCapacity) {
      int newCapacity = (maxCapacity + 1) * 2;
      if (minimumCapacity > newCapacity) {
        newCapacity = minimumCapacity;
      }

      char newValue[] = new char[newCapacity];
      System.arraycopy(value, 0, newValue, 0, count);
      value = newValue;
    }
  }

  /**
   * Sets the length of the String. If the length is reduced, characters
   * are lost. If the length is extended, the values of the new characters
   * are set to 0.
   * @param newLength the new length of the buffer
   * @exception StringIndexOutOfBoundsException  If the length is invalid.
   */
  public void setLength(int newLength) {
    if (newLength < 0) {
      throw new StringIndexOutOfBoundsException(newLength);
    }
    ensureCapacity(newLength);

    if (count < newLength) {
      for (; count < newLength; count++) {
        value[count] = '\0';
      }
    }
    count = newLength;
  }

  /**
   * Clear the content for reuse. Farster than setLength(0).
   */
  public void setEmpty() {
    count = 0;
  }

  /**
   * Returns the character at the specified index. An index ranges
   * from 0..length()-1.
   * @param index the index of the desired character
   * @exception StringIndexOutOfBoundsException If the index is invalid.
   */
  public char charAt(int index) {
    if ((index < 0) || (index >= count)) {
      throw new StringIndexOutOfBoundsException(index);
    }
    return value[index];
  }

  /**
   * Copies the characters of the specified substring (determined by
   * srcBegin and srcEnd) into the character array, starting at the
   * array's dstBegin location. Both srcBegin and srcEnd must be legal
   * indexes into the buffer.
   * @param srcBegin  begin copy at this offset in the String
   * @param srcEnd  stop copying at this offset in the String
   * @param dst   the array to copy the data into
   * @param dstBegin  offset into dst
   * @exception StringIndexOutOfBoundsException If there is an invalid index into the buffer.
   */
  public void getChars(int srcBegin, int srcEnd, char dst[], int dstBegin) {
    if ((srcBegin < 0) || (srcBegin >= count)) {
      throw new StringIndexOutOfBoundsException(srcBegin);
    }
    if ((srcEnd < 0) || (srcEnd > count)) {
      throw new StringIndexOutOfBoundsException(srcEnd);
    }
    if (srcBegin < srcEnd) {
      System.arraycopy(value, srcBegin, dst, dstBegin, srcEnd - srcBegin);
    }
  }

  /**
   * Changes the character at the specified index to be ch.
   * @param index the index of the character
   * @param ch    the new character
   * @exception StringIndexOutOfBoundsException If the index is invalid.
   */
  public void setCharAt(int index, char ch) {
    if ((index < 0) || (index >= count)) {
      throw new StringIndexOutOfBoundsException(index);
    }
    value[index] = ch;
  }

  /**
   * Appends an object to the end of this buffer.
   * @param obj the object to be appended
   * @return  the StringBuf itself, NOT a new one.
   */
  public StringBuf append(Object obj) {
    return append(String.valueOf(obj));
  }

  /**
   * Appends a String to the end of this buffer.
   * @param str the String to be appended
   * @return  the StringBuf itself, NOT a new one.
   */
  public StringBuf append(String str) {
    if (str == null) {
      str = String.valueOf(str);
    }

    int len = str.length();
    ensureCapacity(count + len);
    str.getChars(0, len, value, count);
    count += len;
    return this;
  }

  /**
   * Appends an array of characters to the end of this buffer.
   * @param str the characters to be appended
   * @return  the StringBuf itself, NOT a new one.
   */
  public StringBuf append(char str[]) {
    int len = str.length;
    ensureCapacity(count + len);
    System.arraycopy(str, 0, value, count, len);
    count += len;
    return this;
  }

  public StringBuf assign(char str[]) {
    count = 0;
    return( append(str) );
  }

  public StringBuf assign(String str) {
    count = 0;
    return( append(str) );
  }

  public StringBuf assign(char str[], int offset, int len) {
    count = 0;
    return(append( str, offset, len) );
  }

  /**
   * Appends a part of an array of characters to the end of this buffer.
   * @param str the characters to be appended
   * @param offset  where to start
   * @param len the number of characters to add
   * @return  the StringBuf itself, NOT a new one.
   */
  public StringBuf append(char str[], int offset, int len) {
    ensureCapacity(count + len);
    System.arraycopy(str, offset, value, count, len);
    count += len;
    return this;
  }

  /**
   * Appends a boolean to the end of this buffer.
   * @param b the boolean to be appended
   * @return  the StringBuf itself, NOT a new one.
   */
  public StringBuf append(boolean b) {
    return append(String.valueOf(b));
  }

  /**
   * Appends a character to the end of this buffer.
   * @param ch  the character to be appended
   * @return  the StringBuf itself, NOT a new one.
   */
  public StringBuf append(char c) {
    ensureCapacity(count + 1);
    value[count++] = c;
    return this;
  }

  /**
   * Appends an integer to the end of this buffer.
   * @param i the integer to be appended
   * @return  the StringBuf itself, NOT a new one.
   */
  public StringBuf append(int i) {
    return append(String.valueOf(i));
  }

  /**
   * Appends a long to the end of this buffer.
   * @param l the long to be appended
   * @return  the StringBuf itself, NOT a new one.
   */
  public StringBuf append(long l) {
    return append(String.valueOf(l));
  }

  /**
   * Appends a float to the end of this buffer.
   * @param f the float to be appended
   * @return  the StringBuf itself, NOT a new one.
   */
  public StringBuf append(float f) {
    return append(String.valueOf(f));
  }

  /**
   * Appends a double to the end of this buffer.
   * @param d the double to be appended
   * @return  the StringBuf itself, NOT a new one.
   */
  public StringBuf append(double d) {
    return append(String.valueOf(d));
  }

  /**
   * Inserts an object into the String buffer.
   * @param offset  the offset at which to insert
   * @param obj   the object to insert
   * @return    the StringBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public StringBuf insert(int offset, Object obj) {
    return insert(offset, String.valueOf(obj));
  }

  /**
   * Inserts a String into the String buffer.
   * @param offset  the offset at which to insert
   * @param str   the String to insert
   * @return    the StringBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public StringBuf insert(int offset, String str) {
    if ((offset < 0) || (offset > count)) {
      throw new StringIndexOutOfBoundsException();
    }
    int len = str.length();
    ensureCapacity(count + len);
    System.arraycopy(value, offset, value, offset + len, count - offset);
    str.getChars(0, len, value, offset);
    count += len;
    return this;
  }

  /**
   * Inserts an array of characters into the String buffer.
   * @param offset  the offset at which to insert
   * @param str   the characters to insert
   * @return    the StringBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public StringBuf insert(int offset, char str[]) {
    if ((offset < 0) || (offset > count)) {
      throw new StringIndexOutOfBoundsException();
    }
    int len = str.length;
    ensureCapacity(count + len);
    System.arraycopy(value, offset, value, offset + len, count - offset);
    System.arraycopy(str, 0, value, offset, len);
    count += len;
    return this;
  }

  /**
   * Inserts a boolean into the String buffer.
   * @param offset  the offset at which to insert
   * @param b   the boolean to insert
   * @return    the StringBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public StringBuf insert(int offset, boolean b) {
    return insert(offset, String.valueOf(b));
  }

  /**
   * Inserts a character into the String buffer.
   * @param offset  the offset at which to insert
   * @param ch    the character to insert
   * @return    the StringBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset invalid.
   */
  public StringBuf insert(int offset, char c) {
    ensureCapacity(count + 1);
    System.arraycopy(value, offset, value, offset + 1, count - offset);
    value[offset] = c;
    count += 1;
    return this;
  }

  /**
   * Inserts an integer into the String buffer.
   * @param offset  the offset at which to insert
   * @param i   the integer to insert
   * @return    the StringBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public StringBuf insert(int offset, int i) {
    return insert(offset, String.valueOf(i));
  }

  /**
   * Inserts a long into the String buffer.
   * @param offset  the offset at which to insert
   * @param l   the long to insert
   * @return    the StringBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public StringBuf insert(int offset, long l) {
    return insert(offset, String.valueOf(l));
  }

  /**
   * Inserts a float into the String buffer.
   * @param offset  the offset at which to insert
   * @param f   the float to insert
   * @return    the StringBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public StringBuf insert(int offset, float f) {
    return insert(offset, String.valueOf(f));
  }

  /**
   * Inserts a double into the String buffer.
   * @param offset  the offset at which to insert
   * @param d   the double to insert
   * @return    the StringBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public StringBuf insert(int offset, double d) {
    return insert(offset, String.valueOf(d));
  }

  /**
   * Reverse the order of the characters in the String buffer.
   */
  public StringBuf reverse() {
    int n = count - 1;
    for (int j = (n-1) >> 1; j >= 0; --j) {
      char temp = value[j];
      value[j] = value[n - j];
      value[n - j] = temp;
    }
    return this;
  }

  public void toUpperCase() {
    int n = count;
    for (int i = 0; i < n; i++) {
      value[i] = Character.toUpperCase(value[i]);
    }
  }

  public void toLowerCase() {
    int n = count;
    for (int i = 0; i < n; i++) {
      value[i] = Character.toLowerCase(value[i]);
    }
  }

  /**
   * Converts to a String representing the data in the buffer.
   */
  public String toString() {
    return new String(value, 0, count);
  }

  public char[] toChars() {
    return value;
  }

  /**
   * Compress the whitespace present in the buffer.
   * XXX I suppose there are i18n issues with this after all a space is
   * probably not a space...
   */
  public void compressWhitespace(boolean aStripLeadingWhitespace) {
    int src = 0;
    int dst = 0;
    int srcEnd = count;
    while (src < srcEnd) {
      char ch = value[src];
      if (!Character.isWhitespace(ch)) {
        break;
      }
      if (!aStripLeadingWhitespace) {
        dst++;
      }
      src++;
    }

    // Squish down any intermediate spaces
    while (src < srcEnd) {
      char ch = value[src++];
      value[dst++] = ch;
      if (!Character.isWhitespace(ch)) {
        continue;
      }
      while (src < srcEnd) {
        ch = value[src];
        if (!Character.isWhitespace(ch)) {
          break;
        }
        src++;
      }
    }
    count = dst;

    if (count != 0) {
      // Strip out a trailing space if any. The above loop will leave
      // a single space at the end of the value if the value has any
      // trailing whitespace.
      if (Character.isWhitespace(value[count-1])) {
        count--;
      }
    }
  }

  /**
   * Return true if the string buffer contains nothing but whitespace
   * as defined by Character.isWhitespace()
   */
  public boolean isWhitespace() {
    int n = count;
    for (int i = 0; i < n; i++) {
      if (!Character.isWhitespace(value[i])) return false;
    }
    return true;
  }

  /**
   * Compares this StringBuf to another String.  Returns true if the
   * String is equal to this StringBuf; that is, has the same length
   * and the same characters in the same sequence.  Upper case
   * characters are folded to lower case before they are compared.
   *
   * @param anotherString the String to compare this String against
   * @return true if the Strings are equal, ignoring case; false otherwise.
   */
  public boolean equalsIgnoreCase(String anotherString) {
    return( equalsIgnoreCase( 0, count, anotherString) );
  }

  public boolean equalsIgnoreCase(int aStart, int aLength, String anotherString) {
    if( anotherString==null ||
        aLength!=anotherString.length() ||
        aStart<0 ||
        (aStart + aLength) > count )
      return(false);

    for (int i = 0; i < aLength; i++) {
      char c1 = value[i+aStart];
      char c2 = anotherString.charAt(i);
      if (c1 != c2) {
        // If characters don't match but case may be ignored,
        // try converting both characters to uppercase.
        // If the results match, then the comparison scan should
        // continue.
        char u1 = Character.toUpperCase(c1);
        char u2 = Character.toUpperCase(c2);
        if (u1 == u2)
          continue;

        // Unfortunately, conversion to uppercase does not work properly
        // for the Georgian alphabet, which has strange rules about case
        // conversion.  So we need to make one last check before
        // exiting.
        if (Character.toLowerCase(u1) == Character.toLowerCase(u2))
          continue;
        return false;
      }
    }
    return true;
  }

  /**
   * Compares this StringBuf to another StringBuf.  Returns true if the
   * other StringBuf is equal to this StringBuf; that is, has the same length
   * and the same characters in the same sequence.  Upper case
   * characters are folded to lower case before they are compared.
   *
   * @param anotherString the String to compare this String against
   * @return true if the Strings are equal, ignoring case; false otherwise.
   */
  public boolean equalsIgnoreCase(StringBuf anotherString) {
    if ((anotherString != null) && (anotherString.count == count)) {
      for (int i = 0; i < count; i++) {
        char c1 = value[i];
        char c2 = anotherString.value[i];
        if (c1 != c2) {
          // If characters don't match but case may be ignored,
          // try converting both characters to uppercase.
          // If the results match, then the comparison scan should
          // continue.
          char u1 = Character.toUpperCase(c1);
          char u2 = Character.toUpperCase(c2);
          if (u1 == u2)
            continue;

          // Unfortunately, conversion to uppercase does not work properly
          // for the Georgian alphabet, which has strange rules about case
          // conversion.  So we need to make one last check before
          // exiting.
          if (Character.toLowerCase(u1) == Character.toLowerCase(u2))
            continue;
          return false;
        }
      }
      return true;
    }
    return false;
  }

  public boolean equals(Object aObject) {
    if (aObject != null) {
      if (aObject instanceof StringBuf) {
        return equals((StringBuf) aObject);
      } else if (aObject instanceof String) {
        return equals((String) aObject);
      }
    }
    return false;
  }

  /**
   * Compares this StringBuf to another StringBuf.  Returns true if the
   * other StringBuf is equal to this StringBuf; that is, has the same length
   * and the same characters in the same sequence.
   *
   * @param anotherString the String to compare this String against
   * @return true if the Strings are equal, ignoring case; false otherwise.
   */
  public boolean equals(StringBuf anotherString) {
    if ((anotherString != null) && (anotherString.count == count)) {
      for (int i = 0; i < count; i++) {
        char c1 = value[i];
        char c2 = anotherString.value[i];
        if (c1 != c2) {
          return false;
        }
      }
      return true;
    }
    return false;
  }

  /**
   * Compares this StringBuf to another String.  Returns true if the
   * other String is equal to this StringBuf; that is, has the same length
   * and the same characters in the same sequence.
   *
   * @param anotherString the String to compare this String against
   * @return true if the Strings are equal, ignoring case; false otherwise.
   */
  public boolean equals(String anotherString) {
    if ((anotherString != null) && (anotherString.length() == count)) {
      for (int i = 0; i < count; i++) {
        char c1 = value[i];
        char c2 = anotherString.charAt(i);
        if (c1 != c2) {
          return false;
        }
      }
      return true;
    }
    return false;
  }

  public int indexOf(int ch) {
    return indexOf(ch, 0);
  }

  public int indexOf(int ch, int fromIndex) {
    int max = count;
    char v[] = value;

    if (fromIndex < 0) {
      fromIndex = 0;
    } else if (fromIndex >= count) {
      // Note: fromIndex might be near -1>>>1.
      return -1;
    }
    for (int i = fromIndex ; i < max ; i++) {
      if (v[i] == ch) {
        return i;
      }
    }
    return -1;
  }

  public void remove(int fromIndex) {
    remove(fromIndex, count);
  }

  /**
   * Remove characters from the StringBuf starting at fromIndex and up
   * to but not including toIndex. If toIndex is beyond the length of
   * the StringBuf then it is automatically clamped to the end of the
   * StringBuf. If fromIndex is out of range a StringIndexOutOfBoundsException
   * is thrown.
   */
  public void remove(int fromIndex, int toIndex) {
    if ((fromIndex >= toIndex) || (fromIndex >= count)) {
      throw new StringIndexOutOfBoundsException(fromIndex);
    }
    if (toIndex > count) toIndex = count;
    if (toIndex == count) {
      count = fromIndex;
      return;
    }
    System.arraycopy(value, toIndex, value, fromIndex, count - toIndex);
    count -= toIndex - fromIndex;
  }
}
