/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap.util;

import java.util.*;

import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.io.RandomAccessFile;
import java.io.Serializable;

/**
 * This class is similar to the <CODE>java.lang.StringBuffer</CODE>
 * class.  Instead of storing a string, an object of this class
 * stores an array of bytes.  (This is referred to as a "byte buffer".)
 * <P>
 *
 * This class also differs from <CODE>StringBuffer</CODE> in the
 * following ways:
 * <UL>
 * <LI>None of the methods are synchronized. You cannot share a
 * byte buffer among multiple threads.
 * <LI>Converting to a String requires a copy of the character data.
 * <LI>In order to speed up memory allocation, <CODE>Alloc</CODE>
 * and <CODE>Recycle</CODE> methods are provided.  You can "recycle"
 * any <CODE>ByteBuf</CODE> objects you no longer need by using the
 * <CODE>Recycle</CODE> method.  Calling the <CODE>Alloc</CODE>
 * method will reuse objects that have been "recycled."  To
 * To clear out the cache of these "recycled" objects, use
 * the <CODE>EmptyRecycler</CODE> method.
 * <LI>Additional "helper" methods are provided (for example,
 * functions for comparing data).
 * </UL>
 */
public final class ByteBuf implements Serializable {

    static final long serialVersionUID = -786393456618166871L;
    private byte value[];
    private int count;

    /**
     * Constructs an empty byte buffer with the default length of 16.
     */
    public ByteBuf() {
        this(16);
    }

    /**
     * Constructs an empty byte buffer with the specified initial length.
     * @param length initial number of bytes that this buffer should hold
     */
    public ByteBuf(int length) {
        value = new byte[length];
    }

    /**
     * Constructs a byte buffer with the specified initial value.
     * The new byte buffer is 16 bytes longer than the initial value.
     * @param str initial value that this buffer should hold
     */
    public ByteBuf(String str) {
        this(str.length() + 16);
        append(str);
    }

    /**
     * Constructs a byte buffer with the specified length.
     * An initial value is stored in this buffer, starting
     * at the specified offset.
     * @param bytes array of bytes to initially store in the buffer
     * @param offset index where you want the initial value to start in the array
     * @param length length of the buffer to allocate
     */
    public ByteBuf(byte bytes[], int offset, int length) {
        value = new byte[length];
        System.arraycopy(bytes, offset, value, 0, length);
        count = length;
    }

    /**
     * Returns the length (character count) of the byte buffer.
     */
    public int length() {
        return count;
    }

    /**
     * Returns the current capacity of the byte buffer. The capacity
     * is the amount of storage available for newly inserted bytes.
     * If the capacity is exceeded, more space will be allocated.
     */
    public int capacity() {
        return value.length;
    }

    /**
     * Ensures that the capacity of the buffer is at least equal to the
     * specified minimum capacity.
     * @param minimumCapacity the minimum number of bytes that you want
     * the byte buffer to hold
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
     * Sets the length of the byte buffer. If you set a length that is
     * shorter than the current length, bytes at the end of the buffer
     * are lost. If you increase the length of the buffer, the values
     * of the new bytes in the buffer are set to 0.
     * @param newLength the new length of the buffer
     * @exception StringIndexOutOfBoundsException  You have specified
     * an invalid length.
     */
    public void setLength(int newLength) {
        if (newLength < 0) {
            throw new StringIndexOutOfBoundsException(newLength);
        }
        if (count < newLength) {
            ensureCapacity(newLength);
            for (; count < newLength; count++) {
                value[count] = 0;
            }
        }
        count = newLength;
    }

    /**
     * Returns the byte at the specified index.  The value of an index
     * can range from 0 to length - 1.
     * @param index index of the byte to find
     * @exception StringIndexOutOfBoundsException You have specified an
     * invalid index.
     */
    public byte byteAt(int index) {
        if ((index < 0) || (index >= count)) {
            throw new StringIndexOutOfBoundsException(index);
        }
        return value[index];
    }

    /**
     * Copies the bytes (from the section of the byte buffer from the index
     * <CODE>srcBegin</CODE> to the index <CODE>srcEnd - 1 </CODE>)
     * into the specified byte array.  The copied
     * bytes are inserted in the byte array at the index specified by
     * <CODE>dstBegin</CODE>. Both <CODE>srcBegin</CODE> and
     * <CODE>srcEnd</CODE> must be valid indexes in the buffer.
     * @param srcBegin index identifying the start of the section
     * of the byte buffer to copy
     * @param srcEnd index identifying the end of the section
     * of the byte buffer to copy. (Copy all bytes
     * before the byte identified by this index.)
     * @param dst the byte array to copy the data to
     * @param dstBegin index of the byte array identifying the
     * location to which the byte buffer is copied
     * @exception StringIndexOutOfBoundsException You specified an invalid index into the buffer.
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
     * Sets the value of the byte at the specified index.
     * @param index the index of the byte to set
     * @param b the new value to set
     * @exception StringIndexOutOfBoundsException You have specified
     * an invalid index.
     */
    public void setByteAt(int index, byte b) {
        if ((index < 0) || (index >= count)) {
            throw new StringIndexOutOfBoundsException(index);
        }
        value[index] = b;
    }

    /**
     * Appends an object to the end of this byte buffer.
     * @param obj the object to append to the buffer
     * @return the same <CODE>ByteBuf</CODE> object (not a new object)
     * with the string representation of the specified object appended.
     */
    public ByteBuf append(Object obj) {
        return append(String.valueOf(obj));
    }

    /**
     * Appends a string to the end of this byte buffer.
     * This method appends one byte for each character in the string.
     * The upper 8 bits of the character are stripped off.
     * <P>
     *
     * If you want to retain all bits in the character (not just
     * the lower 8 bits), use <CODE>append(String.getBytes())</CODE>
     * instead.
     * @param str the string that you want appended to the buffer
     * @return the same <CODE>ByteBuf</CODE> object (not a new object)
     * with the specified string appended
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
     * Appends an array of bytes to the end of this byte buffer.
     * @param str the array of bytes to append to this buffer
     * @return the same <CODE>ByteBuf</CODE> object (not a new object)
     * with the specified bytes appended.
     */
    public ByteBuf append(byte str[]) {
        int len = str.length;
        ensureCapacity(count + len);
        System.arraycopy(str, 0, value, count, len);
        count += len;
        return this;
    }

    /**
     * Appends a part of an array of bytes to the end of this byte buffer.
     * @param str the array of bytes to append to this buffer
     * @param offset the index in the array marking the start of the
     * section to copy
     * @param len the number of bytes to add
     * @return the same <CODE>ByteBuf</CODE> object (not a new object)
     * with the specified section of the byte array appended
     */
    public ByteBuf append(byte str[], int offset, int len) {
        ensureCapacity(count + len);
        System.arraycopy(str, offset, value, count, len);
        count += len;
        return this;
    }

    /**
     * Appends a byte buffer to the end of this byte buffer.
     * @param buf the byte buffer to append to this buffer
     * @return the original <CODE>ByteBuf</CODE> object (not a new object)
     * with bytes from the specified byte buffer appended.
     */
    public ByteBuf append(ByteBuf buf) {
        append(buf.toBytes(), 0, buf.length());
        return this;
    }

    /**
     * Appends a boolean to the end of this byte buffer.
     * @param b the boolean value that you want appended to this buffer
     * @return  the original <CODE>ByteBuf</CODE> object (not a new object)
     * with bytes from the string representation of the boolean value appended.
     */
    public ByteBuf append(boolean b) {
        return append(String.valueOf(b));
    }

    /**
     * Appends a byte to the end of this byte buffer.
     * @param ch the byte to append to this buffer
     * @return the original <CODE>ByteBuf</CODE> object (not a new object)
     * with the specified byte appended.
     */
    public ByteBuf append(byte b) {
        ensureCapacity(count + 1);
        value[count++] = b;
        return this;
    }

    /**
     * Appends an integer to the end of this byte buffer.
     * @param i the integer to append to this byte buffer
     * @return the original <CODE>ByteBuf</CODE> object (not a new object)
     * with the string representation of the specified integer appended.
     */
    public ByteBuf append(int i) {
        return append(String.valueOf(i));
    }

    /**
     * Appends a <CODE>long</CODE> value to the end of this byte buffer.
     * @param l the <CODE>long</CODE> value to append to this buffer
     * @return the original <CODE>ByteBuf</CODE> object (not a new object)
     * with the string representation of the specified <CODE>long</CODE>
     * value appended.
     */
    public ByteBuf append(long l) {
        return append(String.valueOf(l));
    }

    /**
     * Appends a <CODE>float</CODE> to the end of this byte buffer.
     * @param f the <CODE>float</CODE> value to append to this buffer
     * @return the original <CODE>ByteBuf</CODE> object (not a new object)
     * with the string representation of the specified <CODE>float</CODE>
     * value appended.
     */
    public ByteBuf append(float f) {
        return append(String.valueOf(f));
    }

    /**
     * Appends a <CODE>double</CODE> to the end of this byte buffer.
     * @param d the <CODE>double</CODE> value to append to this buffer
     * @return the original <CODE>ByteBuf</CODE> object (not a new object)
     * with the string representation of the specified <CODE>double</CODE>
     * value appended.
     */
    public ByteBuf append(double d) {
        return append(String.valueOf(d));
    }

    /**
     * Returns the data in the byte buffer to a string.
     * @return the string representation of the data in the byte buffer.
     */
    public String toString() {
        return new String(value, 0, count);
    }

    /**
     * Returns the data in the byte buffer as a byte array.
     * @return the byte array containing data in the byte buffer.
     */
    public byte[] toBytes() {
        byte[] b = new byte[count];
        System.arraycopy(value, 0, b, 0, count);
        return b;
    }

    /**
     * Invokes the <CODE>InputStream.read</CODE> method and appends the
     * the bytes read to this byte buffer.
     * @param file the input stream from which to read the bytes
     * @param max_bytes the maximum number of bytes to read into the
     * byte buffer
     * @return the number of bytes read, or -1 if there is no more data
     * to read.
     * @exception IOException An I/O error has occurred.
     */
    public int read(InputStream file, int max_bytes)
      throws IOException {
        ensureCapacity(count + max_bytes);
        int i = file.read(value, count, max_bytes);
        if (i > 0) count += i;
        return i;
    }

    /**
     * Invokes the <CODE>RandomAccessFile.read</CODE> method, appending
     * the bytes read to this byte buffer.
     * @param file the <CODE>RandomAccessFile</CODE> object from which 
     * to read the bytes
     * @param max_bytes the maximum number of bytes to read into the
     * byte buffer
     * @return the number of bytes read, or -1 if there is no more data
     * to read.
     * @exception IOException An I/O error has occurred.
     */
    public int read(RandomAccessFile file, int max_bytes)
      throws IOException {
        ensureCapacity(count + max_bytes);
        int i = file.read(value, count, max_bytes);
        if (i > 0) count += i;
        return i;
    }

    /**
     * Writes the contents of the byte buffer to the specified output stream.
     * @param out the output stream
     * @exception IOException An I/O error has occurred.
     */
    public void write(OutputStream out) throws IOException {
        out.write(value, 0, count);
    }

    /**
     * Writes the contents of the byte buffer to the specified
     * <CODE>RandomAccessFile</CODE> object.
     * @param out the <CODE>RandomAccessFile</CODE> object
     * dexception IOException An I/O error has occurred.
     */
    public void write(RandomAccessFile out) throws IOException {
        out.write(value, 0, count);
    }
}
