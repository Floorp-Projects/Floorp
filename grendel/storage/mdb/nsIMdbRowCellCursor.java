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

/*| nsIMdbRowCellCursor: cursor class for iterating row cells
**|
**|| row: the cursor is associated with a specific row, which can be
**| set to a different row (which resets the position to -1 so the
**| next cell acquired is the first in the row.
**|
**|| NextCell: get the next cell in the row and return its position and
**| a new instance of a nsIMdbCell to represent this next cell.
|*/
interface nsIMdbRowCellCursor extends nsIMdbCursor { // cell collection iterator
  
  // { ===== begin nsIMdbRowCellCursor methods =====
  
  // { ----- begin attribute methods -----
  public void SetRow(nsIMdbEnv ev, nsIMdbRow ioRow); // sets pos to -1
  public nsIMdbRow GetRow(nsIMdbEnv ev);
  // } ----- end attribute methods -----
  
  // { ----- begin cell creation methods -----
  public nsIMdbCell MakeCell( // get cell at current pos in the row
                             nsIMdbEnv ev); // context
  // mdb_column* outColumn, // column for this particular cell
  // mdb_pos* outPos, // position of cell in row sequence
  // nsIMdbCell** acqCell); // the cell at inPos
  // } ----- end cell creation methods -----
  
  // { ----- begin cell seeking methods -----
  public nsIMdbCell SeekCell( // same as SetRow() followed by MakeCell()
                             nsIMdbEnv ev, // context
                             int inPos); // position of cell in row sequence
  // mdb_column* outColumn, // column for this particular cell
  // nsIMdbCell** acqCell); // the cell at inPos
  // } ----- end cell seeking methods -----
  
  // { ----- begin cell iteration methods -----
  public nsIMdbCell NextCell( // get next cell in the row
                             nsIMdbEnv ev, // context
                             nsIMdbCell ioCell); // changes to the next cell in the iteration
  // mdb_column* outColumn, // column for this particular cell
  // mdb_pos* outPos); // position of cell in row sequence
  
  public nsIMdbCell PickNextCell( // get next cell in row within filter set
                                 nsIMdbEnv ev, // context
                                 nsIMdbCell ioCell, // changes to the next cell in the iteration
                                 final mdbColumnSet inFilterSet); // col set of actual caller interest
  //mdb_column* outColumn, // column for this particular cell
  //mdb_pos* outPos); // position of cell in row sequence
  
  // Note that inFilterSet should not have too many (many more than 10?)
  // cols, since this might imply a potential excessive consumption of time
  // over many cursor calls when looking for column and filter intersection.
  // } ----- end cell iteration methods -----
  
  // } ===== end nsIMdbRowCellCursor methods =====
};

