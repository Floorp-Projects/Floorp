/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *
 * Created: Mauro Botelho <mabotelh@email.com>, 20 Mar 1999.
 */

package grendel.storage.mdb;

/*| nsIMdbCursor: base cursor class for iterating row cells and table rows
**|
**|| count: the number of elements in the collection (table or row)
**|
**|| seed: the change count in the underlying collection, which is synced
**| with the collection when the iteration position is set, and henceforth
**| acts to show whether the iter has lost collection synchronization, in
**| case it matters to clients whether any change happens during iteration.
**|
**|| pos: the position of the current element in the collection.  Negative
**| means a position logically before the first element.  A positive value
**| equal to count (or larger) implies a position after the last element.
**| To iterate over all elements, set the position to negative, so subsequent
**| calls to any 'next' method will access the first collection element.
**|
**|| doFailOnSeedOutOfSync: whether a cursor should return an error if the
**| cursor's snapshot of a table's seed becomes stale with respect the table's
**| current seed value (which implies the iteration is less than total) in
**| between to cursor calls that actually access collection content.  By
**| default, a cursor should assume this attribute is false until specified,
**| so that iterations quietly try to re-sync when they loose coherence.
|*/
public interface nsIMdbCursor extends nsIMdbObject { // collection iterator
  // { ===== begin nsIMdbCursor methods =====
  
  // { ----- begin attribute methods -----
  public int GetCount(nsIMdbEnv ev); // readonly
  public int GetSeed(nsIMdbEnv ev);    // readonly
  
  public void SetPos(nsIMdbEnv ev, int inPos);   // mutable
  public int GetPos(nsIMdbEnv ev);
  
  public void SetDoFailOnSeedOutOfSync(nsIMdbEnv ev, boolean inFail);
  public boolean GetDoFailOnSeedOutOfSync(nsIMdbEnv ev);
  // } ----- end attribute methods -----
  
  // } ===== end nsIMdbCursor methods =====
};

