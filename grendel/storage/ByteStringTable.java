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
 * Created: Jamie Zawinski <jwz@netscape.com>, 19 Sep 1997.
 */

package grendel.storage;

import calypso.util.ByteBuf;
import java.util.Hashtable;

/** This is a mechanism for uniqueifying header strings (ByteString objects.)
    To save memory, we want all `equal' strings to also be `=='.
    This table is how we accomplish that.  See intern().

    @see ByteString
    @see MessageIDTable
 */

class ByteStringTable extends Obarray {

  /** This is the way we check to see if an object is in the table, or rather,
      if an object constructed from that sequence of bytes would be in the
      table.  This is a kludge to get around the lack of function pointers...
   */
  protected DummyByteString dummy;

  ByteStringTable(int default_size) {
    super(default_size);
    dummy = new DummyByteString();
  }

  ByteStringTable() {
    this(100);
  }

  /** Check whether there is a ByteString representing the given subsequence
      of bytes in the table already.  Returns null, or an object from the
      table (an Integer object.)
    */
  synchronized protected Object checkHash(byte bytes[],
                                          int start, int length) {
    dummy.bytes = bytes;
    dummy.start = start;
    dummy.length = length;
    Object hashed = hashtable.get(dummy);
    dummy.bytes = null;
    return hashed;
  }

  /** Creates a new ByteString object (which will then be interned.) */
  protected Object newInternable(byte bytes[], int start, int length) {
    byte new_bytes[] = new byte[length];
    System.arraycopy(bytes, start, new_bytes, 0, length);
    return new ByteString(new_bytes);
  }
}


/** Kludge used by ByteStringTable.
    This behaves like ByteString, but does its dirty work
    on an array and offset into it, without copying it.

    @see ByteString
    @see ByteStringTable
  */
class DummyByteString extends ByteString {

  int start;
  int length;

  // Note that this object is a subclass rather than a special mode of
  // operation of ByteString to avoid having to make IBS be any
  // larger -- there are a zillion of those, and only one of these per
  // ByteStringTable.

  DummyByteString() {
    super((byte[]) null);
  }

  void setBytes(byte[] data, int start, int length) {
    this.bytes = data;
    this.start = start;
    this.length = length;
  }

  public String toString() {
    return new String(bytes, start, length);
  }

  public ByteBuf toByteBuf() {
    return new ByteBuf(bytes, start, length);
  }

  public int hashCode() {
    return hashBytes(bytes, start, length);
  }

  public boolean equals(Object x) {

    byte this_bytes[] = bytes;
    int this_start = start;
    int this_length = length;

    byte that_bytes[];
    int that_start;
    int that_length;

    if (x instanceof DummyByteString) {
      DummyByteString that = (DummyByteString) x;
      that_length = that.length;
      if (this_length != that_length)
        return false;
      else {
        that_start = that.start;
        that_bytes = that.bytes;
      }
    } else if (x instanceof ByteString) {
      ByteString that = (ByteString) x;
      that_length = that.bytes.length;
      if (this_length != that_length)
        return false;
      else {
        that_start = 0;
        that_bytes = that.bytes;
      }
    } else {
      return false;
    }

    for (int i = 0; i < this_length; i++) {
      if (this_bytes[this_start + i] != that_bytes[that_start + i])
        return false;
    }
    return true;
  }
}
