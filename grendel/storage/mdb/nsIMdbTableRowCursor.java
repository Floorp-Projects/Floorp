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

/*| nsIMdbTableRowCursor: cursor class for iterating table rows
**|
**|| table: the cursor is associated with a specific table, which can be
**| set to a different table (which resets the position to -1 so the
**| next row acquired is the first in the table.
**|
**|| NextRowId: the rows in the table can be iterated by identity alone,
**| without actually reading the cells of any row with this method.
**|
**|| NextRowCells: read the next row in the table, but only read cells
**| from the table which are already present in the row (so no new cells
**| are added to the row, even if they are present in the table).  All the
**| cells will have content specified, even it is the empty string.  No
**| columns will be removed, even if missing from the row (because missing
**| and empty are semantically equivalent).
**|
**|| NextRowAllCells: read the next row in the table, and access all the
**| cells for this row in the table, adding any missing columns to the row
**| as needed until all cells are represented.  All the
**| cells will have content specified, even it is the empty string.  No
**| columns will be removed, even if missing from the row (because missing
**| and empty are semantically equivalent).
**|
|*/
public interface nsIMdbTableRowCursor extends nsIMdbCursor { // table row iterator
  
  // { ===== begin nsIMdbTableRowCursor methods =====
  
  // { ----- begin attribute methods -----
  public void SetTable(nsIMdbEnv ev, nsIMdbTable ioTable); // sets pos to -1
  public nsIMdbTable GetTable(nsIMdbEnv ev);
  // } ----- end attribute methods -----
  
  // { ----- begin oid iteration methods -----
  public mdbOid NextRowOid( // get row id of next row in the table
                           nsIMdbEnv ev); // context
  // out row oid
  // mdb_pos* outRowPos);    // zero-based position of the row in table
  // } ----- end oid iteration methods -----
  
  // { ----- begin row iteration methods -----
  public nsIMdbRow NextRow( // get row cells from table for cells already in row
                           nsIMdbEnv ev); // context
  // acquire next row in table
  // mdb_pos* outRowPos);    // zero-based position of the row in table
  // } ----- end row iteration methods -----
  
  // { ----- begin copy iteration methods -----
  public mdbOid NextRowCopy( // put row cells into sink only when already in sink
                            nsIMdbEnv ev, // context
                            nsIMdbRow ioSinkRow); // sink for row cells read from next row
  // out row oid
  // mdb_pos* outRowPos);    // zero-based position of the row in table
  
  public mdbOid NextRowCopyAll( // put all row cells into sink, adding to sink
                               nsIMdbEnv ev, // context
                               nsIMdbRow ioSinkRow); // sink for row cells read from next row
  //mdbOid* outOid, // out row oid
  // mdb_pos* outRowPos);    // zero-based position of the row in table
  // } ----- end copy iteration methods -----
  
  // } ===== end nsIMdbTableRowCursor methods =====
};

