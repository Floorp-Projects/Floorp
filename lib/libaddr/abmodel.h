/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* file: abmodel.h 
** Many portions derive from public domain IronDoc code and interfaces
** (so of course those portions remain public domain).
**
** This is the private address book interface defining abstract class interfaces
** and other auxiliary classes that comprise the model used by address books
** under the covers.  The intention of this private interface is to allow
** third parties to provide alternative implementations of address books by
** conforming to a database independent interface.  Additionally, this interface
** permits Netscape to use multiple address book dbs with only one interface.
**
** The public address book interface is specified in abtable.h, which defines
** the interface that clients must know and use in order to access address
** book content.  The public interface in abtable.h relies on the private
** interface of abmodel.h to actually create objects or access them by calling
** "factory" methods, and this allows dynamic choice of polymorphic subclasses.
**
** Changes:
** <2> 05Jan1998 first implementation
** <1> 23Dec1997 first interface draft
** <0> 20Nov1997 first draft
*/

#ifndef _ABMODEL_
#define _ABMODEL_ 1

#ifndef _ABTABLE_
#include "abtable.h"
#endif

#ifndef _ABDEQUE_
#include "abdeque.h"
#endif

/* ----- ----- ----- classes defined in this interface ----- ----- ----- */


#ifndef AB_Table_typedef
typedef struct AB_Table AB_Table;
#define AB_Table_typedef 1
#endif

#ifndef AB_Row_typedef
typedef struct AB_Row AB_Row;
#define AB_Row_typedef 1
#endif


#ifndef XP_MAC
/* ===== ===== ===== ===== misc temp kludge defines ===== ===== ===== ===== */

// artifacts of supporting an experimental file format with the old format:
//#define AB_kGromitDbFileName "da5id.0a0"
//#define AB_CONFIG_USE_GROMIT_FILE_FORMAT 1

/* ===== ===== ===== ===== forwards ===== ===== ===== ===== */
#endif /* XP_MAC */

class ab_Change;
class ab_ColumnSet;
class ab_Debugger;
class ab_Defaults;
class ab_Env;
class ab_File;
class ab_FilePrinter;
class ab_FileTracer;
class ab_Model;
class ab_NameSet;
class ab_Object;
class ab_ObjectLink;
class ab_ObjectSet;
class ab_Part;
class ab_Printer;
class ab_Row;
class ab_RowContent;
class ab_RowSet;
class ab_Search;
class ab_Session;
class ab_StaleRowView;
class ab_Store;
class ab_Stream;
class ab_String;
class ab_Table;
class ab_Thumb;
class ab_Tracer;
class ab_View;

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/


/* ===== ===== ===== ===== ab_Usage ===== ===== ===== ===== */

// usage:
#define ab_Usage_kHeap    /*i*/ 0x68656170 /* ascii 'heap' (no endian) */
#define ab_Usage_kStack   /*i*/ 0x7374636B /* ascii 'stck' (no endian) */
#define ab_Usage_kMember  /*i*/ 0x6D656D62 /* ascii 'memb' (no endian) */
#define ab_Usage_kGlobal  /*i*/ 0x676C6F62 /* ascii 'glob' (no endian) */
#define ab_Usage_kGhost   /*i*/ 0x67687374 /* ascii 'ghst' (no endian) */

class ab_Usage {
public:
    ab_u4  mUsage_Code;    /* kHeap, kStack, kMember, or kGhost */
    
public:
    ab_Usage(ab_u4 code);

    ab_Usage(); // does nothing except maybe call EnsureReadyStaticUsage()
    void InitUsage(ab_u4 code) { mUsage_Code = code; }

    ~ab_Usage() { }
    
    ab_u4  Code() const { return mUsage_Code; }
    
    static void EnsureReadyStaticUsage();
    
public:
    static const ab_Usage& kHeap;
    static const ab_Usage& kStack;
    static const ab_Usage& kMember;
    static const ab_Usage& kGlobal;
    static const ab_Usage& kGhost;
    
    static const ab_Usage& GetHeap();   // kHeap safe at static init time
    static const ab_Usage& GetStack();  // kHeap safe at static init time
    static const ab_Usage& GetMember(); // kHeap safe at static init time
    static const ab_Usage& GetGlobal(); // kHeap safe at static init time
    static const ab_Usage& GetGhost();  // kHeap safe at static init time
    
private: // only ab_Usage gets to use the empty constructor
};



/* ===== ===== ===== ===== actions ===== ===== ===== ===== */

typedef ab_bool (* ab_Object_mAction)
(ab_Object* self, ab_Env* ev, void* closure);

/* ===== ===== ===== ===== ab_Object ===== ===== ===== ===== */

// access:
#define ab_Object_kOpen    /*i*/ 0x6F70656E /* ascii 'open' (no endian) */
#define ab_Object_kClosing /*i*/ 0x636C6F73 /* ascii 'clos' (no endian) */
#define ab_Object_kShut    /*i*/ 0x73687574 /* ascii 'shut' (no endian) */
#define ab_Object_kDead    /*i*/ 0x64656164 /* ascii 'dead' (no endian) */

#define ab_Object_kXmlBufSize /*i*/ (256+64) /* size for ObjectAsString() */

class ab_Object /*d*/ {                   
protected:
    ab_ref_count  mObject_RefCount; /* reference count */
    ab_u4         mObject_Access;   /* kOpen, kClosing, kShut, or kDead */
    ab_u4         mObject_Usage;    /* kHeap, kStack, kMember, or kGhost */
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev);
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Object(); // mObject_Access gets ab_Object_kDead
    
// ````` ````` ````` `````   ````` ````` ````` `````  
private: // private non-poly ab_Object memory management methods

	// It's hard to make the first ab_Env when one is needed for new() ...
	friend class ab_Env; // ... so we let ab_Env allocate without an instance

	// void* operator new(size_t inSize, const ab_Usage& inUsage);
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly ab_Object memory management methods

	void* operator new(size_t inSize, ab_Env& ev);
		// the env instance passed can actually be a reference to a null
		// pointer, which needs to happen for the first env created.  (This
		// approach was used to help remove the other operator::new(), because
		// having both operators was giving ambiguity problems on one platform.)

// ````` ````` ````` `````   ````` ````` ````` ````` 
// operator delete should be private but XP_WIN has problems compiling this, so
// we only make it private on the Mac, and this catches illegal object deletes.
#if defined(XP_MAC)
private: // private non-poly ab_Object memory management methods
#endif

	void* operator new(size_t inSize);
	// DO NOT USE THIS OPERATOR.  This operator is intended to quiet compilers
	// which autogenerate calls to operator new() inside object constructors
	// (which was done in old compilers which checked for this==null).  Note
	// that this operator is private on the Mac, so you *will* break the build
	// if you explicitly use this operator.

	// *only* ab_Object::ReleaseObject() is allowed to delete objects
	void operator delete(void* ioAddress)
	{ ::operator delete(ioAddress); }
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Object methods
    ab_Object(const ab_Usage& inUsage);
    // mObject_Access(ab_Object_kOpen), mObject_Usage(inUsage.Code()),
    // mObject_RefCount(1)

	void CastAwayConstCloseObject(ab_Env* ev) const;
    
    void MarkShut() { mObject_Access = ab_Object_kShut; }
    void MarkClosing() { mObject_Access = ab_Object_kClosing; }
    void MarkDead() { mObject_Access = ab_Object_kDead; }

    void TraceObject(ab_Env* ev) const; /* uses ObjectAsString() */
    void BreakObject(ab_Env* ev) const; /* uses ObjectAsString() */
    
    ab_ref_count  ObjectRefCount() const { return mObject_RefCount; }
    ab_ref_count  AcquireObject(ab_Env* ev);
    ab_ref_count  ReleaseObject(ab_Env* ev);
    
    const char* GetObjectAccessAsString() const; // e.g. "open", "shut", etc.
    const char* GetObjectUsageAsString() const; // e.g. "heap", "stack", etc.
    
    ab_u4   ObjectUsage() const { return mObject_Usage; }
    ab_bool IsHeapObject() const { return mObject_Usage == ab_Usage_kHeap; }
    ab_bool IsStackObject() const { return mObject_Usage == ab_Usage_kStack; }
    ab_bool IsGhostObject() const { return mObject_Usage == ab_Usage_kGhost; }
    ab_bool IsGlobalObject() const { return mObject_Usage == ab_Usage_kGlobal; }
    ab_bool IsMemberObject() const { return mObject_Usage == ab_Usage_kMember; }

    ab_bool IsOpenObject() const { return mObject_Access == ab_Object_kOpen; }
    ab_bool IsShutObject() const { return mObject_Access == ab_Object_kShut; }
    ab_bool IsDeadObject() const { return mObject_Access == ab_Object_kDead; }
    ab_bool IsClosingObject() const
    { return mObject_Access == ab_Object_kClosing; }

    ab_bool IsOpenOrClosingObject() const
    { return IsOpenObject() || IsClosingObject(); }

    ab_bool IsObject() const
    { return ( IsOpenObject() || IsShutObject() || IsClosingObject() ); }

    void ReportObjectNotOpen(ab_Env* ev);

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // panic error handling

    void ObjectNotReleasedPanic(const char* inMessage) const;
    void ObjectPanic(const char* inWhere) const;
    
    static void PushPanicEnv(ab_Env* ev, ab_Env* inPanicEnv);
    static ab_Env* PopPanicEnv(ab_Env* ev);
    static ab_Env* TopPanicEnv();
    static int TopPanicEnvDepth();
    
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // copying is sometimes not allowed
    ab_Object(const ab_Object& other, const ab_Usage& inUsage);
    ab_Object& operator=(const ab_Object& other);

private: // copying without usage is not allowed
    ab_Object(const ab_Object& other);
};

enum ab_Object_eError {
    ab_Object_kFaultNotOpen = /*i*/ AB_Fault_kObject, /* !IsOpenObject() */
    
    ab_Object_kFaultNotOpenOrClosing,   /*i*/ /* nn !IsOpenOrClosingObject() */
    ab_Object_kFaultNotObject,          /*i*/ /* nn !IsObject() */
    ab_Object_kFaultNotHeapObject,      /*i*/ /* nn !IsHeapObject() */
    ab_Object_kFaultUnderflowRefCount,
    ab_Object_kFaultNullObjectParameter
};

/* ===== ===== ===== ===== ab_Env ===== ===== ===== ===== */

#define ab_Env_kHeapSafeTag /*i*/ 0x73416645 /* ascii 'sAfE' (no endian) */

#define ab_Env_kDefaultMaxRefDelta /*i*/ (32 * 1024)

#define ab_Env_kFaultMaxStackTop /*i*/ 16
#define ab_Env_kFaultStackSlots /*i*/ (16 + 1)

class ab_Env : public ab_Object { 
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public member variables
	AB_Env         mEnv_Self;

	// tracing/printing flags (four desired for four byte alignment) */
	ab_bool        mEnv_BeShallow;   /* write shallow depth information */
	ab_bool        mEnv_BeComplete;  /* write more inclusive information */
	ab_bool        mEnv_BeConcise;   /* write smaller information */
	ab_bool        mEnv_BeVerbose;   /* write more detailed information */
    
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected member variables

	ab_Debugger*   mEnv_Debugger;      /* optional debugging interface */
	ab_Tracer*     mEnv_Tracer;        /* optional tracing interface */ 

	const void*    mEnv_StackRef;      /* pointer to some local var if nonzero */
	ab_u4          mEnv_MaxRefDelta;   /* allowed distance from StackRef */

	ab_num         mEnv_StackTop; /* top of the fault stack */

	AB_Fault       mEnv_FaultStack[ ab_Env_kFaultStackSlots ];

	// standard convenience objects
	ab_u1          mEnv_EmptyString[ 1 ]; /* must be zero: empty string */ 
	ab_u1          mEnv_Pad[ 3 ]; /* u4 alignment pad until used later */

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev);
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Env();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Env methods
    virtual ab_error_count  CutAllFaults();
    virtual ab_error_count  GetAllFaults(AB_Fault* outVector,
                ab_error_count inSize, ab_error_count* outLength) const;
    
    virtual ab_error_uid    NewFault(ab_error_uid code, ab_u4 space);
    virtual void            BreakString(const char* inMessage);
    virtual void            TraceString(const char* inMessage);

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected non-poly ab_Env methods

	ab_bool   is_stack_depth_okay() const;
	void      error_notify(const AB_Fault* inFault);
	void      push_error(ab_error_uid inCode, ab_u4 inSpace);

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly ab_Env methods

 	// ````` construction `````  
    ab_Env(const ab_Usage& inUsage);
    ab_Env(const ab_Usage& inUsage, const ab_Env& other);
    ab_Env(const ab_Usage& inUsage, ab_Debugger* ioDebugger,
    	ab_Tracer* ioTracer);
	    
 	// ````` standard logging env `````  
 	
	static ab_Env* GetSimpleEnv();
		// This env just has a debugger instance installed.  It is used
		// when instantiating the standard session log file objects.  This
		// env is installed as the top panic env when first instantiated.
		// (Please, never close this env instance.)
		
	static ab_Env* GetLogFileEnv();
		// standard session log env (uses ab_StdioFile::GetLogStdioFile()).
		// This uses standard session debugger, tracer, printer, etc.
		// (Please, never close this env instance.)

 	// ````` destruction `````  
    void CloseEnv(ab_Env* ev); // CloseObject();

 	// ````` type conversion `````  
    AB_Env* AsSelf() { return &mEnv_Self; } // C version of this
	
	static ab_Env* AsThis(AB_Env* cenv)
	{ return ((ab_Env*) (((ab_u1*) cenv) - ((unsigned) &((ab_Env*) 0)->mEnv_Self))); }
   
	// ````` memory management `````  
    void* HeapAlloc(ab_num inSize); // XP_ALLOC with auto error report
    void HeapFree(void* ioAddress); // XP_FREE
    
    char* CopyString(const char* inStringToCopy); // XP_STRLEN, XP_ALLOC, XP_STRCPY etc.
    void FreeString(char* ioStringToFree); // XP_FREE
   
	// ````` tagged memory management `````  
    void* HeapSafeTagAlloc(ab_num inSize); // append a kHeapSafeTag on the front
    void HeapSafeTagFree(void* ioAddress); // check for kHeapSafeTag on the front
    
    static ab_bool GoodHeapSafeTag(void* ioAddress);
    
 	// ````` fault creation covers `````  
    void NewAbookFault(ab_error_uid faultCode);

 	// ````` debugging covers `````  
	void Break(const char* format, ...);

 	// ````` tracing covers `````  
#ifdef AB_CONFIG_TRACE
	void Trace(const char* format, ...);
#endif /*AB_CONFIG_TRACE*/

	void TraceBeginMethod(const char* cls, const char* method) const;
	void TraceEndMethod() const;
 
 	// ````` fault access `````  
	ab_error_count  ErrorCount() const
	{ return mEnv_Self.sEnv_FaultCount; }
	
	ab_error_uid    GetError() const
	{ return mEnv_FaultStack[ mEnv_StackTop ].sFault_Code; }
	
	ab_error_count GetAllErrors(ab_error_uid* outVector,
	      ab_error_count inSize, ab_error_count* outLength);

 	// ````` error status `````  
	ab_bool Good() const { return (mEnv_Self.sEnv_FaultCount == 0); }
	ab_bool Bad() const { return (mEnv_Self.sEnv_FaultCount != 0); }

 	// ````` trace status `````  
	ab_bool DoTrace() const { return mEnv_Self.sEnv_DoTrace; }
	
	ab_bool DoErrBreak() const { return mEnv_Self.sEnv_DoErrBreak; }
	ab_bool DoErrorBreak() const { return mEnv_Self.sEnv_DoErrBreak; }
	 
	ab_bool BeParanoid() const { return mEnv_Self.sEnv_BeParanoid; }
	 
 	// ````` member access `````  
	ab_Debugger*   Debugger() const { return mEnv_Debugger; }
	void           ChangeDebugger(ab_Debugger* ioDebugger);

	ab_Tracer*     Tracer() const { return mEnv_Tracer; }
	void           ChangeTracer(ab_Tracer* ioTracer);

	const void*    StackRef() const { return mEnv_StackRef; }
	ab_u4          MaxRefDelta() const { return mEnv_MaxRefDelta; }

	void           SetStackControl(const void* inRef, ab_u4 inDelta)
	{ mEnv_StackRef = inRef; mEnv_MaxRefDelta = inDelta; }

// ````` ````` ````` `````   ````` ````` ````` `````  
private: // copying is not allowed
    ab_Env(const ab_Env& other);
    ab_Env& operator=(const ab_Env& other);
};

// default ab_IntMap methods using integer pseudo random number generator:
ab_u4 ab_Env_HashKey(ab_Env* ev, ab_map_int inKey);
ab_bool ab_Env_EqualKey(ab_Env* ev, ab_map_int inTest, ab_map_int inMember);
char* ab_Env_AssocString(ab_Env* ev, ab_map_int inKey, ab_map_int inValue,
	char* outBuf, ab_num inHashBucket, ab_num inActualBucket);

#define ab_Env_kSelfOffset ((unsigned) &((ab_Env*) 0)->mEnv_Self)
	// offset of mEnv_Self slot inside ab_Env

#define AB_Env_AsThis(cenv) ((ab_Env*) (((ab_u1*) cenv) - ab_Env_kSelfOffset))
	// Cast AB_Env* to ab_Env*, but using offset of mEnv_Self.
	// This macro is the inverse of ab_Env::AsSelf().


#if defined(AB_CONFIG_TRACE_CALL_TREE) && AB_CONFIG_TRACE_CALL_TREE

#define ab_Env_BeginMethod(ev,c,m) /*i*/ \
  { if ( (ev)->DoTrace() ) (ev)->TraceBeginMethod((c), (m)); } {
  
#define ab_Env_EndMethod(ev) /*i*/ \
  } { if ( (ev)->DoTrace() ) (ev)->TraceEndMethod(); }
  
#else /* end AB_CONFIG_TRACE_CALL_TREE*/

#define ab_Env_BeginMethod(ev,c,m) 
#define ab_Env_EndMethod(ev)
#endif /* end AB_CONFIG_TRACE_CALL_TREE*/

/* ===== ===== ===== ===== ab_Row ===== ===== ===== ===== */

enum ab_Row_eHint {
	ab_Row_kMinCellHint = 3,           /* min for starting cells */
	ab_Row_kDefaultCellHint = 16,      /* recommended default */
	ab_Row_kMaxCellHint = (4 * 1024),  /* max for starting cells */
	ab_Row_kMaxCellSize = (32 * 1024)  /* max long term size for cells */
};

class ab_Row /*d*/ : public ab_Object {

protected:
	ab_Table*     mRow_Table;
	ab_num        mRow_CellSeed;  /* count times cell structure changes */
	ab_cell_count mRow_Capacity;  /* number of allocated cells */
	ab_cell_count mRow_Count;     /* number of used cells */
	AB_Cell*      mRow_Cells;     /* vector of cells */

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseRow()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Row();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Row methods

	const char* get_col_as_ldif_type(ab_Env* ev, ab_column_uid inColUid) const;
	
	void write_cell_as_ldif_attrib(ab_Env* ev, const AB_Cell* inCell,
		ab_Stream* ioStream) const;
	void write_person_class(ab_Env* ev, ab_Stream* ioStream) const;
	void write_list_class(ab_Env* ev, ab_Stream* ioStream) const;
	void write_list_members(ab_Env* ev, ab_row_uid inRowUid,
		ab_Stream* ioStream) const;
		
	void write_leading_dn_to_ldif_stream(ab_Env* ev, ab_Stream* ioStream) const;
	void write_member_dn_to_ldif_stream(ab_Env* ev, ab_Stream* ioStream) const;
	void add_cells_for_ldif_dn_attrib(ab_Env* ev);
	
	ab_RowContent* get_row_content(ab_Env* ev) const;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Row methods
    ab_Row(ab_Env* ev, const ab_Usage& inUsage,
	    ab_Table* ioTable, ab_cell_count inHint);

    void CloseRow(ab_Env* ev); // CloseObject()
 
    const AB_Row* AsConstSelf() const { return (const AB_Row*) this; }
    AB_Row* AsSelf() { return (AB_Row*) this; }

    static ab_Row* AsThis(AB_Row* self) { return (ab_Row*) self; }
    static const ab_Row* AsConstThis(const AB_Row* self)
    { return (const ab_Row*) self; }
    
    ab_bool GetDistinguishedName(ab_Env* ev, char* outBuf256) const;
    	// Write the distinguished name attribute to outBuf256, which must be
    	// at least 256 bytes in size.  The DN is currently composed of
    	// FullName (aka FullName,CommonName, cn, or DisplayName) using the
    	// AB_Column_kFullName column, and the email address using the
    	// AB_Column_kEmail column.  If either of these cells is not in the
    	// row, then an empty string is used instead.  The DN is formated as
    	// follows "cn=<full name>,mail=<email>". True when ev->Good() is true.
    	// for example "cn=John Smith,mail=johns@netscape.com"
    	
    ab_bool WriteToLdifStream(ab_Env* ev, ab_row_uid inRowUid,
	    ab_Stream* ioStream) const;
    
    char* AsVCardString(ab_Env* ev) const;
    	// returned string allocated by ab_Env::CopyString() which advertises
    	// itself as using XP_ALLOC for space allocation.  A null pointer is
    	// typically returned in case of error.  (It would not be that safe
    	// to return an empty static string which could not be deallocated.)
   
	ab_row_uid FindRowUid(ab_Env* ev) const;
		// is this row a duplicate of a row already inside the row's table?
		// If so, then return the row uid for the row already in the table.
		// (according to some (unspecified here) method of identifying rows
		// uniquely -- probably fullname plus email).
	
	ab_cell_count CountCells() const { return mRow_Count; }
    ab_Table* GetRowTable() const { return mRow_Table; }
    
	ab_bool ChangeTable(ab_Env* ev, ab_Table* ioTable);

	ab_Row* DuplicateRow(ab_Env* ev) const;
	ab_bool CopyRow(ab_Env* ev, const ab_Row* other);
	
	ab_bool ClearCells(ab_Env* ev);
	ab_bool WriteCell(ab_Env* ev, const char* inCell, ab_column_uid inCol);
	ab_bool PutCell(ab_Env* ev, const AB_Cell* inCell);
	
	ab_bool PutHexLongCol(ab_Env* ev, long inLong,
		ab_column_uid inCol, ab_bool inDoAdd);
	
	ab_bool PutShortCol(ab_Env* ev, short inShort,
		ab_column_uid inCol, ab_bool inDoAdd);
	
	ab_bool PutBoolCol(ab_Env* ev, short inBool,
		ab_column_uid inCol, ab_bool inDoAdd);

	ab_cell_count GetCells(ab_Env* ev, AB_Cell* outVector,
		ab_cell_count inSize, ab_cell_count* outLength) const;
	AB_Cell* GetCellAt(ab_Env* ev, ab_cell_pos inPos) const;
	AB_Cell* GetColumnCell(ab_Env* ev, ab_column_uid inCol) const;
	
	const char* GetCellContent(ab_Env* ev, ab_column_uid inCol) const;
		// returns null when no such cell exists
		
	const char* GetColumnValue(ab_Env* ev, ab_column_uid inCol) const;
		// returns empty static string when no such cell exists
	
	ab_bool GetCellNames(ab_Env* ev, const char** inGivenName,
		const char** inFamilyName, const char** inFullName) const;
		// For each pointer that is non-null, perform the equivalent
		// of calling GetCellContent() for appropriate columns (which are
		// AB_Column_kGivenName AB_Column_kFamilyName AB_Column_kFullName)
		// and return the cell content pointer in the location pointed at
		// by each argument.  The boolean return true if ev->Good()
		// and if at least one of the names is found and returned.
	
	ab_bool AddColumns(ab_Env* ev, const AB_Column* inVector);
	ab_bool AddCells(ab_Env* ev, const AB_Cell* inVector);
	AB_Cell* AddCell(ab_Env* ev, ab_column_uid inCol, ab_num inSize);

	ab_bool CutCell(ab_Env* ev, ab_column_uid inCol);
    
    ab_bool IsListRow(ab_Env* ev) const
    { const char* value = GetCellContent(ev, AB_Column_kIsPerson);
      return ( value && *value != 't' );
    }
    
    ab_bool IsPersonRow(ab_Env* ev) const
    { const char* value = GetCellContent(ev, AB_Column_kIsPerson);
      return ( !value || *value == 't' );
    }
    
    // ````` list and person predicates requiring EXPLICIT value `````

    ab_bool HasExplicitListOrPersonValue(ab_Env* ev) const
    { const char* value = GetCellContent(ev, AB_Column_kIsPerson);
      return ( value && *value && ev->Good() );
    }

    ab_bool IsExplicitListRow(ab_Env* ev) const
    { const char* value = GetCellContent(ev, AB_Column_kIsPerson);
      return ( value && *value == 'f' );
    }
    
    ab_bool IsExplicitPersonRow(ab_Env* ev) const
    { const char* value = GetCellContent(ev, AB_Column_kIsPerson);
      return ( value || *value == 't' );
    }

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // non-poly ab_Row protected helper methods
	ab_bool  grow_cells(ab_Env* ev, ab_num newCapacity);
	ab_bool  zap_cells(ab_Env* ev);
	ab_bool  sync_cell_columns(ab_Env* ev, ab_Table* ioTable);
	AB_Cell* find_cell(ab_Env* ev, ab_column_uid inColUid) const;
	AB_Cell* new_cell(ab_Env* ev);
	
private: // copying is not allowed
	ab_Row(const ab_Row& other);
	ab_Row& operator=(const ab_Row& other);
};



/*=============================================================================
 * ab_Change: a kind of change
 */

typedef ab_u4 ab_change_uid;  /* unique id for a change */
typedef ab_u4 ab_change_mask; /* bit mask for change codes */

class ab_Change /*d*/ : public ab_Object {
public:
    ab_change_uid     mChange_Uid;       /* id for this session change */
    ab_change_mask    mChange_Mask;      /* mask for change codes */

    ab_model_uid      mChange_ModelUid;  /* id of target row's container */
    ab_store_uid      mChange_StoreUid;  /* target row's model */

    ab_row_uid        mChange_Row;       /* id of target row */
    ab_row_pos        mChange_RowPos;    /* pos of changed target row */
    ab_row_count      mChange_RowCount;  /* number of rows involved */
	ab_row_uid        mChange_RowParent; /* id of row's parent */

    ab_column_uid     mChange_Column;    /* column that changed */
    ab_column_pos     mChange_ColPos;    /* pos of the changed column */
    ab_row_count      mChange_ColCount;  /* number of columns involved */
    

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseChange()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Change();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly methods
    ab_Change(const ab_Usage& inUsage,
              const ab_Model* inModel, ab_change_mask inMask);
              
    void CloseChange(ab_Env* ev); // CloseObject()

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // copying *is* allowed
    ab_Change(const ab_Change& other, const ab_Usage& inUsage);
    ab_Change& operator=(const ab_Change& other);

protected:
	void SetChange(const ab_Change& other); // common copy code
	
private: // ... but not copying without a usage specified
    ab_Change(const ab_Change& other);
};

/*| Init: initialize this change instance to send a change notification for
**| the table ioTable.  True is returned if a notification is needed, which
**| is true when ioTable has any members in it's View list.  Otherwise
**| the change instance is not altered at all.
**|
**|| When a change notification is needed, this change instance is zeroed
**| so all slots contain zero, and then sChange_Table and sChange_Container
**| are set to values appropriate ioTable and it's container.  A new temp uid
**| for the session change uid is coined by ab_Session, and this value is
**| returned in sChange_Uid.
|*/
/*ab_Change::Init(ab_Env* ev, ab_Table* ioTable);*/

typedef enum { /* bit flags for sChange_Mask */
    ab_Change_kNewRow = (1L << 0), /*i*/ /* Target was just created */
    /* AB_Row_NewTableRowAt(), Db, Table, Row, RowCount=1 */
    
    ab_Change_kNewColumn =  (1L << 1), /*i*/ /* column added to db */
    /* AB_Table_NewColumnId(), Db, Col */
    
    ab_Change_kSort = (1L << 2), /*i*/ /* table sort col changed */
    /* AB_Table_SortByUid()/AB_Table_SortByName(), Db, Table, Column */
    
    ab_Change_kLayout =  (1L << 3), /*i*/ /* table column layout modified */
    /* AB_Table_ChangeColumnLayout(), Db, Table, ColCount=n */
    /* AB_Table_AddColumnAt(), Db, Table, Column, ColCount=1 */
    /* AB_Table_CutColumn(), Db, Table, Column, ColCount=1 */
    /* AB_Table_PutColumnAt(), Db, Table, Column, ColCount=1 */
    
    ab_Change_kAddColumn =  (1L << 4), /*i*/ /* layout column added */
    /* AB_Table_AddColumnAt(), Db, Table, Column, ColPos, ColCount=1 */
    
    ab_Change_kCutColumn =  (1L << 5), /*i*/ /* layout column removed */
    /* AB_Table_CutColumn(), Db, Table, Column, ColPos, ColCount=1 */
    
    ab_Change_kPutColumn =  (1L << 6), /*i*/ /* layout column modified */
    /* AB_Table_PutColumnAt(), Db, Table, Column, ColPos, ColCount=1 */
    
    ab_Change_kAddRow =  (1L << 7), /*i*/ /* row added to table */
    /* AB_Table_AddRow(), Db, Table, Row, RowPos, RowCount=1 */
    
    ab_Change_kCutRow =  (1L << 8), /*i*/ /* row cut from table */
    /* AB_Table_CutRow(), Db, Table, Row, RowPos, RowCount=1 */
    
    ab_Change_kPutRow =  (1L << 9), /*i*/ /* row was modified */
    /* AB_Row_UpdateTableRow(), Db, Table, Row, RowCount=1, ColCount=N */
    /* AB_Row_ResetTableRow(), Db, Table, Row, RowCount=1, ColCount=N */
    
    ab_Change_kMoveRow =  (1L << 10), /*i*/ /* unsorted row pos moved */
    /* AB_Table_ChangeRowPos(), Db, Table, Row, RowCount=1 */
    
    ab_Change_kAddRowSet =  (1L << 11), /*i*/ /* set of rows added */
    /* AB_Table_AddAllRows(), Db, Table, RowCount=N */
    
    ab_Change_kCutRowSet =  (1L << 12), /*i*/ /* set of rows removed */
    /* AB_Table_CutRowRange(), Db, Table, RowPos, RowCount=N */
    
    ab_Change_kKillRow =  (1L << 13), /*i*/ /* row destroyed */
    /* AB_Table_DestroyRow(), Db, Table, Row, RowCount=1 */
    
    ab_Change_kNewToken =  (1L << 14), /*i*/ /* another column tokenized */
    /* AB_Table_NewColumnId(), Store, Col, ColCount=1 */
    
    ab_Change_kScramble =  (1L << 15), /*i*/ /* big content change */
    
    ab_Change_kClosing =   (1L << 16) /*i*/ /* model is closing */

} ab_Change_eBits;

#define ab_Change_kAllRowChanges \
    ( ab_Change_kNewRow | ab_Change_kSort | ab_Change_kAddRow |  \
      ab_Change_kCutRow | ab_Change_kPutRow | ab_Change_kMoveRow | \
      ab_Change_kAddRowSet | ab_Change_kCutRowSet | ab_Change_kKillRow | \
      ab_Change_kScramble )

#define ab_Change_kAllColumnChanges \
    ( ab_Change_kNewColumn | ab_Change_kLayout | ab_Change_kAddColumn |  \
      ab_Change_kCutColumn | ab_Change_kPutColumn | ab_Change_kNewToken | \
      ab_Change_kScramble )


/* ===== ===== ===== ===== ab_Debugger ===== ===== ===== ===== */

class ab_Debugger /*d*/ : public ab_Object { /* from IronDoc's FeDebugger */

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Debugger methods
    virtual void BreakString(ab_Env* ev, const char* inMessage);

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseFileTracer()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Debugger();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Debugger methods
    ab_Debugger(const ab_Usage& inUsage);

    void Break(ab_Env* ev, const char* inFormat, ...);
    
    void CloseDebugger(ab_Env* ev);

	static ab_Debugger* GetLogFileDebugger(ab_Env* ev);
		// standard session debugger (uses ab_StdioFile::GetLogStdioFile()).
		// This debugger instance is used by ab_Env::GetLogFileEnv().
		// (Please, never close this debugger instance.)

private: // copying is not allowed
    ab_Debugger(const ab_Debugger& other);
    ab_Debugger& operator=(const ab_Debugger& other);
};

/* ===== ===== ===== ===== line characters ===== ===== ===== ===== */

#define ab_kCR '\015'
#define ab_kLF '\012'
#define ab_kVTAB '\013'
#define ab_kFF '\014'
#define ab_kTAB '\011'
#define ab_kCRLF "\015\012"     /* A CR LF equivalent string */

#ifdef XP_MAC
#  define ab_kNewline             "\015"
#  define ab_kNewlineSize 1
#else
#  if defined(XP_WIN) || defined(XP_OS2)
#    define ab_kNewline           "\015\012"
#    define ab_kNewlineSize       2
#  else
#    ifdef XP_UNIX
#      define ab_kNewline         "\012"
#      define ab_kNewlineSize     1
#    endif /* XP_UNIX */
#  endif /* XP_WIN */
#endif /* XP_MAC */

/* ===== ===== ===== ===== ab_Tracer ===== ===== ===== ===== */

class ab_Tracer /*d*/ : public ab_Object { /* from IronDoc's FeTracer class */


// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected ab_FileTracer members

	ab_u4    mTracer_MethodDepth; // count begins without matching ends

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Tracer methods
    virtual void TraceString(ab_Env* ev, const char* inMessage) = 0;
    virtual void BeginMethod(ab_Env* ev, const char* c, const char* m) = 0;
    virtual void EndMethod(ab_Env* ev) = 0;
    virtual void Flush(ab_Env* ev) = 0;
    virtual ab_Printer* GetPrinter(ab_Env* ev) = 0;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    //virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    //virtual void CloseObject(ab_Env* ev); // CloseFileTracer()
    //virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Tracer();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Tracer methods
    void Trace(ab_Env* ev, const char* inFormat, ...);
    ab_Tracer(const ab_Usage& inUsage);
    
    void CloseTracer(ab_Env* ev) { AB_USED_PARAMS_1(ev); }

	ab_u4    GetTracerMethodDepth() const { return mTracer_MethodDepth; }

private: // copying is not allowed
    ab_Tracer(const ab_Tracer& other);
    ab_Tracer& operator=(const ab_Tracer& other);
};

enum ab_Tracer_eError {
    ab_Tracer_kFaultNullFile = /*i*/ AB_Fault_kTracer
    
};

/* ===== ===== ===== ===== ab_FileTracer ===== ===== ===== ===== */

class ab_FileTracer /*d*/ : public ab_Tracer { /* from the IronDoc class */

// ````` ````` ````` `````   ````` ````` ````` `````  
protected:
	ab_File*    mFileTracer_File;    // presumably uses same file
	ab_Printer* mFileTracer_Printer; // presumably uses same file as tracer

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Tracer methods
    virtual void TraceString(ab_Env* ev, const char* inMessage);
    virtual void BeginMethod(ab_Env* ev, const char* c, const char* m);
    virtual void EndMethod(ab_Env* ev);
    virtual void Flush(ab_Env* ev);
    virtual ab_Printer* GetPrinter(ab_Env* ev);
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseFileTracer()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_FileTracer();

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected non-poly ab_FileTracer methods

	void trace_indent(ab_Env* ev);
	void trace_string(ab_Env* ev, const char* inMessage, ab_bool inXmlWrap);

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly ab_FileTracer methods
    ab_FileTracer(ab_Env* ev, const ab_Usage& inUsage,
	    ab_File* ioFile, ab_Printer* ioPrinter);

	void CloseFileTracer(ab_Env* ev);
	    
	static ab_FileTracer* GetLogFileTracer(ab_Env* ev);
		// standard session file tracer (uses ab_StdioFile::GetLogStdioFile()).
		// This tracer instance is used by ab_Env::GetLogFileEnv().
		// (Please, never close this tracer instance.)
	
	ab_File*    GetFileTracerFile() const { return mFileTracer_File; }
	ab_Printer* GetFileTracerPrinter() const { return mFileTracer_Printer; }
	
private: // copying is not allowed
    ab_FileTracer(const ab_FileTracer& other);
    ab_FileTracer& operator=(const ab_FileTracer& other);
};

/* ===== ===== ===== ===== ab_Printer ===== ===== ===== ===== */

class ab_Printer /*d*/ : public ab_Object { /* from IronDoc's FeSink class */

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // ab_FilePrinter members

	ab_num    mPrinter_Depth;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Printer methods
    virtual void PutString(ab_Env* ev, const char* inMessage) = 0;
    virtual void Newline(ab_Env* ev, ab_num inNewlines) = 0;
    virtual void NewlineIndent(ab_Env* ev, ab_num inNewlines) = 0;
    virtual void Flush(ab_Env* ev) = 0;
    virtual void Hex(ab_Env* ev, const void* b, ab_num size, ab_pos p) = 0;
    virtual ab_num PushDepth(ab_Env* ev) = 0;
    virtual ab_num PopDepth(ab_Env* ev) = 0;
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    //virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    //virtual void CloseObject(ab_Env* ev); // ClosePrinter()
    //virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Printer();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Printer methods
    void Print(ab_Env* ev, const char* inFormat, ...);
    ab_Printer(const ab_Usage& inUsage);
    
    void ClosePrinter(ab_Env* ev) { AB_USED_PARAMS_1(ev); }

	ab_num    GetPrinterDepth() const { return mPrinter_Depth; }
	
	void DefaultPrinterHex(ab_Env* ev, const void* inBuf,
		ab_num inSize, ab_pos inPos);
		// DefaultPrinterHex() is a good default implementation for Hex()
		
private: // copying is not allowed
    ab_Printer(const ab_Printer& other);
    ab_Printer& operator=(const ab_Printer& other);
};

enum ab_Printer_eError {
    ab_Printer_kFaultNullFile = /*i*/ AB_Fault_kPrinter,
    ab_Printer_kFaultFileNotOpen,
    ab_Printer_kFaultHexSizeTooBig,
    ab_Printer_kFaultConfused
};

/* ===== ===== ===== ===== ab_FilePrinter ===== ===== ===== ===== */

class ab_FilePrinter /*d*/ : public ab_Printer { /* from the IronDoc class */

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // ab_FilePrinter members

	ab_File*    mFilePrinter_File;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Printer methods
    virtual void PutString(ab_Env* ev, const char* inMessage);
    virtual void Newline(ab_Env* ev, ab_num inNewlines);
    virtual void NewlineIndent(ab_Env* ev, ab_num inNewlines);
    virtual void Flush(ab_Env* ev) ;
    virtual void Hex(ab_Env* ev, const void* b, ab_num inSize, ab_pos inPos);
    virtual ab_num PushDepth(ab_Env* ev);
    virtual ab_num PopDepth(ab_Env* ev);
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseFilePrinter()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_FilePrinter();

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected non-poly ab_FilePrinter methods

	void file_printer_not_open(ab_Env* ev, const char* inMethod) const;
	void file_printer_bad_file(ab_Env* ev, const char* inMethod) const;
	void file_printer_indent(ab_Env* ev) const;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly ab_FilePrinter methods

    ab_FilePrinter(ab_Env* ev, const ab_Usage& inUsage, ab_File* ioFile);
    
    void CloseFilePrinter(ab_Env* ev);
	    
	static ab_FilePrinter* GetLogFilePrinter(ab_Env* ev);
		// standard session printer (uses ab_StdioFile::GetLogStdioFile()).
		// This printer instance is used by ab_Env::GetLogFileEnv().
		// This printer instance is used by ab_FileTracer::GetLogFileTracer().
		// (Please, never close this printer instance.)

private: // copying is not allowed
    ab_FilePrinter(const ab_FilePrinter& other);
    ab_FilePrinter& operator=(const ab_FilePrinter& other);
};

/*=============================================================================
 * ab_ObjectLink: type specific link subclass for lists (deques) of ab_Object
 */

class ab_ObjectLink /*d*/ : public AB_Link {
public:
  ab_Object*   mObjectLink_Object; /* pointer to object this link knows */
  ab_ref_count mObjectLink_RefCount; /* ref count for this object in a list */

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_ObjectLink methods
    ab_ObjectLink(ab_Object* ioObject)
        : mObjectLink_Object(ioObject), mObjectLink_RefCount(1) { }
    
    ~ab_ObjectLink() { } // do nothing

private: // copying is not allowed
    ab_ObjectLink(const ab_ObjectLink& other);
    ab_ObjectLink& operator=(const ab_ObjectLink& other);
};

/* ===== ===== ===== ===== ab_ObjectSet ===== ===== ===== ===== */

class ab_ObjectSet /*d*/ : public ab_Object {
protected:
    AB_Deque       mObjectSet_Objects;    // list of ab_ObjectLink
    ab_num         mObjectSet_Seed;       // times list is modified
    
    ab_Object*     mObjectSet_Member;     // current obj in iteration
    ab_ObjectLink* mObjectSet_NextLink;   // next link in iteration
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseObjectSet()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_ObjectSet();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_ObjectSet methods
    ab_ObjectSet(const ab_Usage& inUsage);
    void CloseObjectSet(ab_Env* ev); // CloseObject()
    
    void         TraceSet(ab_Env* ev);
    ab_num       GetSeed() const { return mObjectSet_Seed; }

    ab_ref_count AddObject(ab_Env* ev, ab_Object* ioObject);
    ab_ref_count HasObject(ab_Env* ev, const ab_Object* inObject) const;
    ab_ref_count SubObject(ab_Env* ev, ab_Object* ioObject);
    ab_ref_count CutObject(ab_Env* ev, ab_Object* ioObject);
    ab_num       CutAllObjects(ab_Env* ev);
    ab_num       CountObjects(ab_Env* ev) const;
    ab_Object*   CutAnyObject(ab_Env* ev);
            
    ab_bool DoObjects(ab_Env* ev, ab_Object_mAction a, void* closure) const;
    
    ab_bool IsEmpty() const { return AB_Deque_IsEmpty(&mObjectSet_Objects); }
    ab_bool HasMembers() const { return !AB_Deque_IsEmpty(&mObjectSet_Objects); }
    
    // ````` iteration methods `````
    ab_Object*  FirstMember(ab_Env* ev);
    ab_Object*  NextMember(ab_Env* ev);
    ab_Object*  CurrentMember() const { return mObjectSet_Member; }

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // protected by convention
    
    ab_num count_links() const;

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected list manipulation
    
    ab_ObjectLink* get_link(const ab_Object* ioObject) const;
     void cut_link(ab_Env* ev, ab_ObjectLink* link);
   
    friend class ab_Model;
    friend class ab_View;

private: // copying is not allowed
    ab_ObjectSet(const ab_ObjectSet& other);
    ab_ObjectSet& operator=(const ab_ObjectSet& other);
};

enum ab_ObjectSet_eError {
    ab_ObjectSet_kFaultIterOutOfSync = /*i*/ AB_Fault_kObjectSet,
    ab_ObjectSet_kFaultOutOfMemory
    
};

/* ===== ===== ===== ===== ab_Part ===== ===== ===== ===== */

class ab_Part /*d*/ : public ab_Object {
            
protected:
    ab_Store*      mPart_Store;  // this part's database (can be self)
    ab_row_uid     mPart_RowUid; // temp or persistent uid for this part

	ab_num   mPart_NameSetSeed;    // copy of store's name set seed
	ab_num   mPart_ColumnSetSeed;  // copy of store's column set seed
	ab_num   mPart_RowSetSeed;     // copy of store's row set seed
	ab_num   mPart_RowContentSeed; // copy of store's row content seed

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // ClosePart()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Part();
            
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Part methods
    ab_Part(ab_Env* ev, const ab_Usage& inUsage, ab_row_uid inRowUid,
	    ab_Store* ioStore);
	    
    void ClosePart(ab_Env* ev); // CloseObject()
    ab_Store*  GetPartStore() const { return mPart_Store; }
    ab_row_uid GetPartRowUid() const { return mPart_RowUid; }

    ab_Store*  GetOpenStoreFromOpenPart(ab_Env* ev) const;

	ab_bool  PartNameSetSeedMatchesStore() const; // inline defined later
	
	ab_bool  PartColumnSetSeedMatchesStore() const; // inline defined later
	
	ab_bool  PartRowSetSeedMatchesStore() const; // inline defined later
	
	ab_bool  PartRowContentSeedMatchesStore() const; // inline defined later
	
	ab_num   GetPartNameSetSeed() const { return mPart_NameSetSeed; }
	ab_num   GetPartColumnSetSeed() const { return mPart_ColumnSetSeed; }
	ab_num   GetPartRowSetSeed() const { return mPart_RowSetSeed; }
	ab_num   GetPartRowContentSeed() const { return mPart_RowContentSeed; }
    
private: // copying is not allowed
    ab_Part(const ab_Part& other);
    ab_Part& operator=(const ab_Part& other);
};

enum ab_Part_eError {
    ab_Part_kFaultNullStore = /*i*/ AB_Fault_kPart,
    ab_Part_kFaultNotOpen,
    ab_Part_kFaultStoreNotOpen
    
};


/* ===== ===== ===== ===== ab_Model ===== ===== ===== ===== */

class ab_Model /*d*/ : public ab_Part {
            
protected:
    ab_Store*      mModel_Store;  // this model's database (can be self)
    ab_row_uid     mModel_RowUid; // temp or persistent uid for this model
    ab_ObjectSet   mModel_Views;  // list of ab_ObjectLink 

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseModel()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Model();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Model methods (typically no need to override)
    virtual ab_ref_count  AddView(ab_Env* ev, ab_View* ioView);
    virtual ab_ref_count  HasView(ab_Env* ev, const ab_View* inView) const;
    virtual ab_ref_count  SubView(ab_Env* ev, ab_View* ioView);
    virtual ab_ref_count  CutView(ab_Env* ev, ab_View* ioView);
    virtual ab_num        CutAllViews(ab_Env* ev);
    virtual ab_num        CountViews(ab_Env* ev) const;
            
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Model methods
    ab_Model(ab_Env* ev, const ab_Usage& inUsage, ab_row_uid inRowUid,
	    ab_Store* ioStore);
	    
    void CloseModel(ab_Env* ev); // CloseObject()
    
    void BeginModelFlux(ab_Env* ev);
    void EndModelFlux(ab_Env* ev);
    void ClosingModel(ab_Env* ev, const ab_Change* inChange);
    void ChangedModel(ab_Env* ev, const ab_Change* inChange);
    
private: // copying is not allowed
    ab_Model(const ab_Model& other);
    ab_Model& operator=(const ab_Model& other);
};

enum ab_Model_eError {
    ab_Model_kFaultNullStore = /*i*/ AB_Fault_kModel
    
};

/*=============================================================================
 * ab_View: thing which is sees shown model changes
 */
 
class ab_View /*d*/ : public ab_Object {
            
protected:
    ab_ObjectSet    mView_Models; /* list of AB_ObjectLink */
    ab_change_mask  mView_Interests; /* changes this View cares about */
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_View responses to ab_Model messages
    virtual void SeeBeginModelFlux(ab_Env* ev, ab_Model* m) = 0;
    virtual void SeeEndModelFlux(ab_Env* ev, ab_Model* m) = 0;
    
    virtual void SeeChangedModel(ab_Env* ev, ab_Model* m,
                 const ab_Change* c) = 0;
    virtual void SeeClosingModel(ab_Env* ev, ab_Model* m,
                 const ab_Change* c) = 0;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseView()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_View();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_View methods
    ab_View(const ab_Usage& inUsage, ab_change_mask interests);
    void CloseView(ab_Env* ev); // CloseObject();

    ab_ref_count AddModel(ab_Env* ev, ab_Model* ioModel)
    { return mView_Models.AddObject(ev, ioModel); }
    
    ab_ref_count HasModel(ab_Env* ev, const ab_Model* inModel) const
    { return mView_Models.HasObject(ev, inModel); }
    
    ab_ref_count SubModel(ab_Env* ev, ab_Model* ioModel)
    { return mView_Models.SubObject(ev, ioModel); }
    
    ab_ref_count CutModel(ab_Env* ev, ab_Model* ioModel)
    { return mView_Models.CutObject(ev, ioModel); }
    
    ab_num CutAllModels(ab_Env* ev)
    { return mView_Models.CutAllObjects(ev); }
    
    ab_num CountModels(ab_Env* ev) const
    { return mView_Models.CountObjects(ev); }

private: // copying is not allowed
    ab_View(const ab_View& other);
    ab_View& operator=(const ab_View& other);
};

/*=============================================================================
 * ab_StaleRowView: a View that notices stale rows resulting from changes
 */
 
class ab_StaleRowView /*d*/ : public ab_View {
public:
    ab_Object*   mStaleRowView_CloseTarget; /* close/release when row stales */
    ab_row_uid   mStaleRowView_RowTarget;   /* row that might become stale */
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_View responses to ab_Model messages
    virtual void SeeBeginModelFlux(ab_Env* ev, ab_Model* ioModel);
    virtual void SeeEndModelFlux(ab_Env* ev, ab_Model* ioModel);
    
    virtual void SeeChangedModel(ab_Env* ev, ab_Model* ioModel,
                 const ab_Change* inChange);
    virtual void SeeClosingModel(ab_Env* ev, ab_Model* ioModel,
                 const ab_Change* inChange);
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseStaleRowView()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_StaleRowView();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_StaleRowView methods
    ab_StaleRowView(ab_Env* ev, const ab_Usage& inUsage,
	     ab_Object* ioCloseTarget, ab_row_uid inRowTarget);
                    
    void CloseStaleRowView(ab_Env* ev); // CloseObject();
    void PerformStaleRowHandling(ab_Env* ev);

private: // copying is not allowed
    ab_StaleRowView(const ab_View& other);
    ab_StaleRowView& operator=(const ab_View& other);
};

/*=============================================================================
 * ab_TableStoreView: a View that notices stale rows resulting from changes
 */
 
class ab_TableStoreView /*d*/ : public ab_View {
public:
    ab_Store*    mTableStoreView_Store; // store watched by this view
    ab_Table*    mTableStoreView_Table; // table notified by this view
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_View responses to ab_Model messages
    virtual void SeeBeginModelFlux(ab_Env* ev, ab_Model* ioModel);
    virtual void SeeEndModelFlux(ab_Env* ev, ab_Model* ioModel);
    
    virtual void SeeChangedModel(ab_Env* ev, ab_Model* ioModel,
                 const ab_Change* inChange);
    virtual void SeeClosingModel(ab_Env* ev, ab_Model* ioModel,
                 const ab_Change* inChange);
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseTableStoreView()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_TableStoreView();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_TableStoreView methods

    ab_TableStoreView(ab_Env* ev, const ab_Usage& inUsage,
	     ab_Store* ioStore, ab_Table* ioTable);
                    
    void CloseTableStoreView(ab_Env* ev); // CloseObject();

private: // copying is not allowed
    ab_TableStoreView(const ab_View& other);
    ab_TableStoreView& operator=(const ab_View& other);
};


/*=============================================================================
 * ab_File: abstract file interface
 */

typedef enum ab_File_eFormat { /* duplicate of AB_File_eFormat type */
	ab_File_kUnknownFormat = 0x3F3F3F3F,       /* '????' */
	ab_File_kLdifFormat = 0x6C646966,    /* 'ldif' */
	ab_File_kHtmlFormat = 0x68746D6C,    /* 'hmtl' */
	ab_File_kXmlFormat = 0x61786D6C,     /* 'axml' */
	ab_File_kVCardFormat = 0x76637264,   /* 'vcrd' */
	ab_File_kBinaryFormat = 0x626E7279,  /* 'bnry' */
	ab_File_kNativeFormat = 0x6E617476   /* 'natv' current native format */
} ab_File_eFormat;

class ab_File /*d*/ : public ab_Object { /* ````` simple file API ````` */

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected ab_File members (similar to IronDoc)

	ab_u1   mFile_Frozen;   // 'F' => file allows only read access
	ab_u1   mFile_DoTrace;  // 'T' trace if ev->DoTrace()
	ab_u1   mFile_IoOpen;   // 'O' => io stream is open (& needs a close)
	ab_u1   mFile_Active;   // 'A' => file is active and usable

	const char* mFile_Name; // always non-nil (uses static "" when necessary)
	
	ab_u4   mFile_FormatHint;  // hint regarding the file's content format

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_File methods

	virtual ab_pos  Length(ab_Env* ev) const = 0; // eof
	virtual ab_pos  Tell(ab_Env* ev) const = 0;
	virtual ab_num  Read(ab_Env* ev, void* outBuf, ab_num inSize) = 0;
	virtual ab_pos  Seek(ab_Env* ev, ab_pos inPos) = 0;
	virtual ab_num  Write(ab_Env* ev, const void* inBuf, ab_num inSize) = 0;
	virtual void    Flush(ab_Env* ev) = 0;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    // virtual void CloseObject(ab_Env* ev); // CloseFile()
    // virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_File();
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_File methods
    ab_File(const ab_Usage& inUsage);
    
    void CloseFile(ab_Env* ev);

	ab_u4   GetFileFormatHint() const { return mFile_FormatHint; }
	void    ChangeFileFormatHint(ab_u4 inHint) { mFile_FormatHint = inHint; }
	
    ab_File_eFormat  GuessFileFormat(ab_Env* ev);

    const char* GetFileName() const { return mFile_Name; }
    void ChangeFileName(ab_Env* ev, const char* inFileName);
    	// inFileName can be nil (which has the same effect as passing "")

	ab_bool FileFrozen() const  { return mFile_Frozen == 'F'; }
	ab_bool FileDoTrace() const { return mFile_DoTrace == 'T'; }
	ab_bool FileIoOpen() const  { return mFile_IoOpen == 'O'; }
	ab_bool FileActive() const  { return mFile_Active == 'A'; }

	void SetFileFrozen(ab_bool b)  { mFile_Frozen = (b)? 'F' : 0; }
	void SetFileDoTrace(ab_bool b) { mFile_DoTrace = (b)? 'T' : 0; }
	void SetFileIoOpen(ab_bool b)  { mFile_IoOpen = (b)? 'O' : 0; }
	void SetFileActive(ab_bool b)  { mFile_Active = (b)? 'A' : 0; }

	
	ab_bool IsOpenActiveAndMutableFile() const
	{ return ( IsOpenObject() && FileActive() && !FileFrozen() ); }
		// call IsOpenActiveAndMutableFile() before writing a file
	
	ab_bool IsOpenAndActiveFile() const
	{ return ( this->IsOpenObject() && this->FileActive() ); }
		// call IsOpenAndActiveFile() before using a file
		
	void NewFileDownFault(ab_Env* ev) const;
		// call NewFileDownFault() when either IsOpenAndActiveFile()
		// is false, or when IsOpenActiveAndMutableFile() is false.
 
 	void NewFileErrnoFault(ab_Env* ev) const;
   		// call NewFileErrnoFault() to convert std C errno into AB fault

	void WriteNewlines(ab_Env* ev, ab_num inNewlines);
   		
private: // copying is not allowed
    ab_File(const ab_File& other);
    ab_File& operator=(const ab_File& other);
};

enum ab_File_eError {
    ab_File_kFaultNotOpen = /*i*/ AB_Fault_kFile, /* !IsOpenObject() */
    
    ab_File_kFaultNotActive,       /*i*/ /* nn !FileActive() */
    ab_File_kFaultAlreadyActive,   /*i*/ /* nn FileActive() */
    ab_File_kFaultNotIoOpen,       /*i*/ /* nn !FileIoOpen() */
    ab_File_kFaultNoFileName,      /*i*/ /* nn */
    ab_File_kFaultFrozen,          /*i*/ /* nn FileFrozen() */
    ab_File_kFaultDownUnknown,     /*i*/ /* unknown reason for being down */
    ab_File_kFaultMissingIo,       /*i*/ /* file io object is missing */
    ab_File_kFaultNullOpaqueParameter,    /*i*/ /*  */
    ab_File_kFaultZeroErrno,       /*i*/ /* errno surprisingly equals zero*/
    ab_File_kFaultPosBeyondEof,    /*i*/ /* position beyond end of file */
    ab_File_kFaultNullBuffer,      /*i*/ /* io buffer is null */
    ab_File_kFaultCantReadSink,    /*i*/ /* file is output sink/writeonly */
    ab_File_kFaultCantWriteSource, /*i*/ /* file is input source/readonly */
    ab_File_kFaultBadCursorOrder,  /*i*/ /* buffer cursors badly ordered */
    ab_File_kFaultApiDummyNoImpl,  /*i*/ /* no file implementation */
    ab_File_kFaultWrongTag         /*i*/ /* not expected file subclass */
};

// NOTE ON METHOD TRACING IN FILES: it is very important that files installed
// inside a tracer used by the environment NOT have tracing enabled, because
// if they do, then tracing will infinitely recurse and overflow the stack (or
// perhaps cause a panic halt in stack checking code if you are lucky).

// method begin/end trace macros for files, also copied from IronDoc:

#if defined(AB_CONFIG_TRACE_CALL_TREE) && AB_CONFIG_TRACE_CALL_TREE

#define ab_File_BeginMethod(f,ev,c,m) /*i*/ \
  { if ( (f)->FileDoTrace() && (ev)->DoTrace() ) \
  (ev)->TraceBeginMethod((c), (m)); } {
  
#define ab_File_EndMethod(f,ev) /*i*/ \
  } { if ( (f)->FileDoTrace() && (ev)->DoTrace() ) (ev)->TraceEndMethod(); }
  
#else /* end AB_CONFIG_TRACE_CALL_TREE*/

#define ab_File_BeginMethod(ev,c,m) 
#define ab_File_EndMethod(ev)
#endif /* end AB_CONFIG_TRACE_CALL_TREE*/

/*=============================================================================
 * ab_StdioFile: concrete file using standard C file io
 */

class ab_StdioFile /*d*/ : public ab_File { /* `` copied from IronDoc `` */

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected ab_StdioFile members

	void* mStdioFile_File; // actually type FILE*, but using opaque void* type

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_File methods

	virtual ab_pos  Length(ab_Env* ev) const; // eof
	virtual ab_pos  Tell(ab_Env* ev) const;
	virtual ab_num  Read(ab_Env* ev, void* outBuf, ab_num inSize);
	virtual ab_pos  Seek(ab_Env* ev, ab_pos inPos);
	virtual ab_num  Write(ab_Env* ev, const void* inBuf, ab_num inSize);
	virtual void    Flush(ab_Env* ev);

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseFile()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_StdioFile();
    
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected non-poly ab_StdioFile methods

	void new_stdio_file_fault(ab_Env* ev) const;
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly ab_StdioFile methods
    ab_StdioFile(const ab_Usage& inUsage);
    
    ab_StdioFile(ab_Env* ev, const ab_Usage& inUsage,
	    const char* inName, const char* inMode);
	    // calls OpenStdio() after construction

    ab_StdioFile(ab_Env* ev, const ab_Usage& inUsage,
	     void* ioFile, const char* inName, ab_bool inFrozen);
	    // calls UseStdio() after construction
	    
	static ab_StdioFile* GetLogStdioFile(ab_Env* ev);
		// return the standard log file used by ab_StdioFile for each session.
	    
	void CloseStdioFile(ab_Env* ev);
	
	void OpenStdio(ab_Env* ev, const char* inName, const char* inMode);
		// Open a new FILE with name inName, using mode flags from inMode.
	
	void UseStdio(ab_Env* ev, void* ioFile, const char* inName,
		ab_bool inFrozen);
		// Use an existing file, like stdin/stdout/stderr, which should not
		// have the io stream closed when the file is closed.  The ioFile
		// parameter must actually be of type FILE (but we don't want to make
		// this header file include the stdio.h header file).
		
	void CloseStdio(ab_Env* ev);
		// Close the stream io if both and FileActive() and FileIoOpen(), but
		// this does not close this instances (like CloseStdioFile() does).
		// If stream io was made active by means of calling UseStdio(),
		// then this method does little beyond marking the stream inactive
		// because FileIoOpen() is false.
    
private: // copying is not allowed
    ab_StdioFile(const ab_StdioFile& other);
    ab_StdioFile& operator=(const ab_StdioFile& other);
};

/*=============================================================================
 * ab_Stream: buffered file i/o
 */

/*| ab_Stream exists to define an ab_File subclass that provides buffered
**| i/o for an underlying content file.  Naturally this arrangement only makes
**| sense when the underlying content file is itself not efficiently buffered
**| (especially for character by character i/o).
**|
**|| ab_Stream is intended for either reading use or writing use, but not
**| both simultaneously or interleaved.  Pick one when the stream is created
**| and don't change your mind.  This restriction is intended to avoid obscure
**| and complex bugs that might arise from interleaved reads and writes -- so
**| just don't do it.  A stream is either a sink or a source, but not both.
**|
**|| Exactly one of mStream_ReadEnd or mStream_WriteEnd must be a null pointer,
**| and this will cause the right thing to occur when inlines use them, because
**| mStream_At < mStream_WriteEnd (for example) will always be false and the
**| else branch of the statement calls a function that raises an appropriate
**| error to complain about either reading a sink or writing a source.
|*/

#define ab_Stream_kPrintBufSize /*i*/ 512 /* buffer size used by printf() */ 

#define ab_Stream_kMinBufSize /*i*/ 512 /* buffer no fewer bytes */ 
#define ab_Stream_kMaxBufSize /*i*/ (32 * 1024) /* buffer no more bytes */ 

class ab_Stream /*d*/ : public ab_File { /* from Mithril's AgStream class */

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected ab_Stream members
	ab_u1*    mStream_At;       // pointer into mStream_Buf
	ab_u1*    mStream_ReadEnd;  // null or one byte past last readable byte
	ab_u1*    mStream_WriteEnd; // null or mStream_Buf + mStream_BufSize

	ab_File*  mStream_ContentFile;  // where content is read and written

	ab_u1*    mStream_Buf;      // dynamically allocated memory to buffer io
	ab_num    mStream_BufSize;  // requested buf size (fixed by min and max)
	ab_pos    mStream_BufPos;   // logical position of byte at mStream_Buf
	ab_bool   mStream_Dirty;    // does the buffer need to be flushed?
	ab_bool   mStream_HitEof;   // has eof been reached? (only frozen streams)

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_File methods

	virtual ab_pos  Length(ab_Env* ev) const; // eof
	virtual ab_pos  Tell(ab_Env* ev) const;
	virtual ab_num  Read(ab_Env* ev, void* outBuf, ab_num inSize);
	virtual ab_pos  Seek(ab_Env* ev, ab_pos inPos);
	virtual ab_num  Write(ab_Env* ev, const void* inBuf, ab_num inSize);
	virtual void    Flush(ab_Env* ev);

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseStream()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Stream();
    
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected non-poly ab_Stream methods (for char io)

	int     fill_getc(ab_Env* ev);
	void    spill_putc(ab_Env* ev, int c);
	void    spill_buf(ab_Env* ev); // spill/flush from buffer to file
	    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly ab_Stream methods
    ab_Stream(ab_Env* ev, const ab_Usage& inUsage,
	    ab_File* ioContentFile, ab_num inBufSize, ab_bool inFrozen);
	    
	void CloseStream(ab_Env* ev);

	ab_File* GetStreamContentFile() const { return mStream_ContentFile; }
	ab_num   GetStreamBufferSize() const { return mStream_BufSize; }
	
	void    PutString(ab_Env* ev, const char* inString);
	void    PutStringThenNewline(ab_Env* ev, const char* inString);
	
	// ````` ````` stdio type methods ````` ````` 
	void    Printf(ab_Env* ev, const char* inFormat, ...);
	
	ab_pos  Ftell(ab_Env* ev) /*i*/ { return this->Tell(ev); }
	
	void    Fsetpos(ab_Env* ev, ab_pos inPos) /*i*/ { this->Seek(ev, inPos); }
	
	void    Rewind(ab_Env* ev) /*i*/ { this->Fsetpos(ev, 0); }
	
	void    Fflush(ab_Env* ev) /*i*/ { this->Flush(ev); }
	
	ab_bool Feof() /*i*/ { return mStream_HitEof; }
		// Feof() is never true for writable stream output sinks 
	
	void    Puts(ab_Env* ev, const char* inString) /*i*/
	{ this->PutString(ev, inString); }

	void    Ungetc(int c) /*i*/
	{ if ( mStream_At > mStream_Buf && c > 0 ) *--mStream_At = c; }
	
	int     Getc(ab_Env* ev) /*i*/
	{ return ( mStream_At < mStream_ReadEnd )? *mStream_At++ : fill_getc(ev); }
	
	void    Putc(ab_Env* ev, int c) /*i*/
	{ 
		mStream_Dirty = AB_kTrue;
		if ( mStream_At < mStream_WriteEnd )
			*mStream_At++ = c;
		else
			spill_putc(ev, c);
	}

	// ````` ````` high level i/o operations ````` ````` 
	
	void    GetDoubleNewlineDelimitedRecord(ab_Env* ev, ab_String* outString);
		// GetDoubleNewlineDelimitedRecord() reads a "record" from the stream
		// that is intended to correspond to an ldif record in a file being
		// imported into an address book. Such records are delimited by two
		// (or more) empty lines, where all lines separated by only a single
		// empty line are considered part of the record.  The record is written
		// to outString which grows as necessary to hold the record.  (This
		// string is first emptied with ab_String::TruncateStringLength(), so
		// the string should not be a nonempty static readonly string.  The
		// same string can easily be used in successive calls to this method
		// to avoid any more memory management than it needed.)
		//
		// Let LF, CR, and BL denote bytes 0xA, 0xD, and 0x20 respectively.
		// This method collapses sequences of LF and CR to a single LF because
		// this is what ldif parsing code expects to separate lines in each
		// ldif record.
		//
		// UNLIKE the original spec, BLANKS ARE NOT REMOVED, because leading
		// blanks are significant to continued line parsing in ldif format.
		// 
		// Counting the number of empty lines
		// encountered is made more complex by the need to make the process
		// cross platform, where a line-end on various platforms might be a
		// single LF or CR, or a combined CRLF.  Additionally, ldif files were
		// once erroneously written with line-ends composed of CRCRLF, so this
		// should count as a single line as well.  Otherwise, this method will
		// consider two empty lines encountered (thus ending the record) when
		// enough LF's and CR's are seen to account for two lines on some
		// platform or another.  Once either LF or CR is seen, all instances
		// of LF, CR, and BL will be consumed from the input stream, but only a
		// single LF will be written to the output record string.  While this
		// white space is being parsed and discarded, the loop decides whether
		// more than one line is being seen, which ends the record when true.
		// 
		// This method is intended to be a more efficient way to parse groups
		// of lines from an input file than by parsing individual lines which
		// then get concatenated together.  The performance difference might
		// matter for large ldif files, especially when more than one pass must
		// be performed on the same input file.
		// 
		// When this method returns, the current file position should point to
		// either eof (end of file) or the first byte of the next record.
    
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected non-poly high-level support utilities
	
	ab_bool consuming_white_space_ends_record(ab_Env* ev, int c);
		// Consume the rest of the line ending after a leading 0xA or 0xD
		// which is passed in as the value of input argument c.  Analyze the
		// consumed white space as per the description of public method
		// GetDoubleNewlineDelimitedRecord(), and return true if more than
		// one line-ending is detected which signals end of record.
    
private: // copying is not allowed
    ab_Stream(const ab_Stream& other);
    ab_Stream& operator=(const ab_Stream& other);
};

/*=============================================================================
 * ab_IntMap: collection of assocs from key uid to value uid 
 */

#define ab_IntMap_kMailingListImportHint /*i*/ 512
	// hint that less than 512 mailing lists will be typically imported at once

/* ab_IntMap is a "closed" hash table that resolves hash collisions by finding
** an available slot.  Performance is ensured by making sure occupancy is no
** more than 80 percent of capacity before growing the hash table, which of
** course also rehashes all the old associations.  (This hash table follows
** the pattern of closed hash tables in public domain IronDoc.)
**
** The "key" and "value" of associations are each of type ab_map_int, which is
** defined to be a signed integer type whose size is the greater of either
** sizeof(ab_pos) or sizeof(void*), whichever is more.  The intention is that
** the map must be able to contain either pointers or the largest integers that
** we want to store.  The address book implementation might at times want to
** store integers in maps, but at other times addresses of objects.
**
** Zero is assumed never a valid key, so a zero key means a hash bucket slot
** is not being used. No allowance is made for removing associations in the
** table except by removing all associations at one time.  This usage fits the
** need to build a map of uids or of positions while importing, which gets
** thrown away en masse when import is finished.
**
** When a map is constructed, a boolean parameter indicates whether assocs
** should have values in addition to keys, so that every assoc is a pair
** mapping a key to a value.  This is normal usage.  When this flag is false
** at creation time, no values will be saved so the collection is really just
** a set of keys, and this is useful for integer sets that don't need values.
** When values are not supported, every value equals ab_IntMap_kPretendValue
** when the API return the value for an assoc.
**
** Each key maps to a "target" bucket at index ab_IntMap::random_bucket(), and
** a collision is resolved by searching for the next empty slot (holding a
** zero key) with wrap-around at the table's end.  The pseudo random number
** generator is used in bucket calculation to avoid expected clumping of keys
** from alignment patterns in either addresses or uid encodings.
*/

#define ab_IntMap_kPretendValue 0xFFFFFFFF /* used when values don't exist */

ab_i4 AbI4_Random(ab_i4 self); // pseudo RNG, also good for hashing

// define AbMapInt_Hash(i) ( (ab_u4) AbI4_Random((ab_i4) self) )
// #define ab_IntMap_KeyBucket(self,key) \
// 	( ( (ab_u4) AbI4_Random((ab_i4) key) ) % (self)->mIntMap_Capacity )

#define ab_IntMap_kMinSizeHint 3
#define ab_IntMap_kMaxSizeHint (12 * 1024)

/* required invariant: (a equal b) implies (hash a) == (hash b) */

typedef ab_u4 (* ab_IntMap_mHashKey /*d*/)
(ab_Env* ev, ab_map_int inKey);

typedef ab_bool (* ab_IntMap_mEqualKey /*d*/)
(ab_Env* ev, ab_map_int inTestKey, ab_map_int inMapMemberKey);

typedef char* (* ab_IntMap_mAssocString /*d*/)
(ab_Env* ev, ab_map_int inKey, ab_map_int inValue, char* outXmlBuf,
 ab_num inHashBucket, ab_num inActualBucket);

class ab_KeyMethods {
public:
	ab_IntMap_mHashKey        mKeyMethods_Hash;
	ab_IntMap_mEqualKey       mKeyMethods_Equal;
	ab_IntMap_mAssocString  mKeyMethods_AssocString;
	
	ab_KeyMethods(ab_IntMap_mHashKey h, ab_IntMap_mEqualKey e,
		ab_IntMap_mAssocString a)
	{
		mKeyMethods_Hash = h;
		mKeyMethods_Equal = e;
		mKeyMethods_AssocString = a;
	}
};

class ab_IntMap	/*d*/ : public ab_Object {
	// this table derives from public domain IronDoc and Mithril hash tables

// ````` ````` ````` `````   ````` ````` ````` `````  
protected:
	ab_num      mIntMap_ChangeCount;   // count table changes
	ab_num      mIntMap_Threshold;     // max assocs before growth; 80% of cap
	ab_num      mIntMap_Capacity;      // number of buckets
	ab_count    mIntMap_AssocCount;    // number of member associations
	ab_map_int* mIntMap_KeyInts;       // keys for each association
	ab_map_int* mIntMap_ValueInts;     // values for each association
	
	// hash and equal method defaults: ab_Env_HashKey() and ab_Env_EqualKey()
	ab_IntMap_mHashKey   mIntMap_Hash;  // default ab_Env_HashKey()
	ab_IntMap_mEqualKey  mIntMap_Equal; // default ab_Env_EqualKey()
	ab_IntMap_mAssocString  mIntMap_AssocString; // default ab_Env_AssocString()
	
	ab_bool     mIntMap_HasValuesToo;  // use mIntMap_ValueInts to store values
	
	friend class ab_IntMapIter; // let ab_IntMapIter access member variables

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseIntMap()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_IntMap();

// ````` ````` ````` `````   ````` ````` ````` `````  
private: // private non-poly ab_IntMap helper methods

	void grow_int_map(ab_Env* ev); // increase capacity by some percentage
	
	ab_bool init_with_size(ab_Env* ev, const ab_KeyMethods* inKeyMethods, 
		ab_num inSizeHint);

	// ````` ````` bucket location ````` `````  
    
    ab_u4 hash_bucket(ab_Env* ev, ab_map_int inKey) const
    { return ( (*mIntMap_Hash)(ev, inKey) % mIntMap_Capacity ); }
    
    ab_map_int* find_bucket(ab_Env* ev, ab_map_int inKey) const;

	// ````` ````` bucket location ````` `````  
    
    ab_bool equal_key(ab_Env* ev, ab_map_int inTest, ab_map_int inMem) const
    { return (*mIntMap_Equal)(ev, inTest, inMem); }

	// ````` ````` adding with less error detection ````` `````  
	void        fast_add(ab_Env* ev, ab_map_int inKey, ab_map_int inValue);

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_IntMap methods
    ab_IntMap(ab_Env* ev, const ab_Usage& inUsage,
	    const ab_KeyMethods* inKeyMethods, ab_num inSizeHint);
	    // inKeyMethods can be null to get the default suite for integers.
    	// defaults to supporting values in addition to keys.  inSizeHint
    	// is capped between kMinSizeHin and kMaxSizeHint, and this becomes
    	// the new threshold, with capacity equal to 25% more.
    	
    ab_IntMap(ab_Env* ev, const ab_Usage& inUsage,
    	const ab_KeyMethods* inKeyMethods, ab_num inSizeHint,
    	ab_bool inValuesToo);
	    // inKeyMethods can be null to get the default suite for integers.
    	// map contains no values when inValuesToo is false, which
    	// technically creates a simple set and not a map.  inSizeHint
    	// is capped between kMinSizeHin and kMaxSizeHint, and this becomes
    	// the new threshold, with capacity equal to 25% more.
    	
    ab_IntMap(ab_Env* ev, const ab_Usage& inUsage, ab_num inSizeHint,
    	const ab_IntMap& ioOtherMap);
    	// The new map contains values provided ioOtherMap does. inSizeHint is
    	// capped between kMinSizeHin and kMaxSizeHint, but is then increased
    	// to ioOtherMap.GetThreshold() is this is greater, and then used for
    	// the new threshold, with capacity equal to 25% more.  The content in
    	// ioOtherMap is copied into this one.  (This constructor is used when
    	// a map is grown with grow_int_map(), which makes a larger local map
    	// before replacing the old slots when this is successful.)
    	
    void GetKeyMethods(ab_KeyMethods* outMethods)
	{ 
		outMethods->mKeyMethods_Hash = mIntMap_Hash;
		outMethods->mKeyMethods_Equal = mIntMap_Equal;
		outMethods->mKeyMethods_AssocString = mIntMap_AssocString;
	}
    	    	
    void CloseIntMap(ab_Env* ev);

	// ````` ````` accessors ````` `````  

	ab_num      GetChangeCount() const { return mIntMap_ChangeCount; }
	ab_num      GetThreshold() const { return mIntMap_Threshold; }
	ab_num      GetCapacity() const { return mIntMap_Capacity; }
	ab_count    GetAssocCount() const { return mIntMap_AssocCount; }
	ab_bool     HasValuesToo() const { return mIntMap_HasValuesToo; }

	// ````` ````` entire table methods ````` `````  
	ab_bool     AddTable(ab_Env* ev, const ab_IntMap& ioOtherMap);
		// add contents of ioOtherMap, and then return ev->Good().

	// ````` ````` uid assoc methods ````` `````  
	void        AddAssoc(ab_Env* ev, ab_map_int inKey, ab_map_int inValue);
		// add one new association (if an association already exists with 
		// the same key, then the value is simply updated to the new value).
		
	ab_map_int  GetValueForKey(ab_Env* ev, ab_map_int inKey);
		// search for one association keyed by inKey, and return the value.
		// Since values cannot be zero, zero is returned to mean "not found".
		
	void        CutAllAssocs(ab_Env* ev);
		// remove all associations

	// ````` ````` uid key set only methods (with no values used) ````` `````  
	
	void    AddKey(ab_Env* ev, ab_map_int inKey)
	{ this->AddAssoc(ev, inKey, ab_IntMap_kPretendValue); }
	
	ab_bool ContainsKey(ab_Env* ev, ab_map_int inKey)
	{ return (this->GetValueForKey(ev, inKey) != 0); }
};

enum ab_Map_eError {
    ab_Map_kFaultZeroKey = /*i*/ AB_Fault_kMap,  // zero key passed
    ab_Map_kFaultOverThreashold,                 // map not big enough
    ab_Map_kFaultNoBucketFound,                  // search for bucket failed
    ab_Map_kFaultIterPosInvalid,                 // iter position invalid
    ab_Map_kFaultIterEmptyBucket,                // current iter bucket empty
    ab_Map_kFaultMissingVector                   // bad hash table structure
};


class ab_IntMapIter /*d*/ {
	// this iter derives from public domain IronDoc and Mithril hash tables

// ````` ````` ````` `````   ````` ````` ````` `````  
protected:
	const ab_IntMap*  mIntMapIter_Map;         // copy of mIntMap_ChangeCount
	ab_pos            mIntMapIter_BucketPos;   // current bucket in iteration
	ab_num            mIntMapIter_ChangeCount; // copy of mIntMap_ChangeCount

	// ab_ObjectSet_kFaultIterOutOfSync when change count does not match

// ````` ````` ````` `````   ````` ````` ````` `````  
private: // private non-poly ab_IntMapIter helper methods
    
    ab_map_int next_assoc(ab_pos inPos, ab_map_int* outValue);

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_IntMapIter methods

    ab_IntMapIter(ab_Env* ev, const ab_IntMap* ioIntMap);
    // the ioIntMap object is *not* reference counted, because this iter is
    // expected to be only short-lived inside a single method.  Callers must
    // use First() to start an iteration.

	// the following methods return keys, which is zero when iter is finished
	ab_map_int First(ab_Env* ev, ab_map_int* outValue);
	ab_map_int Here(ab_Env* ev, ab_map_int* outValue);
	ab_map_int Next(ab_Env* ev, ab_map_int* outValue);
	
	// query whether ab_ObjectSet_kFaultIterOutOfSync error will not occur:
	ab_bool IsInSyncWithMap() const 
	{ return mIntMapIter_ChangeCount == mIntMapIter_Map->GetChangeCount(); }
	
// ````` ````` ````` `````   ````` ````` ````` ````` 
// these operators should be private but Irix has problems compiling this, so
// we only make it private on the Mac, and this catches illegal object deletes.
#if defined(XP_MAC)
	
// ````` ````` ````` `````   ````` ````` ````` `````  
private: // private heap methods: heap instantiation is *NOT ALLOWED*:

	void* operator new(size_t inSize);     /* priv on Mac (actually always) */
	void operator delete(void* ioAddress); /* priv on Mac (actually always) */
#endif /*XP_MAC*/
};

/*=============================================================================
 * ab_Thumb: represents entry counts, positions, and progress in import/export 
 */

#define ab_Thumb_kAverageRowSize     512 /* assume half K for ldif record */
#define ab_Thumb_kMinSteamBufSize    (4 * 1024)  /* no point reading less */
#define ab_Thumb_kNormalSteamBufSize (16 * 1024) /* > disk block size */
#define ab_Thumb_kMaxSteamBufSize    (48 * 1024) /* < 64K */

#define ab_Thumb_kMaxRealPass 2 /* no more than two passes through input */

class ab_Thumb /*d*/ : public ab_Object {

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // member variables with controlled access

	ab_IntMap*    mThumb_ListPosMap; // map of list uid to list file pos

	ab_policy     mThumb_ImportConflictPolicy; // start at 0
	char*         mThumb_ConflictReportFileName; // initially null
	ab_StdioFile* mThumb_ReportFile;

	void          determine_import_conflict_policy(ab_Env* ev);
	
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // member variables with free access
	ab_pos        mThumb_FilePos; // where to position file to continue import
	ab_row_pos    mThumb_RowPos;  // cumulative total number of rows ported
	
	ab_row_count  mThumb_RowCountLimit;  // max rows to import before stopping
	ab_num        mThumb_ByteCountLimit; // max file bytes read before stopping
	
	/* ````` used to capture progress limit information  ````` */
	ab_pos        mThumb_ImportFileLength; // total file bytes to be imported
	ab_row_count  mThumb_ExportTotalRowCount; // total rows to be exported
	
	/* ````` slots reserved for import and export code ````` */
	ab_count      mThumb_CurrentPass; // pass through import file, in [0..3]
	ab_u4         mThumb_FileFormat;  // ab_File_eFormat (import format used)

	ab_num        mThumb_ListCountInFirstPass; // number lists seen 1st pass
	ab_num        mThumb_ListCountInSecondPass; // number lists done 2nd pass
	
	ab_num        mThumb_PortingCallCount; // number of times thumb reused
	ab_bool       mThumb_HavePreparedSecondPass; // reset file to beginning?
	
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseThumb()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Thumb();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Thumb methods
    ab_Thumb(ab_Env* ev, const ab_Usage& inUsage);
    
    // When IsProgressFinished() returns true, this tells clients
    // they should stop trying to import and export asynchronously, because
    // there is no more content to process and the task is entirely finished.
    ab_bool IsProgressFinished() const
    { return ( mThumb_CurrentPass > ab_Thumb_kMaxRealPass ); }
    
    void SetCompletelyFinished()
    { mThumb_CurrentPass = ab_Thumb_kMaxRealPass + 1; }
    
    ab_count GetMappedListUidCount() const 
    { return (mThumb_ListPosMap)? mThumb_ListPosMap->GetAssocCount(): 0; }
    
    ab_IntMap* GetListPosMap(ab_Env* ev);
    	// Get the hash table that maps list uids to file positions.
    	// This method creates this object lazily on the first request, and
    	// then keeps the map until the thumb is closed.  If one wishes to
    	// know whether there is any content without forcing this table to
    	// exist, one can call GetMappedListUidCount() to get the number of
    	// lists that have been mapped, which can avoid creating the map.
    	
    ab_policy        GetImportConflictPolicy(ab_Env* ev);
    const char*      GetConflictReportFileName(ab_Env* ev);
	ab_StdioFile*    GetReportFile(ab_Env* ev);
    
    ab_num ListsCountedInFirstPass() const
    { return mThumb_ListCountInFirstPass; }
    
    ab_num ListsPortedInSecondPass() const
    { return mThumb_ListCountInSecondPass; }
    
    ab_bool StillOnFirstPassThroughFile() const
    { return ( mThumb_CurrentPass < 2 ); }
    
    ab_bool OnSecondPassThroughFile() const
    { return ( mThumb_CurrentPass >= 2 ); }
    
    ab_bool FinishedSecondPassThroughFile() const
    { return ( mThumb_CurrentPass > 2 ); }
    
    ab_num  PickHeuristicStreamBufferSize() const;
    	// Typically returns ab_Thumb_kNormalSteamBufSize except when values
    	// of either (or both) mThumb_RowCountLimit and mThumb_ByteCountLimit
    	// imply substantially more or less buffer space might make more sense.
    	// Bounded by kMin and kMax values defined near kNormalSteamBufSize.
    	// For sizing purposes, average row size is assumed kAverageRowSize.
    
    void CloseThumb(ab_Env* ev);
};


/* ===== ===== ===== ===== ab_Factory ===== ===== ===== ===== */

class ab_Factory {
public:

	static ab_Store* // for database purposes
	MakeStore(ab_Env* ev, const char* inFileName, ab_num inFootprint);

	static ab_File* // for old format binary readonly import purposes
	MakeOldFile(ab_Env* ev, const char* inFileName);

	static ab_File* // for current format binary readonly import purposes
	MakeNewFile(ab_Env* ev, const char* inFileName);
};

/*=============================================================================
 * ab_Store: abstract database interface
 */

#define ab_Store_kMinFootprint /*i*/ (48 * 1024)
#define ab_Store_kMinFootprintGrowth /*i*/ (32 * 1024)

#define ab_Store_kGoodFootprintSpace /*i*/ (512 * 1024)
#define ab_Store_kGoodFootprintGrowth /*i*/ (1024 * 1024)

#define ab_Store_kBigStartingFootprint /*i*/ (8 * 1024 * 1024)
#define ab_Store_kBigFootprintGrowth /*i*/ (4 * 1024 * 1024)

class ab_Store /*d*/ : public ab_Model { /* ````` database ````` */
protected:
    char*    mStore_FileName; // allocated copy of constructor parameter
    ab_num   mStore_TargetFootprint; // current suggested memory footprint
    ab_u4    mStore_ContentAccess; // ab_Object_kOpen or ab_Object_kShut

	ab_num   mStore_NameSetSeed;    // changed when name set changes
	ab_num   mStore_ColumnSetSeed;  // changed when column set changes
	ab_num   mStore_RowSetSeed;     // changed when row set changes
	ab_num   mStore_RowContentSeed; // changed when row content changes

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Store methods
    virtual ab_Table*  GetTopStoreTable(ab_Env* ev) = 0;
    virtual ab_Table*  AcquireTopStoreTable(ab_Env* ev) = 0;
    
    virtual void    OpenStoreContent(ab_Env* ev) = 0;
    virtual void    SaveStoreContent(ab_Env* ev) = 0;
    virtual void    CloseStoreContent(ab_Env* ev) = 0;
    virtual void    DumpStoreContent(ab_Env* ev, ab_Printer* p, ab_num max) = 0;
	virtual ab_num  StoreContentSize(ab_Env* ev) const = 0;
   
    virtual ab_num  MoreFootprint(ab_Env* ev, ab_num inMoreSpace) = 0;
    virtual ab_num  LessFootprint(ab_Env* ev, ab_num inLessSpace) = 0;
    virtual ab_num  OptimalFootprint(ab_Env* ev) = 0;

    virtual void    StartBatchMode(ab_Env* ev, ab_num inEventThreshold) = 0;
    virtual void    EndBatchMode(ab_Env* ev) = 0;

	// specify import file format:
    virtual void ImportOldBinary(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb) = 0;
    virtual void ImportCurrentNative(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb) = 0;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseStore()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Store();
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly ab_Store methods

	// ab_Store is abstract, so you cannot call this constructor directly:
    ab_Store(ab_Env* ev, const ab_Usage& inUsage,
	    const char* inFileName, ab_num inTargetFootprint);
	   
	// but you can call this method to see what subclass to instantiate:
    // static ab_Store* MakeStoreSubclassAfterSeeingFileContent(ab_Env* ev,
    //	const char* inFileName, ab_num inTargetFootprint);
 	// See new replacement: ab_Factory::MakeStore() for the above.
   
    static void DestroyStoreWithFileName(ab_Env* ev, const char* inFileName);
    	// DestroyStoreWithFileName() deletes the file from the file system
    	// containing the store that would be opened if a store instance was
    	// created using inFileName for the file name. This method is provided
    	// to keep file destruction equally as abstract as the implicit file
    	// creation occuring when a new store is created.  Otherwise it would
    	// be difficult to discard databases as easily as they are created. But
    	// this method is dangerous in the sense that destruction of data is
    	// definitely intended, so don't call this method unless the total
    	// destruction of data in the store is what you have in mind.  This
    	// method does not attempt to verify the file contains a database before
    	// destroying the file with name inFileName in whatever directory is
    	// used to host the database files.  Make sure you name the right file.
    
    void CloseStore(ab_Env* ev); // CloseObject()
    
    ab_bool      ChangeStoreFileName(ab_Env* ev, const char* inFileName);
    const char*  GetStoreFileName(ab_Env* ev) const;
    ab_num       GetTargetFootprint(ab_Env* ev) const;

    ab_bool IsOpenStoreContent() const
    { return mStore_ContentAccess == ab_Object_kOpen; }
    
    ab_bool IsShutStoreContent() const
    { return mStore_ContentAccess == ab_Object_kShut; }

     ab_bool IsReadyStore() const
    { return this->IsOpenObject() && 
             mStore_ContentAccess == ab_Object_kOpen; }

	void    BumpStoreNameSetSeed() { ++mStore_NameSetSeed; }
	ab_num  GetStoreNameSetSeed() const { return mStore_NameSetSeed; }

	void    BumpStoreColumnSetSeed() { ++mStore_ColumnSetSeed; }
	ab_num  GetStoreColumnSetSeed() const { return mStore_ColumnSetSeed; }

	void    BumpStoreRowSetSeed() { ++mStore_RowSetSeed; }
	ab_num  GetStoreRowSetSeed() const { return mStore_RowSetSeed; }

	void    BumpStoreRowContentSeed() { ++mStore_RowContentSeed; }
	ab_num  GetStoreRowContentSeed() const { return mStore_RowContentSeed; }

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Store port utilities using the store's top level table

	void ImportWithPromptForFileName(ab_Env* ev, MWContext* ioContext);
	void ExportWithPromptForFileName(ab_Env* ev, MWContext* ioContext);

    void ExportEntireStoreToNamedFile(ab_Env* ev, const char* inFileName);
    void ExportStoreToNamedFile(ab_Env* ev, const char* inFileName, ab_Thumb* ioThumb);
	
    // prompt for file name, and then guess the format:
    void ImportEntireFileByName(ab_Env* ev, const char* inFileName);
    	// ImportEntireFileByName() just calls ImportFileByName() with a
    	// thumb instance intended to convey "all the file content".
    
    // guess file format:
    void ImportFileByName(ab_Env* ev, const char* inFileName, ab_Thumb* ioThumb);
		// ImportFileByName() opens a file in the file system named inFileName.
		// This is similar to ImportContent(), but ImportContent() does not
		// assume the "file" is a file in the file system (instead, the ab_File
		// subclass might represent a string or some other stream).
		
    // guess file format:
    void ImportContent(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb);
    void ExportContent(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb);

    
    void ImportVCard(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb);
    void ImportLdif(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb);
    void ImportHtml(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb);
    void ImportXml(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb);
    void ImportTabDelimited(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb);

	// specify export file format:
    void ExportLdif(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb);
    void ExportXml(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb);
    void ExportTabDelimited(ab_Env* ev, ab_File* ioFile, ab_Thumb* ioThumb);
    
    // performance recommendations
    ab_num GoodImportFootprint(ab_Env* ev, ab_File* ioFile) const
    { return ioFile->Length(ev) / 2; } // half the file size
    
    ab_num BetterImportFootprint(ab_Env* ev, ab_File* ioFile) const
    { return ioFile->Length(ev); } // the entire file size
    
    // ````` ````` random import for large AB testing ````` ````` 
    
#ifdef AB_CONFIG_DEBUG
    static ab_num MakeImportRandomSeed(ab_Env* ev);
    	// MakeImportRandomSeed makes a new, fresh random seed for import.
    	
    static ab_num* UseSessionImportRandomSeed(ab_Env* ev);
    	// UseSessionImportRandomSeed always returns a pointer to same seed
    	// which gets initialized just once per session with a single call to
    	// MakeImportRandomSeed() the first time.
   
    void ImportRandomEntries(ab_Env* ev, ab_num inCount, ab_num* ioRandomSeed);
    	// The random seed passed in ioRandomSeed is used to drive the content
    	// of random entries added to the store, and a new seed representing the
    	// continuation of this process is returned in ioRandomSeed.   This lets
    	// multiple calls to ImportRandomEntries share the same random number
    	// sequence to ensure duplicate random entries are not possible.  a good
    	// strategy is to call MakeImportRandomSeed() once and save the result
    	// in a global which is passed in all ImportRandomEntries() calls. (We
    	// could do this internally, but we want to pass in the seed so the
    	// process is deterministically repeatable if necessary.)
    	//
    	// A main reason to make many calls to ImportRandomEntries() with small
    	// values for inCount (instead of one huge value for inCount) is to make
    	// the system stay responsive, because ImportRandomEntries() might lock
    	// up the current thread until all the inCount entries are added.
    	//
    	// For small memory footprints, and for dbs containing E entries, when
    	// C new entries are added, elapsed time can be M minutes.  M can be as
    	// bad as M = (C/1000)*5 + (E/1000)*5, or as good as M = (C/10000), but
    	// the good time probably requires footprint exceeding added entries.
    	// (The "bad" equation translates to "five minutes for each new thousand
    	// added plus five minutes for each thousand in the db", and the "good"
    	// equation says "one minute per ten thousand added.")  At the time of
    	// this writing, expected time is about M = (C/1000)*2 + (E/1000)*2,
    	// or two minutes per thousand added plus two minutes per db thousand.
    	//
    	// inCount is capped at a maximum of ten thousand to save your sanity.
#endif /*AB_CONFIG_DEBUG*/
    
private: // copying is not allowed
    ab_Store(const ab_Store& other);
    ab_Store& operator=(const ab_Store& other);
};


enum {
	ab_Store_kFaultMissingDatabase = /*i*/ AB_Fault_kStore,   /* 900 */
	ab_Store_kFaultMissingFilename,
	ab_Store_kFaultEmptyFilename,
	ab_Store_kFaultNotOpen,
	ab_Store_kFaultContentNotOpen,
	ab_Store_kFaultContentAlreadyOpen,
	ab_Store_kFaultNullCachedStore,
	ab_Store_kFaultNullCachedDatabase,
	ab_Store_kFaultExistingDatabase,
	ab_Store_kFaultBadDatabaseCreate,
	ab_Store_kFaultBadDatabaseOpen,
	ab_Store_kFaultBadDatabaseClose,
	ab_Store_kFaultCommitException,
	ab_Store_kFaultDbMetaClassThrow,
	ab_Store_kFaultOldDbFileFormat,
    ab_Store_kFaultUnknownImportFormat,
    ab_Store_kFaultNullStoreForImport,
    ab_Store_kFaultNotOpenStoreForImport,
    ab_Store_kFaultNullEnvForImport,
    ab_Store_kFaultNotOpenEnvForImport,
    ab_Store_kFaultUnknownStoreFormat,
    ab_Store_kFaultStaleStoreFormat,
    ab_Store_kFaultFutureStoreFormat,
    ab_Store_kFaultImportFormatOnly,
    ab_Store_kFaultNullClosureForImport,
    ab_Store_kFaultNotOldBinaryFormat,
    ab_Store_kFaultNotCurrentNativeFormat
};

/* ===== ===== ===== ===== ab_Part (2) ===== ===== ===== ===== */

inline ab_bool ab_Part::PartNameSetSeedMatchesStore() const /*i*/
	{ return mPart_NameSetSeed == mPart_Store->GetStoreNameSetSeed(); }
	
inline ab_bool ab_Part::PartColumnSetSeedMatchesStore() const /*i*/
	{ return mPart_ColumnSetSeed == mPart_Store->GetStoreColumnSetSeed(); }
	
inline ab_bool ab_Part::PartRowSetSeedMatchesStore() const /*i*/
	{ return mPart_RowSetSeed == mPart_Store->GetStoreRowSetSeed(); }
	
inline ab_bool ab_Part::PartRowContentSeedMatchesStore() const /*i*/
	{ return mPart_RowContentSeed == mPart_Store->GetStoreRowContentSeed(); }

/*=============================================================================
 * ab_Defaults: 
 */

class ab_Defaults /*d*/ : public ab_Part {
/* ````` row and cell generator ````` */

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Defaults methods
    virtual ab_Row*  MakeDefaultRow(ab_Env* ev, ab_Table* ioTable);
    virtual ab_Row*  MakeDefaultCellRow(ab_Env* ev, ab_Table* ioTable,
	    const AB_Cell* inCells);
    virtual ab_Row*  MakeDefaultColumnRow(ab_Env* ev, ab_Table* ioTable,
	    const AB_Column* inColumns);

    virtual ab_cell_count
    GetDefaultPersonCells(ab_Env* ev, AB_Cell* outCells,
	     ab_cell_count inSize, ab_cell_count* outLength);
                             
    virtual ab_cell_count
    GetDefaultListCells(ab_Env* ev, AB_Cell* outCells,
	    ab_cell_count inSize, ab_cell_count* outLength);

    virtual ab_column_count
    GetDefaultColumns(ab_Env* ev, AB_Column* outColumns,
	    ab_column_count inSize, ab_column_count* outLength);

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseDefaults()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Defaults();
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Defaults methods
    ab_Defaults(ab_Env* ev, const ab_Usage& inUsage, ab_Store* ioStore);
    void CloseDefaults(ab_Env* ev); // CloseObject()
    
private: // copying is not allowed
    ab_Defaults(const ab_Defaults& other);
    ab_Defaults& operator=(const ab_Defaults& other);
};

/*=============================================================================
 * ab_NameSet: 
 */

class ab_NameSet /*d*/ : public ab_Part { /* ````` token collection ````` */

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseNameSet()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_NameSet();

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_NameSet methods

	// default methods understand only standard column names
	
    virtual const char*    GetName(ab_Env* ev, ab_column_uid token) const;
    virtual ab_column_uid  GetToken(ab_Env* ev, const char* name) const;
    virtual ab_column_uid  NewToken(ab_Env* ev, const char* name);
        
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_NameSet methods
    ab_NameSet(ab_Env* ev, const ab_Usage& inUsage, ab_Store* ioStore);
    
    void CloseNameSet(ab_Env* ev); // CloseObject()
    
private: // copying is not allowed
    ab_NameSet(const ab_NameSet& other);
    ab_NameSet& operator=(const ab_NameSet& other);
};


/*=============================================================================
 * ab_Table: 
 */

class ab_Table /*d*/ : public ab_Model { /* ````` table ````` */
protected:
    AB_Table_eType      mTable_Type;
    
    ab_TableStoreView*  mTable_View; // store to table notification transmuter
    ab_Defaults*        mTable_Defaults;
    ab_NameSet*         mTable_NameSet;
    ab_ColumnSet*       mTable_ColumnSet;
    ab_RowSet*          mTable_RowSet;
    ab_RowContent*      mTable_RowContent;
    
    ab_bool             mTable_HasOwnColumnSet;

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // virtual ab_Table responses to ab_TableStoreView messages

	friend class ab_TableStoreView; // ab_TableStoreView calls these methods

	// `````  ab_View style interface mimics multiple inheritance `````
    virtual void TableSeesBeginStoreFlux(ab_Env* ev);
    virtual void TableSeesEndStoreFlux(ab_Env* ev);
    virtual void TableSeesChangedStore(ab_Env* ev, const ab_Change* inChange);
    virtual void TableSeesClosingStore(ab_Env* ev, const ab_Change* inChange);
	
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseTable()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Table();
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Table methods
    ab_Table(ab_Env* ev, const ab_Usage& inUsage,
	    ab_row_uid inRowUid, ab_Store* ioStore, AB_Table_eType inType);

    ab_Table(ab_Env* ev, const ab_Table& other, ab_RowSet* ioRowSet,
	    ab_row_uid inRowUid, AB_Table_eType inType);
    
    ab_bool TableHasAllSlots(ab_Env* ev) const;

    ab_row_uid FindRowWithDistName(ab_Env* ev, const char* inDistName);
    	// cover for ab_RowContent method with the same name
	    
	static ab_Table* MakeDefaultNameColumnTable(ab_Env* ev,
		ab_row_uid inRowUid,  ab_Store* ioStore, AB_Table_eType inType);

    const AB_Table* AsConstSelf() const { return (const AB_Table*) this; }
    AB_Table* AsSelf() { return (AB_Table*) this; }

    static ab_Table* AsThis(AB_Table* self) { return (ab_Table*) self; }
    static const ab_Table* AsConstThis(const AB_Table* self)
    { return (const ab_Table*) self; }
	    
    void CloseTable(ab_Env* ev); // CloseObject()
    
    /*AB_Table* MakeSimilarTable(ab_Env* ev,
	    ab_row_uid inRowUid, AB_Table_eType inType);*/

    const char* GetTableTypeAsString() const; // e.g. "list", etc.
    const char* GetColumnName(ab_Env* ev, ab_column_uid inColUid) const;
    
    AB_Table_eType TableType() const { return mTable_Type; }
    ab_bool TableHasOwnColumnSet() const { return mTable_HasOwnColumnSet; }
    
    ab_TableStoreView*  TableView() const   { return mTable_View; }
    
    ab_Defaults*    TableDefaults() const   { return mTable_Defaults; }
    ab_NameSet*     TableNameSet() const    { return mTable_NameSet; }
    ab_ColumnSet*   TableColumnSet() const  { return mTable_ColumnSet; }
    ab_RowSet*      TableRowSet() const     { return mTable_RowSet; }
    ab_RowContent*  TableRowContent() const { return mTable_RowContent; }
    
    ab_bool  SetTableDefaults(ab_Env* ev, ab_Defaults* ioDefaults);
    ab_bool  SetTableNameSet(ab_Env* ev, ab_NameSet* ioNameSet);
    ab_bool  SetTableColumnSet(ab_Env* ev, ab_ColumnSet* ioColumnSet);
    ab_bool  SetTableRowSet(ab_Env* ev, ab_RowSet* ioRowSet);
    ab_bool  SetTableRowContent(ab_Env* ev, ab_RowContent* ioRowContent);
    
    
// ````` ````` ````` `````   ````` ````` ````` `````  
private: // general copying is not allowed
    ab_Table(const ab_Table& other);
    ab_Table& operator=(const ab_Table& other);
};

/*=============================================================================
 * ab_ColumnSet: 
 */

class ab_ColumnSet /*d*/ : public ab_Part { /* ````` column collection ````` */

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_ColumnSet methods

	// all default implementations just raise error ab_Env_kFaultMethodStubOnly
	
    virtual ab_column_pos   AddColumn(ab_Env* ev, const AB_Column* col,
                ab_column_pos pos, ab_bool changeIndex);
                
    virtual ab_bool         PutColumn(ab_Env* ev, const AB_Column* col,
                ab_column_pos pos, ab_bool changeIndex);
                
    virtual ab_bool         GetColumn(ab_Env* ev, AB_Column* col, ab_column_pos pos);
                
    virtual ab_column_pos   CutColumn(ab_Env* ev, ab_column_uid inColUid,
			    ab_bool inDoChangeIndex);
                 
    virtual ab_column_pos   FindColumn(ab_Env* ev, const AB_Column* col);
               
    virtual ab_column_count GetAllColumns(ab_Env* ev, AB_Column* outVector,
                ab_column_count inSize, ab_column_count* outLength) const;
                
    virtual ab_column_count PutAllColumns(ab_Env* ev,
                const AB_Column* inVector, ab_bool changeIndex);
                
    virtual ab_column_count CountColumns(ab_Env* ev) const;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseColumnSet()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_ColumnSet();
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_ColumnSet methods
    ab_ColumnSet(ab_Env* ev, const ab_Usage& inUsage, ab_Store* ioStore);
    void CloseColumnSet(ab_Env* ev); // CloseObject()
    
private: // copying is not allowed
    ab_ColumnSet(const ab_ColumnSet& other);
    ab_ColumnSet& operator=(const ab_ColumnSet& other);
};


/*=============================================================================
 * ab_RowContent: 
 */

class ab_RowContent /*d*/ : public ab_Part { /* ````` row attributes ````` */

// ````` ````` ````` `````   ````` ````` ````` `````  
protected:
	ab_Table*       mRowContent_Table;
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_RowContent methods

    virtual ab_row_uid FindRowWithDistName(ab_Env* ev, const char* inDistName) = 0;
    virtual ab_row_uid FindRow(ab_Env* ev, const ab_Row* inRow) = 0; /* find */

    virtual ab_row_uid NewRow(ab_Env* ev, const ab_Row* inRow, ab_row_pos inRowPos) = 0;
    virtual ab_bool    ZapRow(ab_Env* ev, ab_row_uid inRowUid) = 0; /* destroy */

    virtual ab_row_uid CopyRow(ab_Env* ev, ab_RowContent* ioFromOther, ab_row_uid inRowUid) = 0;
    	// make a new row which is a copy of the row inRowUid inside ioFromOther.
    
    virtual ab_bool    GetAllCells(ab_Env* ev, ab_Row* outRow, ab_row_uid inRowUid, ab_cell_size inMaxCellSize) = 0;
    virtual ab_bool    GetCells(ab_Env* ev, ab_Row* outRow, ab_row_uid inRowUid) = 0;
    virtual ab_bool    PutAllCells(ab_Env* ev, const ab_Row* inRow, ab_row_uid inRowUid) = 0;
    virtual ab_bool    PutCells(ab_Env* ev, const ab_Row* inRow, ab_row_uid inRowUid) = 0;
    
    virtual ab_cell_count CountRowCells(ab_Env* ev, ab_row_uid inRowUid) const = 0;

    virtual ab_row_count  CountRowChildren(ab_Env* ev, ab_row_uid inRowUid) const = 0;
    virtual ab_RowSet*    AcquireRowChildren(ab_Env* ev, ab_row_uid inRowUid) = 0;
 	//virtual ab_RowSet*    AcquireRowListChildren(ab_Env* ev, ab_row_uid inRowUid) = 0;
   
    virtual ab_row_count  CountRowParents(ab_Env* ev, ab_row_uid inRowUid) const = 0;
    virtual ab_RowSet*    AcquireRowParents(ab_Env* ev, ab_row_uid inRowUid) = 0;


// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    // virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    // virtual void CloseObject(ab_Env* ev); // CloseRowContent()
    // virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_RowContent();
        
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_RowContentmethods
    ab_RowContent(ab_Env* ev, const ab_Usage& inUsage,
	    ab_Store* ioStore, ab_Table* ioTable);
    
    void CloseRowContent(ab_Env* ev);
    
	ab_bool    SetRowContentTable(ab_Env* ev, ab_Table* ioTable);

	ab_Table*  GetRowContentTable() const { return mRowContent_Table; }

    ab_row_pos FindRowInTableRowSet(ab_Env* ev, ab_row_uid inRowUid);
    	// calls FindRow() on mRowContent_Table->mTable_RowSet

private: // copying is not allowed
    ab_RowContent(const ab_RowContent& other);
    ab_RowContent& operator=(const ab_RowContent& other);
};

enum {
	ab_RowContent_kFaultBadMemberCut = /*i*/ AB_Fault_kRowContent,   /* 925 */
	ab_RowContent_kFaultNoTable,
	ab_RowContent_kFaultNullTable
};

/*=============================================================================
 * ab_RowSet: 
 */

class ab_RowSet /*d*/ : public ab_Part { /* ````` row collection ````` */

// ````` ````` ````` `````   ````` ````` ````` `````  
protected:
	ab_Table*       mRowSet_Table;
	ab_column_uid   mRowSet_SortColUid;
	ab_db_uid       mRowSet_SortDbUid;
	ab_bool         mRowSet_SortForward; // sort in ascending order

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // protected by convention
	
	void ChangeRowSetSortColumnAndDbUid(ab_column_uid col, ab_db_uid dbUid)
	{ mRowSet_SortColUid = col; mRowSet_SortDbUid = dbUid; }

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_RowSet methods

	// called by ab_Table::TableSeesChangedStore():
    virtual void RowSetSeesChangedStore(ab_Env* ev, const ab_Change* inChange);	
	    // this default version notifies a scramble through mRowSet_Table

	// AddAllRows() has a default implementation based on Pick() and Add()
    virtual ab_row_count AddAllRows(ab_Env* ev, const ab_RowSet* inRowSet);
     
	// ChangeRowSetSortForward() has default impl setting mRowSet_SortForward:
    virtual void          ChangeRowSetSortForward(ab_Env* ev, ab_bool inForward);

	// FindRowByPrefix() has default impl raising not-implemented error:
    virtual ab_row_uid    FindRowByPrefix(ab_Env* ev, const char* inPrefix,
                            const ab_column_uid inCol);
   
    // all the other row set methods are abstract and must be overridden
    virtual ab_row_pos    AddRow(ab_Env* ev, ab_row_uid inRow, ab_row_pos inPos) = 0;
    virtual ab_row_uid    GetRow(ab_Env* ev, ab_row_pos inPos) = 0;
    virtual ab_row_pos    FindRow(ab_Env* ev, ab_row_uid inRowUid) = 0;
    virtual ab_row_count  CountRows(ab_Env* ev) = 0;
    virtual ab_row_count  PickRows(ab_Env* ev, ab_row_uid* outRows,
                            ab_row_count inSize, ab_row_pos inPos) = 0;
 
    virtual ab_row_pos    CutRow(ab_Env* ev, ab_row_uid inRowUid) = 0;
	virtual ab_row_count  CutRowRange(ab_Env* ev, ab_row_pos inPos,
	                        ab_row_count inCount) = 0;
    
    virtual ab_RowSet*    AcquireListSubset(ab_Env* ev) = 0;
    virtual ab_RowSet*    AcquireSearchSubset(ab_Env* ev, const char* inValue,
                            const ab_column_uid* inColumnVector) = 0;

    virtual ab_bool       CanSortRows(ab_Env* ev, ab_column_uid inCol) const = 0;
    virtual ab_bool       SortRows(ab_Env* ev, ab_column_uid inSortCol) = 0;
    virtual ab_column_uid GetRowSortOrder(ab_Env* ev) = 0;
    
    virtual ab_RowSet*    NewSortClone(ab_Env* ev, ab_column_uid inSortCol) = 0;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    // virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    // virtual void CloseObject(ab_Env* ev); // CloseRowSet()
    // virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_RowSet();
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_RowSet methods
    ab_RowSet(ab_Env* ev, const ab_Usage& inUsage,
	    ab_row_uid inRowUid, ab_Store* ioStore, ab_Table* ioTable);
    
    void CloseRowSet(ab_Env* ev);
    
    // void UseSessionSortColumnDefault(ab_Env* ev);
    
	ab_bool         SetRowSetTable(ab_Env* ev, ab_Table* ioTable);
    
	ab_Table*       GetRowSetTable() const { return mRowSet_Table; }
	ab_column_uid   GetRowSetSortColumn() const { return mRowSet_SortColUid; }
	ab_db_uid       GetRowSetSortDbUid() const { return mRowSet_SortDbUid; }
	
	ab_bool         GetRowSetSortForward() const { return mRowSet_SortForward; }
	
	void            NotifyRowSetScramble(ab_Env* ev) const;
    
private: // copying is not allowed
    ab_RowSet(const ab_RowSet& other);
    ab_RowSet& operator=(const ab_RowSet& other);
};

enum {
	ab_RowSet_kFaultBadMemberCut = /*i*/ AB_Fault_kRowSet,   /* 675 */
	ab_RowSet_kFaultBadMemberAdd,
	ab_RowSet_kFaultCannotSearchRows,
	ab_RowSet_kFaultNotSortableColumn,
	ab_RowSet_kFaultUnsorted,
	ab_RowSet_kFaultNoSearch,
	ab_RowSet_kFaultNullSearch,
	ab_RowSet_kFaultNullSearchString,
	ab_RowSet_kFaultNoParent,
	ab_RowSet_kFaultNullParent,
	ab_RowSet_kFaultNoTable,
	ab_RowSet_kFaultNullTable,
	ab_RowSet_kFaultNotOpen,
	ab_RowSet_kFaultCannotEditRows,
	ab_RowSet_kFaultCannotFindListRows,
	ab_RowSet_kFaultNoArrayPosFound,
	ab_RowSet_kFaultForwardBackwardOrder,
	ab_RowSet_kFaultCannotSortRows
};




/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ===== ===== ===== ===== ab_Session ===== ===== ===== ===== */

/* There should typically be only one ab_Session instance in use, which is
** the one returned from ab_Session::GetGlobalSession().
*/

#define ab_Session_kTag /*i*/ 0x7345734E /* ascii 'sEsN' (no endian) */

class ab_Session /*d*/ {
protected:
    ab_u4          mSession_Tag;   /* equals ab_Session_kTag */
    AB_Table*      mSession_GlobalTable; /* table for AB_GetGlobalTable() */
    ab_uid         mSession_TempRowSeed;
    ab_uid         mSession_TempChangeSeed;
    ab_column_uid  mSession_DefaultSortColumn;

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_Session methods

	ab_Session(); // does nothing
		// initialization is separated from construction so platforms
		// with poor static construction support will not be a problem.
		
	void InitSession(); // actually constructs the session

    ab_bool    GoodTag() const { return mSession_Tag == ab_Session_kTag; }
    AB_Table*  GlobalTable() const { return mSession_GlobalTable; }
    
    ab_change_uid NewTempChangeUid()
    { return ++mSession_TempChangeSeed; }
 
	ab_row_uid    NewTempRowUid()
	{ return AB_UidSeed_AsTempRowUid(++mSession_TempRowSeed); }
   
    static ab_Session* GetGlobalSession();
    
    // obviously default sort cols must be standard (and not store specific)
    ab_column_uid DefaultSortColumn() const
    { return mSession_DefaultSortColumn ; }
    
    void          SetDefaultSortColumn(ab_Env* ev, ab_column_uid newSortCol);
};

/*=============================================================================
 * AB_Cell: 
 */

/* `````` `````` protos `````` `````` */
AB_BEGIN_C_PROTOS
/* `````` `````` protos `````` `````` */

/*| Init: initialize and construct the cell so sCell_Size is at
**| least inMinSize bytes in size, and contains the content pointed
**| to by inStartingContent when inStartingContent is non-null.
**| If inStartingContent is null, then zero the initial content.
**| The inColumnUid arg cannot be zero or an error will occur; in
**| fact, inColumnUid must return true from AB_Uid_IsColumn().
**|
**|| If strlen(inStartingContent)+1 is greater than inMinSize, then
**| make a cell that size instead (the plus one is for a null byte).  If
**| both inMinSize and the inStartingContent pointer are zero, allocate
**| at least a byte to avoid weird edge cases.
**| True is returned if and only if the environment shows no errors.
|*/
extern ab_bool /* abcell.c */
AB_Cell_Init(AB_Cell* self, ab_Env* ev, ab_column_uid inColumnUid,
    ab_cell_size inMinSize, const char* inStartingContent);

/*| Finalize: deallocate sCell_Content and zero all slots.
**| True is returned if and only if the environment shows no errors.
|*/
extern ab_bool /* abcell.c */
AB_Cell_Finalize(AB_Cell* self, ab_Env* ev);

/*| Grow: increase the cell size sCell_Size to at least inMinSize
**| and reallocate the sCell_Content space if necessary.  But if
**| sCell_Size is already that big, then do nothing.
**| True is returned if and only if the environment shows no errors.
|*/
extern ab_bool /* abcell.c */
AB_Cell_Grow(AB_Cell* self, ab_Env* ev, ab_cell_size inMinSize);

/*| Become: duplicate all slots in other exactly, except the sCell_Content
**| pointer must remain different, of course.  This means sCell_Content
**| almost certainly is reallocated when sCell_Capacity is different.
**| Afterwards self has the same structure and content as other.
**|
**|| This self cell need not start in an initialized state -- it is okay if
**| the cell is finalized, with all slots equal to zero.  In particular, if
**| the content slot is null this causes no problem and no attempt is made
**| to deallocate the null pointer.
**| (This method is intended to support AB_Row_BecomeRowClone().)
**| True is returned if and only if the environment shows no errors.
|*/
#if 0
extern ab_bool /* abcell.c */
AB_Cell_Become(AB_Cell* self, ab_Env* ev, const AB_Cell* other);
#endif

/*| Copy: make this cell contain the same content as other when capacity
**| is greater than other's content.  Otherwise make self hold as much of
**| other's content as will fit without making self any larger.  (This
**| method is intended to support AB_Tuple_CopyRowContent().)
**| True is returned if and only if the environment shows no errors.
|*/
extern ab_bool /* abcell.c */
AB_Cell_Copy(AB_Cell* self, ab_Env* ev, const AB_Cell* other);


extern void /* abrow.c */
AB_Cell_Print(const AB_Cell* self, ab_Env* ev,
	ab_Printer* ioPrinter, const char* inName);
	
#define AB_Cell_kXmlBufSize /*i*/ (256+64) /* size for AB_Cell_AsString() */
	
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	extern char* /* abcell.c */
	AB_Cell_AsString(const AB_Cell* self, ab_Env* ev, char* outBuf);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#ifdef AB_CONFIG_TRACE
	extern void /* abcell.c */
	AB_Cell_Trace(const AB_Cell* self, ab_Env* ev);
#endif /*AB_CONFIG_TRACE*/

#ifdef AB_CONFIG_DEBUG
	extern void /* abcell.c */
	AB_Cell_Break(const AB_Cell* self, ab_Env* ev);
#endif /*AB_CONFIG_DEBUG*/

extern void /* abcell.c */
AB_Cell_TraceAndBreak(const AB_Cell* self, ab_Env* ev);

extern short /* abcell.c */
AB_Cell_AsShort(const AB_Cell* self, ab_Env* ev);

extern ab_bool /* abcell.c */
AB_Cell_WriteHexLong(AB_Cell* self, ab_Env* ev, long inLong);

extern ab_bool /* abcell.c */
AB_Cell_WriteShort(AB_Cell* self, ab_Env* ev, short inShort);

extern ab_bool /* abcell.c */
AB_Cell_WriteBool(AB_Cell* self, ab_Env* ev, ab_bool inBool);

#define AB_Cell_AsBool(self) /*i*/ (*(self)->sCell_Content == 't')


/* `````` `````` protos `````` `````` */
AB_END_C_PROTOS
/* `````` `````` protos `````` `````` */

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ===== ===== ===== ===== ab_String ===== ===== ===== ===== */

#define ab_String_kMaxCapacity (48 * 1024)

class ab_String /*d*/ : public ab_Object {
            
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected member variables

	char*    mString_Content;     // always non-null, even when closed
    ab_num   mString_Capacity;    // number of bytes in string space
    ab_num   mString_Length;      // non-null byte count before null terminator
    ab_bool  mString_IsHeapBased; // must free when closed or content changes
    ab_bool  mString_IsReadOnly;  // cannot modify string
    
    // IsHeapBased and IsReadOnly are typically just opposites, but we have
    // both in case the evolving implementation needs to distinguish these
    // conditions independently.

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseString()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_String();
            
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected non-poly ab_String methods

	ab_bool cut_string_content(ab_Env* ev);
	ab_bool grow_string_content(ab_Env* ev, ab_num inNewCapacity);

#ifdef AB_CONFIG_DEBUG
	void fit_string_content(ab_Env* ev);
#endif /*AB_CONFIG_DEBUG*/
            
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly ab_String methods

    ab_String(ab_Env* ev, const ab_Usage& inUsage, const char* inCopiedString);
    	// Make a heap based copy of inCopiedString, unless inCopiedString is
    	// a nil pointer.  If inCopiedString is a nil pointer, then
    	// mString_Content gets an empty static string, mString_IsReadOnly
    	// gets true, and mString_IsHeapBased gets false.  To construct a new
    	// string that uses a static string, the best approach is to pass a nil
    	// pointer for inCopiedString, and then call TakeStaticStringContent().
    
    ab_bool TakeStaticStringContent(ab_Env* ev, const char* inStaticString);
    	// Use this readonly, non-heap string instead for mString_Content.
    	// Of course this involves casting away const.  Note the content
    	// string should never be written other than using supplied methods,
    	// and writing a readonly string will involve first replacing the
    	// readonly copy with a heap allocated mutable copy before changes.
    	// The return value equals ev->Good().
	    
    void CloseString(ab_Env* ev); // CloseObject()
    	// Nearly equivalent to calling TakeStaticStringContent(ev, "");
	    
    ab_bool TruncateStringLength(ab_Env* ev, ab_num inNewLength);
    	// Make the string have length inNewLength if this is less than
    	// the current length.  The typical expected new length is zero.
    	// The capacity of the string is *not* changed, and this is the
    	// principle difference between TruncateStringLength() and cutting
    	// content with CutStringContent() which might resize the string,
    	// and especially does so for empty strings.  TruncateStringLength()
    	// is intended to be useful for rewriting strings from scratch without
    	// performing any memory allocation that can be avoided. This lets a
    	// string buffer reach a high water mark and stay stable for a while.
    	// The string should not be a readonly static string if nonempty.
    	// The return value equals ev->Good().

	// ````` ````` sensible API features currently unused  ````` `````
#ifdef AB_CONFIG_MAXIMIZE_FEATURES    
    ab_bool SetStringContent(ab_Env* ev, const char* inString);
    	// Make the new string's content equal to inString, with length
    	// strlen(inString).  If this fits inside the old capacity and the
    	// string is mutable, then mString_Content is modified; otherwise
    	// mString_Content is freed and a new heap-based string is allocated.
    	// The return value equals ev->Good().
    
    ab_bool PutStringContent(ab_Env* ev, const char* inString, ab_pos inPos);
    	// Overwrite the string starting at offset inPos.  An error occurs if
    	// inPos is not a value from zero up to and including mString_Length.
    	// When inPos equals the length, this operation appends to the string.
    	// If the string is readonly or if the string must be longer to hold
    	// the new length of the string (plus null terminator), then the
    	// string is grown by replacing mString_Content with bigger space.
      	// The return value equals ev->Good().
    
    ab_bool CutStringContent(ab_Env* ev, ab_pos inPos, ab_num inCount);
    	// Remove inCount bytes of content from the string starting at offset
    	// inPos, where inPos must be a value from zero up to the length of
    	// the string.  inCount can be any value, since any excess simply
    	// ensures that all content through the end of the string will be cut.
    	// So CutStringContent(ev, 0, 0xFFFFFFFF) is roughly equivalent to
    	// SetStringContent(ev, "");
    	// The return value equals ev->Good().
#endif /*AB_CONFIG_MAXIMIZE_FEATURES*/
    
    ab_bool PutBlockContent(ab_Env* ev, const char* inBytes, ab_num inSize,
	    ab_pos inPos);
    	// PutBlockContent() is the same as PutStringContent() except that
		// inSize is used as the string length instead of finding any ending
		// null byte.  This is very useful when it is inconvenient to put a
    	// null byte into an input string (which might be readonly) in order
    	// to end the string.  While this interface allows the input string to
    	// contain null bytes, this is *still a very bad idea* because ab_String
    	// cannot correctly handle embedded null bytes.  You've been warned.
    	//
    	// Overwrite the string starting at offset inPos.  An error occurs if
    	// inPos is not a value from zero up to and including mString_Length.
    	// When inPos equals the length, this operation appends to the string.
    	// If the string is readonly or if the string must be longer to hold
    	// the new length of the string (plus null terminator), then the
    	// string is grown by replacing mString_Content with bigger space.
      	// The return value equals ev->Good().
  
    ab_bool AddStringContent(ab_Env* ev, const char* inString, ab_pos inPos);
    	// Insert new content starting at offset inPos.  An error occurs if
    	// inPos is not a value from zero up to and including mString_Length.
    	// When inPos equals the length, this operation appends to the string.
    	// If the string is readonly or if the string must be longer to hold
    	// the new length of the string (plus null terminator), then the
    	// string is grown by replacing mString_Content with bigger space.
    	// The return value equals ev->Good().
    	
    ab_bool AppendStringContent(ab_Env* ev, const char* inString)
    { return this->AddStringContent(ev, inString, mString_Length); }

	// ````` ````` accessors  ````` `````
	const char* GetStringContent() const { return mString_Content; }
    ab_num      GetStringCapacity() const { return mString_Capacity; }
    ab_num      GetStringLength() const { return mString_Length; }
    ab_bool     IsStringHeapBased() const { return mString_IsHeapBased; }
    ab_bool     IsStringReadOnly() const { return mString_IsReadOnly; }
    ab_bool     IsStringMutable() const { return !mString_IsReadOnly; }
    
private: // copying is not allowed (mimicing a native type is not the intent)
    ab_String(const ab_String& other);
    ab_String& operator=(const ab_String& other);
};

enum ab_String_eError {
    ab_String_kFaultLengthExceedsSize = /*i*/ AB_Fault_kString,
    ab_String_kFaultLengthOutOfSync,
    ab_String_kFaultOverMaxCapacity,
    ab_String_kFaultPosAfterLength,
    ab_String_kFaultBadCursorFields, // garbled slots in string related object
    ab_String_kFaultBadCursorOrder   // bad pointer order in string cursor(s)
};

/* ===== ===== ===== ===== ab_StringFile ===== ===== ===== ===== */

/*| ab_StringFile is a ab_File subclass based on ab_String.  Note that
**| ab_String is not suitable for containing null bytes, so this restricts
**| the content that can be written to this file (don't write null bytes).
|*/

class ab_StringFile /*d*/ : public ab_File { 

// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected ab_StringFile members

	ab_String*  mStringFile_String;  // the content of the "file"
	ab_pos      mStringFile_Pos;     // current file position

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_File methods

	virtual ab_pos  Length(ab_Env* ev) const;
	virtual ab_pos  Tell(ab_Env* ev) const;
	virtual ab_num  Read(ab_Env* ev, void* outBuf, ab_num inSize);
	virtual ab_pos  Seek(ab_Env* ev, ab_pos inPos);
	virtual ab_num  Write(ab_Env* ev, const void* inBuf, ab_num inSize);
	virtual void    Flush(ab_Env* ev);

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseStringFile()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_StringFile();
    
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_StringFile methods
    ab_StringFile(ab_Env* ev, const ab_Usage& inUsage, ab_String* ioString);
    	
    ab_StringFile(ab_Env* ev, const ab_Usage& inUsage, const char* inBytes);
    	// create a readonly string based on inBytes.
    
    void CloseStringFile(ab_Env* ev);

	ab_String*  GetStringFileString() const { return mStringFile_String; }
		// You need to acquire this string if you hold a long term reference.
		// In particular, this file releases the string when the file closes.

// ````` ````` ````` `````   ````` ````` ````` `````  
private: // copying is not allowed
    ab_StringFile(const ab_StringFile& other);
    ab_StringFile& operator=(const ab_StringFile& other);
};

/* ===== ===== ===== ===== ab_StringSink ===== ===== ===== ===== */

/*| ab_StringSink is intended to be a very cheap buffered i/o sink
**| which writes to a ab_String a single byte at a time.  (Roughly the same
**| functionality is also provided by subclassing ab_File using a
**| ab_String instance to hold the "file" content, and then using an instance
**| of ab_Stream to buffer the i/o written to the string.)
**|
**|| The ab_String passed to the constructor is *not* reference counted.  (A
**| more heavy-weight implementation would refcount this string.)  Instances
**| of ab_StringSink are intended to be suitable for creation on the stack.
**| The implementation of ab_String assumes that null bytes are not written,
**| so we don't bother to buffer any attempts to write null bytes.
|*/

#define ab_StringSink_kBufSize 256 /* small enough to go on stack */

class ab_StringSink /*d*/ {

protected: // member variables (put At first for possible speed improvement)

	ab_u1*     mStringSink_At;     // pointer into mStringSink_Buf
	ab_u1*     mStringSink_End;    // one byte past last content byte

	ab_String* mStringSink_String; // where all the bytes are written

	ab_u1      mStringSink_Buf[ ab_StringSink_kBufSize + 2 ];
		// need at least plus one to hold terminating null byte
    
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected non-poly ab_StringSink methods (for char io)

	void    sink_spill_putc(ab_Env* ev, int c); /* abstring.cpp */
	
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly ab_StringSink methods

	ab_StringSink(ab_Env* ev, ab_String* ioString); /* abstring.cpp */

	void    FlushStringSink(ab_Env* ev); /* abstring.cpp */

	void    Putc(ab_Env* ev, int c) /*i*/
	{ 
		if ( c && mStringSink_At < mStringSink_End )
			*mStringSink_At++ = c;
		else
			this->sink_spill_putc(ev, c);
	}

};

	
/* ===== ===== ===== ===== ab_Search ===== ===== ===== ===== */

#define ab_Search_kImmedColCount 8
#define ab_Search_kMaxCapacity 256 /* mainly for sanity checking */

class ab_Search /*d*/ : public ab_Part {
            
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected member variables

	ab_Row*          mSearch_Row; // optional row for finding exact matches
	ab_u4            mSearch_Seed; // might be used for synchronization

	ab_column_count  mSearch_Capacity;  // size capacity of string & col arrays
	ab_column_count  mSearch_ColumnCount; // number of used search columns
	
	ab_column_uid*   mSearch_Columns; // might point to mSearch_ImmedCols
	ab_column_uid    mSearch_ImmedCols[ ab_Search_kImmedColCount ];
	
	ab_String**      mSearch_Strings; // might point to mSearch_ImmedStrings
	ab_String*       mSearch_ImmedStrings[ ab_Search_kImmedColCount ];

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // virtual ab_Object methods
    virtual char* ObjectAsString(ab_Env* ev, char* outXmlBuf) const;
    virtual void CloseObject(ab_Env* ev); // CloseSearch()
    virtual void PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const;
    virtual ~ab_Search();
            
// ````` ````` ````` `````   ````` ````` ````` `````  
protected: // protected non-poly ab_Search methods

	void     CutSearchStorageSpace(ab_Env* ev);
	ab_bool  GrowSearchCapacity(ab_Env* ev, ab_column_count inNewCapacity);
	ab_bool  InitCapacity(ab_Env* ev, ab_column_count inCapacity);
            
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public non-poly ab_Search methods

    ab_Search(ab_Env* ev, const ab_Usage& inUsage, ab_Store* ioStore, 
    	const char* inStringValue, const ab_column_uid* inColUidVector,
    	ab_column_count inCapacity);
    	// inColUidVector is null terminated, so use the number of non-null
    	// uid's as instead of inCapacity if this number is larger (so a
    	// zero value can be passed for  inCapacity when desired).   Use
    	// the same ab_String made from inStringValue for all strings.

    ab_Search(ab_Env* ev, const ab_Usage& inUsage, ab_Store* ioStore,
	    ab_column_count inCapacity);
    	// expect to have inColHint columns added with AddSearchString()
	    
    void CloseSearch(ab_Env* ev); // CloseObject()
    
    ab_bool CutAllSearchStrings(ab_Env* ev);
	     // return value equals ev->Good()
   
    ab_bool AddSearchString(ab_Env* ev, ab_String* ioString,
	     ab_column_uid inColUid);
	     // return value equals ev->Good()

	// ````` ````` accessors ````` `````
	void    BumpSearchSeed() { ++mSearch_Seed; }
	ab_num  GetSearchSeed() const { return mSearch_Seed; }
	
	ab_column_count  GetSearchColumnCapacity() const
	{ return mSearch_Capacity; }

	ab_column_count  GetSearchColumnCount() const 
	{ return mSearch_ColumnCount; }

	ab_bool ChangeSearchRow(ab_Env* ev, ab_Row* ioRow);
		// note that passing in a null pointer for ioRow is allowed.
	ab_Row* GetSearchRow(ab_Env* ev) const;
	
	ab_String* GetStringAt(ab_Env* ev, ab_pos inPos,
		ab_column_uid* outColUid) const;
		// inPos is a zero-based index from zero to GetSearchColumnCount()-1.
    
private: // copying is not allowed
    ab_Search(const ab_Search& other);
    ab_Search& operator=(const ab_Search& other);
};

enum ab_Search_eError {
    ab_Search_kFaultNotOpen = /*i*/ AB_Fault_kSearch,
    ab_Search_kFaultOverMaxCapacity,
    ab_Search_kFaultMissingArray,
    ab_Search_kFaultUnsortedColumn
};


#endif /* _ABMODEL_ */
