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

/** This class represents a Message-ID.
    It does not contain the ID itself: there is no way to reconstruct the
    original string from this object; however, it contains a hash of that
    string which should be sufficiently unique so that the odds of a
    collision are 1 in 2^32 (since it is a 64-bit hash.)
    <P>
    We store the hash instead of the original string because it takes up
    <I>significantly</I> less memory, and in order to do threading, we
    tend to need a huge number of Message-IDs in memory at once.

    @see MessageIDTable
 */

class MessageID {

  long hash;    // not protected for the benfit of a MessageIDTable kludge.

//  String debug_id_string;


  /** Constructs a MessageID object with a null hash. */
  MessageID() {
  }

  /** Constructs a MessageID object with the given hash. */
  MessageID(long hash) {
    this.hash = hash;
  }

  /** Constructs a MessageID object for the given sub-sequence of bytes.
      A pointer to the bytes is not retained.
    */
  MessageID(byte bytes[], int start, int length) {
    hash = hashBytes(bytes, start, length);
//    debug_id_string = new String(bytes, start, length);
  }

  MessageID(String chars) {
    byte bytes[] = chars.getBytes();
    hash = hashBytes(bytes, 0, bytes.length);
//    debug_id_string = chars;
  }

  /** Converts it to a string, for debugging. */
  public String toString() {
    return "<" + this.getClass() + " hash=" + Long.toHexString(hash) +
//      " string=" + debug_id_string +
      ">";
  }

  public int hashCode() {
    // Note that this is a 32-bit hash, not the 64-bit internal hash;
    // it's good enough for hash tables, but not for equality checks.
    return (int) ((hash >> 32) ^ (hash & 0xFFFFFFFF));
  }

  public boolean equals(Object x) {
    if (x instanceof MessageID)
      return (hash == ((MessageID) x).hash);
    else
      return false;
  }

  /** Computes a 64-bit hash of the given subsequence of bytes.
      <P>
      Note: this hash value is written out to mail summary files by the
      MailSummaryFileGrendel class.  If the hash algorithm is changed,
      it will invalidate all existing mail summary files, and necessitate
      a change of the file format version number.
      @see MailSummaryFileGrendel
   */
  protected long hashBytes(byte bytes[], int start, int length) {
    long h = 0;
    int end = start + length;
    int i = start;

    // Just in case, don't count a leading < or trailing > in the hash.
    // All callers should strip those first, but be safe...
    while (i < end && (bytes[i]     == '<' || bytes[i] <= ' '))     i++;
    while (end > i && (bytes[end-1] == '>' || bytes[end-1] <= ' ')) end--;

    while (i < end) {
      long hh = 0;
      for (int j = 0; i < end && j < 16; j++) {
        hh = (hh * 37) + bytes[i++];    // the guts of String.hashCode().
      }
      h = (h << 8) ^ hh;                // fill in high-high-order bits too.
    }
    return h;
  }
}
