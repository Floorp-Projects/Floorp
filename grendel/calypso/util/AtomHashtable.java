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

/**
 * Special hashtable that uses Atoms as keys. This extends HashtableBase to
 * expose a public Atom based api
 * This hastable uses identity comparisons on keys
 *
 * @author  psl   10-15-97 1:22pm
 * @version $Revision: 1.1 $
 * @see
 */
public class AtomHashtable extends HashtableBase
{
  /** Constructs an empty Hashtable. The Hashtable will grow on demand
    * as more elements are added.
    */
  public AtomHashtable ()
  {
    super ();
  }

  /** Constructs a Hashtable capable of holding at least
    * <b>initialCapacity</b> elements before needing to grow.
    */
  public AtomHashtable (int aInitialCapacity)
  {
    super (aInitialCapacity);
  }

  /** Returns an Object array containing the Hashtable's keys.
    */
  public Atom[] keysArray ()
  {
    Atom[] result = new Atom[count];
    getKeysArray (result);
    return result;
  }

  /** Returns an Object array containing the Hashtable's elements.
    */
  public Object[] elementsArray ()
  {
    Object[] result = new Object[count];
    getElementsArray (result);
    return result;
  }

  /** Returns <b>true</b> if the Hashtable contains the element. This method
    * is slow -- O(n) -- because it must scan the table searching for the
    * element.
    */
  public boolean contains (Object aElement)
  {
    return containsElement (aElement);
  }

  /** Returns <b>true</b> if the Hashtable contains the key <b>key</b>.
    */
  public boolean containsKey (Atom aKey)
  {
    return (get (aKey) != null);
  }

  /** Returns the element associated with the <b>key</b>. This method returns
    * <b>null</b> if the Hashtable does not contain <b>key</b>.  Hashtable
    * hashes and compares <b>key</b> using <b>hashCode()</b> and
    * <b>equals()</b>.
    */
  public Object get (Atom aKey)
  {
    // We need to short-circuit here since the data arrays may not have
    // been allocated yet.

    if (count == 0)
      return null;

    return elements[tableIndexFor (aKey, hash (aKey))];
  }

  /** Removes <b>key</b> and the element associated with it from the
    * Hashtable. Returns the element associated with <b>key</b>, or
    * <b>null</b> if <b>key</b> was not present.
    */
  public Object remove (Atom aKey)
  {
    return removeKey (aKey);
  }

  /** Places the <b>key</b>/<b>element</b> pair in the Hashtable. Neither
    * <b>key</b> nor <b>element</b> may be <b>null</b>. Returns the old
    * element associated with <b>key</b>, or <b>null</b> if the <b>key</b>
    * was not present.
    */
  public Object put (Atom aKey, Object aElement)
  {
    return super.put (aKey, aElement);
  }

  int hash (Object aKey)
  {
    return ((Atom)aKey).fHashCode;
  }

  int hash (Atom aKey)
  {
    return aKey.fHashCode;
  }

  /** Primitive method used internally to find slots in the
    * table. If the key is present in the table, this method will return the
    * index
    * under which it is stored. If the key is not present, then this
    * method will return the index under which it can be put. The caller
    * must look at the hashCode at that index to differentiate between
    * the two possibilities.
    */
  int tableIndexFor (Object aKey, int aHash)
  {
    int product, testHash, index, step, removedIndex, probeCount;

    product = aHash * A;
    index = product >>> shift;

    // Probe the first slot in the table.  We keep track of the first
    // index where we found a REMOVED marker so we can return that index
    // as the first available slot if the key is not already in the table.

    testHash = hashCodes[index];

    if (testHash == aHash) {
      if (aKey == keys[index])
        return index;
      removedIndex = -1;
    } else if (testHash == EMPTY)
      return index;
    else if (testHash == REMOVED)
      removedIndex = index;
    else
      removedIndex = -1;

    // Our first probe has failed, so now we need to start looking
    // elsewhere in the table.

    step = ((product >>> (2 * shift - 32)) & indexMask) | 1;
    probeCount = 1;

    do
    {
      probeCount++;
      index = (index + step) & indexMask;

      testHash = hashCodes[index];

      if (testHash == aHash) {
        if (aKey == keys[index])
          return index;
      } else if (testHash == EMPTY) {
        if (removedIndex < 0)
          return index;
        else
          return removedIndex;
      } else if (testHash == REMOVED && removedIndex == -1)
        removedIndex = index;

    } while (probeCount <= totalCount);

    // Something very bad has happened.

    throw new Error("Hashtable overflow");
  }
}

