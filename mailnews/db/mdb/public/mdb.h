// mdb.h (draft 0 Wednesday 13-Jan-1999)

#ifndef _MDB_
#define _MDB_ 1

// { %%%%% begin scalar typedefs %%%%%
typedef unsigned char mdb_bool;  // unsigned byte with zero=false, nonzero=true
typedef unsigned long mdb_id;    // unsigned object identity in a scope
typedef mdb_id mdb_rid;          // unsigned row identity inside scope
typedef mdb_id mdb_tid;          // unsigned table identity inside scope
typedef unsigned long mdb_token; // unsigned token for atomized string
typedef mdb_token mdb_scope;     // token used to id scope for rows
typedef mdb_token mdb_kind;      // token used to id kind for tables
typedef mdb_token mdb_column;    // token used to id columns for rows
typedef mdb_token mdb_cscode;    // token used to id charset names
typedef unsigned long mdb_seed;  // unsigned collection change counter
typedef unsigned long mdb_count; // unsigned collection member count
typedef unsigned long mdb_size;  // unsigned physical media size
typedef unsigned long mdb_fill;  // unsigned logical content size
typedef unsigned long mdb_more;  // more available bytes for larger buffer

// temporary substitute for NS_RESULT, for mdb.h standalone compilation:
typedef unsigned long mdb_err;   // equivalent to NS_RESULT

// sequence position is signed; negative is useful to mean "before first":
typedef long mdb_pos; // signed zero-based ordinal collection position

// order is also signed, so we can use three states for comparison order:
typedef long mdb_order; // neg:lessthan, zero:equalto, pos:greaterthan 

// } %%%%% end scalar typedefs %%%%%

// { %%%%% begin C structs %%%%%

#ifndef mdbScopeStringSet_typedef
typedef struct mdbScopeStringSet mdbScopeStringSet;
#define mdbScopeStringSet_typedef 1
#endif

/*| mdbScopeStringSet: a set of null-terminated C strings that enumerate some
**| names of row scopes, so that row scopes intended for use by an application
**| can be declared by an app when trying to open or create a database file.
**| (We use strings and not tokens because we cannot know the tokens for any
**| particular db without having first opened the db.)  The goal is to inform
**| a db runtime that scopes not appearing in this list can be given relatively
**| short shrift in runtime representation, with the expectation that other
**| scopes will not actually be used.  However, a db should still be prepared
**| to handle accessing row scopes not in this list, rather than raising errors.
**| But it could be quite expensive to access a row scope not on the list.
**| Note a zero count for the string set means no such string set is being
**| specified, and that a db should handle all row scopes efficiently. 
**| (It does NOT mean an app plans to use no content whatsoever.)
|*/
#ifndef mdbScopeStringSet_struct
#define mdbScopeStringSet_struct 1
struct mdbScopeStringSet { // vector of scopes for use in db opening policy
  // when mScopeStringSet_Count is zero, this means no scope constraints 
  mdb_count     mScopeStringSet_Count;    // number of strings in vector below
  const char**  mScopeStringSet_Strings;  // null-ended ascii scope strings
};
#endif /*mdbScopeStringSet_struct*/

#ifndef mdbOpenPolicy_typedef
typedef struct mdbOpenPolicy mdbOpenPolicy;
#define mdbOpenPolicy_typedef 1
#endif

#ifndef mdbOpenPolicy_struct
#define mdbOpenPolicy_struct 1
struct mdbOpenPolicy { // policies affecting db usage for ports and stores
  mdbScopeStringSet  mOpenPolicy_ScopePlan; // predeclare scope usage plan
  mdb_bool           mOpenPolicy_MaxLazy;   // nonzero: do least work
  mdb_bool           mOpenPolicy_MinMemory; // nonzero: use least memory
};
#endif /*mdbOpenPolicy_struct*/

#ifndef mdbTokenSet_typedef
typedef struct mdbTokenSet mdbTokenSet;
#define mdbTokenSet_typedef 1
#endif

#ifndef mdbTokenSet_struct
#define mdbTokenSet_struct 1
struct mdbTokenSet { // array for a set of tokens, and actual slots used
  mdb_count   mTokenSet_Count;   // number of token slots in the array
  mdb_fill    mTokenSet_Fill;    // the subset of count slots actually used
  mdb_more    mTokenSet_More;    // more tokens available for bigger array
  mdb_token*  mTokenSet_Tokens;  // array of count mdb_token instances
};
#endif /*mdbTokenSet_struct*/

#ifndef mdbUsagePolicy_typedef
typedef struct mdbUsagePolicy mdbUsagePolicy;
#define mdbUsagePolicy_typedef 1
#endif

/*| mdbUsagePolicy: another version of mdbOpenPolicy which uses tokens instead
**| of scope strings, because usage policies can be constructed for use with a
**| db that is already open, while an open policy must be constructed before a
**| db has yet been opened.
|*/
#ifndef mdbUsagePolicy_struct
#define mdbUsagePolicy_struct 1
struct mdbUsagePolicy { // policies affecting db usage for ports and stores
  mdbTokenSet  mUsagePolicy_ScopePlan; // current scope usage plan
  mdb_bool     mUsagePolicy_MaxLazy;   // nonzero: do least work
  mdb_bool     mUsagePolicy_MinMemory; // nonzero: use least memory
};
#endif /*mdbUsagePolicy_struct*/

#ifndef mdbOid_typedef
typedef struct mdbOid mdbOid;
#define mdbOid_typedef 1
#endif

#ifndef mdbOid_struct
#define mdbOid_struct 1
struct mdbOid { // identity of some row or table inside a database
  mdb_scope   mOid_Scope;  // scope token for an id's namespace
  mdb_id      mOid_Id;     // identity of object inside scope namespace
};
#endif /*mdbOid_struct*/

#ifndef mdbRange_typedef
typedef struct mdbRange mdbRange;
#define mdbRange_typedef 1
#endif

#ifndef mdbRange_struct
#define mdbRange_struct 1
struct mdbRange { // range of row positions in a table
  mdb_pos   mRange_FirstPos;  // position of first row
  mdb_pos   mRange_LastPos;   // position of last row
};
#endif /*mdbRange_struct*/

#ifndef mdbColumnSet_typedef
typedef struct mdbColumnSet mdbColumnSet;
#define mdbColumnSet_typedef 1
#endif

#ifndef mdbColumnSet_struct
#define mdbColumnSet_struct 1
struct mdbColumnSet { // array of column tokens (just the same as mdbTokenSet)
  mdb_count    mColumnSet_Count;    // number of columns
  mdb_column*  mColumnSet_Columns;  // count mdb_column instances
};
#endif /*mdbColumnSet_struct*/

#ifndef mdbSearch_typedef
typedef struct mdbSearch mdbSearch;
#define mdbSearch_typedef 1
#endif

#ifndef mdbSearch_struct
#define mdbSearch_struct 1
struct mdbSearch { // parallel in and out arrays for search results
  mdb_count    mSearch_Count;    // number of columns and ranges
  mdb_column*  mSearch_Columns;  // count mdb_column instances
  mdbRange*    mSearch_Ranges;   // count mdbRange instances
};
#define mdbSearch_AsColumnSet(me) ((me)->mSearch_Count, (mdbColumnSet*) (me))
#endif /*mdbSearch_struct*/

#ifndef mdbYarn_typedef
typedef struct mdbYarn mdbYarn;
#define mdbYarn_typedef 1
#endif

#ifdef MDB_BEGIN_C_LINKAGE_define
#define MDB_BEGIN_C_LINKAGE_define 1
#define MDB_BEGIN_C_LINKAGE extern "C" {
#define MDB_END_C_LINKAGE }
#endif /*MDB_BEGIN_C_LINKAGE_define*/

/*| mdbYarn_mGrow: an abstract API for growing the size of a mdbYarn
**| instance.  With respect to a specific API that requires a caller
**| to supply a string (mdbYarn) that a callee fills with content
**| that might exceed the specified size, mdbYarn_mGrow is a caller-
**| supplied means of letting a callee attempt to increase the string
**| size to become large enough to receive all content available.
**|
**|| Grow(): a method for requesting that a yarn instance be made
**| larger in size.  Note that such requests need not be honored, and
**| need not be honored in full if only partial size growth is desired.
**| (Note that no mdbEnv instance is passed as argument, although one
**| might be needed in some circumstances.  So if an mdbEnv is needed,
**| a reference to one might be held inside a mdbYarn member slot.)
**|
**|| self: a yarn instance to be grown.  Presumably this yarn is
**| the instance which holds the mYarn_Grow method pointer.  Yarn
**| instancesshould only be passed to grow methods which they were
**| specifically designed to fit, as indicated by the mYarn_Grow slot.
**|
**|| inNewSize: the new desired value for slot mYarn_Size in self.
**| If mYarn_Size is already this big, then nothing should be done.
**| If inNewSize is larger than seems feasible or desirable to honor,
**| then any size restriction policy can be used to grow to some size
**| greater than mYarn_Size.  (Grow() might even grow to a size
**| greater than inNewSize in order to make the increase in size seem
**| worthwhile, rather than growing in many smaller steps over time.)
|*/
typedef void (* mdbYarn_mGrow)(mdbYarn* self, mdb_size inNewSize);
// mdbYarn_mGrow methods must be declared with C linkage in C++

/*| mdbYarn: a variable length "string" of arbitrary binary bytes,
**| whose length is mYarn_Fill, inside a buffer mYarn_Buf that has
**| at most mYarn_Size byte of physical space.
**|
**|| mYarn_Buf: a pointer to space containing content.  This slot
**| might never be nil when mYarn_Size is nonzero, but checks for nil
**| are recommended anyway.
**| (Implementations of mdbYarn_mGrow methods should take care to
**| ensure the existence of a replacement before dropping old Bufs.)
**| Content in Buf can be anything in any format, but the mYarn_Form
**| implies the actual format by some caller-to-callee convention.
**| mYarn_Form==0 implies US-ASCII iso-8859-1 Latin1 string content.
**|
**|| mYarn_Size: the physical size of Buf in bytes.  Note that if one
**| intends to terminate a string with a null byte, that it must not
**| be written at or after mYarn_Buf[mYarn_Size] because this is after
**| the last byte in the physical buffer space.  Size can be zero,
**| which means the string has no content whatsoever; note that when
**| Size is zero, this is a suitable reason for Buf==nil as well.
**|
**|| mYarn_Fill: the logical content in Buf in bytes, where Fill must
**| never exceed mYarn_Size.  Note that yarn strings might not have a
**| terminating null byte (since they might not even be C strings), but
**| when they do, such terminating nulls are considered part of content
**| and therefore Fill will count such null bytes.  So an "empty" C
**| string will have Fill==1, because content includes one null byte.
**| Fill does not mean "length" when applied to C strings for this
**| reason.  However, clients using yarns to hold C strings can infer
**| that length is equal to Fill-1 (but should take care to handle the
**| case where Fill==0).  To be paranoid, one can always copy to a
**| destination with size exceeding Fill, and place a redundant null
**| byte in the Fill position when this simplifies matters.
**|
**|| mYarn_Form: a designation of content format within mYarn_Buf.
**| The semantics of this slot are the least well defined, since the
**| actual meaning is context dependent, to the extent that callers
**| and callees must agree on format encoding conventions when such
**| are not standardized in many computing contexts.  However, in the
**| context of a specific mdb database, mYarn_Form is a token for an
**| atomized string in that database that typically names a preferred
**| mime type charset designation.  If and when mdbYarn is used for
**| other purposes away from the mdb interface, folks can use another
**| convention system for encoding content formats.  However, in all
**| contexts is it useful to maintain the convention that Form==0
**| implies Buf contains US-ASCII iso-8859-1 Latin1 string content.
**|
**|| mYarn_Grow: either a mdbYarn_mGrow method, or else nil.  When
**| a mdbYarn_mGrow method is provided, this method can be used to
**| request a yarn buf size increase.  A caller who constructs the 
**| original mdbYarn instance decides whether a grow method is necessary
**| or desirable, and uses only grow methods suitable for the buffering
**| nature of a specific mdbYarn instance.  (For example, Buf might be a
**| staticly allocated string space which switches to something heap-based
**| when grown, and subsequent calls to grow the yarn must distinguish the
**| original static string from heap allocated space, etc.) Note that the
**| method stored in mYarn_Grow can change, and this might be a common way
**| to track memory managent changes in policy for mYarn_Buf.
|*/
#ifndef mdbYarn_struct
#define mdbYarn_struct 1
struct mdbYarn { // buffer with caller space allocation semantics
  void*         mYarn_Buf;   // space for holding any binary content
  mdb_size      mYarn_Size;  // physical size of Buf in bytes
  mdb_fill      mYarn_Fill;  // logical content in Buf in bytes
  mdb_more      mYarn_More;  // more available bytes if Buf is bigger
  mdb_cscode    mYarn_Form;  // charset format encoding
  mdbYarn_mGrow mYarn_Grow;  // optional method to grow mYarn_Buf
  
  // Subclasses might add further slots after mYarn_Grow in order to
  // maintain bookkeeping needs, such as state info about mYarn_Buf.
};
#endif /*mdbYarn_struct*/

// } %%%%% end C structs %%%%%

// { %%%%% begin class forward defines %%%%%
class mdbISupports;
class mdbEnv;
class mdbObject;
class mdbErrorHook;
class mdbCompare;
class mdbThumb;
class mdbFactory;
class mdbPort;
class mdbStore;
class mdbCursor;
class mdbPortTableCursor;
class mdbCollection;
class mdbTable;
class mdbTableRowCursor;
class mdbRow;
class mdbRowCellCursor;
class mdbBlob;
class mdbCell;
// } %%%%% end class forward defines %%%%%

// { %%%%% begin temporary dummy base class for class hierarchy %%%%%
class mdbISupports { // msg db base class
};
// } %%%%% end temporary dummy base class for class hierarchy %%%%%


// { %%%%% begin C++ abstract class interfaces %%%%%

/*| mdbObject: base class for all message db class interfaces
**|
**|| factory: all mdbObjects from the same code suite have the same factory
**|
**|| refcounting: both strong and weak references, to ensure strong refs are
**| acyclic, while weak refs can cause cycles.  CloseMdbObject() is
**| called when (strong) use counts hit zero, but clients can call this close
**| method early for some reason, if absolutely necessary even though it will
**| thwart the other uses of the same object.  Note that implementations must
**| cope with close methods being called arbitrary numbers of times.  The COM
**| calls to AddRef() and release ref map directly to strong use ref calls,
**| but the total ref count for COM objects is the sum of weak & strong refs.
|*/
class mdbObject : public mdbISupports { // msg db base class
public:
// { ===== begin mdbObject methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err IsFrozenMdbObject(mdbEnv* ev, mdb_bool* outIsReadonly) = 0;
  // same as mdbPort::GetIsPortReadonly() when this object is inside a port.
  // } ----- end attribute methods -----

  // { ----- begin factory methods -----
  virtual mdb_err GetMsgDbFactory(mdbEnv* ev, mdbFactory** acqFactory) = 0; 
  // } ----- end factory methods -----

  // { ----- begin ref counting for well-behaved cyclic graphs -----
  virtual mdb_err GetWeakRefCount(mdbEnv* ev, // weak refs
    mdb_count* outCount) = 0;  
  virtual mdb_err GetStrongRefCount(mdbEnv* ev, // strong refs
    mdb_count* outCount) = 0;

  virtual mdb_err AddWeakRef(mdbEnv* ev) = 0;
  virtual mdb_err AddStrongRef(mdbEnv* ev) = 0;

  virtual mdb_err CutWeakRef(mdbEnv* ev) = 0;
  virtual mdb_err CutStrongRef(mdbEnv* ev) = 0;
  
  virtual mdb_err CloseMdbObject(mdbEnv* ev) = 0; // called at strong refs zero
  virtual mdb_err IsOpenMdbObject(mdbEnv* ev, mdb_bool* outOpen) = 0;
  // } ----- end ref counting -----
  
// } ===== end mdbObject methods =====
};

/*| mdbErrorHook: a base class for clients of this API to subclass, in order
**| to provide a callback installable in mdbEnv for error notifications. If
**| apps that subclass mdbErrorHook wish to maintain a reference to the env
**| that contains the hook, then this should be a weak ref to avoid cycles.
**|
**|| OnError: when mdbEnv has an error condition that causes the total count
**| of errors to increase, then mdbEnv should call OnError() to report the
**| error in some fashion when an instance of mdbErrorHook is installed.  The
**| variety of string flavors is currently due to the uncertainty here in the
**| mdbBlob and mdbCell interfaces.  (Note that overloading by using the
**| same method name is not necessary here, and potentially less clear.)
|*/
class mdbErrorHook { // env callback handler to report errors

// { ===== begin mdbErrorHook methods =====
  virtual mdb_err OnErrorString(mdbEnv* ev, const char* inAscii) = 0;
  virtual mdb_err OnErrorYarn(mdbEnv* ev, const mdbYarn* inYarn) = 0;
// } ===== end mdbErrorHook methods =====
};

/*| mdbCompare: a caller-supplied yarn comparison interface.  When two yarns
**| are compared to each other with Order(), this method should return a signed
**| long integer denoting relation R between the 1st and 2nd yarn instances
**| such that (First R Second), where negative is less than, zero is equal to,
**| and positive is greater than.  Note that both yarns are readonly, and the
**| Order() method should make no attempt to modify the yarn content.
|*/
class mdbCompare { // caller-supplied yarn comparison

// { ===== begin mdbCompare methods =====
  virtual mdb_err Order(mdbEnv* ev,      // compare first to second yarn
    const mdbYarn* inFirst,   // first yarn in comparison
    const mdbYarn* inSecond,  // second yarn in comparison
    mdb_order* outOrder) = 0; // negative="<", zero="=", positive=">"
// } ===== end mdbCompare methods =====
  
};

/*| mdbThumb: 
|*/
class mdbThumb : public mdbObject { // closure for repeating incremental method
public:
// { ===== begin mdbThumb methods =====
  virtual mdb_err GetProgress(mdbEnv* ev,
    mdb_count* outTotal,    // total somethings to do in operation
    mdb_count* outCurrent,  // subportion of total completed so far
    mdb_bool* outDone,      // is operation finished?
    mdb_bool* outBroken     // is operation irreparably dead and broken?
  ) = 0;
  
  virtual mdb_err DoMore(mdbEnv* ev,
    mdb_count* outTotal,    // total somethings to do in operation
    mdb_count* outCurrent,  // subportion of total completed so far
    mdb_bool* outDone,      // is operation finished?
    mdb_bool* outBroken     // is operation irreparably dead and broken?
  ) = 0;
  
  virtual mdb_err CancelAndBreakThumb( // cancel pending operation
    mdbEnv* ev) = 0;
// } ===== end mdbThumb methods =====
};

/*| mdbEnv: a context parameter used when calling most abstract db methods.
**| The main purpose of such an object is to permit a database implementation
**| to avoid the use of globals to share information between various parts of
**| the implementation behind the abstract db interface.  An environment acts
**| like a session object for a given calling thread, and callers should use
**| at least one different mdbEnv instance for each thread calling the API.
**| While the database implementation might not be threaded, it is highly
**| desirable that the db be thread-safe if calling threads use distinct
**| instances of mdbEnv.  Callers can stop at one mdbEnv per thread, or they
**| might decide to make on mdbEnv instance for every mdbPort opened, so that
**| error information is segregated by database instance.  Callers create
**| instances of mdbEnv by calling the MakeEnv() method in mdbFactory. 
**|
**|| tracing: an environment might support some kind of tracing, and this
**| boolean attribute permits such activity to be enabled or disabled.
**|
**|| errors: when a call to the abstract db interface returns, a caller might
**| check the number of outstanding errors to see whether the operation did
**| actually succeed. Each mdbEnv should have all its errors cleared by a
**| call to ClearErrors() before making each call to the abstract db API,
**| because outstanding errors might disable further database actions.  (This
**| is not done inside the db interface, because the db cannot in general know
**| when a call originates from inside or outside -- only the app knows this.)
**|
**|| error hook: callers can install an instance of mdbErrorHook to receive
**| error notifications whenever the error count increases.  The hook can
**| be uninstalled by passing a null pointer.
**|
|*/
class mdbEnv : public mdbObject { // db specific context parameter

// { ===== begin mdbEnv methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err GetErrorCount(mdb_count* outCount,
    mdb_bool* outShouldAbort) = 0;
  
  virtual mdb_err GetDoTrace(mdb_bool* outDoTrace) = 0;
  virtual mdb_err SetDoTrace(mdb_bool inDoTrace) = 0;
  
  virtual mdb_err GetErrorHook(mdbErrorHook** acqErrorHook) = 0;
  virtual mdb_err SetErrorHook(
    mdbErrorHook* ioErrorHook) = 0; // becomes referenced
  // } ----- end attribute methods -----
  
  virtual mdb_err ClearErrors() = 0; // clear errors beore re-entering db API
// } ===== end mdbEnv methods =====
};

/*| mdbFactory: the main entry points to the abstract db interface.  A DLL
**| that supports this mdb interface need only have a single exported method
**| that will return an instance of mdbFactory, so that further methods in
**| the suite can be accessed from objects returned by mdbFactory methods.
**|
**|| mdbYarn: note all mdbFactory subclasses must guarantee null
**| termination of all strings written into mdbYarn instances, as long as
**| mYarn_Size and mYarn_Buf are nonzero.  Even truncated string values must
**| be null terminated.  This is more strict behavior than mdbYarn requires,
**| but it is part of the mdbFactory interface.
**|
**|| envs: an environment instance is required as per-thread context for
**| most of the db method calls, so mdbFactory creates such instances.
**|
**|| rows: callers must be able to create row instances that are independent
**| of storage space that is part of the db content graph.  Many interfaces
**| for data exchange have strictly copy semantics, so that a row instance
**| has no specific identity inside the db content model, and the text in
**| cells are an independenty copy of unexposed content inside the db model.
**| Callers are expected to maintain one or more row instances as a buffer
**| for staging cell content copied into or out of a table inside the db.
**| Callers are urged to use an instance of mdbRow created by the mdbFactory
**| code suite, because reading and writing might be much more efficient than
**| when using a hand-rolled mdbRow subclass with no relation to the suite.
**|
**|| ports: a port is a readonly interface to a specific database file. Most
**| of the methods to access a db file are suitable for a readonly interface,
**| so a port is the basic minimum for accessing content.  This makes it
**| possible to read other external formats for import purposes, without
**| needing the code or competence necessary to write every such format.  So
**| we can write generic import code just once, as long as every format can
**| show a face based on mdbPort. (However, same suite import can be faster.)
**| Given a file name and the first 512 bytes of a file, a factory can say if
**| a port can be opened by this factory.  Presumably an app maintains chains
**| of factories for different suites, and asks each in turn about opening a
**| a prospective file for reading (as a port) or writing (as a store).  I'm
**| not ready to tackle issues of format fidelity and factory chain ordering.
**|
**|| stores: a store is a mutable interface to a specific database file, and
**| includes the port interface plus any methods particular to writing, which
**| are few in number.  Presumably the set of files that can be opened as
**| stores is a subset of the set of files that can be opened as ports.  A
**| new store can be created with CreateNewFileStore() by supplying a new
**| file name which does not yet exist (callers are always responsible for
**| destroying any existing files before calling this method). 
|*/
class mdbFactory : public mdbObject { // suite entry points
public:
// { ===== begin mdbFactory methods =====

  // { ----- begin env methods -----
  virtual mdb_err MakeEnv(mdbEnv** acqEnv) = 0; // acquire new env instance
  // } ----- end env methods -----

  // { ----- begin row methods -----
  virtual mdb_err MakeRow(mdbEnv* ev, mdbRow** acqRow) = 0; // acquire new row
  // } ----- end row methods -----
  
  // { ----- begin port methods -----
  virtual mdb_err CanOpenFilePort(
    mdbEnv* ev, // context
    const char* inFilePath, // the file to investigate
    const mdbYarn* inFirst512Bytes,
    mdb_bool* outCanOpen, // whether OpenFilePort() might succeed
    mdbYarn* outFormatVersion) = 0; // informal file format description
    
  virtual mdb_err OpenFilePort(
    mdbEnv* ev, // context
    const char* inFilePath, // the file to open for readonly import
    const mdbOpenPolicy* inOpenPolicy, // runtime policies for using db
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental port open
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then call mdbFactory::ThumbToOpenPort() to get the port instance.

  virtual mdb_err ThumbToOpenPort( // redeeming a completed thumb from OpenFilePort()
    mdbEnv* ev, // context
    mdbThumb* ioThumb, // thumb from OpenFilePort() with done status
    mdbPort** acqPort) = 0; // acquire new port object
  // } ----- end port methods -----
  
  // { ----- begin store methods -----
  virtual mdb_err CanOpenFileStore(
    mdbEnv* ev, // context
    const char* inFilePath, // the file to investigate
    const mdbYarn* inFirst512Bytes,
    mdb_bool* outCanOpenAsStore, // whether OpenFileStore() might succeed
    mdb_bool* outCanOpenAsPort, // whether OpenFilePort() might succeed
    mdbYarn* outFormatVersion) = 0; // informal file format description
    
  virtual mdb_err OpenFileStore( // open an existing database
    mdbEnv* ev, // context
    const char* inFilePath, // the file to open for general db usage
    const mdbOpenPolicy* inOpenPolicy, // runtime policies for using db
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental store open
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then call mdbFactory::ThumbToOpenStore() to get the store instance.
    
  virtual mdb_err
  ThumbToOpenStore( // redeem completed thumb from OpenFileStore()
    mdbEnv* ev, // context
    mdbThumb* ioThumb, // thumb from OpenFileStore() with done status
    mdbStore** acqStore) = 0; // acquire new db store object
  
  virtual mdb_err CreateNewFileStore( // create a new db with minimal content
    mdbEnv* ev, // context
    const char* inFilePath, // name of file which should not yet exist
    const mdbOpenPolicy* inOpenPolicy, // runtime policies for using db
    mdbStore** acqStore) = 0; // acquire new db store object
  // } ----- end store methods -----

// } ===== end mdbFactory methods =====
};


/*| mdbPort: a readonly interface to a specific database file. The mutable
**| mdbStore interface is a subclass that includes writing behavior, but
**| most of the needed db methods appear in the readonly mdbPort interface.
**|
**|| mdbYarn: note all mdbPort and mdbStore subclasses must guarantee null
**| termination of all strings written into mdbYarn instances, as long as
**| mYarn_Size and mYarn_Buf are nonzero.  Even truncated string values must
**| be null terminated.  This is more strict behavior than mdbYarn requires,
**| but it is part of the mdbPort and mdbStore interface.
**|
**|| attributes: methods are provided to distinguish a readonly port from a
**| mutable store, and whether a mutable store actually has any dirty content.
**|
**|| filepath: the file path used to open the port from the mdbFactory can be
**| queried and discovered by GetPortFilePath(), which includes format info.
**|
**|| export: a port can write itself in other formats, with perhaps a typical
**| emphasis on text interchange formats used by other systems.  A port can be
**| queried to determine its preferred export interchange format, and a port
**| can be queried to see whether a specific export format is supported.  And
**| actually exporting a port requires a new destination file name and format.
**|
**|| tokens: a port supports queries about atomized strings to map tokens to
**| strings or strings to token integers.  (All atomized strings must be in
**| US-ASCII iso-8859-1 Latin1 charset encoding.)  When a port is actually a
**| mutable store and a string has not yet been atomized, then StringToToken()
**| will actually do so and modify the store.  The QueryToken() method will not
**| atomize a string if it has not already been atomized yet, even in stores.
**|
**|| tables: other than string tokens, all port content is presented through
**| tables, which are ordered collections of rows.  Tables are identified by
**| row scope and table kind, which might or might not be unique in a port,
**| depending on app convention.  When tables are effectively unique, then
**| queries for specific scope and kind pairs will find those tables.  To see
**| all tables that match specific row scope and table kind patterns, even in
**| the presence of duplicates, every port supports a GetPortTableCursor()
**| method that returns an iterator over all matching tables.  Table kind is
**| considered scoped inside row scope, so passing a zero for table kind will
**| find all table kinds for some nonzero row scope.  Passing a zero for row
**| scope will iterate over all tables in the port, in some undefined order.
**| (A new table can be added to a port using mdbStore::NewTable(), even when
**| the requested scope and kind combination is already used by other tables.)
**|
**|| memory: callers can request that a database use less memory footprint in
**| several flavors, from an inconsequential idle flavor to a rather drastic
**| panic flavor. Callers might perform an idle purge very frequently if desired
**| with very little cost, since only normally scheduled memory management will
**| be conducted, such as freeing resources for objects scheduled to be dropped.
**| Callers should perform session memory purges infrequently because they might
**| involve costly scanning of data structures to removed cached content, and
**| session purges are recommended only when a caller experiences memory crunch.
**| Callers should only rarely perform a panic purge, in response to dire memory
**| straits, since this is likely to make db operations much more expensive
**| than they would be otherwise.  A panic purge asks a database to free as much
**| memory as possible while staying effective and operational, because a caller
**| thinks application failure might otherwise occur.  (Apps might better close
**| an open db, so panic purges only make sense when a db is urgently needed.)
|*/
class mdbPort : public mdbObject {
public:
// { ===== begin mdbPort methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err GetIsPortReadonly(mdbEnv* ev, mdb_bool* outBool) = 0;
  virtual mdb_err GetIsStore(mdbEnv* ev, mdb_bool* outBool) = 0;
  virtual mdb_err GetIsStoreAndDirty(mdbEnv* ev, mdb_bool* outBool) = 0;

  virtual mdb_err GetUsagePolicy(mdbEnv* ev, 
    mdbUsagePolicy* ioUsagePolicy) = 0;

  virtual mdb_err SetUsagePolicy(mdbEnv* ev, 
    const mdbUsagePolicy* inUsagePolicy) = 0;
  // } ----- end attribute methods -----

  // { ----- begin memory policy methods -----  
  virtual mdb_err IdleMemoryPurge( // do memory management already scheduled
    mdbEnv* ev, // context
    mdb_size* outEstimatedBytesFreed) = 0; // approximate bytes actually freed

  virtual mdb_err SessionMemoryPurge( // determine preferred export format
    mdbEnv* ev, // context
    mdb_size inDesiredBytesFreed, // approximate number of bytes wanted
    mdb_size* outEstimatedBytesFreed) = 0; // approximate bytes actually freed

  virtual mdb_err PanicMemoryPurge( // desperately free all possible memory
    mdbEnv* ev, // context
    mdb_size* outEstimatedBytesFreed) = 0; // approximate bytes actually freed
  // } ----- end memory policy methods -----

  // { ----- begin filepath methods -----
  virtual mdb_err GetPortFilePath(
    mdbEnv* ev, // context
    mdbYarn* outFilePath, // name of file holding port content
    mdbYarn* outFormatVersion) = 0; // file format description
  // } ----- end filepath methods -----

  // { ----- begin export methods -----
  virtual mdb_err BestExportFormat( // determine preferred export format
    mdbEnv* ev, // context
    mdbYarn* outFormatVersion) = 0; // file format description

  // some tentative suggested import/export formats
  // "ns:msg:db:port:format:ldif:ns4.0:passthrough" // necessary
  // "ns:msg:db:port:format:ldif:ns4.5:utf8"        // necessary
  // "ns:msg:db:port:format:ldif:ns4.5:tabbed"
  // "ns:msg:db:port:format:ldif:ns4.5:binary"      // necessary
  // "ns:msg:db:port:format:html:ns3.0:addressbook" // necessary
  // "ns:msg:db:port:format:html:display:verbose"
  // "ns:msg:db:port:format:html:display:concise"
  // "ns:msg:db:port:format:mork:zany:verbose"      // necessary
  // "ns:msg:db:port:format:mork:zany:atomized"     // necessary
  // "ns:msg:db:port:format:rdf:xml"
  // "ns:msg:db:port:format:xml:mork"
  // "ns:msg:db:port:format:xml:display:verbose"
  // "ns:msg:db:port:format:xml:display:concise"
  // "ns:msg:db:port:format:xml:print:verbose"      // recommended
  // "ns:msg:db:port:format:xml:print:concise"

  virtual mdb_err
  CanExportToFormat( // can export content in given specific format?
    mdbEnv* ev, // context
    const char* inFormatVersion, // file format description
    mdb_bool* outCanExport) = 0; // whether ExportSource() might succeed

  virtual mdb_err ExportToFormat( // export content in given specific format
    mdbEnv* ev, // context
    const char* inFilePath, // the file to receive exported content
    const char* inFormatVersion, // file format description
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental export
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the export will be finished.

  // } ----- end export methods -----

  // { ----- begin token methods -----
  virtual mdb_err TokenToString( // return a string name for an integer token
    mdbEnv* ev, // context
    mdb_token inToken, // token for inTokenName inside this port
    mdbYarn* outTokenName) = 0; // the type of table to access
  
  virtual mdb_err StringToToken( // return an integer token for scope name
    mdbEnv* ev, // context
    const char* inTokenName, // Latin1 string to tokenize if possible
    mdb_token* outToken) = 0; // token for inTokenName inside this port
    
  // String token zero is never used and never supported. If the port
  // is a mutable store, then StringToToken() to create a new
  // association of inTokenName with a new integer token if possible.
  // But a readonly port will return zero for an unknown scope name.

  virtual mdb_err QueryToken( // like StringToToken(), but without adding
    mdbEnv* ev, // context
    const char* inTokenName, // Latin1 string to tokenize if possible
    mdb_token* outToken) = 0; // token for inTokenName inside this port
  
  // QueryToken() will return a string token if one already exists,
  // but unlike StringToToken(), will not assign a new token if not
  // already in use.

  // } ----- end token methods -----

  // { ----- begin row methods -----  
  virtual mdb_err HasRow( // contains a row with the specified oid?
    mdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    mdb_bool* outHasRow) = 0; // whether GetRow() might succeed
    
  virtual mdb_err GetRow( // access one row with specific oid
    mdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    mdbRow** acqRow) = 0; // acquire specific row (or null)

  virtual mdb_err GetRowRefCount( // get number of tables that contain a row 
    mdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    mdb_count* outRefCount) = 0; // number of tables containing inRowKey 
  // } ----- end row methods -----

  // { ----- begin table methods -----  
  virtual mdb_err HasTable( // supports a table with the specified oid?
    mdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical table oid
    mdb_bool* outHasTable) = 0; // whether GetTable() might succeed
    
  virtual mdb_err GetTable( // access one table with specific oid
    mdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical table oid
    mdbTable** acqTable) = 0; // acquire specific table (or null)
  
  virtual mdb_err HasTableKind( // supports a table of the specified type?
    mdbEnv* ev, // context
    mdb_scope inRowScope, // rid scope for row ids
    mdb_kind inTableKind, // the type of table to access
    mdb_count* outTableCount, // current number of such tables
    mdb_bool* outSupportsTable) = 0; // whether GetTableKind() might succeed
    
  // row scopes to be supported include the following suggestions:
  // "ns:msg:db:row:scope:address:cards:all"
  // "ns:msg:db:row:scope:mail:messages:all"
  // "ns:msg:db:row:scope:news:articles:all"
 
  // table kinds to be supported include the following suggestions:
  // "ns:msg:db:table:kind:address:cards:main"
  // "ns:msg:db:table:kind:address:lists:all" 
  // "ns:msg:db:table:kind:address:list" 
  // "ns:msg:db:table:kind:news:threads:all" 
  // "ns:msg:db:table:kind:news:thread" 
  // "ns:msg:db:table:kind:mail:threads:all"
  // "ns:msg:db:table:kind:mail:thread"
    
  virtual mdb_err GetTableKind( // access one (random) table of specific type
    mdbEnv* ev, // context
    mdb_scope inRowScope,      // row scope for row ids
    mdb_kind inTableKind,      // the type of table to access
    mdb_count* outTableCount, // current number of such tables
    mdb_bool* outMustBeUnique, // whether port can hold only one of these
    mdbTable** acqTable) = 0;       // acquire scoped collection of rows
    
  virtual mdb_err
  GetPortTableCursor( // get cursor for all tables of specific type
    mdbEnv* ev, // context
    mdb_scope inRowScope, // row scope for row ids
    mdb_kind inTableKind, // the type of table to access
    mdbPortTableCursor** acqCursor) = 0; // all such tables in the port
  // } ----- end table methods -----

// } ===== end mdbPort methods =====
};

/*| mdbStore: a mutable interface to a specific database file.
**|
**|| tables: one can force a new table to exist in a store with NewTable()
**| and nonzero values for both row scope and table kind.  (If one wishes only
**| one table of a certain kind, then one might look for it first using the
**| GetTableKind() method).  One can pass inMustBeUnique to force future
**| users of this store to be unable to create other tables with the same pair
**| of scope and kind attributes.  When inMustBeUnique is true, and the table
**| with the given scope and kind pair already exists, then the existing one
**| is returned instead of making a new table.  Similarly, if one passes false
**| for inMustBeUnique, but the table kind has already been marked unique by a
**| previous user of the store, then the existing unique table is returned.
**|
**|| import: all or some of another port's content can be imported by calling
**| AddPortContent() with a row scope identifying the extent of content to
**| be imported.  A zero row scope will import everything.  A nonzero row
**| scope will only import tables with a matching row scope.  Note that one
**| must somehow find a way to negotiate possible conflicts between existing
**| row content and imported row content, and this involves a specific kind of
**| definition for row identity involving either row IDs or unique attributes,
**| or some combination of these two.  At the moment I am just going to wave
**| my hands, and say the default behavior is to assign all new row identities
**| to all imported content, which will result in no merging of content; this
**| must change later because it is unacceptable in some contexts.
**|
**|| commits: to manage modifications in a mutable store, very few methods are
**| really needed to indicate global policy choices that are independent of 
**| the actual modifications that happen in objects at the level of tables,
**| rows, and cells, etc.  The most important policy to specify is which sets
**| of changes are considered associated in a manner such that they should be
**| applied together atomically to a given store.  We call each such group of
**| changes a transaction.  We handle three different grades of transaction,
**| but they differ only in semantic significance to the application, and are
**| not intended to nest.  (If small transactions were nested inside large
**| transactions, that would imply that a single large transaction must be
**| atomic over all the contained small transactions; but actually we intend
**| smalls transaction never be undone once commited due to, say, aborting a
**| transaction of greater significance.)  The small, large, and session level
**| commits have equal granularity, and differ only in risk of loss from the
**| perspective of an application.  Small commits characterize changes that
**| can be lost with relatively small risk, so small transactions can delay
**| until later if they are expensive or impractical to commit.  Large commits
**| involve changes that would probably inconvenience users if lost, so the
**| need to pay costs of writing is rather greater than with small commits.
**| Session commits are last ditch attempts to save outstanding changes before
**| stopping the use of a particular database, so there will be no later point
**| in time to save changes that have been delayed due to possible high cost.
**| If large commits are never delayed, then a session commit has about the
**| same performance effect as another large commit; but if small and large
**| commits are always delayed, then a session commit is likely to be rather
**| expensive as a runtime cost compared to any earlier database usage.
**|
**|| aborts: the only way to abort changes to a store is by closing the store.
**| So there is no specific method for causing any abort.  Stores must discard
**| all changes made that are uncommited when a store is closed.  This design
**| choice makes the implementations of tables, rows, and cells much less
**| complex because they need not maintain a record of undobable changes.  When
**| a store is closed, presumably this precipitates the closure of all tables,
**| rows, and cells in the store as well.   So an application can revert the
**| state of a store in the user interface by quietly closing and reopening a
**| store, because this will discard uncommited changes and show old content.
**| This implies an app that closes a store will need to send a "scramble"
**| event notification to any views that depend on old discarded content.
|*/
class mdbStore : public mdbPort {
public:
// { ===== begin mdbStore methods =====

  // { ----- begin table methods -----
  virtual mdb_err NewTable( // make one new table of specific type
    mdbEnv* ev, // context
    mdb_scope inRowScope,    // row scope for row ids
    mdb_kind inTableKind,    // the type of table to access
    mdb_bool inMustBeUnique, // whether store can hold only one of these
    mdbTable** acqTable) = 0;     // acquire scoped collection of rows
  // } ----- end table methods -----

  // { ----- begin row scope methods -----
  virtual mdb_err RowScopeHasAssignedIds(mdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned) = 0; // nonzero if store db assigned specified

  virtual mdb_err SetCallerAssignedIds(mdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned) = 0; // nonzero if store db assigned specified

  virtual mdb_err SetStoreAssignedIds(mdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned) = 0; // nonzero if store db assigned specified
  // } ----- end row scope methods -----

  // { ----- begin row methods -----
  virtual mdb_err NewRowWithOid(mdbEnv* ev, // new row w/ caller assigned oid
    mdb_scope inRowScope,   // row scope for row ids
    const mdbOid* inOid,   // caller assigned oid
    mdbRow** acqRow) = 0; // create new row

  virtual mdb_err NewRow(mdbEnv* ev, // new row with db assigned oid
    mdb_scope inRowScope,   // row scope for row ids
    mdbRow** acqRow) = 0; // create new row
  // Note this row must be added to some table or cell child before the
  // store is closed in order to make this row persist across sesssions.
  // } ----- end row methods -----

  // { ----- begin inport/export methods -----
  virtual mdb_err ImportContent( // import content from port
    mdbEnv* ev, // context
    mdb_scope inRowScope, // scope for rows (or zero for all?)
    mdbPort* ioPort, // the port with content to add to store
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental import
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the import will be finished.
  // } ----- end inport/export methods -----

  // { ----- begin hinting methods -----
  virtual mdb_err
  ShareAtomColumnsHint( // advise re shared column content atomizing
    mdbEnv* ev, // context
    mdb_scope inScopeHint, // zero, or suggested shared namespace
    const mdbColumnSet* inColumnSet) = 0; // cols desired tokenized together

  virtual mdb_err
  AvoidAtomColumnsHint( // advise column with poor atomizing prospects
    mdbEnv* ev, // context
    const mdbColumnSet* inColumnSet) = 0; // cols with poor atomizing prospects
  // } ----- end hinting methods -----

  // { ----- begin commit methods -----
  virtual mdb_err SmallCommit( // save minor changes if convenient and uncostly
    mdbEnv* ev) = 0; // context
  
  virtual mdb_err LargeCommit( // save important changes if at all possible
    mdbEnv* ev, // context
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental commit
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.

  virtual mdb_err SessionCommit( // save all changes if large commits delayed
    mdbEnv* ev, // context
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental commit
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.

  virtual mdb_err
  CompressCommit( // commit and make db physically smaller if possible
    mdbEnv* ev, // context
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental commit
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.
  // } ----- end commit methods -----

// } ===== end mdbStore methods =====
};

/*| mdbCursor: base cursor class for iterating row cells and table rows
**|
**|| count: the number of elements in the collection (table or row)
**|
**|| seed: the change count in the underlying collection, which is synced
**| with the collection when the iteration position is set, and henceforth
**| acts to show whether the iter has lost collection synchronization, in
**| case it matters to clients whether any change happens during iteration.
**|
**|| pos: the position of the current element in the collection.  Negative
**| means a position logically before the first element.  A positive value
**| equal to count (or larger) implies a position after the last element.
**| To iterate over all elements, set the position to negative, so subsequent
**| calls to any 'next' method will access the first collection element.
**|
**|| doFailOnSeedOutOfSync: whether a cursor should return an error if the
**| cursor's snapshot of a table's seed becomes stale with respect the table's
**| current seed value (which implies the iteration is less than total) in
**| between to cursor calls that actually access collection content.  By
**| default, a cursor should assume this attribute is false until specified,
**| so that iterations quietly try to re-sync when they loose coherence.
|*/
class mdbCursor : public mdbObject { // collection iterator

// { ===== begin mdbCursor methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err GetCount(mdbEnv* ev, mdb_count* outCount) = 0; // readonly
  virtual mdb_err GetSeed(mdbEnv* ev, mdb_seed* outSeed) = 0;    // readonly
  
  virtual mdb_err SetPos(mdbEnv* ev, mdb_pos inPos) = 0;   // mutable
  virtual mdb_err GetPos(mdbEnv* ev, mdb_pos* outPos) = 0;
  
  virtual mdb_err SetDoFailOnSeedOutOfSync(mdbEnv* ev, mdb_bool inFail) = 0;
  virtual mdb_err SetDoFailOnSeedOutOfSync(mdbEnv* ev, mdb_bool* outFail) = 0;
  // } ----- end attribute methods -----

// } ===== end mdbCursor methods =====
};

/*| mdbPortTableCursor: cursor class for iterating port tables
**|
**|| port: the cursor is associated with a specific port, which can be
**| set to a different port (which resets the position to -1 so the
**| next table acquired is the first in the port.
**|
|*/
class mdbPortTableCursor : public mdbCursor { // table collection iterator

// { ===== begin mdbPortTableCursor methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err SetPort(mdbEnv* ev, mdbPort* ioPort) = 0; // sets pos to -1
  virtual mdb_err GetPort(mdbEnv* ev, mdbPort** acqPort) = 0;
  
  virtual mdb_err SetRowScope(mdbEnv* ev, // sets pos to -1
    mdb_scope inRowScope) = 0;
  virtual mdb_err GetRowScope(mdbEnv* ev, mdb_scope* outRowScope) = 0; 
  // setting row scope to zero iterates over all row scopes in port
    
  virtual mdb_err SetTableKind(mdbEnv* ev, // sets pos to -1
    mdb_kind inTableKind) = 0;
  virtual mdb_err GetTableKind(mdbEnv* ev, mdb_kind* outTableKind) = 0;
  // setting table kind to zero iterates over all table kinds in row scope
  // } ----- end attribute methods -----

  // { ----- begin table iteration methods -----
  virtual mdb_err NextTable( // get table at next position in the db
    mdbEnv* ev, // context
    mdbTable** acqTable) = 0; // the next table in the iteration
  // } ----- end table iteration methods -----

// } ===== end mdbPortTableCursor methods =====
};

/*| mdbCollection: an object that collects a set of other objects as members.
**| The main purpose of this base class is to unify the perceived semantics
**| of tables and rows where their collection behavior is similar.  This helps
**| isolate the mechanics of collection behavior from the other semantics that
**| are more characteristic of rows and tables.
**|
**|| count: the number of objects in a collection is the member count. (Some
**| collection interfaces call this attribute the 'size', but that can be a
**| little ambiguous, and counting actual members is harder to confuse.)
**|
**|| seed: the seed of a collection is a counter for changes in membership in
**| a specific collection.  This seed should change when members are added to
**| or removed from a collection, but not when a member changes internal state.
**| The seed should also change whenever the internal collection of members has
**| a complex state change that reorders member positions (say by sorting) that
**| would affect the nature of an iteration over that collection of members.
**| The purpose of a seed is to inform any outstanding collection cursors that
**| they might be stale, without incurring the cost of broadcasting an event
**| notification to such cursors, which would need more data structure support.
**| Presumably a cursor in a particular mdb code suite has much more direct
**| access to a collection seed member slot that this abstract COM interface,
**| so this information is intended more for clients outside mdb that want to
**| make inferences similar to those made by the collection cursors.  The seed
**| value as an integer magnitude is not very important, and callers should not
**| assume meaningful information can be derived from an integer value beyond
**| whether it is equal or different from a previous inspection.  A seed uses
**| integers of many bits in order to make the odds of wrapping and becoming
**| equal to an earlier seed value have probability that is vanishingly small.
**|
**|| port: every collection is associated with a specific database instance.
**|
**|| cursor: a subclass of mdbCursor suitable for this specific collection
**| subclass.  The ability to GetCursor() from the base mdbCollection class
**| is not really as useful as getting a more specifically typed cursor more
**| directly from the base class without any casting involved.  So including
**| this method here is more for conceptual illustration.
**|
**|| oid: every collection has an identity that persists from session to
**| session. Implementations are probably able to distinguish row IDs from
**| table IDs, but we don't specify anything official in this regard.  A
**| collection has the same identity for the lifetime of the collection,
**| unless identity is swapped with another collection by means of a call to
**| BecomeContent(), which is considered a way to swap a new representation
**| for an old well-known object.  (Even so, only content appears to change,
**| while the identity seems to stay the same.)
**|
**|| become: developers can effectively cause two objects to swap identities,
**| in order to effect a complete swap between what persistent content is
**| represented by two oids.  The caller should consider this a content swap,
**| and not identity wap, because identities will seem to stay the same while
**| only content changes.  However, implementations will likely do this
**| internally by swapping identities.  Callers must swap content only
**| between objects of similar type, such as a row with another row, and a
**| table with another table, because implementations need not support
**| cross-object swapping because it might break object name spaces.
**|
**|| dropping: when a caller expects a row or table will no longer be used, the
**| caller can tell the collection to 'drop activity', which means the runtime
**| object can have it's internal representation purged to save memory or any
**| other resource that is being consumed by the collection's representation.
**| This has no effect on the collection's persistent content or semantics,
**| and is only considered a runtime effect.  After a collection drops
**| activity, the object should still be as usable as before (because it has
**| NOT been closed), but further usage can be expensive to re-instate because
**| it might involve reallocating space and/or re-reading disk space.  But
**| since this future usage is not expected, the caller does not expect to
**| pay the extra expense.  An implementation can choose to implement
**| 'dropping activity' in different ways, or even not at all if this
**| operation is not really feasible.  Callers cannot ask objects whether they
**| are 'dropped' or not, so this should be transparent. (Note that
**| implementors might fear callers do not really know whether future
**| usage will occur, and therefore might delay the act of dropping until
**| the near future, until seeing whether the object is used again
**| immediately elsewhere. Such use soon after the drop request might cause
**| the drop to be cancelled.)
|*/
class mdbCollection : public mdbObject { // sequence of objects

// { ===== begin mdbCollection methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err GetSeed(mdbEnv* ev,
    mdb_seed* outSeed) = 0;    // member change count
  virtual mdb_err GetCount(mdbEnv* ev,
    mdb_count* outCount) = 0; // member count

  virtual mdb_err GetPort(mdbEnv* ev,
    mdbPort** acqPort) = 0; // collection container
  // } ----- end attribute methods -----

  // { ----- begin cursor methods -----
  virtual mdb_err GetCursor( // make a cursor starting iter at inMemberPos
    mdbEnv* ev, // context
    mdb_pos inMemberPos, // zero-based ordinal pos of member in collection
    mdbCursor** acqCursor) = 0; // acquire new cursor instance
  // } ----- end cursor methods -----

  // { ----- begin ID methods -----
  virtual mdb_err GetOid(mdbEnv* ev,
    const mdbOid* outOid) = 0; // read object identity
  virtual mdb_err BecomeContent(mdbEnv* ev,
    const mdbOid* inOid) = 0; // exchange content
  // } ----- end ID methods -----

  // { ----- begin activity dropping methods -----
  virtual mdb_err DropActivity( // tell collection usage no longer expected
    mdbEnv* ev) = 0;
  // } ----- end activity dropping methods -----

// } ===== end mdbCollection methods =====
};

/*| mdbTable: an ordered collection of rows
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
**| by moving content to or from an instance of mdbRow.  Note that mdbRow
**| never represents the actual row inside a table, and this is the reason
**| why mdbRow instances do not have row IDs or row scopes.  So an instance
**| of mdbRow always and only contains a snapshot of some or all content in
**| past, present, or future persistent row inside a table.  This means that
**| reading and writing rows in tables has strictly copy semantics, and we
**| currently do not plan any exceptions for specific performance reasons.
**|
**|| sorting: note all rows are assumed sorted by row ID as a secondary
**| sort following the primary column sort, when table rows are sorted.
**|
**|| indexes:
|*/
class mdbTable : public mdbCollection { // a collection of rows
public:
// { ===== begin mdbTable methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err GetTableKind(mdbEnv* ev, mdb_kind* outTableKind) = 0;
  virtual mdb_err GetRowScope(mdbEnv* ev, mdb_scope* outRowScope) = 0;
  
  virtual mdb_err GetPort( // get port containing this table
    mdbEnv* ev, // context
    mdbPort** acqPort) = 0; // acquire containing port or store
  // } ----- end attribute methods -----

  // { ----- begin cursor methods -----
  virtual mdb_err GetTableRowCursor( // make a cursor, starting iteration at inRowPos
    mdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    mdbTableRowCursor** acqCursor) = 0; // acquire new cursor instance
  // } ----- end row position methods -----

  // { ----- begin row position methods -----
  virtual mdb_err RowPosToOid( // get row member for a table position
    mdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    mdbOid* outOid) = 0; // row oid at the specified position
    
  // Note that HasRow() performs the inverse oid->pos mapping
  // } ----- end row position methods -----

  // { ----- begin oid set methods -----
  virtual mdb_err AddOid( // make sure the row with inOid is a table member 
    mdbEnv* ev, // context
    const mdbOid* inOid) = 0; // row to ensure membership in table

  virtual mdb_err HasOid( // test for the table position of a row member
    mdbEnv* ev, // context
    const mdbOid* inOid, // row to find in table
    mdb_pos* outPos) = 0; // zero-based ordinal position of row in table

  virtual mdb_err CutOid( // make sure the row with inOid is not a member 
    mdbEnv* ev, // context
    const mdbOid* inOid) = 0; // row to remove from table
  // } ----- end oid set methods -----

  // { ----- begin row set methods -----
  virtual mdb_err NewRow( // create a new row instance in table
    mdbEnv* ev, // context
    mdbOid* ioOid, // please use zero (unbound) rowId for db-assigned IDs
    mdbRow** acqRow) = 0; // create new row

  virtual mdb_err AddRow( // make sure the row with inOid is a table member 
    mdbEnv* ev, // context
    mdbRow* ioRow) = 0; // row to ensure membership in table

  virtual mdb_err HasRow( // test for the table position of a row member
    mdbEnv* ev, // context
    mdbRow* ioRow, // row to find in table
    mdb_pos* outPos) = 0; // zero-based ordinal position of row in table

  virtual mdb_err CutRow( // make sure the row with inOid is not a member 
    mdbEnv* ev, // context
    mdbRow* ioRow) = 0; // row to remove from table
  // } ----- end row set methods -----

  // { ----- begin searching methods -----
  virtual mdb_err SearchOneSortedColumn( // search only currently sorted col
    mdbEnv* ev, // context
    const mdbYarn* inPrefix, // content to find as prefix in row's column cell
    mdbRange* outRange) = 0; // range of matching rows
    
  virtual mdb_err SearchManyColumns( // search variable number of sorted cols
    mdbEnv* ev, // context
    const mdbYarn* inPrefix, // content to find as prefix in row's column cell
    mdbSearch* ioSearch, // columns to search and resulting ranges
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental search
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the search will be finished.  Until that time, the ioSearch argument
  // is assumed referenced and used by the thumb; one should not inspect any
  // output results in ioSearch until after the thumb is finished with it.
  // } ----- end searching methods -----

  // { ----- begin hinting methods -----
  virtual mdb_err SearchColumnsHint( // advise re future expected search cols  
    mdbEnv* ev, // context
    const mdbColumnSet* inColumnSet) = 0; // columns likely to be searched
    
  virtual mdb_err SortColumnsHint( // advise re future expected sort columns  
    mdbEnv* ev, // context
    const mdbColumnSet* inColumnSet) = 0; // columns for likely sort requests
  // } ----- end hinting methods -----

  // { ----- begin sorting methods -----
  // sorting: note all rows are assumed sorted by row ID as a secondary
  // sort following the primary column sort, when table rows are sorted.

  virtual mdb_err
  CanSortColumn( // query which column is currently used for sorting
    mdbEnv* ev, // context
    mdb_column inColumn, // column to query sorting potential
    mdb_bool* outCanSort) = 0; // whether the column can be sorted
  
  virtual mdb_err
  NewSortColumn( // change the column used for sorting in the table
    mdbEnv* ev, // context
    mdb_column inColumn, // requested new column for sorting table
    mdb_column* outActualColumn, // column actually used for sorting
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental table resort
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the sort will be finished. 
  
  virtual mdb_err
  NewSortColumnWithCompare( // change sort column with explicit compare
    mdbEnv* ev, // context
    mdbCompare* ioCompare, // explicit interface for yarn comparison
    mdb_column inColumn, // requested new column for sorting table
    mdb_column* outActualColumn, // column actually used for sorting
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental table resort
  // Note the table will hold a reference to inCompare if this object is used
  // to sort the table.  Until the table closes, callers can only force release
  // of the compare object by changing the sort (by say, changing to unsorted).
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the sort will be finished. 
  
  virtual mdb_err GetSortColumn( // query which col is currently sorted
    mdbEnv* ev, // context
    mdb_column* outColumn) = 0; // col the table uses for sorting (or zero)

  
  virtual mdb_err CloneSortColumn( // view same table with a different sort
    mdbEnv* ev, // context
    mdb_column inColumn, // requested new column for sorting table
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental table clone
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then call mdbTable::ThumbToCloneSortTable() to get the table instance.
    
  virtual mdb_err
  ThumbToCloneSortTable( // redeem complete CloneSortColumn() thumb
    mdbEnv* ev, // context
    mdbThumb* ioThumb, // thumb from CloneSortColumn() with done status
    mdbTable** acqTable) = 0; // new table instance (or old if sort unchanged)
  // } ----- end sorting methods -----

  // { ----- begin moving methods -----
  // moving a row does nothing unless a table is currently unsorted
  
  virtual mdb_err MoveOid( // change position of row in unsorted table
    mdbEnv* ev, // context
    const mdbOid* inOid,  // row oid to find in table
    mdb_pos inHintFromPos, // suggested hint regarding start position
    mdb_pos inToPos,       // desired new position for row inRowId
    mdb_pos* outActualPos) = 0; // actual new position of row in table

  virtual mdb_err MoveRow( // change position of row in unsorted table
    mdbEnv* ev, // context
    mdbRow* ioRow,  // row oid to find in table
    mdb_pos inHintFromPos, // suggested hint regarding start position
    mdb_pos inToPos,       // desired new position for row inRowId
    mdb_pos* outActualPos) = 0; // actual new position of row in table
  // } ----- end moving methods -----
  
  // { ----- begin index methods -----
  virtual mdb_err AddIndex( // create a sorting index for column if possible
    mdbEnv* ev, // context
    mdb_column inColumn, // the column to sort by index
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental index building
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the index addition will be finished.
  
  virtual mdb_err CutIndex( // stop supporting a specific column index
    mdbEnv* ev, // context
    mdb_column inColumn, // the column with index to be removed
    mdbThumb** acqThumb) = 0; // acquire thumb for incremental index destroy
  // Call mdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the index removal will be finished.
  
  virtual mdb_err HasIndex( // query for current presence of a column index
    mdbEnv* ev, // context
    mdb_column inColumn, // the column to investigate
    mdb_bool* outHasIndex) = 0; // whether column has index for this column

  
  virtual mdb_err EnableIndexOnSort( // create an index for col on first sort
    mdbEnv* ev, // context
    mdb_column inColumn) = 0; // the column to index if ever sorted
  
  virtual mdb_err QueryIndexOnSort( // check whether index on sort is enabled
    mdbEnv* ev, // context
    mdb_column inColumn, // the column to investigate
    mdb_bool* outIndexOnSort) = 0; // whether column has index-on-sort enabled
  
  virtual mdb_err DisableIndexOnSort( // prevent future index creation on sort
    mdbEnv* ev, // context
    mdb_column inColumn) = 0; // the column to index if ever sorted
  // } ----- end index methods -----

// } ===== end mdbTable methods =====
};

/*| mdbTableRowCursor: cursor class for iterating table rows
**|
**|| table: the cursor is associated with a specific table, which can be
**| set to a different table (which resets the position to -1 so the
**| next row acquired is the first in the table.
**|
**|| NextRowId: the rows in the table can be iterated by identity alone,
**| without actually reading the cells of any row with this method.
**|
**|| NextRowCells: read the next row in the table, but only read cells
**| from the table which are already present in the row (so no new cells
**| are added to the row, even if they are present in the table).  All the
**| cells will have content specified, even it is the empty string.  No
**| columns will be removed, even if missing from the row (because missing
**| and empty are semantically equivalent).
**|
**|| NextRowAllCells: read the next row in the table, and access all the
**| cells for this row in the table, adding any missing columns to the row
**| as needed until all cells are represented.  All the
**| cells will have content specified, even it is the empty string.  No
**| columns will be removed, even if missing from the row (because missing
**| and empty are semantically equivalent).
**|
|*/
class mdbTableRowCursor : public mdbCursor { // table row iterator
public:
// { ===== begin mdbTableRowCursor methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err SetTable(mdbEnv* ev, mdbTable* ioTable) = 0; // sets pos to -1
  virtual mdb_err GetTable(mdbEnv* ev, mdbTable** acqTable) = 0;
  // } ----- end attribute methods -----

  // { ----- begin oid iteration methods -----
  virtual mdb_err NextRowOid( // get row id of next row in the table
    mdbEnv* ev, // context
    const mdbOid* outOid, // out row oid
    mdb_pos* outRowPos) = 0;    // zero-based position of the row in table
  // } ----- end oid iteration methods -----

  // { ----- begin row iteration methods -----
  virtual mdb_err NextRow( // get row cells from table for cells already in row
    mdbEnv* ev, // context
    mdbRow** acqRow, // acquire next row in table
    mdb_pos* outRowPos) = 0;    // zero-based position of the row in table
  // } ----- end row iteration methods -----

  // { ----- begin copy iteration methods -----
  virtual mdb_err NextRowCopy( // put row cells into sink only when already in sink
    mdbEnv* ev, // context
    mdbRow* ioSinkRow, // sink for row cells read from next row
    const mdbOid* outOid, // out row oid
    mdb_pos* outRowPos) = 0;    // zero-based position of the row in table

  virtual mdb_err NextRowCopyAll( // put all row cells into sink, adding to sink
    mdbEnv* ev, // context
    mdbRow* ioSinkRow, // sink for row cells read from next row
    const mdbOid* outOid, // out row oid
    mdb_pos* outRowPos) = 0;    // zero-based position of the row in table
  // } ----- end copy iteration methods -----

// } ===== end mdbTableRowCursor methods =====
};

/*| mdbRow: a collection of cells
**|
|*/
class mdbRow : public mdbCollection { // cell tuple

// { ===== begin mdbRow methods =====

  // { ----- begin cursor methods -----
  virtual mdb_err
  GetRowCellCursor( // make a cursor starting iteration at inRowPos
    mdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    mdbTableRowCursor** acqCursor) = 0; // acquire new cursor instance
  // } ----- end cursor methods -----

  // { ----- begin column methods -----
  virtual mdb_err AddColumn( // make sure a particular column is inside row
    mdbEnv* ev, // context
    mdb_column inColumn, // column to add
    const mdbYarn* inYarn) = 0; // cell value to install

  virtual mdb_err CutColumn( // make sure a column is absent from the row
    mdbEnv* ev, // context
    mdb_column inColumn) = 0; // column to ensure absent from row

  virtual mdb_err CutAllColumns( // remove all columns from the row
    mdbEnv* ev) = 0; // context
  // } ----- end column methods -----

  // { ----- begin cell methods -----
  virtual mdb_err NewCell( // get cell for specified column, or add new one
    mdbEnv* ev, // context
    mdb_column inColumn, // column to add
    mdbCell** acqCell) = 0; // cell column and value
    
  virtual mdb_err AddCell( // copy a cell from another row to this row
    mdbEnv* ev, // context
    const mdbCell* inCell) = 0; // cell column and value
    
  virtual mdb_err GetCell( // find a cell in this row
    mdbEnv* ev, // context
    mdb_column inColumn, // column to find
    mdbCell** acqCell) = 0; // cell for specified column, or null
    
  virtual mdb_err EmptyAllCells( // make all cells in row empty of content
    mdbEnv* ev) = 0; // context
  // } ----- end cell methods -----

  // { ----- begin row methods -----
  virtual mdb_err AddRow( // add all cells in another row to this one
    mdbEnv* ev, // context
    mdbRow* ioSourceRow) = 0; // row to union with
    
  virtual mdb_err SetRow( // make exact duplicate of another row
    mdbEnv* ev, // context
    mdbRow* ioSourceRow) = 0; // row to duplicate
  // } ----- end row methods -----

// } ===== end mdbRow methods =====
};

/*| mdbRowCellCursor: cursor class for iterating row cells
**|
**|| row: the cursor is associated with a specific row, which can be
**| set to a different row (which resets the position to -1 so the
**| next cell acquired is the first in the row.
**|
**|| NextCell: get the next cell in the row and return its position and
**| a new instance of a mdbCell to represent this next cell.
|*/
class mdbRowCellCursor : public mdbCursor { // cell collection iterator

// { ===== begin mdbRowCellCursor methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err SetRow(mdbEnv* ev, mdbRow* ioRow) = 0; // sets pos to -1
  virtual mdb_err GetRow(mdbEnv* ev, mdbRow** acqRow) = 0;
  // } ----- end attribute methods -----

  // { ----- begin cell iteration methods -----
  virtual mdb_err NextCell( // get next cell in the row
    mdbEnv* ev, // context
    mdb_column* outColumn, // column for this particular cell
    mdb_pos* outPos, // position of cell in row sequence
    mdbCell** acqCell) = 0; // the next cell in the iteration
    
  virtual mdb_err PickNextCell( // get next cell in row within filter set
    mdbEnv* ev, // context
    const mdbColumnSet* inFilterSet, // set of cols with actual caller interest
    mdb_column* outColumn, // column for this particular cell
    mdb_pos* outPos, // position of cell in row sequence
    mdbCell** acqCell) = 0; // the next cell in the iteration

  // Note that inFilterSet should not have too many (many more than 10?)
  // cols, since this might imply a potential excessive consumption of time
  // over many cursor calls when looking for column and filter intersection.
  // } ----- end cell iteration methods -----

// } ===== end mdbRowCellCursor methods =====
};

/*| mdbBlob: a base class for objects composed mainly of byte sequence state.
**| (This provides a base class for mdbCell, so that cells themselves can
**| be used to set state in another cell, without extracting a buffer.)
|*/
class mdbBlob : public mdbObject { // a string with associated charset

// { ===== begin mdbBlob methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err SetBlob(mdbEnv* ev,
    mdbBlob* ioBlob) = 0; // reads inBlob slots
  // when inBlob is in the same suite, this might be fastest cell-to-cell
  
  virtual mdb_err ClearBlob( // make empty (so content has zero length)
    mdbEnv* ev) = 0;
  // clearing a yarn is like SetYarn() with empty yarn instance content
  
  virtual mdb_err GetBlobFill(mdbEnv* ev,
    mdb_fill* outFill) = 0;  // size of blob 
  // Same value that would be put into mYarn_Fill, if one called GetYarn()
  // with a yarn instance that had mYarn_Buf==nil and mYarn_Size==0.
  
  virtual mdb_err SetYarn(mdbEnv* ev, 
    const mdbYarn* inYarn) = 0;   // reads from yarn slots
  // make this text object contain content from the yarn's buffer
  
  virtual mdb_err GetYarn(mdbEnv* ev, 
    mdbYarn* outYarn) = 0;  // writes some yarn slots 
  // copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
  
  virtual mdb_err AliasYarn(mdbEnv* ev, 
    mdbYarn* outYarn) = 0; // writes ALL yarn slots
  // AliasYarn() reveals sensitive internal text buffer state to the caller
  // by setting mYarn_Buf to point into the guts of this text implementation.
  //
  // The caller must take great care to avoid writing on this space, and to
  // avoid calling any method that would cause the state of this text object
  // to change (say by directly or indirectly setting the text to hold more
  // content that might grow the size of the buffer and free the old buffer).
  // In particular, callers should scrupulously avoid making calls into the
  // mdb interface to write any content while using the buffer pointer found
  // in the returned yarn instance.  Best safe usage involves copying content
  // into some other kind of external content representation beyond mdb.
  //
  // (The original design of this method a week earlier included the concept
  // of very fast and efficient cooperative locking via a pointer to some lock
  // member slot.  But let's ignore that complexity in the current design.)
  //
  // AliasYarn() is specifically intended as the first step in transferring
  // content from mdbBlob to a nsString representation, without forcing extra
  // allocations and/or memory copies. (A standard mdbBlob_AsString() utility
  // will use AliasYarn() as the first step in setting a nsString instance.)
  //
  // This is an alternative to the GetYarn() method, which has copy semantics
  // only; AliasYarn() relaxes a robust safety principle only for performance
  // reasons, to accomodate the need for callers to transform text content to
  // some other canonical representation that would necessitate an additional
  // copy and transformation when such is incompatible with the mdbYarn format.
  //
  // The implementation of AliasYarn() should have extremely little overhead
  // besides the virtual dispatch to the method implementation, and the code
  // necessary to populate all the mdbYarn member slots with internal buffer
  // address and metainformation that describes the buffer content.  Note that
  // mYarn_Grow must always be set to nil to indicate no resizing is allowed.
  
  // } ----- end attribute methods -----

// } ===== end mdbBlob methods =====
};

/*| mdbCell: the text in a single column of a row.  The base mdbBlob
**| class provides all the interface related to accessing cell text.
**|
**|| column: each cell in a row appears in a specific column, where this
**| column is identified by the an integer mdb_scope value (generated by
**| the StringToScopeToken() method in the containing mdbPort instance).
**| Because a row cannot have more than one cell with the same column,
**| something must give if one calls SetColumn() with an existing column
**| in the same row. When this happens, the other cell is replaced with
**| this cell (and the old cell is closed if it has outstanding refs).
**|
**|| row: every cell instance is a part of some row, and every cell knows
**| which row is the parent row.  (Note this should be represented by a
**| weak backpointer, so that outstanding cell references cannot keep a
**| row open that should be closed. Otherwise we'd have ref graph cycles.)
**|
**|| text: a cell can either be text, or it can have a child row or table,
**| but not both at once.  If text is read from a cell with a child, the text
**| content should be empty (for AliasYarn()) or a description of the type
**| of child (perhaps "mdb:cell:child:row" or "mdb:cell:child:table").
**|
**|| child: a cell might reference another row or a table, rather than text.
**| The interface for putting and getting children rows and tables was first
**| defined in the mdbTable interface, but then this was moved to this cell
**| interface as more natural. 
|*/
class mdbCell : public mdbBlob { // text attribute in row with column scope

// { ===== begin mdbCell methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err SetColumn(mdbEnv* ev, mdb_column inColumn) = 0; 
  virtual mdb_err GetColumn(mdbEnv* ev, mdb_column* outColumn) = 0;
  
  virtual mdb_err GetCellInfo(  // all cell metainfo except actual content
    mdbEnv* ev, 
    mdb_column* outColumn,           // the column in the containing row
    mdb_fill*   outBlobFill,         // the size of text content in bytes
    mdbOid*     outChildOid,         // oid of possible row or table child
    mdb_bool*   outIsRowChild) = 0;  // nonzero if child, and a row child

  // Checking all cell metainfo is a good way to avoid forcing a large cell
  // in to memory when you don't actually want to use the content.
  
  virtual mdb_err GetRow(mdbEnv* ev, // parent row for this cell
    mdbRow** acqRow) = 0;
  virtual mdb_err GetPort(mdbEnv* ev, // port containing cell
    mdbPort** acqPort) = 0;
  // } ----- end attribute methods -----

  // { ----- begin children methods -----
  virtual mdb_err HasAnyChild( // does cell have a child instead of text?
    mdbEnv* ev,
    const mdbOid* outOid,  // out id of row or table (or unbound if no child)
    mdb_bool* outIsRow) = 0; // nonzero if child is a row (rather than a table)

  virtual mdb_err GetAnyChild( // access table of specific attribute
    mdbEnv* ev, // context
    mdbRow** acqRow, // child row (or null)
    mdbTable** acqTable) = 0; // child table (or null)


  virtual mdb_err SetChildRow( // access table of specific attribute
    mdbEnv* ev, // context
    mdbRow* ioRow) = 0; // inRow must be bound inside this same db port

  virtual mdb_err GetChildRow( // access row of specific attribute
    mdbEnv* ev, // context
    mdbRow** acqRow) = 0; // acquire child row (or nil if no child)


  virtual mdb_err SetChildTable( // access table of specific attribute
    mdbEnv* ev, // context
    mdbTable* inTable) = 0; // table must be bound inside this same db port

  virtual mdb_err GetChildTable( // access table of specific attribute
    mdbEnv* ev, // context
    mdbTable** acqTable) = 0; // acquire child table (or nil if no child)
  // } ----- end children methods -----

// } ===== end mdbCell methods =====
};

// } %%%%% end C++ abstract class interfaces %%%%%

#endif /* _MDB_ */

