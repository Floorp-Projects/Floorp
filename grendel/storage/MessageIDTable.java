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

/** This is a mechanism for uniqueifying MessageID objects.
    To save memory, we want all `equal' IDs to also be `=='.
    This table is how we accomplish that.  See intern().

    @see MessageID
    @see ByteStringTable
 */

class MessageIDTable extends Obarray {

  /** This is the way we check to see if an object is in the table, or rather,
      if an object constructed from that sequence of bytes would be in the
      table.  This is a kludge to get around the lack of function pointers...
   */
  protected MessageID dummy;

  MessageIDTable(int default_size) {
    super(default_size);
    dummy = new MessageID();
  }

  MessageIDTable() {
    this(100);
  }

  /** Check whether there is a MessageID representing the given subsequence
      of bytes in the table already.  Returns null, or an object from the
      table (an Integer object.)
    */
  synchronized protected Object checkHash(byte bytes[],
                                          int start, int length) {
    dummy.hash = dummy.hashBytes(bytes, start, length);
    return hashtable.get(dummy);
  }

  /** Creates a new MessageID object (which will then be interned.) */
  protected Object newInternable(byte bytes[], int start, int length) {
    return new MessageID(bytes, start, length);
  }

  /** Ensures that an equivalent MessageID is present in the table, adding
      it if necessary.  Returns the index in the table of that object.
      The given MessageID object <I>may</I> be stored in the table,
      without being copied.
    */
  public int intern(MessageID id) {
    Object hashed = hashtable.get(id);
    if (hashed != null) {
      return ((Integer) hashed).intValue();
    } else {
      ensureCapacity(count+1);
      hashtable.put(id, new Integer(count));
      array[count] = id;
      count++;
      return count-1;
    }
  }
}
