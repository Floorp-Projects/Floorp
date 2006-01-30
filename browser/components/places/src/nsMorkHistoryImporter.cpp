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

#include "nsMorkHistoryImporter.h"
#include "nsNavHistory.h"
#include "mozStorageHelper.h"
#include "prprf.h"
#include "nsNetUtil.h"

NS_IMPL_ISUPPORTS1(nsMorkHistoryImporter, nsIMorkHistoryImporter)

// Columns for entry (non-meta) history rows
enum {
  kURLColumn,
  kNameColumn,
  kVisitCountColumn,
  kHiddenColumn,
  kTypedColumn,
  kLastVisitColumn,
  kColumnCount // keep me last
};

static const char * const gColumnNames[] = {
  "URL", "Name", "VisitCount", "Hidden", "Typed", "LastVisitDate"
};

struct TableReadClosure
{
  TableReadClosure(nsMorkReader *aReader, nsINavHistoryService *aHistory)
    : reader(aReader), history(aHistory), swapBytes(PR_FALSE)
  {
    voidString.SetIsVoid(PR_TRUE);
  }

  // Backpointers to the reader and history we're operating on
  nsMorkReader *reader;
  nsINavHistoryService *history;

  // A voided string to use for the user title
  nsString voidString;

  // Whether we need to swap bytes (file format is other-endian)
  PRBool swapBytes;

  // Column ids of the columns that we care about
  nsCString columnIDs[kColumnCount];
  nsCString byteOrderColumn;
};

// Enumerator callback to build up the column list
/* static */ PLDHashOperator PR_CALLBACK
nsMorkHistoryImporter::EnumerateColumnsCB(const nsACString &aColumnID,
                                          nsCString aName, void *aData)
{
  TableReadClosure *data = NS_STATIC_CAST(TableReadClosure*, aData);
  for (PRUint32 i = 0; i < kColumnCount; ++i) {
    if (aName.Equals(gColumnNames[i])) {
      data->columnIDs[i].Assign(aColumnID);
      return PL_DHASH_NEXT;
    }
  }
  if (aName.EqualsLiteral("ByteOrder")) {
    data->byteOrderColumn.Assign(aColumnID);
  }
  return PL_DHASH_NEXT;
}

// Reverses the high and low bytes in a PRUnichar buffer.
// This is used if the file format has a different endianness from the
// current architecture.
static void
SwapBytes(PRUnichar *buffer)
{
  for (PRUnichar *b = buffer; *b; ++b) {
    PRUnichar c = *b;
    *b = (c << 8) | (c >> 8);
  }
}

// Enumerator callback to add a table row to the NavHistoryService
/* static */ PLDHashOperator PR_CALLBACK
nsMorkHistoryImporter::AddToHistoryCB(const nsACString &aRowID,
                                      const nsMorkReader::StringMap *aMap,
                                      void *aData)
{
  TableReadClosure *data = NS_STATIC_CAST(TableReadClosure*, aData);
  nsMorkReader *reader = data->reader;
  nsCString values[kColumnCount];
  nsCString *columnIDs = data->columnIDs;

  for (PRInt32 i = 0; i < kColumnCount; ++i) {
    aMap->Get(columnIDs[i], &values[i]);
    reader->NormalizeValue(values[i]);
  }

  // title is really a UTF-16 string at this point
  nsCString &titleC = values[kNameColumn];

  PRUint32 titleLength;
  const char *titleBytes;
  if (titleC.IsEmpty()) {
    titleBytes = "\0";
    titleLength = 0;
  } else {
    titleLength = titleC.Length() / 2;

    // add an extra null byte onto the end, so that the buffer ends
    // with a complete unicode null character.
    titleC.Append('\0');

    // Swap the bytes in the unicode characters if necessary.
    if (data->swapBytes) {
      SwapBytes(NS_REINTERPRET_CAST(PRUnichar*, titleC.BeginWriting()));
    }
    titleBytes = titleC.get();
  }

  const PRUnichar *title = NS_REINTERPRET_CAST(const PRUnichar*, titleBytes);

  PRInt32 err;
  PRInt32 count = values[kVisitCountColumn].ToInteger(&err);
  if (err != 0 || count == 0) {
    count = 1;
  }

  PRTime date;
  if (PR_sscanf(values[kLastVisitColumn].get(), "%lld", &date) != 1) {
    date = -1;
  }

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), values[kURLColumn]);

  if (uri) {
    PRBool isTyped = values[kTypedColumn].EqualsLiteral("1");
    nsINavHistoryService *history = data->history;

    if (date != -1 && count != 0) {
      // We have a last visit date, so we'll be adding a visit on that date.
      // Since that will increment the visit count by 1, we need to initially
      // add the entry with count - 1 visits.
      --count;
    }

    history->SetPageDetails(uri, nsDependentString(title, titleLength),
                            data->voidString, count,
                            values[kHiddenColumn].EqualsLiteral("1"), isTyped);

    if (date != -1) {
      PRInt32 transition = isTyped ? nsINavHistoryService::TRANSITION_TYPED
        : nsINavHistoryService::TRANSITION_LINK;
      // Referrer is not handled at present -- doing this requires adding
      // visits in such an order that we have a visit id for the referring
      // page already.

      PRInt64 visitID;
      history->AddVisit(uri, date, 0, transition, 0, &visitID);
    }
  }
  return PL_DHASH_NEXT;
}

// ImportHistory is the main entry point to the importer.
// It sets up the file stream and loops over the lines in the file to
// parse them, then adds the resulting row set to history.

NS_IMETHODIMP
nsMorkHistoryImporter::ImportHistory(nsIFile *aFile,
                                     nsINavHistoryService *aHistory)
{
  NS_ENSURE_TRUE(aFile && aHistory, NS_ERROR_NULL_POINTER);

  // Check that the file exists before we try to open it
  PRBool exists;
  aFile->Exists(&exists);
  if (!exists) {
    return NS_OK;
  }
  
  // Read in the mork file
  nsMorkReader reader;
  nsresult rv = reader.Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = reader.Read(aFile);
  NS_ENSURE_SUCCESS(rv, rv);

  // Gather up the column ids so we don't need to find them on each row
  TableReadClosure data(&reader, aHistory);
  reader.EnumerateColumns(EnumerateColumnsCB, &data);

  // Determine the byte order from the table's meta-row.
  nsCString byteOrder;
  if (reader.GetMetaRow().Get(data.byteOrderColumn, &byteOrder)) {
    // Note whether the file uses a non-native byte ordering.
    // If it does, we'll have to swap bytes for PRUnichar values.
    reader.NormalizeValue(byteOrder);
#ifdef IS_LITTLE_ENDIAN
    data.swapBytes = !byteOrder.EqualsLiteral("LE");
#else
    data.swapBytes = !byteOrder.EqualsLiteral("BE");
#endif
  }

  // Now add the results to history
  nsNavHistory *history = NS_STATIC_CAST(nsNavHistory*, aHistory);
  mozIStorageConnection *conn = history->GetStorageConnection();
  NS_ENSURE_TRUE(conn, NS_ERROR_NOT_INITIALIZED);
  mozStorageTransaction transaction(conn, PR_FALSE);

  reader.EnumerateRows(AddToHistoryCB, &data);
  return transaction.Commit();
}
