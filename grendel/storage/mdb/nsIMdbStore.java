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

/*| nsIMdbStore: a mutable interface to a specific database file.
**|
**|| tables: one can force a new table to exist in a store with NewTable()
**| and nonzero values for both row scope and table kind.  (If one wishes only
**| one table of a certain kind, then one might look for it first using the
**| GetTableKind() method).  One can pass inMustBeUnique to force future
**| users of this store to be unable to create other tables with the same pair
**| of scope and kind attributes.  When inMustBeUnique is true, and the table
**| with the given scope and kind pair already exists, then the existing one
**| is returned instead of making a new table.  Similarly, if one passes false
**| for inMustBeUnique, but the table kind has already been marked unique by a
**| previous user of the store, then the existing unique table is returned.
**|
**|| import: all or some of another port's content can be imported by calling
**| AddPortContent() with a row scope identifying the extent of content to
**| be imported.  A zero row scope will import everything.  A nonzero row
**| scope will only import tables with a matching row scope.  Note that one
**| must somehow find a way to negotiate possible conflicts between existing
**| row content and imported row content, and this involves a specific kind of
**| definition for row identity involving either row IDs or unique attributes,
**| or some combination of these two.  At the moment I am just going to wave
**| my hands, and say the default behavior is to assign all new row identities
**| to all imported content, which will result in no merging of content; this
**| must change later because it is unacceptable in some contexts.
**|
**|| commits: to manage modifications in a mutable store, very few methods are
**| really needed to indicate global policy choices that are independent of 
**| the actual modifications that happen in objects at the level of tables,
**| rows, and cells, etc.  The most important policy to specify is which sets
**| of changes are considered associated in a manner such that they should be
**| applied together atomically to a given store.  We call each such group of
**| changes a transaction.  We handle three different grades of transaction,
**| but they differ only in semantic significance to the application, and are
**| not intended to nest.  (If small transactions were nested inside large
**| transactions, that would imply that a single large transaction must be
**| atomic over all the contained small transactions; but actually we intend
**| smalls transaction never be undone once commited due to, say, aborting a
**| transaction of greater significance.)  The small, large, and session level
**| commits have equal granularity, and differ only in risk of loss from the
**| perspective of an application.  Small commits characterize changes that
**| can be lost with relatively small risk, so small transactions can delay
**| until later if they are expensive or impractical to commit.  Large commits
**| involve changes that would probably inconvenience users if lost, so the
**| need to pay costs of writing is rather greater than with small commits.
**| Session commits are last ditch attempts to save outstanding changes before
**| stopping the use of a particular database, so there will be no later point
**| in time to save changes that have been delayed due to possible high cost.
**| If large commits are never delayed, then a session commit has about the
**| same performance effect as another large commit; but if small and large
**| commits are always delayed, then a session commit is likely to be rather
**| expensive as a runtime cost compared to any earlier database usage.
**|
**|| aborts: the only way to abort changes to a store is by closing the store.
**| So there is no specific method for causing any abort.  Stores must discard
**| all changes made that are uncommited when a store is closed.  This design
**| choice makes the implementations of tables, rows, and cells much less
**| complex because they need not maintain a record of undobable changes.  When
**| a store is closed, presumably this precipitates the closure of all tables,
**| rows, and cells in the store as well.   So an application can revert the
**| state of a store in the user interface by quietly closing and reopening a
**| store, because this will discard uncommited changes and show old content.
**| This implies an app that closes a store will need to send a "scramble"
**| event notification to any views that depend on old discarded content.
|*/
public interface nsIMdbStore extends nsIMdbPort {
  
  // { ===== begin nsIMdbStore methods =====
  
  // { ----- begin table methods -----
  public nsIMdbTable NewTable( // make one new table of specific type
                              nsIMdbEnv ev, // context
                              int inRowScope,    // row scope for row ids
                              int inTableKind,    // the type of table to access
                              boolean inMustBeUnique); // whether store can hold only one of these
  // acquire scoped collection of rows
  // } ----- end table methods -----
  
  // { ----- begin row scope methods -----
  public boolean RowScopeHasAssignedIds(nsIMdbEnv ev,
                                        int inRowScope);   // row scope for row ids
  //mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
  //mdb_bool* outStoreAssigned); // nonzero if store db assigned specified
  
  public boolean SetCallerAssignedIds(nsIMdbEnv ev,
                                      int inRowScope);   // row scope for row ids
  //mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
  //mdb_bool* outStoreAssigned); // nonzero if store db assigned specified
  
  public boolean SetStoreAssignedIds(nsIMdbEnv ev,
                                     int inRowScope);   // row scope for row ids
  // mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
  // mdb_bool* outStoreAssigned); // nonzero if store db assigned specified
  // } ----- end row scope methods -----
  
  // { ----- begin row methods -----
  public nsIMdbRow NewRowWithOid(nsIMdbEnv ev, // new row w/ caller assigned oid
                                 final mdbOid inOid);   // caller assigned oid
  // create new row
  
  public nsIMdbRow NewRow(nsIMdbEnv ev, // new row with db assigned oid
                          int inRowScope);   // row scope for row ids
  // create new row
  // Note this row must be added to some table or cell child before the
  // store is closed in order to make this row persist across sesssions.
  // } ----- end row methods -----
  
  // { ----- begin inport/export methods -----
  public nsIMdbThumb ImportContent( // import content from port
                                   nsIMdbEnv ev, // context
                                   int inRowScope, // scope for rows (or zero for all?)
                                   nsIMdbPort ioPort); // the port with content to add to store
  // acquire thumb for incremental import
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the import will be finished.
  // } ----- end inport/export methods -----
  
  // { ----- begin hinting methods -----
  public void
    ShareAtomColumnsHint( // advise re shared column content atomizing
                         nsIMdbEnv ev, // context
                         int inScopeHint, // zero, or suggested shared namespace
                         final mdbColumnSet inColumnSet); // cols desired tokenized together
  
  public void
    AvoidAtomColumnsHint( // advise column with poor atomizing prospects
                         nsIMdbEnv ev, // context
                         final mdbColumnSet inColumnSet); // cols with poor atomizing prospects
  // } ----- end hinting methods -----
  
  // { ----- begin commit methods -----
  public void SmallCommit( // save minor changes if convenient and uncostly
                          nsIMdbEnv ev); // context
  
  public nsIMdbThumb LargeCommit( // save important changes if at all possible
                                 nsIMdbEnv ev); // context
  // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.
  
  public nsIMdbThumb SessionCommit( // save all changes if large commits delayed
                                   nsIMdbEnv ev); // context
  // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.
  
  public nsIMdbThumb
    CompressCommit( // commit and make db physically smaller if possible
                   nsIMdbEnv ev); // context
  // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.
  // } ----- end commit methods -----
  
  // } ===== end nsIMdbStore methods =====
};

