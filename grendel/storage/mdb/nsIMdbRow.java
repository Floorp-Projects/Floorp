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

/*| nsIMdbRow: a collection of cells
**|
|*/
public interface nsIMdbRow extends nsIMdbCollection { // cell tuple
  
  // { ===== begin nsIMdbRow methods =====
  
  // { ----- begin cursor methods -----
  public nsIMdbRowCellCursor GetRowCellCursor( // make a cursor starting iteration at inRowPos
                                              nsIMdbEnv ev, // context
                                              int inRowPos); // zero-based ordinal position of row in table
  // } ----- end cursor methods -----
  
  // { ----- begin column methods -----
  public void AddColumn( // make sure a particular column is inside row
                        nsIMdbEnv ev, // context
                        int inColumn, // column to add
                        final String inYarn); // cell value to install
  
  public void CutColumn( // make sure a column is absent from the row
                        nsIMdbEnv ev, // context
                        int inColumn); // column to ensure absent from row
  
  public void CutAllColumns( // remove all columns from the row
                            nsIMdbEnv ev); // context
  // } ----- end column methods -----
  
  // { ----- begin cell methods -----
  public nsIMdbCell NewCell( // get cell for specified column, or add new one
                            nsIMdbEnv ev, // context
                            int inColumn); // column to add
  
  public void AddCell( // copy a cell from another row to this row
                      nsIMdbEnv ev, // context
                      final nsIMdbCell inCell); // cell column and value
  
  public nsIMdbCell GetCell( // find a cell in this row
                            nsIMdbEnv ev, // context
                            int inColumn); // column to find
  
  public void EmptyAllCells( // make all cells in row empty of content
                            nsIMdbEnv ev); // context
  // } ----- end cell methods -----
  
  // { ----- begin row methods -----
  public void AddRow( // add all cells in another row to this one
                     nsIMdbEnv ev, // context
                     nsIMdbRow ioSourceRow); // row to union with
  
  public void SetRow( // make exact duplicate of another row
                     nsIMdbEnv ev, // context
                     nsIMdbRow ioSourceRow); // row to duplicate
  // } ----- end row methods -----
  
  // } ===== end nsIMdbRow methods =====
};

