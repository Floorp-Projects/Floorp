/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *
 * Created: Jamie Zawinski <jwz@netscape.com>, 16 Sep 1997.
 */

package grendel.storage;

/** This class implements a fixed-size array capable of holding `long'
    values in the range [0, 1<<60).  It packs the data efficiently,
    so that if the array actually only holds values less than 1<<15,
    it will take up no more space than a short[] array.  (The overhead
    is 1 bit per 16.)
  */
class PackedIntArray {
  private short data[] = null;

  /** Make a new, empty PackedIntArray.  Use setCapacity() to initialize it.
    */
  PackedIntArray() {
  }

  /** Make a new PackedIntArray capable of holding at most <I>size</I>
      elements. */
  PackedIntArray(int size) {
    setCapacity(size);
  }

  /** Set the number of elements this object is capable of holding.
      If the size is reduced, some elements will be discarded.
      If the size is increased, the new elements will default to 0.
    */
  void setCapacity(int size) {
    if (data == null)
      data = new short[size];
    else {
      long old_data[] = new long[data.length];
      int old_size = 0;
      try {
        for (old_size = 0; old_size < data.length; old_size++)
          old_data[old_size] = elementAt(old_size);
      } catch (ArrayIndexOutOfBoundsException c) {
      }

      data = new short[size];
      for (int i = 0; i < old_size; i++)
        setElementAt(i, old_data[i]);
    }
  }

  /** Returns the nth element as a long. */
  long elementAt(int index) {

    int i = 0;
    // find the index-th entry in the set
    for (int j = 0; j < index; j++) {
      while ((data[i] & 0x8000) != 0)
        i++;
      i++;
    }

    // pack together the bytes composing that entry
    long result = 0;
    int n;
    do {
      n = ((int) data[i++]) & 0xFFFF;
      result = (result << 15) | (n & 0x7FFF);
    } while ((n & 0x8000) != 0);

    return result;
  }

  /** Stores the given value into the nth slot.
      The value must be in the range [0, 1<<60), or
      ArrayIndexOutOfBoundsException is thrown.
    */
  void setElementAt(int index, long value) {

    if (value < 0 || value >= (1L << 60))     // 15 bits per short; 15*4=60.
      throw new ArrayIndexOutOfBoundsException();

    int i = 0;
    // find the index-th entry in the set
    for (int j = 0; j < index; j++) {
      while ((data[i] & 0x8000) != 0)
        i++;
      i++;
    }

    // find the length of that entry
    int old_length = 0;
    while ((data[i + old_length] & 0x8000) != 0)
      old_length++;
    old_length++;

    // find the length of the new value
    int new_length = ((value < (1L << 15)) ? 1 :
                      (value < (1L << 30)) ? 2 :
                      (value < (1L << 45)) ? 3 :
                      4);

    if (new_length != old_length) {
      // change the size used by this number in the middle of the array.
      short new_data[] = new short[data.length + (new_length - old_length)];
      System.arraycopy(data, 0, new_data, 0, i+1);
      System.arraycopy(data, i + old_length,
                       new_data, i + new_length,
                       data.length - (i + old_length));
      data = new_data;
    }

    // write the packed bytes into the array

    if (new_length >= 4)
      data[i++] = (short) (((value >> 45) & 0x7FFF) | 0x8000);
    if (new_length >= 3)
      data[i++] = (short) (((value >> 30) & 0x7FFF) | 0x8000);
    if (new_length >= 2)
      data[i++] = (short) (((value >> 15) & 0x7FFF) | 0x8000);
    data[i] = (short) (value & 0x7FFF);
  }


  public String toString() {
    String r = "[";
/*
    for (int i = 0; i < data.length; i++) {
      String s = Integer.toHexString(((int) data[i]) & 0xFFFF);
      if (s.length() == 1) s = "000" + s;
      else if (s.length() == 2) s = "00" + s;
      else if (s.length() == 3) s = "0" + s;
      r += " 0x" + s;
    }
    r += " /";
 */
    try {
      int i = 0;
      while (true)
        r += " " + elementAt(i++);
    } catch (ArrayIndexOutOfBoundsException c) {
    }

    r += " ]";
    return r;
  }


  private void check(long i0, long i1, long i2, long i3, long i4) {
    if (elementAt(0) != i0 || elementAt(1) != i1 || elementAt(2) != i2 ||
        elementAt(3) != i3 || elementAt(4) != i4)
      throw new Error("self test failed");
  }

  public static void selfTest() {
    PackedIntArray p = new PackedIntArray(5);
    p.check(0, 0, 0, 0, 0);

    p.setElementAt(0, 0);
    p.setElementAt(1, 1);
    p.setElementAt(2, 2);
    p.setElementAt(3, 3);
    p.setElementAt(4, 4);
    p.check(0, 1, 2, 3, 4);

    p.setElementAt(0, 0x7FFF);
    p.check(0x7FFF, 1, 2, 3, 4);
    p.setElementAt(0, 0x8000);
    p.check(0x8000, 1, 2, 3, 4);
    p.setElementAt(0, 0x8001);
    p.check(0x8001, 1, 2, 3, 4);

    p.setElementAt(0, 0x7FFFFFFFL);
    p.check(0x7FFFFFFFL, 1, 2, 3, 4);
    p.setElementAt(0, 0x80000000L);
    p.check(0x80000000L, 1, 2, 3, 4);
    p.setElementAt(0, 0x80000001L);
    p.check(0x80000001L, 1, 2, 3, 4);

    p.setElementAt(0, 0x7FFFFFFFFFFFL);
    p.check(0x7FFFFFFFFFFFL, 1, 2, 3, 4);
    p.setElementAt(0, 0x800000000000L);
    p.check(0x800000000000L, 1, 2, 3, 4);
    p.setElementAt(0, 0x800000000001L);
    p.check(0x800000000001L, 1, 2, 3, 4);

    p.setElementAt(0, 0x800000000000000L);
    p.check(0x800000000000000L, 1, 2, 3, 4);

    p.setElementAt(1, 0xFFFFFFFFFFFFFFFL);
    p.check(0x800000000000000L, 0xFFFFFFFFFFFFFFFL, 2, 3, 4);

    p.setElementAt(4, 0xFFFFFFFFFFFFFFFL);
    p.check(0x800000000000000L, 0xFFFFFFFFFFFFFFFL, 2, 3, 0xFFFFFFFFFFFFFFFL);

    p.setElementAt(0, 0);
    p.check(0, 0xFFFFFFFFFFFFFFFL, 2, 3, 0xFFFFFFFFFFFFFFFL);

    p.setElementAt(2, 2);
    p.check(0, 0xFFFFFFFFFFFFFFFL, 2, 3, 0xFFFFFFFFFFFFFFFL);

    p.setElementAt(1, 1);
    p.check(0, 1, 2, 3, 0xFFFFFFFFFFFFFFFL);

    p.setElementAt(3, 3);
    p.check(0, 1, 2, 3, 0xFFFFFFFFFFFFFFFL);

    p.setElementAt(4, 4);
    p.check(0, 1, 2, 3, 4);
  }

  public static final void main(String arg[]) { selfTest(); }
}
