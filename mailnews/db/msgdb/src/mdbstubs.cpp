#include "mdb.h"
#include "stdio.h"

   mdb_err
	   mdbFactory::ThumbToOpenStore( // redeem completed thumb from OpenFileStore()
    mdbEnv* ev, // context
    mdbThumb* ioThumb, // thumb from OpenFileStore() with done status
    mdbStore** acqStore)  // acquire new db store object
   {
	   *acqStore = new mdbStore;
	   return 0;
   }
  
   mdb_err mdbFactory::CreateNewFileStore( // create a new db with minimal content
    mdbEnv* ev, // context
    const char* inFilePath, // name of file which should not yet exist
    const mdbOpenPolicy* inOpenPolicy, // runtime policies for using db
    mdbStore** acqStore)
   {
	   printf("new file store for %s\n", inFilePath);
	   *acqStore = new mdbStore;
	   return 0;
   }

   mdb_err mdbStore::SmallCommit( // save minor changes if convenient and uncostly
	   mdbEnv* ev)
   {
	   return 0;
   }
   mdb_err mdbStore::LargeCommit( // save important changes if at all possible
    mdbEnv* ev, // context
    mdbThumb** acqThumb) 
   {
	   return 0;
   }

   mdb_err mdbStore::SessionCommit( // save all changes if large commits delayed
    mdbEnv* ev, // context
    mdbThumb** acqThumb)
   {
	   return 0;
   }

   mdb_err
  mdbStore::CompressCommit( // commit and make db physically smaller if possible
    mdbEnv* ev, // context
    mdbThumb** acqThumb)
   {
	   return 0;
   }

  mdb_err mdbStore::NewTable( // make one new table of specific type
    mdbEnv* ev, // context
    mdb_scope inRowScope,    // row scope for row ids
    mdb_kind inTableKind,    // the type of table to access
    mdb_bool inMustBeUnique, // whether store can hold only one of these
    mdbTable** acqTable) ;     // acquire scoped collection of rows

  mdb_err mdbPort::GetTable( // access one table with specific oid
    mdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical table oid
    mdbTable** acqTable)
  {
	  *acqTable = new mdbTable;
	  return 0;
  }

mdb_err mdbPort::StringToToken (  mdbEnv* ev, // context
    const char* inTokenName, // Latin1 string to tokenize if possible
    mdb_token* outToken)
{
	*outToken = (mdb_token) inTokenName;
	return 0;
}
 
mdb_err mdbPort::GetTableKind (
    mdbEnv* ev, // context
    mdb_scope inRowScope,      // row scope for row ids
    mdb_kind inTableKind,      // the type of table to access
    mdb_count* outTableCount, // current number of such tables
    mdb_bool* outMustBeUnique, // whether port can hold only one of these
    mdbTable** acqTable) 
{
	*acqTable = new mdbTable;
	return 0;
}


mdb_err mdbTable::HasOid( // test for the table position of a row member
    mdbEnv* ev, // context
    const mdbOid* inOid, // row to find in table
    mdb_pos* outPos)
{
	mdb_pos iteratePos;
	*outPos = -1;

	for (iteratePos = 0; iteratePos  < m_rows.Count(); iteratePos++)
	{
		mdbRow *row = (mdbRow *) m_rows.ElementAt(iteratePos);
		if (row && row->m_oid.mOid_Id == inOid->mOid_Id)
		{
			*outPos = iteratePos;
			break;
		}
	}
	return 0;
}

  mdb_err mdbTable::GetTableRowCursor( // make a cursor, starting iteration at inRowPos
    mdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    mdbTableRowCursor** acqCursor)
  {
	  *acqCursor = new mdbTableRowCursor;
	  (*acqCursor)->SetTable(ev, this);
	  (*acqCursor)->m_pos = inRowPos;
	  return 0;
  }

 mdb_err mdbTableRowCursor::SetTable(mdbEnv* ev, mdbTable* ioTable) 
 {
	 m_table = ioTable;
	 m_pos = -1;
	 return 0;
 }

mdb_err  mdbTableRowCursor::NextRow( // get row cells from table for cells already in row
    mdbEnv* ev, // context
    mdbRow** acqRow, // acquire next row in table
    mdb_pos* outRowPos)
{
	if (m_pos < 0)
		m_pos = 0;

	*outRowPos = m_pos;
	*acqRow = (mdbRow *) m_table->m_rows.ElementAt(m_pos++);
	return 0;
}

mdb_err mdbTable::CutRow  ( // make sure the row with inOid is not a member 
    mdbEnv* ev, // context
    mdbRow* ioRow) 
{
	return 0;
}

mdb_err mdbStore::NewRowWithOid (mdbEnv* ev, // new row w/ caller assigned oid
    mdb_scope inRowScope,   // row scope for row ids
    const mdbOid* inOid,   // caller assigned oid
    mdbRow** acqRow) 
{
	*acqRow = new mdbRow;
	(*acqRow)->m_oid = *inOid;	
	return 0;
}

mdb_err mdbTable::AddRow ( // make sure the row with inOid is a table member 
    mdbEnv* ev, // context
    mdbRow* ioRow) 
{
	m_rows.AppendElement(ioRow);
	return 0;
}



mdb_err mdbRow::AddColumn( // make sure a particular column is inside row
    mdbEnv* ev, // context
    mdb_column inColumn, // column to add
    const mdbYarn* inYarn)
{
	// evilly, I happen to know the column token is a char * const str pointer.
	printf("adding column %s : %s\n", inColumn, inYarn->mYarn_Buf);
	mdbCellImpl	newCell;
	mdbCell *existingCell = NULL;

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

mdb_err mdbRow::GetCell( // find a cell in this row
    mdbEnv* ev, // context
    mdb_column inColumn, // column to find
    mdbCell** acqCell) 
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

mdb_err mdbCollection::GetOid   (mdbEnv* ev,
    const mdbOid* outOid) 
{
	return 0;
}

mdb_err mdbTableRowCursor::NextRowOid  ( // get row id of next row in the table
    mdbEnv* ev, // context
    mdbOid* outOid, // out row oid
    mdb_pos* outRowPos)
{
	mdbRow *curRow;
	if (m_pos < 0)
		m_pos = 0;

	*outRowPos = m_pos;
	curRow = (mdbRow *) m_table->m_rows.ElementAt(m_pos++);
	if (curRow)
		*outOid = curRow->m_oid;
	else
		*outRowPos = -1;
	return 0;
 }

mdb_err mdbBlob::AliasYarn(mdbEnv* ev, 
    mdbYarn* outYarn)
{
	return 0;
}

mdb_err mdbObject::CutStrongRef(mdbEnv* ev)
{
	return 0;
}

mdb_err mdbFactory::MakeEnv(mdbEnv** acqEnv)
{
	return 0;
}

mdb_err mdbThumb::DoMore(mdbEnv* ev,
    mdb_count* outTotal,    // total somethings to do in operation
    mdb_count* outCurrent,  // subportion of total completed so far
    mdb_bool* outDone,      // is operation finished?
    mdb_bool* outBroken     // is operation irreparably dead and broken?
  ) 
{
	*outDone = 1;
	return 0;
}

mdb_err mdbFactory::OpenFileStore(class mdbEnv *,char const *fileName,struct mdbOpenPolicy const *,class mdbThumb **retThumb)
{

	*retThumb = new mdbThumb;
	return 0;
}

mdb_err mdbStore::NewTable(class mdbEnv *,unsigned long,unsigned long,unsigned char,class mdbTable **retTable)
{
	*retTable = new mdbTable;
	return 0;
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


mdb_err mdbCellImpl::AliasYarn(mdbEnv* ev, 
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

