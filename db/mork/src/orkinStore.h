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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

#ifndef _ORKINSTORE_
#define _ORKINSTORE_ 1

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKHANDLE_
#include "morkHandle.h"
#endif

#ifndef _MORKStore_
#include "morkStore.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkMagic_kStore 0x53746F72 /* ascii 'Stor' */
 
/*| orkinStore: 
|*/
class orkinStore : public morkHandle, public nsIMdbStore { // nsIMdbPort

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  // virtual void CloseMorkNode(morkEnv* ev); // morkHandle is fine
  virtual ~orkinStore(); // morkHandle destructor does everything
  
protected: // construction is protected (use the static Make() method)
  orkinStore(morkEnv* ev, // note morkUsage is always morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkStore* ioObject); // must not be nil, the object for this handle
    
  // void CloseHandle(morkEnv* ev); // don't need to specialize closing

private: // copying is not allowed
  orkinStore(const morkHandle& other);
  orkinStore& operator=(const morkHandle& other);

// public: // dynamic type identification
  // mork_bool IsHandle() const //
  // { return IsNode() && mNode_Derived == morkDerived_kHandle; }
// } ===== end morkNode methods =====

protected: // morkHandle memory management operators
  void* operator new(size_t inSize, morkPool& ioPool, morkEnv* ev)
  { return ioPool.NewHandle(ev, inSize); }
  
  void* operator new(size_t inSize, morkHandleFace* ioFace)
  { return ioFace; }
  
  void operator delete(void* ioAddress)
  { morkNode::OnDeleteAssert(ioAddress); }
  // do NOT call delete on morkHandle instances.  They are collected.
  
public: // construction:

  static orkinStore* MakeStore(morkEnv* ev, morkStore* ioObject);

public: // utilities:

  morkEnv* CanUseStore(nsIMdbEnv* mev, mork_bool inMutable,
    mdb_err* outErr) const;

public: // type identification
  mork_bool IsOrkinStore() const
  { return mHandle_Magic == morkMagic_kStore; }

  mork_bool IsOrkinStoreHandle() const
  { return this->IsHandle() && this->IsOrkinStore(); }

// { ===== begin nsIMdbISupports methods =====
  virtual mdb_err AddRef(); // add strong ref with no
  virtual mdb_err Release(); // cut strong ref
// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbObject methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err IsFrozenMdbObject(nsIMdbEnv* ev, mdb_bool* outIsReadonly);
  // same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
  // } ----- end attribute methods -----

  // { ----- begin factory methods -----
  virtual mdb_err GetMdbFactory(nsIMdbEnv* ev, nsIMdbFactory** acqFactory); 
  // } ----- end factory methods -----

  // { ----- begin ref counting for well-behaved cyclic graphs -----
  virtual mdb_err GetWeakRefCount(nsIMdbEnv* ev, // weak refs
    mdb_count* outCount);  
  virtual mdb_err GetStrongRefCount(nsIMdbEnv* ev, // strong refs
    mdb_count* outCount);

  virtual mdb_err AddWeakRef(nsIMdbEnv* ev);
  virtual mdb_err AddStrongRef(nsIMdbEnv* ev);

  virtual mdb_err CutWeakRef(nsIMdbEnv* ev);
  virtual mdb_err CutStrongRef(nsIMdbEnv* ev);
  
  virtual mdb_err CloseMdbObject(nsIMdbEnv* ev); // called at strong refs zero
  virtual mdb_err IsOpenMdbObject(nsIMdbEnv* ev, mdb_bool* outOpen);
  // } ----- end ref counting -----
  
// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbPort methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err GetIsPortReadonly(nsIMdbEnv* ev, mdb_bool* outBool);
  virtual mdb_err GetIsStore(nsIMdbEnv* ev, mdb_bool* outBool);
  virtual mdb_err GetIsStoreAndDirty(nsIMdbEnv* ev, mdb_bool* outBool);

  virtual mdb_err GetUsagePolicy(nsIMdbEnv* ev, 
    mdbUsagePolicy* ioUsagePolicy);

  virtual mdb_err SetUsagePolicy(nsIMdbEnv* ev, 
    const mdbUsagePolicy* inUsagePolicy);
  // } ----- end attribute methods -----

  // { ----- begin memory policy methods -----  
  virtual mdb_err IdleMemoryPurge( // do memory management already scheduled
    nsIMdbEnv* ev, // context
    mdb_size* outEstimatedBytesFreed); // approximate bytes actually freed

  virtual mdb_err SessionMemoryPurge( // request specific footprint decrease
    nsIMdbEnv* ev, // context
    mdb_size inDesiredBytesFreed, // approximate number of bytes wanted
    mdb_size* outEstimatedBytesFreed); // approximate bytes actually freed

  virtual mdb_err PanicMemoryPurge( // desperately free all possible memory
    nsIMdbEnv* ev, // context
    mdb_size* outEstimatedBytesFreed); // approximate bytes actually freed
  // } ----- end memory policy methods -----

  // { ----- begin filepath methods -----
  virtual mdb_err GetPortFilePath(
    nsIMdbEnv* ev, // context
    mdbYarn* outFilePath, // name of file holding port content
    mdbYarn* outFormatVersion); // file format description
  // } ----- end filepath methods -----

  // { ----- begin export methods -----
  virtual mdb_err BestExportFormat( // determine preferred export format
    nsIMdbEnv* ev, // context
    mdbYarn* outFormatVersion); // file format description

  virtual mdb_err
  CanExportToFormat( // can export content in given specific format?
    nsIMdbEnv* ev, // context
    const char* inFormatVersion, // file format description
    mdb_bool* outCanExport); // whether ExportSource() might succeed

  virtual mdb_err ExportToFormat( // export content in given specific format
    nsIMdbEnv* ev, // context
    const char* inFilePath, // the file to receive exported content
    const char* inFormatVersion, // file format description
    nsIMdbThumb** acqThumb); // acquire thumb for incremental export
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the export will be finished.

  // } ----- end export methods -----

  // { ----- begin token methods -----
  virtual mdb_err TokenToString( // return a string name for an integer token
    nsIMdbEnv* ev, // context
    mdb_token inToken, // token for inTokenName inside this port
    mdbYarn* outTokenName); // the type of table to access
  
  virtual mdb_err StringToToken( // return an integer token for scope name
    nsIMdbEnv* ev, // context
    const char* inTokenName, // Latin1 string to tokenize if possible
    mdb_token* outToken); // token for inTokenName inside this port
    
  // String token zero is never used and never supported. If the port
  // is a mutable store, then StringToToken() to create a new
  // association of inTokenName with a new integer token if possible.
  // But a readonly port will return zero for an unknown scope name.

  virtual mdb_err QueryToken( // like StringToToken(), but without adding
    nsIMdbEnv* ev, // context
    const char* inTokenName, // Latin1 string to tokenize if possible
    mdb_token* outToken); // token for inTokenName inside this port
  
  // QueryToken() will return a string token if one already exists,
  // but unlike StringToToken(), will not assign a new token if not
  // already in use.

  // } ----- end token methods -----

  // { ----- begin row methods -----  
  virtual mdb_err HasRow( // contains a row with the specified oid?
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    mdb_bool* outHasRow); // whether GetRow() might succeed
    
  virtual mdb_err GetRow( // access one row with specific oid
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    nsIMdbRow** acqRow); // acquire specific row (or null)

  virtual mdb_err GetRowRefCount( // get number of tables that contain a row 
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    mdb_count* outRefCount); // number of tables containing inRowKey 
  // } ----- end row methods -----

  // { ----- begin table methods -----  
  virtual mdb_err HasTable( // supports a table with the specified oid?
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical table oid
    mdb_bool* outHasTable); // whether GetTable() might succeed
    
  virtual mdb_err GetTable( // access one table with specific oid
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical table oid
    nsIMdbTable** acqTable); // acquire specific table (or null)
  
  virtual mdb_err HasTableKind( // supports a table of the specified type?
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope, // rid scope for row ids
    mdb_kind inTableKind, // the type of table to access
    mdb_count* outTableCount, // current number of such tables
    mdb_bool* outSupportsTable); // whether GetTableKind() might succeed
        
  virtual mdb_err GetTableKind( // access one (random) table of specific type
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope,      // row scope for row ids
    mdb_kind inTableKind,      // the type of table to access
    mdb_count* outTableCount, // current number of such tables
    mdb_bool* outMustBeUnique, // whether port can hold only one of these
    nsIMdbTable** acqTable);       // acquire scoped collection of rows
    
  virtual mdb_err
  GetPortTableCursor( // get cursor for all tables of specific type
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope, // row scope for row ids
    mdb_kind inTableKind, // the type of table to access
    nsIMdbPortTableCursor** acqCursor); // all such tables in the port
  // } ----- end table methods -----

// } ===== end nsIMdbPort methods =====

// { ===== begin nsIMdbStore methods =====

  // { ----- begin table methods -----
  virtual mdb_err NewTable( // make one new table of specific type
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope,    // row scope for row ids
    mdb_kind inTableKind,    // the type of table to access
    mdb_bool inMustBeUnique, // whether store can hold only one of these
    const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying
    nsIMdbTable** acqTable);     // acquire scoped collection of rows
    
  virtual mdb_err NewTableWithOid( // make one new table of specific type
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,   // caller assigned oid
    mdb_kind inTableKind,    // the type of table to access
    mdb_bool inMustBeUnique, // whether store can hold only one of these
    const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying
    nsIMdbTable** acqTable);     // acquire scoped collection of rows
  // } ----- end table methods -----

  // { ----- begin row scope methods -----
  virtual mdb_err RowScopeHasAssignedIds(nsIMdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned); // nonzero if store db assigned specified

  virtual mdb_err SetCallerAssignedIds(nsIMdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned); // nonzero if store db assigned specified

  virtual mdb_err SetStoreAssignedIds(nsIMdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned); // nonzero if store db assigned specified
  // } ----- end row scope methods -----

  // { ----- begin row methods -----
  virtual mdb_err NewRowWithOid(nsIMdbEnv* ev, // new row w/ caller assigned oid
    const mdbOid* inOid,   // caller assigned oid
    nsIMdbRow** acqRow); // create new row

  virtual mdb_err NewRow(nsIMdbEnv* ev, // new row with db assigned oid
    mdb_scope inRowScope,   // row scope for row ids
    nsIMdbRow** acqRow); // create new row
  // Note this row must be added to some table or cell child before the
  // store is closed in order to make this row persist across sesssions.
  // } ----- end row methods -----

  // { ----- begin inport/export methods -----
  virtual mdb_err ImportContent( // import content from port
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope, // scope for rows (or zero for all?)
    nsIMdbPort* ioPort, // the port with content to add to store
    nsIMdbThumb** acqThumb); // acquire thumb for incremental import
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the import will be finished.
  // } ----- end inport/export methods -----

  // { ----- begin hinting methods -----
  virtual mdb_err
  ShareAtomColumnsHint( // advise re shared column content atomizing
    nsIMdbEnv* ev, // context
    mdb_scope inScopeHint, // zero, or suggested shared namespace
    const mdbColumnSet* inColumnSet); // cols desired tokenized together

  virtual mdb_err
  AvoidAtomColumnsHint( // advise column with poor atomizing prospects
    nsIMdbEnv* ev, // context
    const mdbColumnSet* inColumnSet); // cols with poor atomizing prospects
  // } ----- end hinting methods -----

  // { ----- begin commit methods -----
  virtual mdb_err SmallCommit( // save minor changes if convenient and uncostly
    nsIMdbEnv* ev); // context
  
  virtual mdb_err LargeCommit( // save important changes if at all possible
    nsIMdbEnv* ev, // context
    nsIMdbThumb** acqThumb); // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.

  virtual mdb_err SessionCommit( // save all changes if large commits delayed
    nsIMdbEnv* ev, // context
    nsIMdbThumb** acqThumb); // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.

  virtual mdb_err
  CompressCommit( // commit and make db physically smaller if possible
    nsIMdbEnv* ev, // context
    nsIMdbThumb** acqThumb); // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.
  // } ----- end commit methods -----

// } ===== end nsIMdbStore methods =====
};
 

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _ORKINSTORE_ */
