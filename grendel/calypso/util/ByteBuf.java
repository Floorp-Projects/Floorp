/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 */

package calypso.util;

import java.util.*;

import java.io.ByteArrayInputStream;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.io.RandomAccessFile;

/**
 * This class is similar to java/lang/StringBuffer with the following changes:
 * <UL><LI>Stores bytes, not chars
 * <LI>None of the methods are synchronized
 * <LI>There is no sharing of the character array
 * <LI>Converting to a String requires a copy of the character data
 * <LI>Alloc and Recycle are provided to speed up allocation
 * <LI>A bunch of useful operations are provided (comparisons, etc.)
 * </UL>
 */
public final class ByteBuf {
  private byte value[];
  private int count;

//  static ByteBufRecycler gRecycler;

  /**
   * Constructs an empty String buffer, reusing one from the
   * recycler.
   */
  static synchronized public ByteBuf Alloc() {
      return new ByteBuf();
//    if (gRecycler == null) {
//      gRecycler = new ByteBufRecycler();
//    }
//    return gRecycler.alloc();
  }

  /**
   * Release a ByteBuf to the recycler. It is up to the caller
   * to ensure that buffer is no longer being used otherwise
   * unpredicatable program behavior will result.
   */
  static synchronized public void Recycle(ByteBuf aBuf) {
//    if (gRecycler == null) {
//      gRecycler = new ByteBufRecycler();
//    }
//    gRecycler.recycle(aBuf);
  }

  /**
   * Empty the recycler discarding any cached ByteBuf objects
   */
  static synchronized public void EmptyRecycler() {
//    if (null != gRecycler) {
//      gRecycler = null;
//    }
  }

  static void classFinalize() throws Throwable {
    // Poof! Now we are unloadable even though we have a static
    // variable.
  }

  /**
   * Constructs an empty String buffer.
   */
  public ByteBuf() {
    this(16);
  }

  /**
   * Constructs an empty byte buffer with the specified initial length.
   * @param length  the initial length
   */
  public ByteBuf(int length) {
    value = new byte[length];
  }

  /**
   * Constructs a byte buffer with the specified initial value.
   * @param str the initial value of the buffer
   */
  public ByteBuf(String str) {
    this(str.length() + 16);
    append(str);
  }

  public ByteBuf(byte bytes[], int offset, int length) {
    value = new byte[length];
    System.arraycopy(bytes, offset, value, 0, length);
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

      byte newValue[] = new byte[newCapacity];
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
    if (count < newLength) {
      ensureCapacity(newLength);
      for (; count < newLength; count++) {
        value[count] = (byte)'\0';
      }
    }
    count = newLength;
  }

  /**
   * Returns the byte at the specified index. An index ranges
   * from 0..length()-1.
   * @param index the index of the desired character
   * @exception StringIndexOutOfBoundsException If the index is invalid.
   */
  public byte byteAt(int index) {
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
  public void getBytes(int srcBegin, int srcEnd, byte dst[], int dstBegin) {
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
   * Changes the byte at the specified index to be ch.
   * @param index the index of the character
   * @param ch    the new character
   * @exception StringIndexOutOfBoundsException If the index is invalid.
   */
  public void setByteAt(int index, byte b) {
    if ((index < 0) || (index >= count)) {
      throw new StringIndexOutOfBoundsException(index);
    }
    value[index] = b;
  }

  /**
   * Appends an object to the end of this buffer.
   * @param obj the object to be appended
   * @return  the ByteBuf itself, NOT a new one.
   */
  public ByteBuf append(Object obj) {
    return append(String.valueOf(obj));
  }

  /**
   * Appends a String to the end of this buffer.  This just appends one byte
   * per char; it strips off the upper 8 bits.  If you want the localized
   * method of converting chars to bytes, use append(String.getBytes()).
   * @param str the String to be appended
   * @return  the ByteBuf itself, NOT a new one.
   */
  public ByteBuf append(String str) {
    if (str == null) {
      str = String.valueOf(str);
    }

    int len = str.length();
    ensureCapacity(count + len);
    for (int i=0 ; i<len ; i++) {
      value[count++] = (byte) str.charAt(i);
    }
    return this;
  }

  /**
   * Appends an array of bytes to the end of this buffer.
   * @param str the characters to be appended
   * @return  the ByteBuf itself, NOT a new one.
   */
  public ByteBuf append(byte str[]) {
    int len = str.length;
    ensureCapacity(count + len);
    System.arraycopy(str, 0, value, count, len);
    count += len;
    return this;
  }

  /**
   * Appends a part of an array of characters to the end of this buffer.
   * @param str the characters to be appended
   * @param offset  where to start
   * @param len the number of characters to add
   * @return  the ByteBuf itself, NOT a new one.
   */
  public ByteBuf append(byte str[], int offset, int len) {
    ensureCapacity(count + len);
    System.arraycopy(str, offset, value, count, len);
    count += len;
    return this;
  }

  public ByteBuf append(ByteBuf buf) {
    append(buf.toBytes(), 0, buf.length());
    return this;
  }

  /**
   * Appends a boolean to the end of this buffer.
   * @param b the boolean to be appended
   * @return  the ByteBuf itself, NOT a new one.
   */
  public ByteBuf append(boolean b) {
    return append(String.valueOf(b));
  }

  /**
   * Appends a byte to the end of this buffer.
   * @param ch  the character to be appended
   * @return  the ByteBuf itself, NOT a new one.
   */
  public ByteBuf append(byte b) {
    ensureCapacity(count + 1);
    value[count++] = b;
    return this;
  }

  /**
   * Appends an integer to the end of this buffer.
   * @param i the integer to be appended
   * @return  the ByteBuf itself, NOT a new one.
   */
  public ByteBuf append(int i) {
    return append(String.valueOf(i));
  }

  /**
   * Appends a long to the end of this buffer.
   * @param l the long to be appended
   * @return  the ByteBuf itself, NOT a new one.
   */
  public ByteBuf append(long l) {
    return append(String.valueOf(l));
  }

  /**
   * Appends a float to the end of this buffer.
   * @param f the float to be appended
   * @return  the ByteBuf itself, NOT a new one.
   */
  public ByteBuf append(float f) {
    return append(String.valueOf(f));
  }

  /**
   * Appends a double to the end of this buffer.
   * @param d the double to be appended
   * @return  the ByteBuf itself, NOT a new one.
   */
  public ByteBuf append(double d) {
    return append(String.valueOf(d));
  }

  /**
   * Inserts an object into the String buffer.
   * @param offset  the offset at which to insert
   * @param obj   the object to insert
   * @return    the ByteBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public ByteBuf insert(int offset, Object obj) {
    return insert(offset, String.valueOf(obj));
  }

  /**
   * Inserts a String into the String buffer.
   * @param offset  the offset at which to insert
   * @param str   the String to insert
   * @return    the ByteBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public ByteBuf insert(int offset, String str) {
    if ((offset < 0) || (offset > count)) {
      throw new StringIndexOutOfBoundsException();
    }
    int len = str.length();
    ensureCapacity(count + len);
    System.arraycopy(value, offset, value, offset + len, count - offset);
    for (int i=0 ; i<len ; i++) {
      value[offset++] = (byte) str.charAt(i);
    }
    count += len;
    return this;
  }

  /**
   * Inserts an array of bytes into the String buffer.
   * @param offset  the offset at which to insert
   * @param str   the characters to insert
   * @return    the ByteBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public ByteBuf insert(int offset, byte str[]) {
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
   * @return    the ByteBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public ByteBuf insert(int offset, boolean b) {
    return insert(offset, String.valueOf(b));
  }

  /**
   * Inserts a byte into the String buffer.
   * @param offset  the offset at which to insert
   * @param ch    the character to insert
   * @return    the ByteBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset invalid.
   */
  public ByteBuf insert(int offset, byte b) {
    ensureCapacity(count + 1);
    System.arraycopy(value, offset, value, offset + 1, count - offset);
    value[offset] = b;
    count += 1;
    return this;
  }

  /**
   * Inserts an integer into the String buffer.
   * @param offset  the offset at which to insert
   * @param i   the integer to insert
   * @return    the ByteBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public ByteBuf insert(int offset, int i) {
    return insert(offset, String.valueOf(i));
  }

  /**
   * Inserts a long into the String buffer.
   * @param offset  the offset at which to insert
   * @param l   the long to insert
   * @return    the ByteBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public ByteBuf insert(int offset, long l) {
    return insert(offset, String.valueOf(l));
  }

  /**
   * Inserts a float into the String buffer.
   * @param offset  the offset at which to insert
   * @param f   the float to insert
   * @return    the ByteBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public ByteBuf insert(int offset, float f) {
    return insert(offset, String.valueOf(f));
  }

  /**
   * Inserts a double into the String buffer.
   * @param offset  the offset at which to insert
   * @param d   the double to insert
   * @return    the ByteBuf itself, NOT a new one.
   * @exception StringIndexOutOfBoundsException If the offset is invalid.
   */
  public ByteBuf insert(int offset, double d) {
    return insert(offset, String.valueOf(d));
  }

  /**
   * Reverse the order of the characters in the String buffer.
   */
  public ByteBuf reverse() {
    int n = count - 1;
    for (int j = (n-1) >> 1; j >= 0; --j) {
      byte temp = value[j];
      value[j] = value[n - j];
      value[n - j] = temp;
    }
    return this;
  }


  /**
   * Converts to a String representing the data in the buffer.
   */
  public String toString() {
    return new String(value, 0, count);
  }

  public byte[] toBytes() {
    return value;
  }


  /**
   * Compares this ByteBuf to another ByteBuf.  Returns true if the
   * other ByteBuf is equal to this ByteBuf; that is, has the same length
   * and the same characters in the same sequence.  Upper case
   * characters are folded to lower case before they are compared.
   *
   * @param anotherString the String to compare this String against
   * @return true if the Strings are equal, ignoring case; false otherwise.
   */
  public boolean equalsIgnoreCase(ByteBuf anotherString) {
    if ((anotherString != null) && (anotherString.count == count)) {
      for (int i = 0; i < count; i++) {
        byte c1 = value[i];
        byte c2 = anotherString.value[i];
        if (c1 != c2) {
          // If characters don't match but case may be ignored,
          // try converting both characters to uppercase.
          // If the results match, then the comparison scan should
          // continue.
          char u1 = Character.toUpperCase((char) c1);
          char u2 = Character.toUpperCase((char) c2);
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
      if (aObject instanceof ByteBuf) {
        return equals((ByteBuf) aObject);
      } else if (aObject instanceof String) {
        return equals((String) aObject);
      }
    }
    return false;
  }

  /**
   * Compares this ByteBuf to another ByteBuf.  Returns true if the
   * other ByteBuf is equal to this ByteBuf; that is, has the same length
   * and the same characters in the same sequence.
   *
   * @param anotherString the String to compare this String against
   * @return true if the Strings are equal, ignoring case; false otherwise.
   */
  public boolean equals(ByteBuf anotherString) {
    if ((anotherString != null) && (anotherString.count == count)) {
      for (int i = 0; i < count; i++) {
        byte c1 = value[i];
        byte c2 = anotherString.value[i];
        if (c1 != c2) {
          return false;
        }
      }
      return true;
    }
    return false;
  }

  /**
   * Compares this ByteBuf to another String.  Returns true if the
   * other String is equal to this ByteBuf; that is, has the same length
   * and the same characters in the same sequence.  (No localization is done;
   * if the string doesn't contain 8-bit chars, it won't be equal to this
   * ByteBuf.)
   *
   * @param anotherString the String to compare this String against
   * @return true if the Strings are equal, ignoring case; false otherwise.
   */
  public boolean equals(String anotherString) {
    if ((anotherString != null) && (anotherString.length() == count)) {
      for (int i = 0; i < count; i++) {
        byte c1 = value[i];
        byte c2 = (byte) anotherString.charAt(i);
        if (c1 != c2) {
          return false;
        }
      }
      return true;
    }
    return false;
  }

  /**
    * Tests if two byte regions are equal.
    * <p>
    * If <code>toffset</code> or <code>ooffset</code> is negative, or
    * if <code>toffset</code>+<code>length</code> is greater than the
    * length of this ByteBuf, or if
    * <code>ooffset</code>+<code>length</code> is greater than the
    * length of the argument, then this method returns
    * <code>false</code>.
    *
    * @param   toffset   the starting offset of the subregion in this ByteBuf.
    * @param   other     the other bytes.
    * @param   ooffset   the starting offset of the subregion in the argument.
    * @param   len       the number of bytes to compare.
    * @return  <code>true</code> if the specified subregion of this ByteBuf
    *          exactly matches the specified subregion of the argument;
    *          <code>false</code> otherwise.
    */
  public boolean regionMatches(int toffset, byte other[],
                               int ooffset, int len) {
    return regionMatches(false, toffset, other, ooffset, len);
  }


  /**
    * Tests if two byte regions are equal.
    * <p>
    * If <code>toffset</code> or <code>ooffset</code> is negative, or
    * if <code>toffset</code>+<code>length</code> is greater than the
    * length of this ByteBuf, or if
    * <code>ooffset</code>+<code>length</code> is greater than the
    * length of the argument, then this method returns
    * <code>false</code>.
    *
    * @param   ignoreCase   if <code>true</code>, ignore case when comparing
    *                       bytes (treating them as characters).
    * @param   toffset      the starting offset of the subregion in this
    *                       ByteBuf.
    * @param   other        the other bytes.
    * @param   ooffset      the starting offset of the subregion in the
    *                       argument.
    * @param   len          the number of bytes to compare.
    * @return  <code>true</code> if the specified subregion of this ByteBuf
    *          matches the specified subregion of the argument;
    *          <code>false</code> otherwise. Whether the matching is exact
    *          or case insensitive depends on the <code>ignoreCase</code>
    *          argument.
    */
  public boolean regionMatches(boolean ignoreCase,
                               int toffset,
                               byte other[],
                               int ooffset,
                               int len) {
    /* Lifted (and changed from char to byte) from java.lang.String. */
    byte ta[] = value;
    int to = toffset;
    int tlim = count;
    byte pa[] = other;
    int po = ooffset;
    // Note: toffset, ooffset, or len might be near -1>>>1.
    if ((ooffset < 0) || (toffset < 0) || (toffset > count - len) ||
        (ooffset > other.length - len)) {
      return false;
    }
    while (len-- > 0) {
      byte c1 = ta[to++];
      byte c2 = pa[po++];
      if (c1 == c2)
        continue;
      if (ignoreCase) {
        // If characters don't match but case may be ignored,
        // try converting both characters to uppercase.
        // If the results match, then the comparison scan should
        // continue.
        char u1 = Character.toUpperCase((char) c1);
        char u2 = Character.toUpperCase((char) c2);
        if (u1 == u2)
          continue;
        // Unfortunately, conversion to uppercase does not work properly
        // for the Georgian alphabet, which has strange rules about case
        // conversion.  So we need to make one last check before
        // exiting.
        if (Character.toLowerCase(u1) == Character.toLowerCase(u2))
          continue;
      }
      return false;
    }
    return true;
  }


  public boolean regionMatches(int toffset, ByteBuf other,
                               int ooffset, int len) {
    return regionMatches(false, toffset, other.value, ooffset, len);
  }

  public boolean regionMatches(boolean ignoreCase,
                               int toffset, ByteBuf other,
                               int ooffset, int len) {
    return regionMatches(ignoreCase, toffset, other.value, ooffset, len);
  }


  /**
    * Tests if two byte regions are equal.
    * <p>
    * If <code>toffset</code> or <code>ooffset</code> is negative, or
    * if <code>toffset</code>+<code>length</code> is greater than the
    * length of this ByteBuf, or if
    * <code>ooffset</code>+<code>length</code> is greater than the
    * length of the argument, then this method returns
    * <code>false</code>.
    *
    * @param   toffset   the starting offset of the subregion in this ByteBuf.
    * @param   other     the other String.
    * @param   ooffset   the starting offset of the subregion in the argument.
    * @param   len       the number of bytes/characters to compare.
    * @return  <code>true</code> if the specified subregion of this ByteBuf
    *          exactly matches the specified subregion of the String argument;
    *          <code>false</code> otherwise.
    */
  public boolean regionMatches(int toffset, String other,
                               int ooffset, int len) {
    return regionMatches(false, toffset, other, ooffset, len);
  }


  /**
    * Tests if two byte regions are equal.
    * <p>
    * If <code>toffset</code> or <code>ooffset</code> is negative, or
    * if <code>toffset</code>+<code>length</code> is greater than the
    * length of this ByteBuf, or if
    * <code>ooffset</code>+<code>length</code> is greater than the
    * length of the argument, then this method returns
    * <code>false</code>.
    *
    * @param   ignoreCase   if <code>true</code>, ignore case when comparing
    *                       bytes (treating them as characters).
    * @param   toffset      the starting offset of the subregion in this
    *                       ByteBuf.
    * @param   other        the other String.
    * @param   ooffset      the starting offset of the subregion in the
    *                       String argument.
    * @param   len          the number of bytes to compare.
    * @return  <code>true</code> if the specified subregion of this ByteBuf
    *          matches the specified subregion of the String argument;
    *          <code>false</code> otherwise. Whether the matching is exact
    *          or case insensitive depends on the <code>ignoreCase</code>
    *          argument.
    */
  public boolean regionMatches(boolean ignoreCase,
                               int toffset,
                               String other,
                               int ooffset,
                               int len) {
    /* Lifted (and changed from char to byte) from java.lang.String. */
    byte ta[] = value;
    int to = toffset;
    int tlim = count;
    int po = ooffset;
    // Note: toffset, ooffset, or len might be near -1>>>1.
    if ((ooffset < 0) || (toffset < 0) || (toffset > count - len) ||
        (ooffset > other.length() - len)) {
      return false;
    }
    while (len-- > 0) {
      byte c1 = ta[to++];
      byte c2 = (byte) other.charAt(po++);
      if (c1 == c2)
        continue;
      if (ignoreCase) {
        // If characters don't match but case may be ignored,
        // try converting both characters to uppercase.
        // If the results match, then the comparison scan should
        // continue.
        char u1 = Character.toUpperCase((char) c1);
        char u2 = Character.toUpperCase((char) c2);
        if (u1 == u2)
          continue;
        // Unfortunately, conversion to uppercase does not work properly
        // for the Georgian alphabet, which has strange rules about case
        // conversion.  So we need to make one last check before
        // exiting.
        if (Character.toLowerCase(u1) == Character.toLowerCase(u2))
          continue;
      }
      return false;
    }
    return true;
  }


  public int indexOf(int ch) {
    return indexOf(ch, 0);
  }

  public int indexOf(int ch, int fromIndex) {
    int max = count;
    byte v[] = value;

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
   * Remove characters from the ByteBuf starting at fromIndex and up
   * to but not including toIndex. If toIndex is beyond the length of
   * the ByteBuf then it is automatically clamped to the end of the
   * ByteBuf. If fromIndex is out of range a StringIndexOutOfBoundsException
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

  public int toInteger() throws NumberFormatException {
      int result = 0;
      int sign = 1;
      int i = 0;
      if (count == 0) {
          throw new NumberFormatException();
      }
      while (i < count - 1 && (value[i] <= ' ')) {
          i++;
      }
      if (value[i] == '-' && i < count - 1) {
          sign = -1;
          i++;
      }
      do {
          byte b = value[i];
          if (b >= '0' || b <= '9') {
              result = (result * 10) + (b - '0');
          } else {
              throw new NumberFormatException();
          }
          i++;
      } while (i < count && value[i] > ' ');
      while (i < count && value[i] < ' ') {
          i++;
      }
      if (i < count) {
          throw new NumberFormatException();
      }
      return result * sign;
  }

  public ByteBuf trim() {
    int i=0;
    while (i < count && value[i] <= ' ') i++;
    if (i > 0) {
      count -= i;
      System.arraycopy(value, i, value, 0, count);
    }
    while (count > 0 && value[count-1] <= ' ') count--;
    return this;
  }


  /** Write to the given output stream a detailed description of each
    byte in this buffer. */
  public void fullDump(PrintStream out) {
    fullDump(out, 0, count);
  }

  /** Write to the given output stream a detailed description of the given
    bytes in this buffer. */
  public void fullDump(PrintStream out, int start, int end) {
    for (int i=start ; i<end ; i++) {
      out.write(value[i]);
      out.print("(" + ((int) value[i]) + ") ");
    }
    out.println("");
  }

  /** Invokes InputStream.read(), appending the bytes to this Bytebuf.
      @return the number of bytes read, or -1 if eof.
    */
  public int read(InputStream file, int max_bytes)
    throws IOException {
    ensureCapacity(count + max_bytes);
    int i = file.read(value, count, max_bytes);
    if (i > 0) count += i;
    return i;
  }

  /** Invokes RandomAccessFile.read(), appending the bytes to this Bytebuf.
      (A RandomAccessFile is not an InputStream, because Java is a crock.)
      @return the number of bytes read, or -1 if eof.
    */
  public int read(RandomAccessFile file, int max_bytes)
    throws IOException {
    ensureCapacity(count + max_bytes);
    int i = file.read(value, count, max_bytes);
    if (i > 0) count += i;
    return i;
  }

  /** Writes the contents to the given output stream. */
  public void write(OutputStream out) throws IOException {
    out.write(value, 0, count);
  }

  /** Writes the contents to the given RandomAccessFile. */
  public void write(RandomAccessFile out) throws IOException {
    out.write(value, 0, count);
  }

  /** Creates a new InputStream whose content is this ByteBuf.  Note that
   changing the ByteBuf can affect the stream; the data is <i>not</i>
   copied. */
  public InputStream makeInputStream() {
    return new ByteArrayInputStream(value, 0, count);
  }
}
