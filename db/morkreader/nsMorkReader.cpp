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
 * The Original Code is the Mork Reader.
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

#include "nsMorkReader.h"
#include "prio.h"
#include "nsNetUtil.h"
#include "nsVoidArray.h"

// A FixedString implementation that can hold 2 80-character lines
class nsCLineString : public nsFixedCString
{
public:
  nsCLineString() : fixed_string_type(mStorage, sizeof(mStorage), 0) {}
  explicit nsCLineString(const substring_type& str)
    : fixed_string_type(mStorage, sizeof(mStorage), 0)
  {
    Assign(str);
  }

private:
  char_type mStorage[160];
};

// Convert a hex character (0-9, A-F) to its corresponding byte value.
// The character pointed to by 'c' is modified in place.
inline PRBool
ConvertChar(char *c)
{
  char c1 = *c;
  if ('0' <= c1 && c1 <= '9') {
    *c = c1 - '0';
    return PR_TRUE;
  }
  if ('A' <= c1 && c1 <= 'F') {
    *c = c1 - 'A' + 10;
    return PR_TRUE;
  }
  return PR_FALSE;
}

// Unescape a Mork value.  Mork uses $xx escaping to encode non-ASCII
// characters.  Additionally, '$' and '\' are backslash-escaped.
// The result of the unescape is in returned into aResult.

static void
MorkUnescape(const nsCSubstring &aString, nsCString &aResult)
{
  PRUint32 len = aString.Length();

  // We optimize for speed over space here -- size the result buffer to
  // the size of the source, which is an upper bound on the size of the
  // unescaped string.
  // FIXME: Mork assume there will never be errors
  if (!EnsureStringLength(aResult, len)) {
    aResult.Truncate();
    return; // out of memory.
  }

  char *result = aResult.BeginWriting();
  const char *source = aString.BeginReading();
  const char *sourceEnd = source + len;

  const char *startPos = nsnull;
  PRUint32 bytes;
  for (; source < sourceEnd; ++source) {
    char c = *source;
    if (c == '\\') {
      if (startPos) {
        bytes = source - startPos;
        memcpy(result, startPos, bytes);
        result += bytes;
        startPos = nsnull;
      }
      if (source < sourceEnd - 1) {
        *(result++) = *(++source);
      }
    } else if (c == '$') {
      if (startPos) {
        bytes = source - startPos;
        memcpy(result, startPos, bytes);
        result += bytes;
        startPos = nsnull;
      }
      if (source < sourceEnd - 2) {
        // Would be nice to use ToInteger() here, but it currently
        // requires a null-terminated string.
        char c2 = *(++source);
        char c3 = *(++source);
        if (ConvertChar(&c2) && ConvertChar(&c3)) {
          *(result++) = ((c2 << 4) | c3);
        }
      }
    } else if (!startPos) {
      startPos = source;
    }
  }
  if (startPos) {
    bytes = source - startPos;
    memcpy(result, startPos, bytes);
    result += bytes;
  }
  aResult.SetLength(result - aResult.BeginReading());
}

nsresult
nsMorkReader::Init()
{
  NS_ENSURE_TRUE(mValueMap.Init(), NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mTable.Init(), NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
DeleteStringArray(const nsCSubstring& aKey,
                  nsTArray<nsCString> *aData,
                  void *aUserArg)
{
  delete aData;
  return PL_DHASH_NEXT;
}

nsMorkReader::~nsMorkReader()
{
  mTable.EnumerateRead(DeleteStringArray, nsnull);
}

struct AddColumnClosure
{
  AddColumnClosure(nsTArray<nsMorkReader::MorkColumn> *a,
                   nsMorkReader::IndexMap *c)
    : array(a), columnMap(c), result(NS_OK) {}

  nsTArray<nsMorkReader::MorkColumn> *array;
  nsMorkReader::IndexMap *columnMap;
  nsresult result;
};

PR_STATIC_CALLBACK(PLDHashOperator)
AddColumn(const nsCSubstring &id, nsCString name, void *userData)
{
  AddColumnClosure *closure = NS_STATIC_CAST(AddColumnClosure*, userData);
  nsTArray<nsMorkReader::MorkColumn> *array = closure->array;

  if (!array->AppendElement(nsMorkReader::MorkColumn(id, name)) ||
      !closure->columnMap->Put(id, array->Length() - 1)) {
    closure->result = NS_ERROR_OUT_OF_MEMORY;
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

nsresult
nsMorkReader::Read(nsIFile *aFile)
{
  nsCOMPtr<nsIFileInputStream> stream =
    do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID);
  NS_ENSURE_TRUE(stream, NS_ERROR_FAILURE);

  nsresult rv = stream->Init(aFile, PR_RDONLY, 0, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  mStream = do_QueryInterface(stream);
  NS_ASSERTION(mStream, "file input stream must impl nsILineInputStream");

  nsCLineString line;
  rv = ReadLine(line);
  if (!line.EqualsLiteral("// <!-- <mdb:mork:z v=\"1.4\"/> -->")) {
    return NS_ERROR_FAILURE; // unexpected file format
  }

  IndexMap columnMap;
  NS_ENSURE_TRUE(columnMap.Init(), NS_ERROR_OUT_OF_MEMORY);

  while (NS_SUCCEEDED(ReadLine(line))) {
    // Trim off leading spaces
    PRUint32 idx = 0, len = line.Length();
    while (idx < len && line[idx] == ' ') {
      ++idx;
    }
    if (idx >= len) {
      continue;
    }

    const nsCSubstring &l = Substring(line, idx);

    // Look at the line to figure out what section type this is
    if (StringBeginsWith(l, NS_LITERAL_CSTRING("< <(a=c)>"))) {
      // Column map.  We begin by creating a hash of column id to column name.
      StringMap columnNameMap;
      NS_ENSURE_TRUE(columnNameMap.Init(), NS_ERROR_OUT_OF_MEMORY);

      rv = ParseMap(l, &columnNameMap);
      NS_ENSURE_SUCCESS(rv, rv);

      // Now that we have the list of columns, we put them into a flat array.
      // Rows will have value arrays of the same size, with indexes that
      // correspond to the columns array.  As we insert each column into the
      // array, we also make an entry in columnMap so that we can look up the
      // index given the column id.
      mColumns.SetCapacity(columnNameMap.Count());

      AddColumnClosure closure(&mColumns, &columnMap);
      columnNameMap.EnumerateRead(AddColumn, &closure);
      if (NS_FAILED(closure.result)) {
        return closure.result;
      }
    } else if (StringBeginsWith(l, NS_LITERAL_CSTRING("<("))) {
      // Value map
      rv = ParseMap(l, &mValueMap);
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (l[0] == '{' || l[0] == '[') {
      // Table / table row
      rv = ParseTable(l, columnMap);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // Don't know, hopefully don't care
    }
  }

  return NS_OK;
}

void
nsMorkReader::EnumerateRows(RowEnumerator aCallback, void *aUserData) const
{
  // Constify the table values
  typedef const nsDataHashtable<IDKey, const nsTArray<nsCString>* > ConstTable;
  NS_REINTERPRET_CAST(ConstTable*, &mTable)->EnumerateRead(aCallback,
                                                           aUserData);
}

// Parses a key/value map of the form
// <(k1=v1)(k2=v2)...>

nsresult
nsMorkReader::ParseMap(const nsCSubstring &aLine, StringMap *aMap)
{
  nsCLineString line(aLine);
  nsCAutoString key;
  nsresult rv = NS_OK;

  // If the first line is the a=c line (column map), just skip over it.
  if (StringBeginsWith(line, NS_LITERAL_CSTRING("< <(a=c)>"))) {
    rv = ReadLine(line);
  }

  for (; NS_SUCCEEDED(rv); rv = ReadLine(line)) {
    PRUint32 idx = 0;
    PRUint32 len = line.Length();
    PRUint32 tokenStart;

    while (idx < len) {
      switch (line[idx++]) {
      case '(':
        // Beginning of a key/value pair
        if (!key.IsEmpty()) {
          NS_WARNING("unterminated key/value pair?");
          key.Truncate(0);
        }

        tokenStart = idx;
        while (idx < len && line[idx] != '=') {
          ++idx;
        }
        key = Substring(line, tokenStart, idx - tokenStart);
        break;
      case '=':
        {
          // Beginning of the value
          if (key.IsEmpty()) {
            NS_WARNING("stray value");
            break;
          }

          tokenStart = idx;
          while (idx < len && line[idx] != ')') {
            if (line[idx] == '\\') {
              ++idx; // skip escaped ')' characters
            }
            ++idx;
          }
          PRUint32 tokenEnd = PR_MIN(idx, len);
          ++idx;

          nsCString value;
          MorkUnescape(Substring(line, tokenStart, tokenEnd - tokenStart),
                       value);
          aMap->Put(key, value);
          key.Truncate(0);
          break;
        }
      case '>':
        // End of the map.
        NS_WARN_IF_FALSE(key.IsEmpty(),
                         "map terminates inside of key/value pair");
        return NS_OK;
      }
    }
  }

  // We ran out of lines and the map never terminated.  This probably indicates
  // a parsing error.
  NS_WARNING("didn't find end of key/value map");
  return NS_ERROR_FAILURE;
}

// Parses a table row of the form [123(^45^67)..]
// (row id 123 has the value with id 67 for the column with id 45).
// A '^' prefix for a column or value references an entry in the column or
// value map.  '=' is used as the separator when the value is a literal.

nsresult
nsMorkReader::ParseTable(const nsCSubstring &aLine, const IndexMap &aColumnMap)
{
  nsCLineString line(aLine);
  const PRUint32 columnCount = mColumns.Length(); // total number of columns

  PRInt32 columnIndex = -1; // column index of the cell we're parsing
  // value array for the row we're parsing
  nsTArray<nsCString> *currentRow = nsnull;
  PRBool inMetaRow = PR_FALSE;

  do {
    PRUint32 idx = 0;
    PRUint32 len = line.Length();
    PRUint32 tokenStart, tokenEnd;

    while (idx < len) {
      switch (line[idx++]) {
      case '{':
        // This marks the beginning of a table section.  There's a lot of
        // junk before the first row that looks like cell values but isn't.
        // Skip to the first '['.
        while (idx < len && line[idx] != '[') {
          if (line[idx] == '{') {
            inMetaRow = PR_TRUE; // the meta row is enclosed in { }
          } else if (line[idx] == '}') {
            inMetaRow = PR_FALSE;
          }
          ++idx;
        }
        break;
      case '[':
        {
          // Start of a new row.  Consume the row id, up to the first '('.
          // Row edits also have a table namespace, separated from the row id
          // by a colon.  We don't make use of the namespace, but we need to
          // make sure not to consider it part of the row id.
          if (currentRow) {
            NS_WARNING("unterminated row?");
            currentRow = nsnull;
          }

          // Check for a '-' at the start of the id.  This signifies that
          // if the row already exists, we should delete all columns from it
          // before adding the new values.
          PRBool cutColumns;
          if (idx < len && line[idx] == '-') {
            cutColumns = PR_TRUE;
            ++idx;
          } else {
            cutColumns = PR_FALSE;
          }

          tokenStart = idx;
          while (idx < len &&
                 line[idx] != '(' &&
                 line[idx] != ']' &&
                 line[idx] != ':') {
            ++idx;
          }
          tokenEnd = idx;
          while (idx < len && line[idx] != '(' && line[idx] != ']') {
            ++idx;
          }
          
          if (inMetaRow) {
            mMetaRow = NewVoidStringArray(columnCount);
            NS_ENSURE_TRUE(mMetaRow, NS_ERROR_OUT_OF_MEMORY);
            currentRow = mMetaRow;
          } else {
            const nsCSubstring& row = Substring(line, tokenStart,
                                                tokenEnd - tokenStart);
            if (!mTable.Get(row, &currentRow)) {
              currentRow = NewVoidStringArray(columnCount);
              NS_ENSURE_TRUE(currentRow, NS_ERROR_OUT_OF_MEMORY);

              NS_ENSURE_TRUE(mTable.Put(row, currentRow),
                             NS_ERROR_OUT_OF_MEMORY);
            }
          }
          if (cutColumns) {
            // Set all of the columns to void
            // (this differentiates them from columns which are empty strings).
            for (PRUint32 i = 0; i < columnCount; ++i) {
              currentRow->ElementAt(i).SetIsVoid(PR_TRUE);
            }
          }
          break;
        }
      case ']':
        // We're done with the row
        currentRow = nsnull;
        inMetaRow = PR_FALSE;
        break;
      case '(':
        {
          if (!currentRow) {
            NS_WARNING("cell value outside of row");
            break;
          }

          NS_WARN_IF_FALSE(columnIndex == -1, "unterminated cell?");

          PRBool columnIsAtom;
          if (line[idx] == '^') {
            columnIsAtom = PR_TRUE;
            ++idx; // this is not part of the column id, advance past it
          } else {
            columnIsAtom = PR_FALSE;
          }
          tokenStart = idx;
          while (idx < len && line[idx] != '^' && line[idx] != '=') {
            if (line[idx] == '\\') {
              ++idx; // skip escaped characters
            }
            ++idx;
          }

          tokenEnd = PR_MIN(idx, len);

          nsCAutoString column;
          const nsCSubstring &colValue =
            Substring(line, tokenStart, tokenEnd - tokenStart);
          if (columnIsAtom) {
            column.Assign(colValue);
          } else {
            MorkUnescape(colValue, column);
          }

          if (!aColumnMap.Get(colValue, &columnIndex)) {
            NS_WARNING("Column not in column map, discarding it");
            columnIndex = -1;
          }
        }
        break;
      case '=':
      case '^':
        {
          if (columnIndex == -1) {
            NS_WARNING("stray ^ or = marker");
            break;
          }

          PRBool valueIsAtom = (line[idx - 1] == '^');
          tokenStart = idx - 1;  // include the '=' or '^' marker in the value
          while (idx < len && line[idx] != ')') {
            if (line[idx] == '\\') {
              ++idx; // skip escaped characters
            }
            ++idx;
          }
          tokenEnd = PR_MIN(idx, len);
          ++idx;

          const nsCSubstring &value =
            Substring(line, tokenStart, tokenEnd - tokenStart);
          if (valueIsAtom) {
            (*currentRow)[columnIndex] = value;
          } else {
            nsCAutoString value2;
            MorkUnescape(value, value2);
            (*currentRow)[columnIndex] = value2;
          }
          columnIndex = -1;
        }
        break;
      }
    }
  } while (currentRow && NS_SUCCEEDED(ReadLine(line)));

  return NS_OK;
}

nsresult
nsMorkReader::ReadLine(nsCString &aLine)
{
  PRBool res;
  nsresult rv = mStream->ReadLine(aLine, &res);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!res) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  while (!aLine.IsEmpty() &&  aLine.Last() == '\\') {
    // There is a continuation for this line.  Read it and append.
    nsCLineString line2;
    rv = mStream->ReadLine(line2, &res);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!res) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    aLine.Truncate(aLine.Length() - 1);
    aLine.Append(line2);
  }

  return NS_OK;
}

void
nsMorkReader::NormalizeValue(nsCString &aValue) const
{
  PRUint32 len = aValue.Length();
  if (len == 0) {
    return;
  }
  const nsCSubstring &str = Substring(aValue, 1, len - 1);
  char c = aValue[0];
  if (c == '^') {
    if (!mValueMap.Get(str, &aValue)) {
      aValue.Truncate(0);
    }
  } else if (c == '=') {
    aValue.Assign(str);
  } else {
    aValue.Truncate(0);
  }
}

/* static */ nsTArray<nsCString>*
nsMorkReader::NewVoidStringArray(PRInt32 aCount)
{
  nsAutoPtr< nsTArray<nsCString> > array(new nsTArray<nsCString>(aCount));
  NS_ENSURE_TRUE(array, nsnull);

  for (PRInt32 i = 0; i < aCount; ++i) {
    nsCString *elem = array->AppendElement();
    NS_ENSURE_TRUE(elem, nsnull);
    elem->SetIsVoid(PR_TRUE);
  }

  return array.forget();
}
