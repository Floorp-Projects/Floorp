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

/*| nsIMdbCollection: an object that collects a set of other objects as members.
**| The main purpose of this base class is to unify the perceived semantics
**| of tables and rows where their collection behavior is similar.  This helps
**| isolate the mechanics of collection behavior from the other semantics that
**| are more characteristic of rows and tables.
**|
**|| count: the number of objects in a collection is the member count. (Some
**| collection interfaces call this attribute the 'size', but that can be a
**| little ambiguous, and counting actual members is harder to confuse.)
**|
**|| seed: the seed of a collection is a counter for changes in membership in
**| a specific collection.  This seed should change when members are added to
**| or removed from a collection, but not when a member changes internal state.
**| The seed should also change whenever the internal collection of members has
**| a complex state change that reorders member positions (say by sorting) that
**| would affect the nature of an iteration over that collection of members.
**| The purpose of a seed is to inform any outstanding collection cursors that
**| they might be stale, without incurring the cost of broadcasting an event
**| notification to such cursors, which would need more data structure support.
**| Presumably a cursor in a particular mdb code suite has much more direct
**| access to a collection seed member slot that this abstract COM interface,
**| so this information is intended more for clients outside mdb that want to
**| make inferences similar to those made by the collection cursors.  The seed
**| value as an integer magnitude is not very important, and callers should not
**| assume meaningful information can be derived from an integer value beyond
**| whether it is equal or different from a previous inspection.  A seed uses
**| integers of many bits in order to make the odds of wrapping and becoming
**| equal to an earlier seed value have probability that is vanishingly small.
**|
**|| port: every collection is associated with a specific database instance.
**|
**|| cursor: a subclass of nsIMdbCursor suitable for this specific collection
**| subclass.  The ability to GetCursor() from the base nsIMdbCollection class
**| is not really as useful as getting a more specifically typed cursor more
**| directly from the base class without any casting involved.  So including
**| this method here is more for conceptual illustration.
**|
**|| oid: every collection has an identity that persists from session to
**| session. Implementations are probably able to distinguish row IDs from
**| table IDs, but we don't specify anything official in this regard.  A
**| collection has the same identity for the lifetime of the collection,
**| unless identity is swapped with another collection by means of a call to
**| BecomeContent(), which is considered a way to swap a new representation
**| for an old well-known object.  (Even so, only content appears to change,
**| while the identity seems to stay the same.)
**|
**|| become: developers can effectively cause two objects to swap identities,
**| in order to effect a complete swap between what persistent content is
**| represented by two oids.  The caller should consider this a content swap,
**| and not identity wap, because identities will seem to stay the same while
**| only content changes.  However, implementations will likely do this
**| internally by swapping identities.  Callers must swap content only
**| between objects of similar type, such as a row with another row, and a
**| table with another table, because implementations need not support
**| cross-object swapping because it might break object name spaces.
**|
**|| dropping: when a caller expects a row or table will no longer be used, the
**| caller can tell the collection to 'drop activity', which means the runtime
**| object can have it's internal representation purged to save memory or any
**| other resource that is being consumed by the collection's representation.
**| This has no effect on the collection's persistent content or semantics,
**| and is only considered a runtime effect.  After a collection drops
**| activity, the object should still be as usable as before (because it has
**| NOT been closed), but further usage can be expensive to re-instate because
**| it might involve reallocating space and/or re-reading disk space.  But
**| since this future usage is not expected, the caller does not expect to
**| pay the extra expense.  An implementation can choose to implement
**| 'dropping activity' in different ways, or even not at all if this
**| operation is not really feasible.  Callers cannot ask objects whether they
**| are 'dropped' or not, so this should be transparent. (Note that
**| implementors might fear callers do not really know whether future
**| usage will occur, and therefore might delay the act of dropping until
**| the near future, until seeing whether the object is used again
**| immediately elsewhere. Such use soon after the drop request might cause
**| the drop to be cancelled.)
|*/
public interface nsIMdbCollection extends nsIMdbObject { // sequence of objects
  // { ===== begin nsIMdbCollection methods =====
  
  // { ----- begin attribute methods -----
  public int GetSeed(nsIMdbEnv ev); // member change count
  public int GetCount(nsIMdbEnv ev); // member count
  
  public nsIMdbPort GetPort(nsIMdbEnv ev); // collection container
  // } ----- end attribute methods -----
  
  // { ----- begin cursor methods -----
  public nsIMdbCursor GetCursor( // make a cursor starting iter at inMemberPos
                                nsIMdbEnv ev, // context
                                int inMemberPos); // zero-based ordinal pos of member in collection
  // } ----- end cursor methods -----
  
  // { ----- begin ID methods -----
  public mdbOid GetOid(nsIMdbEnv ev); // read object identity
  public void BecomeContent(nsIMdbEnv ev,
                            final mdbOid inOid); // exchange content
  // } ----- end ID methods -----
  
  // { ----- begin activity dropping methods -----
  public void DropActivity( // tell collection usage no longer expected
                           nsIMdbEnv ev);
  // } ----- end activity dropping methods -----
  
  // } ===== end nsIMdbCollection methods =====
};

