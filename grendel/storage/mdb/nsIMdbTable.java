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

/*| nsIMdbTable: an ordered collection of rows
**|
**|| row scope: an integer token for an atomized string in this database
**| that names a space for row IDs.  This attribute of a table is intended
**| as guidance metainformation that helps with searching a database for
**| tables that operate on collections of rows of the specific type.  By
**| convention, a table with a specific row scope is expected to focus on
**| containing rows that belong to that scope, however exceptions are easily
**| allowed because all rows in a table are known by both row ID and scope.
**| (A table with zero row scope is never allowed because this would make it
**| ambiguous to use a zero row scope when iterating over tables in a port to
**| indicate that all row scopes should be seen by a cursor.)
**|
**|| table kind: an integer token for an atomized string in this database
**| that names a kind of table as a subset of the associated row scope. This
**| attribute is intended as guidance metainformation to clarify the role of
**| this table with respect to other tables in the same row scope, and this
**| also helps search for such tables in a database.  By convention, a table
**| with a specific table kind has a consistent role for containing rows with
**| respect to other collections of such rows in the same row scope.  Also by
**| convention, at least one table in a row scope has a table kind purporting
**| to contain ALL the rows that belong in that row scope, so that at least
**| one table exists that allows all rows in a scope to be interated over.
**| (A table with zero table kind is never allowed because this would make it
**| ambiguous to use a zero table kind when iterating over tables in a port to
**| indicate that all table kinds in a row scope should be seen by a cursor.)
**|
**|| port: every table is considered part of some port that contains the
**| table, so that closing the containing port will cause the table to be
**| indirectly closed as well.  We make it easy to get the containing port for
**| a table, because the port supports important semantic interfaces that will
**| affect how content in table is presented; the most important port context
**| that affects a table is specified by the set of token to string mappings
**| that affect all tokens used throughout the database, and which drive the
**| meanings of row scope, table kind, cell columns, etc.
**|
**|| cursor: a cursor that iterates over the rows in this table, where rows
**| have zero-based index positions from zero to count-1.  Making a cursor
**| with negative position will next iterate over the first row in the table.
**|
**|| position: given any position from zero to count-1, a table will return
**| the row ID and row scope for the row at that position.  (One can use the
**| GetRowAllCells() method to read that row, or else use a row cursor to both
**| get the row at some position and read its content at the same time.)  The
**| position depends on whether a table is sorted, and upon the actual sort.
**| Note that moving a row's position is only possible in unsorted tables.
**|
**|| row set: every table contains a collection of rows, where a member row is
**| referenced by the table using the row ID and row scope for the row.  No
**| single table owns a given row instance, because rows are effectively ref-
**| counted and destroyed only when the last table removes a reference to that
**| particular row.  (But a row can be emptied of all content no matter how
**| many refs exist, and this might be the next best thing to destruction.)
**| Once a row exists in a least one table (after NewRow() is called), then it
**| can be added to any other table by calling AddRow(), or removed from any
**| table by calling CutRow(), or queried as a member by calling HasRow().  A
**| row can only be added to a table once, and further additions do nothing and
**| complain not at all.  Cutting a row from a table only does something when
**| the row was actually a member, and otherwise does nothing silently.
**|
**|| row ref count: one can query the number of tables (and or cells)
**| containing a row as a member or a child.
**|
**|| row content: one can access or modify the cell content in a table's row
**| by moving content to or from an instance of nsIMdbRow.  Note that nsIMdbRow
**| never represents the actual row inside a table, and this is the reason
**| why nsIMdbRow instances do not have row IDs or row scopes.  So an instance
**| of nsIMdbRow always and only contains a snapshot of some or all content in
**| past, present, or future persistent row inside a table.  This means that
**| reading and writing rows in tables has strictly copy semantics, and we
**| currently do not plan any exceptions for specific performance reasons.
**|
**|| sorting: note all rows are assumed sorted by row ID as a secondary
**| sort following the primary column sort, when table rows are sorted.
**|
**|| indexes:
|*/
public interface nsIMdbTable extends nsIMdbCollection { // a collection of rows
  
  // { ===== begin nsIMdbTable methods =====
  
  // { ----- begin attribute methods -----
  public int GetTableKind(nsIMdbEnv ev);
  public int GetRowScope(nsIMdbEnv ev);
  
  // } ----- end attribute methods -----
  
  // { ----- begin cursor methods -----
  public nsIMdbTableRowCursor GetTableRowCursor( // make a cursor, starting iteration at inRowPos
                                                nsIMdbEnv ev, // context
                                                int inRowPos); // zero-based ordinal position of row in table
  // acquire new cursor instance
  // } ----- end row position methods -----
  
  // { ----- begin row position methods -----
  public mdbOid RowPosToOid( // get row member for a table position
                            nsIMdbEnv ev, // context
                            int inRowPos); // zero-based ordinal position of row in table
  // row oid at the specified position
  
  // Note that HasRow() performs the inverse oid->pos mapping
  // } ----- end row position methods -----
  
  // { ----- begin oid set methods -----
  public void AddOid( // make sure the row with inOid is a table member 
                     nsIMdbEnv ev, // context
                     final mdbOid inOid); // row to ensure membership in table
  
  public int HasOid( // test for the table position of a row member
                    nsIMdbEnv ev, // context
                    final mdbOid inOid); // row to find in table
  // zero-based ordinal position of row in table
  
  public void CutOid( // make sure the row with inOid is not a member 
                     nsIMdbEnv ev, // context
                     final mdbOid inOid); // row to remove from table
  // } ----- end oid set methods -----
  
  // { ----- begin row set methods -----
  public nsIMdbRow NewRow( // create a new row instance in table
                          nsIMdbEnv ev, // context
                          mdbOid ioOid); // please use zero (unbound) rowId for db-assigned IDs
  // create new row
  
  public void AddRow( // make sure the row with inOid is a table member 
                     nsIMdbEnv ev, // context
                     nsIMdbRow ioRow); // row to ensure membership in table
  
  public int HasRow( // test for the table position of a row member
                    nsIMdbEnv ev, // context
                    nsIMdbRow ioRow); // row to find in table
  // zero-based ordinal position of row in table
  
  public void CutRow( // make sure the row with inOid is not a member 
                     nsIMdbEnv ev, // context
                     nsIMdbRow ioRow); // row to remove from table
  // } ----- end row set methods -----
  
  // { ----- begin searching methods -----
  public mdbRange SearchOneSortedColumn( // search only currently sorted col
                                        nsIMdbEnv ev, // context
                                        final String inPrefix); // content to find as prefix in row's column cell
  // range of matching rows
  
  public nsIMdbThumb SearchManyColumns( // search variable number of sorted cols
                                       nsIMdbEnv ev, // context
                                       final String inPrefix, // content to find as prefix in row's column cell
                                       mdbSearch ioSearch); // columns to search and resulting ranges
  // acquire thumb for incremental search
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the search will be finished.  Until that time, the ioSearch argument
  // is assumed referenced and used by the thumb; one should not inspect any
  // output results in ioSearch until after the thumb is finished with it.
  // } ----- end searching methods -----
  
  // { ----- begin hinting methods -----
  public void SearchColumnsHint( // advise re future expected search cols  
                                nsIMdbEnv ev, // context
                                final mdbColumnSet inColumnSet); // columns likely to be searched
  
  public void SortColumnsHint( // advise re future expected sort columns  
                              nsIMdbEnv ev, // context
                              final mdbColumnSet inColumnSet); // columns for likely sort requests
  
  public void StartBatchChangeHint( // advise before many adds and cuts  
                                   nsIMdbEnv ev, // context
                                   final String inLabel); // intend unique address to match end call
  // If batch starts nest by virtue of nesting calls in the stack, then
  // the address of a local variable makes a good batch start label that
  // can be used at batch end time, and such addresses remain unique.
  public void EndBatchChangeHint( // advise before many adds and cuts  
                                 nsIMdbEnv ev, // context
                                 final String inLabel); // label matching start label
  // Suppose a table is maintaining one or many sort orders for a table,
  // so that every row added to the table must be inserted in each sort,
  // and every row cut must be removed from each sort.  If a db client
  // intends to make many such changes before needing any information
  // about the order or positions of rows inside a table, then a client
  // might tell the table to start batch changes in order to disable
  // sorting of rows for the interim.  Presumably a table will then do
  // a full sort of all rows at need when the batch changes end, or when
  // a surprise request occurs for row position during batch changes.
  // } ----- end hinting methods -----
  
  // { ----- begin sorting methods -----
  // sorting: note all rows are assumed sorted by row ID as a secondary
  // sort following the primary column sort, when table rows are sorted.
  
  public boolean
    CanSortColumn( // query which column is currently used for sorting
                  nsIMdbEnv ev, // context
                  int inColumn); // column to query sorting potential
  // whether the column can be sorted
  
  public nsIMdbThumb
    NewSortColumn( // change the column used for sorting in the table
                  nsIMdbEnv ev, // context
                  int inColumn); // requested new column for sorting table
  //mdb_column* outActualColumn, // column actually used for sorting
  // acquire thumb for incremental table resort
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the sort will be finished. 
  
  public nsIMdbThumb
    NewSortColumnWithCompare( // change sort column with explicit compare
                             nsIMdbEnv ev, // context
                             nsIMdbCompare ioCompare, // explicit interface for yarn comparison
                             int inColumn); // requested new column for sorting table
  // mdb_column* outActualColumn, // column actually used for sorting
  // acquire thumb for incremental table resort
  // Note the table will hold a reference to inCompare if this object is used
  // to sort the table.  Until the table closes, callers can only force release
  // of the compare object by changing the sort (by say, changing to unsorted).
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the sort will be finished. 
  
  public int GetSortColumn( // query which col is currently sorted
                           nsIMdbEnv ev); // context
  // col the table uses for sorting (or zero)
  
  
  public nsIMdbThumb CloneSortColumn( // view same table with a different sort
                                     nsIMdbEnv ev, // context
                                     int inColumn); // requested new column for sorting table
  // acquire thumb for incremental table clone
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then call nsIMdbTable::ThumbToCloneSortTable() to get the table instance.
  
  public nsIMdbTable
    ThumbToCloneSortTable( // redeem complete CloneSortColumn() thumb
                          nsIMdbEnv ev, // context
                          nsIMdbThumb ioThumb); // thumb from CloneSortColumn() with done status
  // new table instance (or old if sort unchanged)
  // } ----- end sorting methods -----
  
  // { ----- begin moving methods -----
  // moving a row does nothing unless a table is currently unsorted
  
  public int MoveOid( // change position of row in unsorted table
                     nsIMdbEnv ev, // context
                     final mdbOid inOid,  // row oid to find in table
                     int inHintFromPos, // suggested hint regarding start position
                     int inToPos);       // desired new position for row inRowId
  // actual new position of row in table
  
  public int MoveRow( // change position of row in unsorted table
                     nsIMdbEnv ev, // context
                     nsIMdbRow ioRow,  // row oid to find in table
                     int inHintFromPos, // suggested hint regarding start position
                     int inToPos);       // desired new position for row inRowId
  // actual new position of row in table
  // } ----- end moving methods -----
  
  // { ----- begin index methods -----
  public nsIMdbThumb AddIndex( // create a sorting index for column if possible
                              nsIMdbEnv ev, // context
                              int inColumn); // the column to sort by index
  // acquire thumb for incremental index building
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the index addition will be finished.
  
  public nsIMdbThumb CutIndex( // stop supporting a specific column index
                              nsIMdbEnv ev, // context
                              int inColumn); // the column with index to be removed
  // acquire thumb for incremental index destroy
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the index removal will be finished.
  
  public boolean HasIndex( // query for current presence of a column index
                          nsIMdbEnv ev, // context
                          int inColumn); // the column to investigate
  // whether column has index for this column
  
  
  public void EnableIndexOnSort( // create an index for col on first sort
                                nsIMdbEnv ev, // context
                                int inColumn); // the column to index if ever sorted
  
  public boolean QueryIndexOnSort( // check whether index on sort is enabled
                                  nsIMdbEnv ev, // context
                                  int inColumn); // the column to investigate
  // whether column has index-on-sort enabled
  
  public void DisableIndexOnSort( // prevent future index creation on sort
                                 nsIMdbEnv ev, // context
                                 int inColumn); // the column to index if ever sorted
  // } ----- end index methods -----
  
  // } ===== end nsIMdbTable methods =====
};

