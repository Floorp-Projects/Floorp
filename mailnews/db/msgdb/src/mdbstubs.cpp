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
	return 0;
}

  mdb_err mdbTable::GetTableRowCursor( // make a cursor, starting iteration at inRowPos
    mdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    mdbTableRowCursor** acqCursor)
  {
	  return 0;
  }

mdb_err  mdbTableRowCursor::NextRow( // get row cells from table for cells already in row
    mdbEnv* ev, // context
    mdbRow** acqRow, // acquire next row in table
    mdb_pos* outRowPos)
{
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
	return 0;
}

mdb_err mdbTable::AddRow ( // make sure the row with inOid is a table member 
    mdbEnv* ev, // context
    mdbRow* ioRow) 
{
	return 0;
}



mdb_err mdbRow::AddColumn( // make sure a particular column is inside row
    mdbEnv* ev, // context
    mdb_column inColumn, // column to add
    const mdbYarn* inYarn)
{
	// evilly, I happen to know the column token is a char * const str pointer.
	printf("adding column %s : %s\n", inColumn, inYarn->mYarn_Buf);
	return 0;
}

mdb_err mdbRow::GetCell( // find a cell in this row
    mdbEnv* ev, // context
    mdb_column inColumn, // column to find
    mdbCell** acqCell) 
{
	return 0;
}

mdb_err mdbCollection::GetOid   (mdbEnv* ev,
    const mdbOid* outOid) 
{
	return 0;
}

mdb_err mdbTableRowCursor::NextRowOid  ( // get row id of next row in the table
    mdbEnv* ev, // context
    const mdbOid* outOid, // out row oid
    mdb_pos* outRowPos)
{
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