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
 * Created: Jamie Zawinski <jwz@netscape.com>, 20 Sep 1997.
 */

package grendel.storage;

import java.util.Enumeration;
import java.util.NoSuchElementException;
import calypso.util.ByteBuf;
import java.util.Hashtable;


/** Base class for objects which enable interning (uniquification) of
    objects.
 */
abstract class Obarray {

  /** Holds unique (non-equal) objects.
    */
  protected Object array[];

  /** How much of the array is in use; we don't use Vector for this because
      Vector likes to expand itself by doubling its size, and we will have
      enough entries that that would suck.
  */
  protected int count;

  /** Hashes from object keys to index-in-the-array values.  (Which are
      unfortunately `Integer' objects instead of ints, because the Java
      object model is hopelessly stupid about such things.)
    */
  protected Hashtable hashtable;

  Obarray(int default_size) {
    hashtable = new Hashtable(default_size);
    array = new Object[default_size];
    count = 0;
  }

  Obarray() {
    this(100);
  }

  /** Check whether there is an object representing the given subsequence
      of bytes in the table already.  This should end up doing hashtable.get()
      and returning null, or an object from the table (an Integer object.)
    */
  abstract protected Object checkHash(byte bytes[], int start, int length);

  /** Creates a new object (which will then be interned.)
    */
  abstract protected Object newInternable(byte bytes[], int start, int length);


  protected synchronized void ensureCapacity(int count) {
    // we're adding an item; grow the array if necessary.
    if (count >= array.length) {
      int new_size = ((array.length * 13) / 10) + 10;   // grow by 30% + 10.
      Object new_array[] = new Object[new_size];
      System.arraycopy(array, 0, new_array, 0, array.length);
      array = new_array;
    }
  }


  /** Returns the numeric ID of an object containing the given bytes.
      This will be reused if possible; otherwise, a new one will be made
      and remembered in the table.
      <P>
      The ID-number may be turned into an interned object with getObject().
    */
  public synchronized int intern(byte bytes[], int start, int length) {

    if (length <= 0) return -1;

    Object hashed = checkHash(bytes, start, length);

    if (hashed != null) {
      return ((Integer) hashed).intValue();
    } else {
      ensureCapacity(count+1);
      Object b = newInternable(bytes, start, length);
      hashtable.put(b, new Integer(count));
      array[count] = b;
      count++;
      return count-1;
    }
  }

  public int intern(ByteBuf buf) {
    return intern(buf.toBytes(), 0, buf.length());
  }

  public int intern(String s) {
    byte b[] = s.getBytes();
    return intern(b, 0, b.length);
  }

  /** Given a numberic ID number, converts it to the corresponding
      object in this table.  This ID number should be one that
      was previously returned by the table's `intern()' method.
    */
  public Object getObject(int id) {
    if (id == -1 || count <= id)
      return null;
    else
      return array[id];
  }

  public boolean isEmpty() {
    return (count == 0);
  }

  public int size() {
    return count;
  }

  public void clear() {
    hashtable.clear();
    while (count > 0)
      array[--count] = null;
  }

  public Enumeration elements() {
    return new ObarrayEnumeration(this);
  }
}

class ObarrayEnumeration implements Enumeration {
  int i = 0;
  Obarray a;
  public ObarrayEnumeration(Obarray a) { this.a = a; }
  public Object nextElement() {
    if (i < a.count) return a.array[i++];
    else throw new NoSuchElementException();
  }
  public boolean hasMoreElements() { return (a != null && i < a.count); }
}
