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
 * An identifier mapper. This provides a 1-1 map between strings and
 * integers where the strings are unique within the map.
 */
public class IDMap {
  private Object[] fKeys;
  private int[] fHashCodes;
  private int[] fValues;
  private String[] fStrings;
  private int fNumStrings;
  private int fShift;
  private int fIndexMask;
  private int fCount;
  private int fCapacity;

  public IDMap() {
    fShift = 32 - 3 + 1;
  }

  /**
   * Given a string, find an id for it. If the string is already
   * present return the old id. Otherwise allocate a new one.
   */
  public synchronized int stringToID(String aStr) {
    if (fHashCodes == null) {
      grow();
    }
    int hash = hashCodeForString(aStr);    // hashCodeFor(aStr);
    int slot = tableIndexFor(aStr, hash);
    if (fHashCodes[slot] == EMPTY) {
      // If the total number of occupied slots gets too big, grow the
      // table.
      if (fCount >= fCapacity) {
        grow();
        return stringToID(aStr);
      }
      fCount++;
      fHashCodes[slot] = hash;
      fKeys[slot] = aStr;
      fValues[slot] = addString(aStr);
    }
    return fValues[slot];
  }

  /**
   * Given a StringBuf, find an id for it. If the string is already
   * present return the old id. Otherwise allocate a new one.
   */
  public synchronized int stringBufToID(StringBuf aStrBuf) {
    if (fHashCodes == null) {
      return( stringToID( aStrBuf.toString() ) );
    }
    int hash = hashCodeForStringBuf(aStrBuf);
    int slot = tableIndexFor(aStrBuf, hash);
    if (fHashCodes[slot] == EMPTY) {
      // not found, insert it.
      return( stringToID( aStrBuf.toString() ) );
    }
    return fValues[slot];
  }

  public synchronized String stringBufToString(StringBuf aStrBuf) {
    int id = stringBufToID(aStrBuf);
    return fStrings[id];
  }

  /**
   * If the argument string is already present in the map return the
   * original string object. Otherwise, allocate a new id and return
   * the argument string.
   */
  public synchronized String stringToString(String aStr) {
    int id = stringToID(aStr);
    return fStrings[id];
  }

  /**
   * Given an id, return the string it maps to.
   */
  public String idToString(int aID) {
    if ((aID < 0) || (aID >= fNumStrings)) {
      throw new IllegalArgumentException("invalid id: " + aID);
    }
    return fStrings[aID];
  }

  /**
   * Add string to the end of the internal strings array. Return the
   * index in the array.
   */
  private int addString(String aString) {
    int rv = fNumStrings;
    if (fStrings == null) {
      fStrings = new String[4];
    } else if (rv == fStrings.length) {
      int newLen = rv;
      if (newLen < 32) {
        // Double the size of small tables
        newLen = newLen * 2;
      } else {
        // Grow by 20% for larger tables
        newLen = (int) (newLen * 1.2f);
      }
      String[] ns = new String[newLen];
      System.arraycopy(fStrings, 0, ns, 0, rv);
      fStrings = ns;
    }
    fStrings[fNumStrings++] = aString;
    return rv;
  }

  //----------------------------------------------------------------------
  // Hashtable code lifted from the netscape.util package; retargeted
  // to use int's as the value's; also removed the "REMOVE" code.

  /** For the multiplicative hash, choose the golden ratio:
   * <pre>
   *     A = ((sqrt(5) - 1) / 2) * (1 << 32)
   * </pre>
   * ala Knuth...
   */
  private static final int A = 0x9e3779b9;

  /**
   * We use EMPTY and REMOVED as special markers in the table. If some
   * poor object returns one of these two values as their hashCode, it
   * is wacked to DEFAULT.
   */
  private static final int EMPTY = 0;
  private static final int DEFAULT = 2;

  /**
   * Provide hashcode for a given key
   */
/*  private final int hashCodeFor(Object aKey) {
    int hash = aKey.hashCode();
    if (hash == EMPTY)
      hash = DEFAULT;
    return hash;
  }
*/
  // We want StringBuf and String have the same hash code, if
  // they contain the same string. Therefor, we need to do our
  // own hash code for String.
  private final int hashCodeForString(String str ) {
    int h = 0;
    int len = str.length();
    if (len < 16) {
      for (int i = 0; i < len; i++) {
        h = (h * 37) + str.charAt(i);
      }
    } else {
      // only sample some characters
      int skip = len / 8;
      for (int i = len, off = 0 ; i > 0; i -= skip, off += skip) {
        h = (h * 39) + str.charAt(off);
      }
    }
    if (h == EMPTY)
      h = DEFAULT;
    return h;
  }

  private final int hashCodeForStringBuf(StringBuf strBuf) {
    int h = 0;
    int len = strBuf.length();
    if (len < 16) {
      for (int i = 0; i < len; i++) {
        h = (h * 37) + strBuf.charAt(i);
      }
    } else {
      // only sample some characters
      int skip = len / 8;
      for (int i = len, off = 0 ; i > 0; i -= skip, off += skip) {
        h = (h * 39) + strBuf.charAt(off);
      }
    }
    if (h == EMPTY)
      h = DEFAULT;
    return h;
  }

  private final boolean keysAreEqual(Object aKey1, Object aKey2) {
    // It is emportant to use aKey1's equals because aKey1 can be
    // a String or StringBuf.
    return aKey1.equals (aKey2);
  }

  /**
   * Primitive method used internally to find slots in the table. If
   * the key is present in the table, this method will return the
   * index under which it is stored. If the key is not present, then
   * this method will return the index under which it can be put. The
   * caller must look at the hashCode at that index to differentiate
   * between the two possibilities.
   */
  private int tableIndexFor(Object aKey, int aHash) {
    // Probe the first slot in the table.  We keep track of the first
    // index where we found a REMOVED marker so we can return that index
    // as the first available slot if the key is not already in the table.
    int product = aHash * A;
    int index = product >>> fShift;
    int testHash = fHashCodes[index];
    if (testHash == aHash) {
      if (keysAreEqual(aKey, fKeys[index]))
        return index;
    } else if (testHash == EMPTY)
      return index;

    // Our first probe has failed, so now we need to start looking
    // elsewhere in the table.
    int step = ((product >>> (2 * fShift - 32)) & fIndexMask) | 1;
    int probeCount = 1;
    do {
      probeCount++;
      index = (index + step) & fIndexMask;

      testHash = fHashCodes[index];

      if (testHash == aHash) {
        if (keysAreEqual(aKey, fKeys[index]))
          return index;
      } else if (testHash == EMPTY) {
        return index;
      }
    } while (probeCount <= fCount);

    // Something very bad has happened.
    throw new Error ("Hashtable overflow");
  }

  /**
   * Grows the table by a factor of 2 (or creates it if necessary). All
   * the REMOVED markers go away and the elements are rehashed into the
   * bigger table.
   */
  private void grow() {
    // The table size needs to be a power of two, and it should double
    // when it grows.  We grow when we are more than 3/4 full.
    fShift--;
    int power = 32 - fShift;
    fIndexMask = (1 << power) - 1;
    fCapacity = (3 * (1 << power)) / 4;

    int[] oldHashCodes = fHashCodes;
    Object[] oldKeys = fKeys;
    int[] oldValues = fValues;

    fHashCodes = new int[1 << power];
    fKeys = new Object[1 << power];
    fValues = new int[1 << power];

    // Reinsert the old elements into the new table if there are any.  Be
    // sure to reset the counts and increment them as the old entries are
    // put back in the table.
    if (fCount > 0) {
      fCount = 0;
      for (int i = 0; i < oldHashCodes.length; i++) {
        Object key = oldKeys[i];
        if (key != null) {
          int hash = oldHashCodes[i];
          int index = tableIndexFor(key, hash);

          fHashCodes[index] = hash;
          fKeys[index] = key;
          fValues[index] = oldValues[i];

          fCount++;
        }
      }
    }
  }
}
