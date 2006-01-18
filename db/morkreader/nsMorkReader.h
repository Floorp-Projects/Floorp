/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMorkReader_h_
#define nsMorkReader_h_

#include "nsDataHashtable.h"
#include "nsILineInputStream.h"

// The nsMorkReader object allows a consumer to read in a mork-format
// file and enumerate the rows that it contains.  It does not provide
// any functionality for modifying mork tables.

// References:
//  http://www.mozilla.org/mailnews/arch/mork/primer.txt
//  http://www.mozilla.org/mailnews/arch/mork/grammar.txt
//  http://www.jwz.org/hacks/mork.pl

class nsMorkReader
{
 public:
  typedef nsDataHashtable<nsCStringHashKey,nsCString> StringMap;

  // Enumerator callback type for processing column ids.
  // A column id is a short way to reference a particular column in the table.
  // These column ids can be used to look up cell values when enumerating rows.
  // columnID is the table-unique column id
  // name is the name of the column
  // userData is the opaque pointer passed to EnumerateColumns()
  // The callback can return PL_DHASH_NEXT to continue enumerating,
  // or PL_DHASH_STOP to stop.
  typedef PLDHashOperator
  (*PR_CALLBACK ColumnEnumerator)(const nsACString &columnID,
                                  nsCString name,
                                  void *userData);

  // Enumerator callback type for processing table rows.
  // A row contains cells.  Each cell specifies a column id, and the value
  // for the column for that row.
  // rowID is the table-unique row id
  // values contains the cell values, keyed by column id.
  //   You should call NormalizeValue() on any cell value that you plan to use.
  // userData is the opaque pointer passed to EnumerateRows()
  // The callback can return PL_DHASH_NEXT to continue enumerating,
  // or PL_DHASH_STOP to stop.
  typedef PLDHashOperator
  (*PR_CALLBACK RowEnumerator)(const nsACString &rowID, 
                               const StringMap *values,
                               void *userData);

  // Initialize the importer object's data structures
  nsresult Init();

  // Read in the given mork file
  // Note: currently, only single-table mork files are supported
  nsresult Read(nsIFile *aFile);

  // Enumerate the columns in the current table.
  void EnumerateColumns(ColumnEnumerator aCallback, void *aUserData) const;

  // Enumerate the rows in the current table.
  void EnumerateRows(RowEnumerator aCallback, void *aUserData) const;

  // Get the "meta row" for the table.  Each table has at most one meta row,
  // which records information about the table.  Like normal rows, the
  // meta row is a collection of column id / value pairs.
  const StringMap& GetMetaRow() const { return mMetaRow; }

  // Normalizes the cell value (resolves references to the value map).
  // aValue is modified in-place.
  void NormalizeValue(nsCString &aValue) const;

  nsMorkReader() {}
  ~nsMorkReader();

private:
  // Parses a line of the file which contains key/value pairs (either
  // the column map or the value map).  Additional lines are read from
  // mStream if the line ends mid-entry.  The pairs are added to aMap.
  nsresult ParseMap(const nsCSubstring &aLine, StringMap *aMap);

  // Parses a line of the file which contains a table or row definition.
  // Additional lines are read from mStream of the line ends mid-row.
  // An entry is added to mTable using the row ID as the key, which contains
  // a column id -> value map for the row.
  nsresult ParseTable(const nsCSubstring &aLine);

  // Reads a single logical line from mStream into aLine.
  // Any continuation lines are consumed and appended to the line.
  nsresult ReadLine(nsCString &aLine);

  StringMap mColumnMap;
  StringMap mValueMap;
  StringMap mMetaRow;
  nsDataHashtable<nsCStringHashKey,StringMap*> mTable;
  nsCOMPtr<nsILineInputStream> mStream;
};

#endif // nsMorkReader_h_
