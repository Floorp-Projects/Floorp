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
 */

package grendel.storage;

import calypso.util.Assert;
import calypso.util.ByteBuf;

/** Represents a set of bytes.  This is a spiritual cousin of String,
    but without the misfeature that characters are two bytes each, and
    there are 16 bytes of extranious object header.  Since we store
    message headers internally in their on-the-wire form, and that
    is 7-bit ASCII (and occasionally 8-bit if people are being
    nonstandard) this is a much more tolerable representation.
    <P>
    This class doesn't provide all of the utilities that String does,
    but that's pretty much just because I didn't need them (yet?)
    <P>
    this is used by ByteStringTable to uniqueify the strings, to
    further reduce memory usage.

    @see ByteStringTable
    @see MessageID
 */
class ByteString {

  protected byte[] bytes;

  // Warning, don't add more instance variables unless you're really
  // really sure.  There are zillions of these objects, so they should
  // be as small as we can make them.


  ByteString(byte bytes[]) {
    this.bytes = bytes;    // this bites!  ha ha.
  }

  ByteString(ByteBuf buf) {
    bytes = new byte[buf.length()];
    System.arraycopy(bytes, 0, buf.toBytes(), 0, bytes.length);
  }

  public String toString() {
    return new String(bytes);
  }

  public ByteBuf toByteBuf() {
    return new ByteBuf(bytes, 0, bytes.length);
  }

  public byte[] toBytes() {
    return bytes;
  }

  protected int hashBytes(byte b[], int start, int len) {
    // Copied from java.lang.String.hashCode() and adapted to work
    // on bytes instead of characters.

    int h = 0;
    int off = start;
    int end = start + len;

    if (len < 16) {
      for (int i = len ; i > 0; i--) {
        h = (h * 37) + bytes[off++];
      }
    } else {
      // only sample some characters
      int skip = len / 8;
      for (int i = len ; i > 0; i -= skip, off += skip) {
        h = (h * 39) + bytes[off];
      }
    }
    return h;
  }

  public int hashCode() {
    return hashBytes(bytes, 0, bytes.length);
  }

  public boolean equals(Object x) {
    if (x instanceof ByteString) {
      int L = bytes.length;
      ByteString ix = (ByteString) x;
      if (L != ix.bytes.length)
        return false;
      else {
        for (int i = 0; i < L; i++) {
          if (bytes[i] != ix.bytes[i])
            return false;
        }
        return true;
      }
    }
    else
      return false;
  }
}
