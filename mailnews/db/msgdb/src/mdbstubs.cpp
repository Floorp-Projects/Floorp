/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "mdb.h"
#include "stdio.h"
// for LINEBREAK
#include "fe_proto.h"

nsIMdbFactory *NS_NewIMdbFactory()
{
	return new nsIMdbFactory;
}

nsIMdbFactory *MakeMdbFactory()
{
	return NS_NewIMdbFactory();
}

mdb_err	   nsIMdbFactory::ThumbToOpenStore( // redeem completed thumb from OpenFileStore()
    nsIMdbEnv* ev, // context
    nsIMdbThumb* ioThumb, // thumb from OpenFileStore() with done status
    nsIMdbStore** acqStore)  // acquire new db store object
{
	mdb_err ret = 0;
	nsIMdbStore *resultStore;

	resultStore = new nsIMdbStore;
	resultStore->m_fileStream = ioThumb->m_fileStream;
	resultStore->m_backingFile = ioThumb->m_backingFile;
	*acqStore = resultStore;
	// this means its an existing store that we have to load.
	if (resultStore->m_fileStream)
	{
		ret = resultStore->ReadAll(ev);
	}
   return ret;
}

mdb_err nsIMdbFactory::CreateNewFileStore( // create a new db with minimal content
	nsIMdbEnv* ev, // context
	nsIMdbHeap* ioHeap, // can be nil to cause ev's heap attribute to be used
	const char* inFilePath, // name of file which should not yet exist
	const mdbOpenPolicy* inOpenPolicy, // runtime policies for using db
	nsIMdbStore** acqStore)
{
	printf("new file store for %s\n", inFilePath);
	nsIMdbStore *resultStore;

	resultStore = new nsIMdbStore;
	resultStore->m_backingFile = inFilePath;
	resultStore->m_fileStream = NULL;
	*acqStore = resultStore;
	return 0;
}

mdb_err nsIMdbStore::SmallCommit( // save minor changes if convenient and uncostly
   nsIMdbEnv* ev)
{
   return 0;
}

mdb_err nsIMdbStore::LargeCommit( // save important changes if at all possible
nsIMdbEnv* ev, // context
nsIMdbThumb** acqThumb) 
{
    return WriteAll(ev, acqThumb);
}

mdb_err nsIMdbStore::SessionCommit( // save all changes if large commits delayed
nsIMdbEnv* ev, // context
nsIMdbThumb** acqThumb)
{
    return WriteAll(ev, acqThumb);
}

mdb_err
nsIMdbStore::CompressCommit( // commit and make db physically smaller if possible
nsIMdbEnv* ev, // context
nsIMdbThumb** acqThumb)
{
    return WriteAll(ev, acqThumb);
}

mdb_err
   nsIMdbStore::WriteAll(nsIMdbEnv* ev, // context
nsIMdbThumb** acqThumb)
{
	*acqThumb = new nsIMdbThumb;

	m_fileStream = new nsIOFileStream(nsFileSpec(m_backingFile));
        if (m_fileStream == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
	WriteTokenList();
	WriteTableList();
	delete m_fileStream;  // delete closes the stream.
	m_fileStream = NULL;
	return NS_OK;
}

mdb_err
   nsIMdbStore::ReadAll(nsIMdbEnv* ev)
{
	ReadTokenList();
	ReadTableList();
	return NS_OK;
}

const char *kStartTokenList = "token list";
const char *kEndTokenList = "end token list";

mdb_err nsIMdbStore::WriteTokenList()
{
   *m_fileStream << kStartTokenList;
   *m_fileStream << MSG_LINEBREAK;
   PRInt32 i;

   for (i = 0; i < m_tokenStrings.Count(); i++)
   {
	   nsString outputNSString;
	   char *outputString;
	   m_tokenStrings.StringAt(i, outputNSString);
	   outputString = outputNSString.ToNewCString();

	   *m_fileStream << outputString;
	   delete [] outputString;
	   *m_fileStream << MSG_LINEBREAK;
   }
   *m_fileStream << kEndTokenList;
   *m_fileStream << MSG_LINEBREAK;
   return 0;
}

// read in the token strings.
mdb_err nsIMdbStore::ReadTokenList()
{
   char readlineBuffer[100];

   m_tokenStrings.Clear();

   m_fileStream->readline(readlineBuffer, sizeof(readlineBuffer));
   if (strcmp(readlineBuffer, kStartTokenList))
	   return -1;

   while (TRUE)
   {
	   if (m_fileStream->eof())
		   break;

		m_fileStream->readline(readlineBuffer, sizeof(readlineBuffer));

		if (!strcmp(readlineBuffer, kEndTokenList))
			break;

		nsString unicodeStr(readlineBuffer);
		m_tokenStrings.AppendString(unicodeStr);
   }
   return 0;
}

const char *kStartTableList = "table list";
const char *kEndTableList = "end table list";

mdb_err nsIMdbStore::WriteTableList()
{
	*m_fileStream << kStartTableList;
	*m_fileStream << MSG_LINEBREAK;

	PRInt32 i;

	*m_fileStream << (long) m_tables.Count();
	*m_fileStream << MSG_LINEBREAK;
	for (i = 0; i < m_tables.Count(); i++)
	{
	   nsIMdbTable *table = (nsIMdbTable *) m_tables.ElementAt(i);
	   table->Write();
	}
	*m_fileStream << kEndTableList;
	*m_fileStream << MSG_LINEBREAK;
	return 0;
}

mdb_err nsIMdbStore::ReadTableList()
{
	char readlineBuffer[100];

	m_tables.Clear();

	m_fileStream->readline(readlineBuffer, sizeof(readlineBuffer));
	if (strcmp(readlineBuffer, kStartTableList))
		return -1;

	m_fileStream->readline(readlineBuffer, sizeof(readlineBuffer));
	PRInt32 numTables = atoi(readlineBuffer);

	for (PRInt32 i = 0; i < numTables; i++)
	{
		nsIMdbTable *table = new nsIMdbTable(this, /* we don't know the kind yet*/ 0);
		if (table)
		{
			m_tables.AppendElement(table);
			table->Read();
		}
	}
	return 0;
}

mdb_err nsIMdbStore::NewTable( // make one new table of specific type
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope,    // row scope for row ids
    mdb_kind inTableKind,    // the type of table to access
    mdb_bool inMustBeUnique, // whether store can hold only one of these
    nsIMdbTable** acqTable)      // acquire scoped collection of rows

{
  *acqTable = new nsIMdbTable(this, inTableKind);
  if (*acqTable == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
  (*acqTable)->m_Oid.mOid_Id  = 1;
  (*acqTable)->m_Oid.mOid_Scope = inRowScope;
  PRBool inserted = m_tables.AppendElement(*acqTable);
  return inserted ? NS_OK : NS_ERROR_FAILURE;
}

nsIMdbPort::nsIMdbPort() : m_backingFile("")
{
}

mdb_err nsIMdbPort::GetTable( // access one table with specific oid
nsIMdbEnv* ev, // context
const mdbOid* inOid,  // hypothetical table oid
nsIMdbTable** acqTable)
{
	mdb_err result = -1;
	nsIMdbTable *retTable = NULL;
	*acqTable = NULL;
	for (PRInt32 i = 0; i < m_tables.Count(); i++)
	{
		nsIMdbTable *table = (nsIMdbTable *) m_tables[i];
		if (table->m_Oid.mOid_Id == inOid->mOid_Id 
			&& table->m_Oid.mOid_Scope == inOid->mOid_Scope)
		{
			retTable = table;
			table->AddRef();
			*acqTable = table;
			result = 0;
			break;
		}
	}
	return result;
}
 
mdb_err nsIMdbPort::StringToToken (  nsIMdbEnv* ev, // context
    const char* inTokenName, // Latin1 string to tokenize if possible
    mdb_token* outToken)
{
	nsString unicodeStr(inTokenName);
	PRInt32 tokenPos = m_tokenStrings.IndexOf(unicodeStr);
	if (tokenPos >= 0)
	{
		*outToken = tokenPos;
	}
	else
	{
		m_tokenStrings.AppendString(unicodeStr);
		*outToken = m_tokenStrings.Count() - 1;
	}
//	*outToken = (mdb_token) inTokenName;
	return 0;
}
 
mdb_err nsIMdbPort::GetTableKind (
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope,      // row scope for row ids
    mdb_kind inTableKind,      // the type of table to access
    mdb_count* outTableCount, // current number of such tables
    mdb_bool* outMustBeUnique, // whether port can hold only one of these
    nsIMdbTable** acqTable) 
{
	nsIMdbTable *retTable = NULL;

	mdb_count tableCount = 0;
	for (PRInt32 i = 0; i < m_tables.Count(); i++)
	{
		nsIMdbTable *table = (nsIMdbTable *) m_tables[i];
		if (table->m_kind == inTableKind)
		{
			tableCount++;
			if (!*acqTable)
			{
				retTable = table;
				table->AddRef();
				*acqTable = table;
			}
		}
	}
	*outTableCount = tableCount;

	if (! retTable )
		*acqTable = new nsIMdbTable (this, inTableKind);
	return 0;
}

nsIMdbTable::nsIMdbTable(nsIMdbPort* owner, mdb_kind kind)
{
	m_owningPort = owner;
	m_kind = kind;
}

mdb_err nsIMdbTable::HasOid( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    const mdbOid* inOid, // row to find in table
    mdb_pos* outPos)
{
	mdb_pos iteratePos;
	*outPos = -1;

	for (iteratePos = 0; iteratePos  < m_rows.Count(); iteratePos++)
	{
		nsIMdbRow *row = (nsIMdbRow *) m_rows.ElementAt(iteratePos);
		if (row && row->m_oid.mOid_Id == inOid->mOid_Id)
		{
			*outPos = iteratePos;
			break;
		}
	}
	return 0;
}

mdb_err nsIMdbTable::GetTableRowCursor( // make a cursor, starting iteration at inRowPos
    nsIMdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    nsIMdbTableRowCursor** acqCursor)
{
    *acqCursor = new nsIMdbTableRowCursor;
	if (*acqCursor == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;
    (*acqCursor)->SetTable(ev, this);
    (*acqCursor)->m_pos = inRowPos;
    return NS_OK;
}

mdb_err nsIMdbTable::Write()

{
	nsIOFileStream *stream = m_owningPort->m_fileStream;
	*stream << m_kind;
	*stream << ",";
	*stream << m_Oid.mOid_Id;
	*stream << ",";
	*stream << m_Oid.mOid_Scope;
	*stream << ",";
	*stream << (long) m_rows.Count();
	*stream << MSG_LINEBREAK;

	PRInt32 i;

	for (i = 0; i < m_rows.Count(); i++)
	{
		nsIMdbRow *row = (nsIMdbRow *) m_rows.ElementAt(i);
		if (row)
			row->Write(stream);
	}
	return 0;
}

mdb_err nsIMdbTable::Read()

{
	char lineBuf[100];

	nsIOFileStream *stream = m_owningPort->m_fileStream;

	stream->readline(lineBuf, sizeof(lineBuf));
	m_kind = atoi(lineBuf);

	char *p;
	for (p = lineBuf; *p; p++)
	{
		if (*p == ',')
		{
			p++;
			break;
		}
	}
	PRInt32 oid = atoi(p);
	m_Oid.mOid_Id = oid;

	for (p; *p; p++)
	{
		if (*p == ',')
		{
			p++;
			break;
		}
	}

	PRInt32 scope = atoi(p);
	m_Oid.mOid_Scope = scope;

	for (p; *p; p++)
	{
		if (*p == ',')
			break;
	}

	PRInt32 numRows = atoi(p + 1);

	for (PRInt32 i = 0; i < numRows; i++)
	{
		nsIMdbRow *row = new nsIMdbRow(this, m_owningPort);
		if (row)
		{
			m_rows.AppendElement(row);
			row->Read(stream);
		}
	}

	return NS_OK;
}

 mdb_err nsIMdbTableRowCursor::SetTable(nsIMdbEnv* ev, nsIMdbTable* ioTable) 
 {
	 m_table = ioTable;
	 m_pos = -1;
	 return 0;
 }

mdb_err  nsIMdbTableRowCursor::NextRow( // get row cells from table for cells already in row
    nsIMdbEnv* ev, // context
    nsIMdbRow** acqRow, // acquire next row in table
    mdb_pos* outRowPos)
{
	if (m_pos < 0)
		m_pos = 0;

	*outRowPos = m_pos;
	*acqRow = (nsIMdbRow *) m_table->m_rows.ElementAt(m_pos++);
	return *acqRow != nsnull ? NS_OK : NS_ERROR_FAILURE;
}

mdb_err nsIMdbTable::CutRow  ( // make sure the row with inOid is not a member 
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow) 
{
	PRBool found = m_rows.RemoveElement(ioRow);
	if (found)
		ioRow->Release();	// is this right? Who deletes the row?

	return found ? NS_OK : NS_ERROR_FAILURE;
}

mdb_err nsIMdbStore::NewRowWithOid (nsIMdbEnv* ev, // new row w/ caller assigned oid
    const mdbOid* inOid,   // caller assigned oid
    nsIMdbRow** acqRow) 
{
	*acqRow = new nsIMdbRow (NULL, this);
	(*acqRow)->m_oid = *inOid;	
	return 0;
}

mdb_err nsIMdbTable::AddRow ( // make sure the row with inOid is a table member 
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow) 
{
	PRBool inserted = m_rows.AppendElement(ioRow);
	ioRow->m_owningTable = this;
	return inserted ? NS_OK : NS_ERROR_FAILURE;
}

nsIMdbRow::nsIMdbRow(nsIMdbTable *owningTable, nsIMdbPort *owningPort)
{
	m_owningTable = owningTable;
	m_owningPort = owningPort;
}

mdb_err nsIMdbRow::AddColumn( // make sure a particular column is inside row
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // column to add
    const mdbYarn* inYarn)
{
	char *columnName;

	if (m_owningPort)
	{
		nsString columnStr;

		m_owningPort->m_tokenStrings.StringAt(inColumn, columnStr);

		columnName = columnStr.ToNewCString();
#ifdef DEBUG_DB
		printf("adding column %s : %s\n", columnName, inYarn->mYarn_Buf);
#endif
		delete [] columnName;
	}
	mdbCellImpl	newCell;
	nsIMdbCell *existingCell = NULL;

	newCell.m_column = inColumn;
	GetCell(ev, inColumn, &existingCell);
	if (existingCell)
	{
		mdbCellImpl *evilCastCell = (mdbCellImpl *) existingCell;
		PR_FREEIF(evilCastCell->m_cellValue);
		evilCastCell->m_cellValue = PL_strdup((const char *) inYarn->mYarn_Buf);
	}
	else
	{
		mdbCellImpl *cellToAdd = new mdbCellImpl;
		cellToAdd->m_column = inColumn;
		cellToAdd->m_cellValue = PL_strdup((const char *)inYarn->mYarn_Buf);
		m_cells.AppendCell(*cellToAdd);
	}

	return 0;
}

mdb_err nsIMdbRow::GetCell( // find a cell in this row
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // column to find
    nsIMdbCell** acqCell) 
{
	mdbCellImpl	newCell;

	newCell.m_column = inColumn;
	PRInt32 cellIndex = m_cells.IndexOf(newCell);
	if (cellIndex < 0)
		*acqCell = NULL;
	else 
		*acqCell = m_cells.CellAt(cellIndex);
	return 0;
}

mdb_err nsIMdbRow::Write(nsIOFileStream *stream)
{
	mdb_pos iteratePos;

	// write out the number of cells.
	*stream << (long) m_cells.Count();
	*stream << ",";
	*stream << (long) m_oid.mOid_Id;
	*stream << MSG_LINEBREAK;

	for (iteratePos = 0; iteratePos  < m_cells.Count(); iteratePos++)
	{
		mdbCellImpl *cell = (mdbCellImpl *) m_cells.ElementAt(iteratePos);
		if (cell)
			cell->Write(stream);
	}
	return 0;
}

mdb_err nsIMdbRow::Read(nsIOFileStream *stream)
{
	char	line[50];

	stream->readline(line, sizeof(line));

	PRInt32 numCells = atoi(line);

	char *p;
	for (p = line; *p; p++)
	{
		if (*p == ',')
			break;
	}

	m_oid.mOid_Id = atoi(p + 1);
#ifdef DEBUG_DB
	printf("reading row id = %d\n", m_oid.mOid_Id);
#endif
	for (PRInt32 i = 0; i < numCells; i++)
	{
		mdbCellImpl *cell = new mdbCellImpl;
		if (cell)
		{
			cell->Read(stream);
			m_cells.AppendCell(*cell);
#ifdef DEBUG_DB
			nsString columnStr;

			m_owningPort->m_tokenStrings.StringAt(cell->m_column, columnStr);
			char *column = columnStr.ToNewCString();
			printf("column = %s value = %s\n", (column) ? column : "", cell->m_cellValue);
			delete [] column;
#endif

		}
	}

	return 0;
}

mdb_err nsIMdbCollection::GetOid   (nsIMdbEnv* ev,
    mdbOid* outOid) 
{
	*outOid = m_Oid;
	return 0;
}

mdb_err nsIMdbTableRowCursor::NextRowOid  ( // get row id of next row in the table
    nsIMdbEnv* ev, // context
    mdbOid* outOid, // out row oid
    mdb_pos* outRowPos)
{
	nsIMdbRow *curRow = NULL;
	if (m_pos < 0)
		m_pos = 0;

	*outRowPos = m_pos;
	if (m_table)
		curRow = (nsIMdbRow *) m_table->m_rows.ElementAt(m_pos++);
	if (curRow)
		*outOid = curRow->m_oid;
	else
		*outRowPos = -1;
	return 0;
 }

mdb_err nsIMdbBlob::AliasYarn(nsIMdbEnv* ev, 
    mdbYarn* outYarn)
{
	return 0;
}

mdb_err nsIMdbObject::CutStrongRef(nsIMdbEnv* ev)
{
	return 0;
}

mdb_err nsIMdbFactory::MakeEnv(nsIMdbHeap* ioHeap, nsIMdbEnv** acqEnv)
{
	return 0;
}

nsIMdbThumb::nsIMdbThumb() : m_backingFile("")
{
	m_fileStream = NULL;
}

mdb_err nsIMdbThumb::DoMore(nsIMdbEnv* ev,
    mdb_count* outTotal,    // total somethings to do in operation
    mdb_count* outCurrent,  // subportion of total completed so far
    mdb_bool* outDone,      // is operation finished?
    mdb_bool* outBroken     // is operation irreparably dead and broken?
  ) 
{
	*outDone = 1;
	return 0;
}

mdb_err nsIMdbFactory::OpenFileStore(class nsIMdbEnv *, nsIMdbHeap* , char const *fileName,struct mdbOpenPolicy const *,class nsIMdbThumb **retThumb)
{

	*retThumb = new nsIMdbThumb;
	nsFilePath filePath(fileName);
	(*retThumb)->m_fileStream = new nsIOFileStream(nsFileSpec(filePath));
	(*retThumb)->m_backingFile = fileName;

	return 0;
}

mdbCellImpl::mdbCellImpl()
{
	m_cellValue = NULL;
	m_column = 0;
}

mdbCellImpl::mdbCellImpl(const mdbCellImpl &anotherCell)
{
	m_column = anotherCell.m_column;
	m_cellValue = anotherCell.m_cellValue;
	NS_INIT_REFCNT();
}

mdbCellImpl& 
mdbCellImpl::operator=(const mdbCellImpl& other)
{
	m_column = other.m_column;
	m_cellValue = PL_strdup((const char *) other.m_cellValue);
	return *this;
}

PRBool	mdbCellImpl::Equals(const mdbCellImpl& other)
{
	// I think equality is just whether the columns are the same...
	return (m_column == other.m_column);
}

mdb_err mdbCellImpl::Write(nsIOFileStream *stream)
{
	*stream << m_column;
	*stream << "=";
	if (m_cellValue)
		*stream << m_cellValue;
	*stream << MSG_LINEBREAK;
	return 0;
}

const int kLineBufLength = 1000;

mdb_err mdbCellImpl::Read(nsIOFileStream *stream)
{
	char line[kLineBufLength];

	stream->readline(line, kLineBufLength);
	m_column = atoi(line);

	char *p;
	for (p = line; *p; p++)
	{
		if (*p == '=')
			break;
	}
	m_cellValue = strdup(p + 1);
	return 0;
}

mdb_err mdbCellImpl::AliasYarn(nsIMdbEnv* ev, 
    mdbYarn* outYarn)
{
	outYarn->mYarn_Buf = m_cellValue;
	outYarn->mYarn_Size = PL_strlen(m_cellValue) + 1;
	outYarn->mYarn_Fill = outYarn->mYarn_Size;
	return 0;
}

//----------------------------------------------------------------
// MDBCellArray

MDBCellArray::MDBCellArray(void)
  : nsVoidArray()
{
}

MDBCellArray::~MDBCellArray(void)
{
  Clear();
}

MDBCellArray& 
MDBCellArray::operator=(const MDBCellArray& other)
{
  if (nsnull != mArray) {
    delete mArray;
  }
  PRInt32 otherCount = other.mCount;
  mArraySize = otherCount;
  mCount = otherCount;
  if (0 < otherCount) {
    mArray = new void*[otherCount];
    while (0 <= --otherCount) {
      mdbCellImpl* otherCell = (mdbCellImpl*)(other.mArray[otherCount]);
      mArray[otherCount] = new mdbCellImpl(*otherCell);
    }
  } else {
    mArray = nsnull;
  }
  return *this;
}

void 
MDBCellArray::CellAt(PRInt32 aIndex, mdbCellImpl& aCell) const
{
  mdbCellImpl* cell = (mdbCellImpl*)nsVoidArray::ElementAt(aIndex);
  if (nsnull != cell) {
    aCell = *cell;
  }
  else {
    aCell.m_cellValue = 0;
  }
}

mdbCellImpl*
MDBCellArray::CellAt(PRInt32 aIndex) const
{
  return (mdbCellImpl*)nsVoidArray::ElementAt(aIndex);
}

PRInt32 
MDBCellArray::IndexOf(const mdbCellImpl& aPossibleCell) const
{
  void** ap = mArray;
  void** end = ap + mCount;
  while (ap < end) {
    mdbCellImpl* cell = (mdbCellImpl*)*ap;
    if (cell->Equals(aPossibleCell)) {
      return ap - mArray;
    }
    ap++;
  }
  return -1;
}


PRBool 
MDBCellArray::InsertCellAt(const mdbCellImpl& aCell, PRInt32 aIndex)
{
  mdbCellImpl* cell = new mdbCellImpl(aCell);
  if (nsVoidArray::InsertElementAt(cell, aIndex)) {
    return PR_TRUE;
  }
  delete cell;
  return PR_FALSE;
}

PRBool
MDBCellArray::ReplaceCellAt(const mdbCellImpl& aCell, PRInt32 aIndex)
{
  mdbCellImpl* cell = (mdbCellImpl*)nsVoidArray::ElementAt(aIndex);
  if (nsnull != cell) {
    *cell = aCell;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool 
MDBCellArray::RemoveCell(const mdbCellImpl& aCell)
{
  PRInt32 index = IndexOf(aCell);
  if (-1 < index) {
    return RemoveCellAt(index);
  }
  return PR_FALSE;
}

PRBool MDBCellArray::RemoveCellAt(PRInt32 aIndex)
{
  mdbCellImpl* cell = CellAt(aIndex);
  if (nsnull != cell) {
    nsVoidArray::RemoveElementAt(aIndex);
    delete cell;
    return PR_TRUE;
  }
  return PR_FALSE;
}

void 
MDBCellArray::Clear(void)
{
  PRInt32 index = mCount;
  while (0 <= --index) {
    mdbCellImpl* cell = (mdbCellImpl*)mArray[index];
    delete cell;
  }
  nsVoidArray::Clear();
}



PRBool 
MDBCellArray::EnumerateForwards(MDBCellArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = -1;
  PRBool  running = PR_TRUE;

  while (running && (++index < mCount)) {
    running = (*aFunc)(*((mdbCellImpl*)mArray[index]), aData);
  }
  return running;
}

PRBool 
MDBCellArray::EnumerateBackwards(MDBCellArrayEnumFunc aFunc, void* aData)
{
  PRInt32 index = mCount;
  PRBool  running = PR_TRUE;

  while (running && (0 <= --index)) {
    running = (*aFunc)(*((mdbCellImpl*)mArray[index]), aData);
  }
  return running;
}

