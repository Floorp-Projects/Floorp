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

/* file: abtable.h 
** Some portions derive from public domain IronDoc code and interfaces.
**
** This is the public address book interface that should be included by
** clients that want to access address book content without knowing any
** more details than necessary about the internal implementation.  This
** interface deliberately hides details concerning how this database
** independent layer can be targeted by third parties by subclassing.
**
** The private address book interface is specified in abmodel.h, which
** defines the interfaces of classes that allow third parties to target
** the address book interface by subclassing appropriate classes in the
** address book model.  The private interface defines the structure and
** polymorphic vtables (if needed) for any classes in the public interface
** whose structure is hidden.  If the public interface is constrained to
** call "factory" methods to construct and generate objects needed to
** access address book content, then under the covers these objects might
** be dynamically bound subclasses appropriate for use in a given model. 
**
** Changes:
** <0> 22Oct1997 first draft
*/

#ifndef _ABTABLE_
#define _ABTABLE_ 1

/* ----- ----- ----- ----- switches ----- ----- ----- ----- */

#define AB_CONFIG_MACINTOSH /*i*/  1

/* #define AB_CONFIG_UNIX  1 */ /*i*/
/* #define AB_CONFIG_WINDOWS  1 */ /*i*/

/*#define AB_CONFIG_LOGGING 1 */

/* #define AB_CONFIG_MOZ_NEWADDR 1 */ /*i*/

#ifdef AB_CONFIG_MOZ_NEWADDR
#define AB_CONFIG_SMALLER_INDEXES 1 /* use new smaller indexes */
#define AB_CONFIG_FIRST_LAST_INDEXES 1 /* use new name indexes */
#define AB_CONFIG_TUPLE_ENTRY 1 /* use new tuple entry class */
#endif /*AB_CONFIG_MOZ_NEWADDR*/

#define AB_CONFIG_USE_NEO_CACHE_TABLES 1 /*i*/

#define AB_CONFIG_TRUNCATE_FULL_NAME 1 /* force rather short */

/*#define AB_CONFIG_ENABLE_INDEX_REFRESH 1*/ /* allow */

#define AB_CONFIG_DEBUG /*i*/ 1

#ifdef AB_CONFIG_DEBUG
#define AB_CONFIG_PRINT /*i*/ 1
#endif

#ifdef AB_CONFIG_DEBUG
#define AB_CONFIG_TRACE /*i*/ 1
#endif

#ifdef AB_CONFIG_DEBUG
#define AB_CONFIG_MAXIMIZE_FEATURES /*i*/ 1
#endif

#ifdef AB_CONFIG_TRACE
#define AB_CONFIG_TRACE_CALL_TREE /*i*/ 1
#else
#define AB_CONFIG_TRACE_CALL_TREE 0
#endif

#if defined(AB_CONFIG_TRACE) || defined(AB_CONFIG_DEBUG) \
    || defined(AB_CONFIG_PRINT)
#define AB_CONFIG_TRACE_orDEBUG_orPRINT /*i*/ 1
#else
#define AB_CONFIG_TRACE_orDEBUG_orPRINT 0
#endif

#if defined(AB_CONFIG_TRACE) || defined(AB_CONFIG_LOGGING)
#define AB_CONFIG_TRACE_andLOGGING /*i*/ 1
#else
#define AB_CONFIG_TRACE_andLOGGING 0
#endif

#ifdef AB_CONFIG_DEBUG
#define AB_CONFIG_KNOW_FAULT_STRINGS 1
#endif

/* ----- ----- ----- disable unused param warnings ----- ----- ----- */

#define AB_USED_PARAMS_1(x) (void)(&x)
#define AB_USED_PARAMS_2(x,y) (void)(&x,&y)
#define AB_USED_PARAMS_3(x,y,z) (void)(&x,&y,&z)
#define AB_USED_PARAMS_4(w,x,y,z) (void)(&w,&x,&y,&z)

/* ----- ----- ----- assertions ----- ----- ----- */

#define AB_ASSERT(x) XP_ASSERT(x)

/* ----- ----- ----- utilities ----- ----- ----- */

#define AB_MEMCPY(dest,src,size)   XP_MEMCPY(dest,src,size)
#define AB_MEMMOVE(dest,src,size)  XP_MEMMOVE(dest,src,size)
#define AB_MEMSET(dest,byte,size)  XP_MEMSET(dest,byte,size)
#define AB_STRCPY(dest,src)        XP_STRCPY(dest,src)
#define AB_STRCAT(dest,src)        XP_STRCAT(dest,src)
#define AB_STRCMP(one,two)         XP_STRCMP(one,two)
#define AB_STRNCMP(one,two,length) XP_STRNCMP(one,two,length)
#define AB_STRLEN(string)          XP_STRLEN(string)

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/
/* ----- ----- ----- ----- signatures ----- ----- ----- ----- */

#include "xp_core.h"
#include "xp.h"

#if defined(XP_CPLUSPLUS)
# define AB_BEGIN_C_PROTOS extern "C" {
# define AB_END_C_PROTOS }
#else
# define AB_BEGIN_C_PROTOS
# define AB_END_C_PROTOS
#endif

#define AB_PUBLIC_API(returnType) /*i*/ returnType
    /* API() wraps return types of header file library public methods  */

#define AB_API_IMPL(returnType) returnType
    /* API_IMPL() wraps return types of .c file library public methods  */

#define AB_MODEL(returnType) /*i*/ returnType
    /* MODEL() wraps return types of header file semi-private model methods  */

#define AB_MODEL_IMPL(returnType) returnType
    /* MODEL_IMPL() wraps return types of .c file semi-private model methods  */
    
#define AB_LIB(returnType) /*i*/ returnType
    /* LIB() wraps return types of header file library private methods  */

#define AB_LIB_IMPL(returnType) /*i*/ returnType
    /* LIB_IMPL() wraps return types of .c file library private methods  */

#define AB_FILE_IMPL(returnType) static returnType
    /* FILE_IMPL() wraps return types of .c file scoped methods  */

/* ----- ----- ----- ----- primitives ----- ----- ----- ----- */

#ifdef AB_BOOL_STAND_ALONE

typedef unsigned char  ab_bool /*d*/; /* any nonzero value implies true */
#define AB_kFalse /*i*/ 0
#define AB_kTrue  /*i*/ 1

#else /* else ! AB_BOOL_STAND_ALONE ------- */

#include "xp_core.h"
typedef XP_Bool ab_bool; /* boolean */
#define AB_kFalse FALSE
#define AB_kTrue  TRUE

#endif /* end AB_BOOL_STAND_ALONE ------- */

typedef unsigned char  ab_u1 /*d*/;  /* unsigned 1 byte */

typedef short          ab_i2 /*d*/;  /* signed 2 bytes */
typedef unsigned short ab_u2 /*d*/;  /* unsigned 2 bytes */

typedef long           ab_i4 /*d*/;  /* signed 4 bytes */
typedef unsigned long  ab_u4 /*d*/;  /* unsigned 4 bytes */

typedef ab_u4          ab_num /*d*/;  /* a "count" with many bits */
typedef ab_u4          ab_pos /*d*/;  /* an index "position" with many bits */
typedef ab_u4          ab_uid /*d*/;  /* a unique id with many bits */

typedef ab_i4  ab_map_int /*d*/; /* size must be at least sizeof(void*) */
typedef ab_u4  ab_policy /*d*/; /* generally to hold policy enum values */

/* ----- ----- ----- ----- forwards ----- ----- ----- ----- */

#ifndef AB_Fault_typedef
typedef struct AB_Fault AB_Fault;
#define AB_Fault_typedef 1
#endif

#ifndef AB_PosPair_typedef
typedef struct AB_PosPair AB_PosPair;
#define AB_PosPair_typedef 1
#endif

#ifndef AB_PosRange_typedef
typedef struct AB_PosRange AB_PosRange;
#define AB_PosRange_typedef 1
#endif

#ifndef AB_Env_typedef
typedef struct AB_Env AB_Env;
#define AB_Env_typedef 1
#endif

#ifndef AB_Sink_typedef
typedef struct AB_Sink AB_Sink;
#define AB_Sink_typedef 1
#endif

#ifndef AB_Debugger_typedef
typedef struct AB_Debugger AB_Debugger;
#define AB_Debugger_typedef 1
#endif

#ifndef AB_Tracer_typedef
typedef struct AB_Tracer AB_Tracer;
#define AB_Tracer_typedef 1
#endif

#ifndef AB_Table_typedef
typedef struct AB_Table AB_Table;
#define AB_Table_typedef 1
#endif

#ifndef AB_AsyncTask_typedef
typedef struct AB_AsyncTask AB_AsyncTask;
#define AB_AsyncTask_typedef 1
#endif

#ifndef AB_AsyncResult_typedef 
typedef struct AB_AsyncResult AB_AsyncResult;
#define AB_AsyncResult_typedef 1
#endif

#ifndef AB_Column_typedef
typedef struct AB_Column AB_Column;
#define AB_Column_typedef 1
#endif

#ifndef AB_Row_typedef
typedef struct AB_Row AB_Row;
#define AB_Row_typedef 1
#endif

#ifndef AB_Cell_typedef
typedef struct AB_Cell AB_Cell;
#define AB_Cell_typedef 1
#endif

#ifndef AB_Store_typedef
typedef struct AB_Store AB_Store;
#define AB_Store_typedef 1
#endif

#ifndef AB_Thumb_typedef
typedef struct AB_Thumb AB_Thumb;
#define AB_Thumb_typedef 1
#endif

#ifndef AB_File_typedef
typedef struct AB_File AB_File;
#define AB_File_typedef 1
#endif

#ifndef DIR_Server_typedef
typedef struct DIR_Server DIR_Server;
#define DIR_Server_typedef 1
#endif

/* ----- ----- ----- ----- types ----- ----- ----- ----- */

typedef ab_u4 ab_ref_count; /* counts times any object is acquired */
    /*- This reference count is needed for tables and might be used by other
    runtime objects as well. A reference count is a runtime session effect and
    not a persistent value. Nothing stored persistently in a database implies
    anything about runtime reference counts. -*/

#define AB_Fault_kNone 0 /* zero means no error */
    /*- No error unique id uses the value of zero. -*/

typedef ab_i4 ab_error_uid; /* unique id for an error */
    /*- Unique id for an error specific to address books.
    Zero means no error. -*/

typedef ab_num ab_error_count; /* numeric count of errors */
    /*- Type that clearly counts errors.  -*/

typedef ab_num ab_count;     /* numeric count of something */
typedef ab_num ab_uid_count; /* numeric count of uids */
typedef ab_pos ab_uid_pos;   /* array index for individual uid */

typedef ab_uid ab_db_uid; /* unique id from a database */
    /*- A db uid is a unique id generated by a database, which need not
    follow any conventions used by address books.  The unique ids used by
    address books for rows and columns are constructed from database uids
    by bitshifting the db uids and ORing in tag bits that distinguish
    various kinds of address book unique ids.  This db uid type is intended
    to explicitly indicate that tag bits are not present as understood by
    address books, and that bitshifting and other transformations are needed
    to convert between db uids and other kinds of uids. -*/

typedef ab_db_uid ab_db_index_uid; /* unique id for db index */
typedef ab_db_uid ab_db_name_uid; /* unique id for db name token */
typedef ab_db_uid ab_db_set_uid; /* unique id for db set of things */
typedef ab_db_set_uid ab_db_list_set_uid; /* uid for db set of lists */
typedef ab_db_uid ab_db_row_uid; /* unique id for db row */
typedef ab_db_row_uid ab_db_parent_uid; /* uid for parent db row */
typedef ab_db_row_uid ab_db_child_uid; /* uid for child db row */

typedef ab_db_uid ab_db_list_uid; /* unique id for db row with children */
typedef ab_num ab_list_count; /* numeric count of lists */
typedef ab_pos ab_list_pos;   /* array index for individual list */

typedef ab_uid ab_row_uid; /* unique id for a row */
typedef ab_uid ab_column_uid; /* unique id for a column */
    /*- Row and column unique ids have address book scope, so all tables
    inside a single address book share the same unique id name space for
    rows and columns. There need not be a unique id type for tables
    because all tables are alwo rows.  -*/

typedef ab_uid ab_model_uid; /* unique id for a model */
typedef ab_uid ab_store_uid; /* unique id for a store */

typedef ab_row_uid ab_table_uid; /* unique id for a table */
typedef ab_table_uid ab_container_uid; /* unique id for database container */

typedef ab_num ab_row_count; /* numeric count of rows */
typedef ab_num ab_column_count; /* numeric count of columns */
    /*- These types help clarify when integer values are intended to count
    differnt kinds of objects.  -*/

typedef ab_pos ab_row_pos;    /* array index for individual row */
typedef ab_pos ab_column_pos; /* array index for individual column */
typedef ab_pos ab_cell_pos;   /* array index for individual cell */
    /*- These types help clarify when integer values are intended to specifiy 
    an array position within an ordered sequence of rows, columns, or cells.
    Currently row and column positions are one-based so zero can be
    reserved to mean an exceptional condition.  -*/

/* `````` `````` protos `````` `````` */
AB_BEGIN_C_PROTOS
/* `````` `````` protos `````` `````` */

/* ----- ----- ----- ----- position sets ----- ----- ----- ----- */

struct AB_PosPair /*d*/ {
    ab_pos    sPosPair_First; /* first position in a sequence */
    ab_pos    sPosPair_Last;  /* last position in a sequence */
};


#if AB_CONFIG_TRACE_orDEBUG_orPRINT
AB_PUBLIC_API(char*) /* abtable.cpp */
AB_PosPair_AsXmlString(const AB_PosPair* self, char* outXmlBuf);
  /* <ab:pos:pair first="%lu" last="%lu"/> */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/

struct AB_PosRange /*d*/ {
    ab_pos    sPosRange_First;  /* first position in a sequence */
    ab_count  sPosRange_Count;  /* number of positions in sequence */
};

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
AB_PUBLIC_API(char*) /* abtable.cpp */
AB_PosRange_AsXmlString(const AB_PosRange* self, char* outXmlBuf);
  /* <ab:pos:range first="%lu" count="%lu"/> */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/

/* ----- ----- ----- ----- Unique Ids ----- ----- ----- ----- */

#define AB_Bit_kTempBit     ( 1 << 0 ) /* 2^0 bit: not persistent */
#define AB_Bit_kGlobalBit   ( 1 << 1 ) /* 2^1 bit: global if also temp */
#define AB_Bit_kColumnBit   ( 1 << 1 ) /* 2^1 bit: column if also persistent */
#define AB_Bit_kStandardBit ( 1 << 2 ) /* 2^2 bit: uid is standard constant */
#define AB_Uid_kGlobal      ( AB_Bit_kGlobalBit | AB_Bit_kTempBit )
#define AB_Uid_kColumn      AB_Bit_kColumnBit
#define AB_Uid_kPersistentRow 0
#define AB_Uid_kStandardColumn   ( AB_Bit_kColumnBit | AB_Bit_kStandardBit )
#define AB_Uid_kTagBitCount     3
#define AB_Bit_kLowTwoBits    0x3
#define AB_Bit_kLowThreeBits  0x7
    /*- These constants define bit patterns used to encode unique ids to
    determine the purpose of a specific uids. Unique ids which are only
    meaningful during a single session are called temporary or transient
    (as opposed to persistent). Transient uids are to global tables and to
    tables generated dynamically to describe the results of queries or
    searches.

    The column bit and the global bit are the same bit because there is no
    overlap in practical usage, because columns are never transient, and
    globals are always transient. (We want to use as few bits for encoding as
    possible because this reserves most of the bits for a larger name space
    for uids. Using only two bits leaves 30 bits for 2^30 distinct persistent
    ids in a db.)  -*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#define AB_Uid_IsTransient(uid)  ((uid) & AB_Bit_kTempBit)
    /*- IsTransientId indicates whether a uid is transient (not persistent) 
    withmeaning only during the current session. -*/

#define AB_Uid_IsGlobal(uid)     (((uid) & AB_Bit_kLowTwoBits) == AB_Uid_kGlobal)
    /*- IsGlobal indicates whether a uid is the transient uid assigned to a 
    globaltable for a personal address book or an LDAP directory. -*/

#define AB_Uid_IsPersistent(uid) (((uid) & AB_Bit_kTempBit) == 0)
    /*- IsPersistent indicates whether a uid has meaning across multiple
    sessions because a database persistently associates this uid with some
    entity. -*/

#define AB_Uid_IsPersistentRow(uid) \
          (((uid) & AB_Bit_kLowTwoBits) == AB_Uid_kPersistentRow)

#define AB_Uid_IsRow(uid) \
          (((uid) & AB_Bit_kLowTwoBits) != AB_Uid_kColumn)
    /*- IsRow indicates whether a uid denotes a row. This includes both
    transient and persistent rows. Basically anything not a column is a row.
    All tables are considered rows, including address books and directories. -*/

#define AB_Uid_IsColumn(uid) \
          (((uid) & AB_Bit_kLowTwoBits) == AB_Uid_kColumn)
    /*- IsColumn indicates whether a uid denotes a column. Column uids are
    always peristent and have a non-overlapping scope only inside specific
    address books. That is, all column uids for a given address book are
    unique, but different address books can use the same column uids to
    mean different column names.  -*/

#define AB_Uid_IsStandard(uid) \
          (((uid) & AB_Bit_kStandardBit) != 0)

#define AB_Uid_IsStandardColumn(uid) \
          (((uid) & AB_Bit_kLowThreeBits) == AB_Uid_kStandardColumn)


#define AB_Attrib_AsStdColUid(attrib) \
  (( ((ab_column_uid) attrib) << AB_Uid_kTagBitCount) | AB_Uid_kStandardColumn)
  
#define AB_ColumnUid_AsAttrib(uid) \
 ( (AB_Uid_IsStandard(uid))? ( (uid) >> AB_Uid_kTagBitCount ) : 0 )
  
#define AB_DbUid_AsColumnUid(uid) \
  (( (uid) << AB_Uid_kTagBitCount) | AB_Uid_kColumn)
  
#define AB_ColumnUid_AsDbUid(uid) \
 ( (AB_Uid_IsColumn(uid))? ( (uid) >> AB_Uid_kTagBitCount ) : 0 )
  
#define AB_UidSeed_AsTempRowUid(uid) \
  ( ((uid) << AB_Uid_kTagBitCount) | AB_Bit_kTempBit )
  
#define AB_DbUid_AsRowUid(uid) \
  ( (uid) << AB_Uid_kTagBitCount )
  
#define AB_RowUid_AsDbUid(uid) \
 ( (AB_Uid_IsRow(uid))? ( (uid) >> AB_Uid_kTagBitCount ) : 0 )
  
#define AB_PersistentRowUid_AsDbUid(uid) \
 ( (AB_Uid_IsPersistentRow(uid))? ( (uid) >> AB_Uid_kTagBitCount ) : 0 )

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

typedef ab_num ab_cell_length; /* amount of cell content in AB_Row */
    /*- This number characterizes the length of content bytes in a AB_Row,
    buffered in memory, and is typically less than ab_cell_size. In other
    words this is the length of content stored in memory, as opposed to the
    extent (which is persistent content size).  -*/

typedef ab_num ab_cell_extent; /* amount of cell content in table row */
    /*- This number characterizes the persistent size of content in a row's 
    cell inside a table. This is the persistent idea content length but which 
    might be greater than ab_cell_length when a memory buffer is not big
    enough to hold the persistent value.  -*/

typedef ab_num ab_cell_size; /* bytes for a cell in row column */
    /*- This number characterizes a number of bytes of space used to buffer a
    row's cell value. This is independent of either length or extent.  -*/

typedef ab_num ab_cell_count; /* number of cells in a row */
    /*- This is a number of cells inside a row. -*/

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- Fault ----- ----- ----- ----- */

#define AB_Fault_kErrnoSpace   /*i*/ 0x65526E4F /* ASCII 'eRnO' */
#define AB_Fault_kAbookSpace   /*i*/ 0x61426752 /* ASCII 'aBgR' (no endian) */

/* AB_Fault structure and interface derive from public domain IronDoc */

struct AB_Fault /*d*/ {
  ab_error_uid  sFault_Code;
  ab_u4         sFault_Space;
};

#define AB_Fault_Clear(f)   /*i*/ \
  ((f)->sFault_Code = 0, (f)->sFault_Space = 0)

#define AB_Fault_Assign(e,other)   /*i*/ (*(e) = *(other))

#define AB_Fault_Init(f,c,s)   /*i*/ \
  ((f)->sFault_Code = (c), (f)->sFault_Space = (s))

#define AB_Fault_Code(f)     /*i*/ ((f)->sFault_Code)
#define AB_Fault_Space(f)    /*i*/ ((f)->sFault_Space)

enum AB_Fault_ePartition { /* number ranges for AB classes and features */
    AB_Fault_kErrno =          0,    /*i*/
    AB_Fault_kErrno_end =      99,   /*i*/

    AB_Fault_kEnv =            100,  /*i*/
    AB_Fault_kEnv_end =        149,  /*i*/

    AB_Fault_kObject =         150,  /*i*/
    AB_Fault_kObject_end =     199,  /*i*/

    AB_Fault_kTable =          200,  /*i*/
    AB_Fault_kTable_end =      299,  /*i*/

    AB_Fault_kRow =            300,  /*i*/
    AB_Fault_kRow_end =        349,  /*i*/

    AB_Fault_kColumn =         350,  /*i*/
    AB_Fault_kColumn_end =     399,  /*i*/

    AB_Fault_kFile =           400,  /*i*/
    AB_Fault_kFile_end =       449,  /*i*/

    AB_Fault_kEntry =          450,  /*i*/
    AB_Fault_kEntry_end =      499,  /*i*/

    AB_Fault_kCell =           500,  /*i*/
    AB_Fault_kCell_end =       549,  /*i*/

    AB_Fault_kView =           550,  /*i*/
    AB_Fault_kView_end =       574,  /*i*/

    AB_Fault_kModel =          575,  /*i*/
    AB_Fault_kModel_end =      599,  /*i*/

    AB_Fault_kPort =           600,  /*i*/
    AB_Fault_kPort_end =       624,  /*i*/

    AB_Fault_kMap =            625,  /*i*/
    AB_Fault_kMap_end =        649,  /*i*/

    AB_Fault_kPart =           650,  /*i*/
    AB_Fault_kPart_end =       674,  /*i*/

    AB_Fault_kRowSet =         675,  /*i*/
    AB_Fault_kRowSet_end =     699,  /*i*/

    AB_Fault_kDebugger =       700,  /*i*/
    AB_Fault_kDebugger_end =   724,  /*i*/

    AB_Fault_kPrinter =        725,  /*i*/
    AB_Fault_kPrinter_end =    749,  /*i*/

    AB_Fault_kTracer =         750,  /*i*/
    AB_Fault_kTracer_end =     774,  /*i*/

    AB_Fault_kObjectSet =      775,  /*i*/
    AB_Fault_kObjectSet_end =  799,  /*i*/

    AB_Fault_kTuple =          800,  /*i*/
    AB_Fault_kTuple_end =      849,  /*i*/

    AB_Fault_kSearch =         850,  /*i*/
    AB_Fault_kSearch_end =     874,  /*i*/

    AB_Fault_kString =         875,  /*i*/
    AB_Fault_kString_end =     899,  /*i*/

    AB_Fault_kStore =          900,  /*i*/
    AB_Fault_kStore_end =      924,  /*i*/

    AB_Fault_kRowContent =     925,  /*i*/
    AB_Fault_kRowContent_end = 949   /*i*/
};

enum {
    AB_Entry_kFaultDuplicateNickname = /*i*/ AB_Fault_kEntry,   /* 450 */
    AB_Entry_kFaultNotPersonType,                /*i*/ /* 451 */
    AB_Entry_kFaultNotListType,                  /*i*/ /* 452 */
    AB_Entry_kFaultNotListOrPersonType,          /*i*/ /* 453 */
    AB_Entry_kFaultMissingGivenName,             /*i*/ /* 454 */
    AB_Entry_kFaultMissingEmailAddress,          /*i*/ /* 455 */
    AB_Entry_kFaultDuplicateEmail,               /*i*/ /* 456 */
    AB_Entry_kFaultNoSuchRowUid,                 /*i*/ /* 457 */
    AB_Entry_kFaultNotFoundByIter,               /*i*/ /* 458 */
    AB_Entry_kFaultMissingFullName               /*i*/ /* 459 */
};

AB_PUBLIC_API(const char*) /* abfault.c */
AB_Fault_String(const AB_Fault* self);
  /* Return a static string describing error e, provided the space is equal
   * to either AB_Fault_kErrnoSpace or AB_Fault_kAbookSpace,
   * and provided AB_CONFIG_KNOW_FAULT_STRINGS is
   * defined.  Otherwise returns a static string for "{unknown-fault-space}"
   * or for "{no-fault-strings}".
   */

#define AB_Fault_kXmlBufSize /*i*/ 128 /* size AB_Fault_AsXmlString() needs */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
    AB_PUBLIC_API(char*) /* abfault.c */
    AB_Fault_AsXmlString(const AB_Fault* self, AB_Env* ev, char* outXmlBuf);
        /* <ab:fault string=\"%.32s\" code=\"#%08lX\" space =\"#%08lX/%.4s\"/> */
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#ifdef AB_CONFIG_DEBUG
  AB_PUBLIC_API(void) /* abfault.c */
  AB_Fault_Break(const AB_Fault* self, AB_Env* ev);
    /* e.g. AB_Env_Break(ev, AB_Fault_AsXmlString(self, ev, buf)); */
#endif /*AB_CONFIG_DEBUG*/

#ifdef AB_CONFIG_TRACE
  AB_PUBLIC_API(void) /* abfault.c */
  AB_Fault_Trace(const AB_Fault* eself, AB_Env* ev);
    /* e.g. AB_Env_Trace(ev, AB_Fault_AsXmlString(self, ev, buf)); */
#endif /*AB_CONFIG_TRACE*/

/* ----- ----- ----- ----- Env ----- ----- ----- ----- */


#ifndef AB_Env_typedef
typedef struct AB_Env AB_Env;
#define AB_Env_typedef 1
#endif

#define AB_Env_kFormatBufferSize /*i*/ 512 /* for var arg format methods */

/* AB_Env structures and interfaces derive from public domain IronDoc */

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

enum AB_Policy_eImportConflicts {
    AB_Policy_ImportConflicts_kUnknown = 0, /* need to call policy method */
    AB_Policy_ImportConflicts_kSignalError, /* AB_Env_kFaultImportDuplicate */
    AB_Policy_ImportConflicts_kIgnoreNewDuplicates, /* quietly ignore new  */
    AB_Policy_ImportConflicts_kReportAndIgnore,     /* report, then ignore */
    AB_Policy_ImportConflicts_kReplaceOldWithNew,   /* use new entry defn */
    AB_Policy_ImportConflicts_kReportAndReplace,    /* report, then replace */
    AB_Policy_ImportConflicts_kUpdateOldWithNew,    /* merge new entry defn */
    AB_Policy_ImportConflicts_kReportAndUpdate,     /* report, then merge */
    
    /* default: report conflicts (to file "ab.import.conflicts") and ignore */
    AB_Policy_ImportConflicts_kDefault =
        AB_Policy_ImportConflicts_kIgnoreNewDuplicates,

    AB_Policy_ImportConflicts_kMax =
        AB_Policy_ImportConflicts_kReportAndUpdate
};

#define AB_Env_kConflictReportFileNameSize 256 /* plus one byte for end null */

typedef ab_policy (* AB_Env_mImportConflictPolicy /*d*/)
(AB_Env* ev, char* outReportFileName256); /* only 255 bytes for file name */

struct AB_Env /*d*/ {
    ab_num         sEnv_FaultCount;  /* total number of stored faults */
    
    ab_bool        sEnv_DoTrace;
    ab_bool        sEnv_DoDebug;
    ab_bool        sEnv_DoErrBreak;
    ab_bool        sEnv_BeParanoid;
    
    /* ````` callbacks (slots similar to polymorphic object vtable) ````` */
    
    AB_Env_mImportConflictPolicy  sEnv_ImportConflictPolicy;
};

#define AB_Env_Good(ev)          /*i*/ ((ev)->sEnv_FaultCount == 0)
#define AB_Env_Bad(ev)           /*i*/ ((ev)->sEnv_FaultCount != 0)
#define AB_Env_DoTrace(ev)       /*i*/ ((ev)->sEnv_DoTrace)

enum AB_Env_eError {
    AB_Env_kFaultNullMethods = /*i*/ AB_Fault_kEnv,  /* 100 null vtable pointer */
    AB_Env_kFaultWrongMethodTag,     /*i*/ /* 101 vtable tag is wrong */
    
    AB_Env_kFaultNullCheckStack,     /*i*/ /* 102 missing CheckStack() method */
    AB_Env_kFaultNullFree,           /*i*/ /* 103 missing Free() method */
    AB_Env_kFaultNullForgetErrors,   /*i*/ /* 104 missing ForgetErrors() method */
    
    AB_Env_kFaultNullErrorCount,     /*i*/ /* 105 missing ErrorCount() method */
    AB_Env_kFaultNullGetError,       /*i*/ /* 106 missing GetError() method */
    AB_Env_kFaultNullGetAllErrors,   /*i*/ /* 107 missing GetAllErrors() method */
    AB_Env_kFaultNullNewFault,       /*i*/ /* 108 missing NewFault() method */
    
    AB_Env_kFaultNullBreakString,    /*i*/ /* 109 missing NewFault() method */
    AB_Env_kFaultNullTraceString,    /*i*/ /* 110 missing NewFault() method */
    
    AB_Env_kFaultZeroErrno,          /*i*/ /* 111 errno is unexpectedly zero */
    
    AB_Env_kFaultBrokenEndian,       /*i*/ /* 112 bad number in endianess code */
    
    AB_Env_kFaultMethodStubOnly,     /*i*/ /* 113 method not implemented */
    AB_Env_kFaultWrongTag,           /*i*/ /* 114 method not implemented */
    AB_Env_kFaultNullSelfHandle,     /*i*/ /* 115 method not implemented */
    AB_Env_kFaultOutOfMemory,        /*i*/ /* 116 failed memory allocation */
    AB_Env_kFaultImportDuplicate,    /*i*/ /* 117 duplicate entry imported */
    AB_Env_kFaultOddImportPolicy     /*i*/ /* 118 unknown import polity */
};

/* ----- ----- polymorphic dispatching virtual methods ----- ----- */

#define AB_Table_MakeEnv AB_OBSOLETE_METHOD
    /*- MakeEnv creates a new AB_Env instances that must be destroyed later
    with AB_Env_Free(), unless the caller feels like leaking the env, which
    might be okay if very few are ever created. 

    The frontend will not need more than one environment, but making more
    won't hurt. They are all interchangeable. Each env returns error status
    after methods return.  -*/

/* ----- ----- creation / ref counting ----- ----- */

AB_PUBLIC_API(AB_Env*) /* abenv.cpp */
AB_Env_New();

AB_PUBLIC_API(AB_Env*) /* abenv.cpp */
AB_Env_GetLogFileEnv(); /* do *not* release this (unless you first acquire) */

AB_PUBLIC_API(ab_ref_count) /* abenv.cpp */
AB_Env_Acquire(AB_Env* self);

AB_PUBLIC_API(ab_ref_count) /* abenv.cpp */
AB_Env_Release(AB_Env* self);

/* ----- ----- error access ----- ----- */

AB_PUBLIC_API(ab_error_count) /* abenv.cpp */
AB_Env_ForgetErrors(AB_Env* self);
    /*- Discard all error information. -*/

AB_PUBLIC_API(ab_error_count) /* abenv.cpp */
AB_Env_ErrorCount(const AB_Env* self);
    /*- Number of errors since last forget/reset. -*/

AB_PUBLIC_API(ab_error_uid) /* abenv.cpp */
AB_Env_GetError(const AB_Env* self);
    /*- Last error since forget/reset, otherwise zero. -*/

AB_PUBLIC_API(ab_error_count) /* abenv.cpp */
AB_Env_GetAllErrors(const AB_Env* self,
                     ab_error_uid* outVector, ab_error_count inSize,
                     ab_error_count* outLength);
    /*- All errors (up to inSize) since forget/reset. -*/


AB_PUBLIC_API(ab_error_uid) /* abenv.cpp */
AB_Env_NewFault(AB_Env* ev, ab_error_uid faultCode, ab_u4 faultSpace);

/* ----- ----- static dispatching methods ----- ----- */

AB_PUBLIC_API(ab_error_uid) /* abenv.cpp */
AB_Env_NewAbookFault(AB_Env* ev, ab_error_uid faultCode);

AB_PUBLIC_API(void) /* abenv.cpp */
AB_Env_Break(AB_Env* ev, const char* format, ...);
 
AB_PUBLIC_API(void) /* abenv.cpp */
AB_Env_Trace(AB_Env* ev, const char* format, ...);

AB_PUBLIC_API(void) /* abenv.cpp */
AB_Env_TraceBeginMethod(const AB_Env* ev, const char* cls, const char* method);

AB_PUBLIC_API(void) /* abenv.cpp */
AB_Env_TraceEndMethod(const AB_Env* ev);


/* ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ */

#if defined(AB_CONFIG_TRACE_CALL_TREE) && AB_CONFIG_TRACE_CALL_TREE

#define AB_Env_BeginMethod(ev,c,m) /*i*/ \
  { if ( (ev)->sEnv_DoTrace ) AB_Env_TraceBeginMethod((ev), (c), (m)); } {
  
#define AB_Env_EndMethod(ev) /*i*/ \
  } { if ( (ev)->sEnv_DoTrace ) AB_Env_TraceEndMethod(ev); }
  
#else /* end AB_CONFIG_TRACE_CALL_TREE*/

#define AB_Env_BeginMethod(ev,c,m) 
#define AB_Env_EndMethod(ev)
#endif /* end AB_CONFIG_TRACE_CALL_TREE*/

/* ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ */


/* ----- ----- ----- ----- Async ----- ----- ----- ----- */

    /*- We could arrange for AB_AsyncTask and AB_AsyncResult to be the
    same object, but this causes interfaces that are less clear with respect to
    cause an effect. -*/

#ifndef AB_AsyncTask_typedef
typedef struct AB_AsyncTask AB_AsyncTask;
#define AB_AsyncTask_typedef 1
#endif

    /*- AB_AsyncTask is an abstract data type. You don't know its internal
    structure. All that is known about this class is the methods that use this
    type as an argument or return value. 

    AB_AsyncTask is a request to perform some operation asynchronously,
    plus any information and callback hooks required to send progress
    information and notifications to the task requestor. -*/

AB_PUBLIC_API(AB_AsyncResult*)
AB_AsyncTask_OpenTable(AB_AsyncTask* self, AB_Env* ev, DIR_Server* dir);
    /*- OpenBook is an asynchronous version of AB_InitAddressBook(). -*/

#ifndef AB_AsyncResult_typedef 
typedef struct AB_AsyncResult AB_AsyncResult;
#define AB_AsyncResult_typedef 1
#endif
    /*- AB_AsyncResult is an abstract data type. You don't know its internal
    structure. All that is known about this class is the methods that use this
    type as an argument or return value. 

    AB_AsyncResult is a ticket (a task receipt) that allows a caller to
    manipulate, examine, signal, etc. the asynchronous task assoociated
    with an earlier task request (AB_AsyncTask). -*/

AB_PUBLIC_API(AB_Table*)
AB_AsyncResult_GetTable(AB_AsyncResult* self, AB_Env* ev);
    /*- GetTable returns the table opened if ticket t was used in the call to
    AB_Env_OpenTable(). This is a way of turning the OpenTable()
    asynchronous call into a synchronous wait for the table to open. -*/

                          
/* ----- ----- ----- ----- Tables ----- ----- ----- ----- */


#ifndef AB_Table_typedef
typedef struct AB_Table AB_Table;
#define AB_Table_typedef 1
#endif
/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

typedef enum AB_Table_eError { /* AT_Table errors */
    AB_Table_kFaultNullMethods = /*i*/ AB_Fault_kTable, /* 200 */
    AB_Table_kFaultWrongMethodTag,                /*i*/ /* 201 */

    AB_Table_kFaultNullMakeEnv,                   /*i*/ /* 202 */
    AB_Table_kFaultNullGetBookTable,              /*i*/ /* 203 */
    AB_Table_kFaultNullGetType,                   /*i*/ /* 204 */
    AB_Table_kFaultNullGetTableRowUid,            /*i*/ /* 205 */
    AB_Table_kFaultNullGetRefCount,               /*i*/ /* 206 */

    AB_Table_kFaultNullAcquire,                   /*i*/ /* 207 */
    AB_Table_kFaultNullRelease,                   /*i*/ /* 208 */
    AB_Table_kFaultNullGetColumnName,             /*i*/ /* 209 */
    AB_Table_kFaultNullGetColumnId,               /*i*/ /* 210 */
    AB_Table_kFaultNullNewColumnId,               /*i*/ /* 211 */

    AB_Table_kFaultNullHasDisplayColumnProperty,  /*i*/ /* 212 */
    AB_Table_kFaultNullCountColumns,              /*i*/ /* 213 */
    AB_Table_kFaultNullGetColumnLayout,           /*i*/ /* 214 */
    AB_Table_kFaultNullGetDefaultLayout,          /*i*/ /* 215 */
    AB_Table_kFaultNullChangeColumnLayout,        /*i*/ /* 216 */

    AB_Table_kFaultNullGetColumnAt,               /*i*/ /* 217 */
    AB_Table_kFaultNullPutColumnAt,               /*i*/ /* 218 */
    AB_Table_kFaultNullAddColumnAt,               /*i*/ /* 219 */
    AB_Table_kFaultNullCutColumn,                 /*i*/ /* 220 */
    AB_Table_kFaultNullCanSortByUid,              /*i*/ /* 221 */

    AB_Table_kFaultNullCanSortByName,             /*i*/ /* 222 */
    AB_Table_kFaultNullSortByUid,                 /*i*/ /* 223 */
    AB_Table_kFaultNullSortByName,                /*i*/ /* 224 */
    AB_Table_kFaultNullGetSortColumn,             /*i*/ /* 225 */
    AB_Table_kFaultNullAcquireSortedTable,        /*i*/ /* 226 */

    AB_Table_kFaultNullAcquireSearchTable,        /*i*/ /* 227 */
    AB_Table_kFaultNullAddAllRows,                /*i*/ /* 228 */
    AB_Table_kFaultNullAddRow,                    /*i*/ /* 229 */
    AB_Table_kFaultNullCutRow,                    /*i*/ /* 230 */
    AB_Table_kFaultNullDestroyRow,                /*i*/ /* 231 */

    AB_Table_kFaultNullCountRows,                 /*i*/ /* 232 */
    AB_Table_kFaultNullAcquireListsTable,         /*i*/ /* 233 */
    AB_Table_kFaultNullCountRowParents,           /*i*/ /* 234 */
    AB_Table_kFaultNullAcquireRowParentsTable,    /*i*/ /* 235 */
    AB_Table_kFaultNullCountRowChildren,          /*i*/ /* 236 */

    AB_Table_kFaultNullAcquireRowChildrenTable,   /*i*/ /* 237 */
    AB_Table_kFaultNullGetRowAt,                  /*i*/ /* 238 */
    AB_Table_kFaultNullGetRows,                   /*i*/ /* 239 */
    AB_Table_kFaultNullDoesSortRows,              /*i*/ /* 240 */
    AB_Table_kFaultNullRowPos,                    /*i*/ /* 241 */

    AB_Table_kFaultNullChangeRowPos,              /*i*/ /* 242 */
    AB_Table_kFaultNullCountRowCells,             /*i*/ /* 243 */
    AB_Table_kFaultNullMakeDefaultRow,            /*i*/ /* 244 */
    AB_Table_kFaultNullMakeRow,                   /*i*/ /* 245 */
    AB_Table_kFaultNullMakeRowFromColumns,        /*i*/ /* 246 */

    AB_Table_kFaultNullGetPersonCells,            /*i*/ /* 247 */
    AB_Table_kFaultNullGetListCells,              /*i*/ /* 248 */
    AB_Table_kFaultNullCutRowRange,               /*i*/ /* 249 */
    
    AB_Table_kFaultNullNewRowAt,                  /*i*/ /* 250 */
    AB_Table_kFaultNullReadRowAt,                 /*i*/ /* 251 */
    AB_Table_kFaultNullReadRow,                   /*i*/ /* 252 */
    AB_Table_kFaultNullReadAllRowCells,           /*i*/ /* 253 */
    AB_Table_kFaultNullUpdateRow,                 /*i*/ /* 254 */
    AB_Table_kFaultNullResetRow,                  /*i*/ /* 255 */

    AB_Table_kFaultWrongBodyTag,                  /*i*/ /* 256 */
    AB_Table_kFaultMiscTagTypeFailure,            /*i*/ /* 257 */
    AB_Table_kFaultWrongTag,                      /*i*/ /* 258 */
    AB_Table_kFaultOutOfMemory,                   /*i*/ /* 259 */
    AB_Table_kFaultMissingDeadTag,                /*i*/ /* 260 */
    
    AB_Table_kFaultBodyNotAvailable,              /*i*/ /* 261 */
    AB_Table_kFaultMissingContainerTable,         /*i*/ /* 262 */
    AB_Table_kFaultRowsNotSortable,               /*i*/ /* 263 */
    AB_Table_kFaultRowsNotSearchable,             /*i*/ /* 264 */
    
    AB_Table_kFaultNullDefaultsSlot,              /*i*/ /* 265 */
    AB_Table_kFaultNullNameSetSlot,               /*i*/ /* 266 */
    AB_Table_kFaultNullColumnSetSlot,             /*i*/ /* 267 */
    AB_Table_kFaultNullRowSetSlot,                /*i*/ /* 268 */
    AB_Table_kFaultNullRowContentSlot,            /*i*/ /* 269 */
    
    AB_Table_kFaultNotOpenTable,                  /*i*/ /* 270 */
    AB_Table_kFaultUnknownStaticFail,             /*i*/ /* 271 */
    AB_Table_kFaultNullViewSlot                   /*i*/ /* 272 */
} AB_Table_eError;

/*#define AB_Table_kFaultNull Foo    (AB_Fault_kTable + x)*/


    /*- AB_Table is an abstract data type. You don't know its internal structure.
    All that is known about this class is the methods that use this type as an
    argument or return value. -*/

AB_PUBLIC_API(AB_Table*) /* abtable.cpp */
AB_GetGlobalTable();
    /*- GetGlobalTable returns a global table manager. All address book
    content is part of this table. If there are multiple personal address books,
    then each of these address books appears as one row inside this uber
    table. Among other things, this interface unifies the appearance of per
    address book properties, because address book properties are just values
    appearing in the cells of columns for each. 

    (This global table can have a row describing itself, but this might be
    sick.) 

    We could put LDAP directories into this table to unify directories and
    address books. At least this would allow directories to be described by
    propertries appearing in columns for each directory row. -*/

AB_PUBLIC_API(AB_Table*) /* abtable.cpp */
AB_Table_GetBookTable(const AB_Table* self, AB_Env* ev);
    /*- GetBookTable returns the address book table containing this table. If
    self is itself the address book then self is returned. -*/


/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

typedef enum AB_Table_eType /*d*/ { 
  AB_Table_kNone = 0,         /* row is not a table */ 
  AB_Table_kGlobal = 1,       /* see AB_GetGlobalTable() */ 
  AB_Table_kAddressBook = 2,  /* personal address book */ 
  AB_Table_kDirectory = 3,    /* LDAP directory */ 
  AB_Table_kMailingList = 4,  /* a mailing list entry */ 
  AB_Table_kParentList = 5,   /* all tables containing a table or row */ 
  AB_Table_kListSubset = 6,   /* row subset of lists only */ 
  AB_Table_kSearchResult = 7, /* typedown or other search result */ 
  
  AB_Table_kNumberOfTypes = 8  /* must be last of enums */ 

} AB_Table_eType;

AB_PUBLIC_API(AB_Table_eType) /* abtable.cpp */
AB_Table_GetType(AB_Table* self, AB_Env* ev);
    /*- IsBookTable returns whether table t is an address book. -*/


AB_PUBLIC_API(ab_row_uid) /* abtable.cpp */
AB_Table_GetTableRowUid(AB_Table* self, AB_Env* ev);
    /*- GetTableRowUid returns the row uid for table self. If self is a global
    table (AB_Type_kGlobalTable, AB_GetGlobalTable()), then all such
    global uid's are globally unique. If self is a list inside a p -*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_PUBLIC_API(ab_ref_count) /* abtable.cpp */
AB_Table_GetRefCount(const AB_Table* self, AB_Env* ev);
    /*- GetRefCount returns the current number of references to this table
    instance in the current runtime session. This reference count is not
    persistent. It only reflects the number of times that a table has been
    acquired and not released. When a table is released enough times that it's
    ref count becomes zero, then the table is deallocated. 

    Frontends might not need to deal with table reference counts. The
    interface will try to be set up so this issue can be ignored as long as all
    usage rules are followed. -*/

AB_PUBLIC_API(ab_ref_count) /* abtable.cpp */
AB_Table_Acquire(AB_Table* self, AB_Env* ev);
    /*- Acquire increments the table's refcount. Perhaps frontends will never
    need to do this. For example, this occurs automatically when a frontend
    allocates a row by calling AB_Table_MakeRow(); -*/

AB_PUBLIC_API(ab_ref_count) /* abtable.cpp */
AB_Table_Release(AB_Table* self, AB_Env* ev);
    /*- Release decrements the table's refcount. Perhaps frontends will never
    need to do this. For example, this occurs automatically when a frontend
    deallocates a row by calling AB_Row_Free(); -*/

AB_PUBLIC_API(void) /* abtable.cpp */
AB_Table_Close(AB_Table* self, AB_Env* ev);
    /*- Close closes the table. -*/

                         
/* ----- ----- ----- ----- Columns ----- ----- ----- ----- */


    /*- (The notion of column replaces the old notion of token. A token
    means something more general, but the previously described address
    book model was only using tokens to descibe attribute names in
    schemas, and in the table model these are just names of columns.) -*/

AB_PUBLIC_API(const char*) /* abtable.cpp */
AB_Table_GetColumnName(AB_Table* self, AB_Env* ev, ab_column_uid col);
    /*- GetColumnName returns a constant string (do not modify or delete)
    which is the string representation of col. A null pointer is returned if
    this table does not know this given column uid. 

    The self table need not be the address book, but if not the address book
    table will be looked up (with AB_Table_GetBookTable()) and this table
    will be used instead. -*/

AB_PUBLIC_API(ab_column_uid) /* abtable.cpp */
AB_Table_GetColumnId(const AB_Table* self, AB_Env* ev, const char* name);
    /*- GetColumnId returns the column id associated. A zero uid is returned if
    this table does not know the given column name. This method must be
    used instead of NewColumnId when the address book is readonly and
    cannot be modified. 

    The self table need not be the address book, but if not the address book
    table will be looked up (with AB_Table_GetBookTable()) and this table
    will be used instead. -*/

AB_PUBLIC_API(ab_column_uid) /* abtable.cpp */
AB_Table_NewColumnId(AB_Table* self, AB_Env* ev, const char* name);
    /*- NewColumnId is like GetColumnId except that if name is not
    already known as a column in the address book, this method will modify
    the address book and its table so that in the future name is known by
    the persisent column uid returned. So a zero uid will not be returned
    unless an error occurs. 

    The self table need not be the address book, but if not the address book
    table will be looked up (with AB_Table_GetBookTable()) and this table
    will be used instead. -*/

AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_HasDisplayColumnProperty(const AB_Table* self, AB_Env* ev);
    /*- HasDisplayColumnProperty returns whether this table has its own
    specialized property that configures which columns it will display. If
    not, this table will default to using the columns for the containing
    address book. This method allows inspection of whether this table has
    overriden the address book defaults that will otherwise be used by the
    various column methods defined on AB_Table. -*/

AB_PUBLIC_API(ab_column_count) /* abtable.cpp */
AB_Table_CountColumns(const AB_Table* self, AB_Env* ev);
    /*- CountColumns returns the number of display columns for this table.
    This is just a subset of information returned by GetColumns. -*/


/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#ifndef AB_Column_typedef
typedef struct AB_Column AB_Column;
#define AB_Column_typedef 1
#endif

struct AB_Column {
  const char*    sColumn_Name;       /* the "real"  name of this column */
  const char*    sColumn_DisplayName; /* display name for column if non-null */
  ab_column_uid  sColumn_Uid;        /* the uid for sColumn_Name if nonzero */
  ab_column_uid  sColumn_SecondSort; /* if non-zero, secondary sort attribute */
  ab_cell_size   sColumn_CellSize;   /* size of row cells for this column */
  ab_bool        sColumn_CanSort;    /* the table can sort by this column */
};

enum {
    AB_Column_kFaultNotStandardName = /*i*/ AB_Fault_kColumn,   /* 400 */
    AB_Column_kFaultNotStandardUid,                /*i*/ /* 401 */
    AB_Column_kFaultNotColumnUid,                  /*i*/ /* 402 */
    AB_Column_kFaultSortNotSupported,              /*i*/ /* 403 */
    AB_Column_kFaultColumnUidTooLarge              /*i*/ /* 404 */
};


/* You *cannot* change following enum names or values.  So comments containing
** the numbers will never need different numbers, so you'll never experience
** the tedium of changing the comment numbers.  The enum names and column
** name strings should have exactly the same (case independent) letters.
**
** So if you feel compelled to change column names, by all means do so, but
** add a new enum with the new column name, and *do not change existing enums*
** either by name or numerical value.  Just stop using old standard columns
** if you don't want to use them.
**
** Don't even bother changing the white space that groups names five at a
** time, since grouping no longer matters as much as not moving names. This
** enum represents a pre-tokenized standard name table, and the names and
** tokens cannot be changed without breaking the table.
*/

typedef enum { 
    AB_Attrib_kIsPerson = 0,  /* 00 - "isperson" (t or f) */ 
    AB_Attrib_kModTime,       /* 01 - "modtime" */
    AB_Attrib_kFullName,      /* 02 - "fullname" */
    AB_Attrib_kNickname,      /* 03 - "nickname" */
    AB_Attrib_kMiddleName,    /* 04 - "middlename" */
    
    AB_Attrib_kFamilyName,    /* 05 - "familyname" */
    AB_Attrib_kCompanyName,   /* 06 - "companyname" */
    AB_Attrib_kRegion,        /* 07 - "region" */
    AB_Attrib_kEmail,         /* 08 - "email" */
    AB_Attrib_kInfo,          /* 09 - "info" */  
    
    AB_Attrib_kHtmlMail,      /* 10 - "htmlmail" (t or f) */      
    AB_Attrib_kExpandedName,  /* 11 - "expandedname" */
    AB_Attrib_kTitle,         /* 12 - "title" */
    AB_Attrib_kAddress,       /* 13 - "address" */
    AB_Attrib_kZip,           /* 14 - "zip" */
    
    AB_Attrib_kCountry,       /* 15 - "country" */
    AB_Attrib_kWorkPhone,     /* 16 - "workphone" */
    AB_Attrib_kHomePhone,     /* 17 - "homephone" */
    AB_Attrib_kSecurity,      /* 18 - "security" */
    AB_Attrib_kCoolAddress,   /* 19 - "cooladdress" (conference related) */ 
    
    AB_Attrib_kUseServer,     /* 20 - "useserver" (conference related) */ 
    AB_Attrib_kPager,         /* 21 - "pager" */
    AB_Attrib_kFax,           /* 22 - "fax" */
    AB_Attrib_kDisplayName,   /* 23 - "displayname" */
    AB_Attrib_kSender,        /* 24 - "sender" (mail and news) */
    
    AB_Attrib_kSubject,          /* 25 - "subject" */
    AB_Attrib_kBody,          /* 26 - "body" */
    AB_Attrib_kDate,          /* 27 - "date" */
    AB_Attrib_kPriority,      /* 28 - "priority" (mail only) */
    AB_Attrib_kMsgStatus,      /* 29 - "msgstatus" */
    
    AB_Attrib_kTo,            /* 30 - "to" */
    AB_Attrib_kCC,            /* 31 - "cc" */
    AB_Attrib_kToOrCC,        /* 32 - "toorcc" */
    AB_Attrib_kCommonName,    /* 33 - "commonname" (LDAP only) */
    AB_Attrib_k822Address,    /* 34 - "822address" */
    
    AB_Attrib_kPhoneNumber,   /* 35 - "phonenumber" */
    AB_Attrib_kOrganization,  /* 36 - "organization" */
    AB_Attrib_kOrgUnit,       /* 37 - "orgunit" */
    AB_Attrib_kLocality,      /* 38 - "locality" */
    AB_Attrib_kStreetAddress, /* 39 - "streetaddress" */
    
    AB_Attrib_kSize,          /* 40 - "size" */
    AB_Attrib_kAnyText,       /* 41 - "anytext" (any header or body) */
    AB_Attrib_kKeywords,      /* 42 - "keywords" */
    AB_Attrib_kDistName,      /* 43 - "distname" (distinguished name) */ 
    AB_Attrib_kObjectClass,   /* 44 - "objectclass" */
         
    AB_Attrib_kJpegFile,      /* 45 - "jpegfile" */
    AB_Attrib_kLocation,      /* 46 - "location" (result list only */
    AB_Attrib_kMessageKey,    /* 47 - "messagekey" (message result elems) */
    AB_Attrib_kAgeInDays,     /* 48 - "ageindays" (purging old news) */
    AB_Attrib_kGivenName,     /* 49 - "givenname" (sorting LDAP results) */

    AB_Attrib_kSurname,       /* 50 - "surname" */
    AB_Attrib_kFolderInfo,      /* 51 - "folderinfo" (view thread context) */
    AB_Attrib_kCustom1,          /* 52 - "custom1" (custom LDAP attribs) */
    AB_Attrib_kCustom2,       /* 53 - "custom2" */
    AB_Attrib_kCustom3,       /* 54 - "custom3" */
    
    AB_Attrib_kCustom4,       /* 55 - "custom4" */
    AB_Attrib_kCustom5,       /* 56 - "custom5" */
    AB_Attrib_kMessageId,     /* 57 - "messageid" */
    AB_Attrib_kHomeUrl,       /* 58 - "homeurl" */
    AB_Attrib_kWorkUrl,       /* 59 - "workurl" */
    
    AB_Attrib_kImapUrl,       /* 60 - "imapurl" */
    AB_Attrib_kNotifyUrl,     /* 61 - "notifyurl" */
    AB_Attrib_kPrefUrl,       /* 62 - "prefurl" */
    AB_Attrib_kPagerEmail,    /* 63 - "pageremail" */
    AB_Attrib_kParentPhone,   /* 64 - "parentphone" */
    
    AB_Attrib_kGender,        /* 65 - "gender" */
    AB_Attrib_kPostalAddress, /* 66 - "postaladdress" */
    AB_Attrib_kEmployeeId,    /* 67 - "employeeid" */
    AB_Attrib_kAgent,         /* 68 - "agent" */
    AB_Attrib_kBbs,           /* 69 - "bbs" */
    
    AB_Attrib_kBday,          /* 70 - "bday" (birthdate) */
    AB_Attrib_kCalendar,      /* 71 - "calendar" */
    AB_Attrib_kCar,           /* 72 - "car" */
    AB_Attrib_kCarPhone,      /* 73 - "carphone" */
    AB_Attrib_kCategories,    /* 74 - "categories" */
    
    AB_Attrib_kCell,          /* 75 - "cell" */
    AB_Attrib_kCellPhone,     /* 76 - "cellphone" */
    AB_Attrib_kCharSet,       /* 77 - "charset" (cs, csid) */
    AB_Attrib_kClass,         /* 78 - "class" */
    AB_Attrib_kGeo,           /* 79 - "geo" */
    
    AB_Attrib_kGif,           /* 80 - "gif" */
    AB_Attrib_kKey,           /* 81 - "key" (publickey) */
    AB_Attrib_kLanguage,      /* 82 - "language" */
    AB_Attrib_kLogo,          /* 83 - "logo" */
    AB_Attrib_kModem,         /* 84 - "modem" */
    
    AB_Attrib_kMsgPhone,      /* 85 - "msgphone" */
    AB_Attrib_kN,             /* 86 - "n" */
    AB_Attrib_kNote,          /* 87 - "note" */
    AB_Attrib_kPagerPhone,    /* 88 - "pagerphone" */
    AB_Attrib_kPgp,           /* 89 - "pgp" */
    
    AB_Attrib_kPhoto,         /* 90 - "photo" */
    AB_Attrib_kRev,           /* 91 - "rev" */
    AB_Attrib_kRole,          /* 92 - "role" */
    AB_Attrib_kSound,         /* 93 - "sound" */
    AB_Attrib_kSortString,    /* 94 - "sortstring" */
    
    AB_Attrib_kTiff,          /* 95 - "tiff" */
    AB_Attrib_kTz,            /* 96 - "tz" (timezone) */
    AB_Attrib_kUid,           /* 97 - "uid" (uniqueid) */
    AB_Attrib_kVersion,       /* 98 - "version" */
    AB_Attrib_kVoice,         /* 99 - "voice" */

    AB_Attrib_kNumColumnAttributes /* 100 - must be last enum value listed */
    
} AB_Column_eAttribute;

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/


typedef enum AB_Column_eDefaultSizes { /* defaults for new row cell sizes */
    AB_Column_kSize_IsPerson =      4, 
    AB_Column_kSize_ModTime =       18, 
    AB_Column_kSize_FullName =      256,  
    AB_Column_kSize_Nickname =      64,  
              
    AB_Column_kSize_MiddleName =    64,   
    AB_Column_kSize_FamilyName =    64,   
    AB_Column_kSize_CompanyName =   128,   
    AB_Column_kSize_Region =        128,  
    AB_Column_kSize_Email =         256,   
    AB_Column_kSize_Info =          1024,  
    AB_Column_kSize_HtmlMail =      8,  
    AB_Column_kSize_ExpandedName =  256,   
    AB_Column_kSize_Title =         64,  
    AB_Column_kSize_Address =       256,   
    AB_Column_kSize_Zip =           48,

    AB_Column_kSize_Country =       48, 
    AB_Column_kSize_WorkPhone =     48, 
    AB_Column_kSize_HomePhone =     48, 
    AB_Column_kSize_Security =      16, 
    AB_Column_kSize_CoolAddress =   256,
    AB_Column_kSize_UseServer =     16,
    AB_Column_kSize_Pager =         48, 
    AB_Column_kSize_Fax =           48, 
    AB_Column_kSize_DisplayName =   64, 

    AB_Column_kSize_Sender =        128,
    AB_Column_kSize_Subject =       256,    
    AB_Column_kSize_Body =          1024,    
    AB_Column_kSize_Date =          64,    

    AB_Column_kSize_Priority =      32,
    AB_Column_kSize_MsgStatus =     32,    
    AB_Column_kSize_To =            256,
    AB_Column_kSize_CC =            256,
    AB_Column_kSize_ToOrCC =        256,

    AB_Column_kSize_CommonName =    256,
    AB_Column_kSize_822Address =    256,
    AB_Column_kSize_PhoneNumber =   64,
    AB_Column_kSize_Organization =  128,
    AB_Column_kSize_OrgUnit =       64,
    AB_Column_kSize_Locality =      128,
    AB_Column_kSize_StreetAddress = 256,
    AB_Column_kSize_Size =          32,
    AB_Column_kSize_AnyText =       256,
    AB_Column_kSize_Keywords =      256,

    AB_Column_kMaxSize_DistName =   (32 * 1024),
    AB_Column_kSize_DistName =      512,
    AB_Column_kSize_ObjectClass =   32,       
    AB_Column_kSize_JpegFile =      1024,

    AB_Column_kSize_Location =      64,
    AB_Column_kSize_MessageKey =    32,

    AB_Column_kSize_AgeInDays =     16,

    AB_Column_kSize_GivenName =     64,
    AB_Column_kSize_Surname =       64, 

    AB_Column_kSize_FolderInfo =    256,

    AB_Column_kSize_Custom1 =       64,
    AB_Column_kSize_Custom2 =       64,
    AB_Column_kSize_Custom3 =       64,
    AB_Column_kSize_Custom4 =       64,
    AB_Column_kSize_Custom5 =       64,

    AB_Column_kSize_MessageId =     32, 
    
    AB_Column_kSize_HomeUrl =       128, 
    AB_Column_kSize_WorkUrl =       128, 
    AB_Column_kSize_ImapUrl =       128, 
    AB_Column_kSize_NotifyUrl =     128, 
    AB_Column_kSize_PrefUrl =       128, 
    AB_Column_kSize_PagerEmail =    48, 

    AB_Column_kSize_ParentPhone =   48, 
    AB_Column_kSize_Gender =        16, 
    AB_Column_kSize_PostalAddress = 128,
 
    AB_Column_kSize_EmployeeId =    32,

    AB_Column_kSize_Agent =         128,
    AB_Column_kSize_Bbs =           48,
    AB_Column_kSize_Bday =          32,
    AB_Column_kSize_Calendar =      256,
    AB_Column_kSize_Car =           48,
    AB_Column_kSize_CarPhone =      48,
    AB_Column_kSize_Categories =    64,
    AB_Column_kSize_Cell =          48,
    AB_Column_kSize_CellPhone =     48,
    AB_Column_kSize_CharSet =       16,
    AB_Column_kSize_Class =         64,
    AB_Column_kSize_Geo =           256,
    AB_Column_kSize_Gif =           1024,
    AB_Column_kSize_Key =           256,
    AB_Column_kSize_Language =      64,
    AB_Column_kSize_Logo =          1024,
    AB_Column_kSize_Modem =         48,
    AB_Column_kSize_MsgPhone =      48,
    AB_Column_kSize_N =             256,
    AB_Column_kSize_Note =          512,
    AB_Column_kSize_PagerPhone =    48,
    AB_Column_kSize_Pgp =           512,
    AB_Column_kSize_Photo =         1024,
    AB_Column_kSize_Rev =           128,
    AB_Column_kSize_Role =          64,
    AB_Column_kSize_Sound =         1024,
    AB_Column_kSize_SortString =    64,
    AB_Column_kSize_Tiff =          1024,
    AB_Column_kSize_Tz =            64,
    AB_Column_kSize_Uid =           128,
    AB_Column_kSize_Version =       64,
    AB_Column_kSize_Voice =         48
} AB_Column_eDefaultSizes;

typedef enum AB_Column_eUid { 
    AB_Column_kIsPerson =      AB_Attrib_AsStdColUid(AB_Attrib_kIsPerson), 
    AB_Column_kModTime =       AB_Attrib_AsStdColUid(AB_Attrib_kModTime), 
    AB_Column_kFullName =      AB_Attrib_AsStdColUid(AB_Attrib_kFullName),  
    AB_Column_kNickname =      AB_Attrib_AsStdColUid(AB_Attrib_kNickname),  
              
    AB_Column_kMiddleName =    AB_Attrib_AsStdColUid(AB_Attrib_kMiddleName),   
    AB_Column_kFamilyName =    AB_Attrib_AsStdColUid(AB_Attrib_kFamilyName),   
    AB_Column_kCompanyName =   AB_Attrib_AsStdColUid(AB_Attrib_kCompanyName),   
    AB_Column_kRegion =        AB_Attrib_AsStdColUid(AB_Attrib_kRegion),  
    AB_Column_kEmail =         AB_Attrib_AsStdColUid(AB_Attrib_kEmail),   
    AB_Column_kInfo =          AB_Attrib_AsStdColUid(AB_Attrib_kInfo),  
    AB_Column_kHtmlMail =      AB_Attrib_AsStdColUid(AB_Attrib_kHtmlMail),  
    AB_Column_kExpandedName =  AB_Attrib_AsStdColUid(AB_Attrib_kExpandedName),   
    AB_Column_kTitle =         AB_Attrib_AsStdColUid(AB_Attrib_kTitle),  
    AB_Column_kAddress =       AB_Attrib_AsStdColUid(AB_Attrib_kAddress),   
    AB_Column_kZip =           AB_Attrib_AsStdColUid(AB_Attrib_kZip),

    AB_Column_kCountry =       AB_Attrib_AsStdColUid(AB_Attrib_kCountry), 
    AB_Column_kWorkPhone =     AB_Attrib_AsStdColUid(AB_Attrib_kWorkPhone), 
    AB_Column_kHomePhone =     AB_Attrib_AsStdColUid(AB_Attrib_kHomePhone), 
    AB_Column_kSecurity =      AB_Attrib_AsStdColUid(AB_Attrib_kSecurity), 
    AB_Column_kCoolAddress =   AB_Attrib_AsStdColUid(AB_Attrib_kCoolAddress),
    AB_Column_kUseServer =     AB_Attrib_AsStdColUid(AB_Attrib_kUseServer),
    AB_Column_kPager =         AB_Attrib_AsStdColUid(AB_Attrib_kPager), 
    AB_Column_kFax =           AB_Attrib_AsStdColUid(AB_Attrib_kFax), 
    AB_Column_kDisplayName =   AB_Attrib_AsStdColUid(AB_Attrib_kDisplayName), 

    AB_Column_kSender =        AB_Attrib_AsStdColUid(AB_Attrib_kSender),
    AB_Column_kSubject =       AB_Attrib_AsStdColUid(AB_Attrib_kSubject),    
    AB_Column_kBody =          AB_Attrib_AsStdColUid(AB_Attrib_kBody),    
    AB_Column_kDate =          AB_Attrib_AsStdColUid(AB_Attrib_kDate),    

    AB_Column_kPriority =      AB_Attrib_AsStdColUid(AB_Attrib_kPriority),
    AB_Column_kMsgStatus =     AB_Attrib_AsStdColUid(AB_Attrib_kMsgStatus),    
    AB_Column_kTo =            AB_Attrib_AsStdColUid(AB_Attrib_kTo),
    AB_Column_kCC =            AB_Attrib_AsStdColUid(AB_Attrib_kCC),
    AB_Column_kToOrCC =        AB_Attrib_AsStdColUid(AB_Attrib_kToOrCC),

    AB_Column_kCommonName =    AB_Attrib_AsStdColUid(AB_Attrib_kCommonName),
    AB_Column_k822Address =    AB_Attrib_AsStdColUid(AB_Attrib_k822Address),
    AB_Column_kPhoneNumber =   AB_Attrib_AsStdColUid(AB_Attrib_kPhoneNumber),
    AB_Column_kOrganization =  AB_Attrib_AsStdColUid(AB_Attrib_kOrganization),
    AB_Column_kOrgUnit =       AB_Attrib_AsStdColUid(AB_Attrib_kOrgUnit),
    AB_Column_kLocality =      AB_Attrib_AsStdColUid(AB_Attrib_kLocality),
    AB_Column_kStreetAddress = AB_Attrib_AsStdColUid(AB_Attrib_kStreetAddress),
    AB_Column_kSize =          AB_Attrib_AsStdColUid(AB_Attrib_kSize),
    AB_Column_kAnyText =       AB_Attrib_AsStdColUid(AB_Attrib_kAnyText),
    AB_Column_kKeywords =      AB_Attrib_AsStdColUid(AB_Attrib_kKeywords),

    AB_Column_kDistName =      AB_Attrib_AsStdColUid(AB_Attrib_kDistName),
    AB_Column_kObjectClass =   AB_Attrib_AsStdColUid(AB_Attrib_kObjectClass),       
    AB_Column_kJpegFile =      AB_Attrib_AsStdColUid(AB_Attrib_kJpegFile),

    AB_Column_kLocation =      AB_Attrib_AsStdColUid(AB_Attrib_kLocation),
    AB_Column_kMessageKey =    AB_Attrib_AsStdColUid(AB_Attrib_kMessageKey),

    AB_Column_kAgeInDays =     AB_Attrib_AsStdColUid(AB_Attrib_kAgeInDays),

    AB_Column_kGivenName =     AB_Attrib_AsStdColUid(AB_Attrib_kGivenName),
    AB_Column_kSurname =       AB_Attrib_AsStdColUid(AB_Attrib_kSurname), 

    AB_Column_kFolderInfo =    AB_Attrib_AsStdColUid(AB_Attrib_kFolderInfo),

    AB_Column_kCustom1 =       AB_Attrib_AsStdColUid(AB_Attrib_kCustom1),
    AB_Column_kCustom2 =       AB_Attrib_AsStdColUid(AB_Attrib_kCustom2),
    AB_Column_kCustom3 =       AB_Attrib_AsStdColUid(AB_Attrib_kCustom3),
    AB_Column_kCustom4 =       AB_Attrib_AsStdColUid(AB_Attrib_kCustom4),
    AB_Column_kCustom5 =       AB_Attrib_AsStdColUid(AB_Attrib_kCustom5),

    AB_Column_kMessageId =     AB_Attrib_AsStdColUid(AB_Attrib_kMessageId), 
    
    AB_Column_kHomeUrl =       AB_Attrib_AsStdColUid(AB_Attrib_kHomeUrl), 
    AB_Column_kWorkUrl =       AB_Attrib_AsStdColUid(AB_Attrib_kWorkUrl), 
    AB_Column_kImapUrl =       AB_Attrib_AsStdColUid(AB_Attrib_kImapUrl), 
    AB_Column_kNotifyUrl =     AB_Attrib_AsStdColUid(AB_Attrib_kNotifyUrl), 
    AB_Column_kPrefUrl =       AB_Attrib_AsStdColUid(AB_Attrib_kPrefUrl), 
    AB_Column_kPagerEmail =    AB_Attrib_AsStdColUid(AB_Attrib_kPagerEmail), 

    AB_Column_kParentPhone =   AB_Attrib_AsStdColUid(AB_Attrib_kParentPhone), 
    AB_Column_kGender =        AB_Attrib_AsStdColUid(AB_Attrib_kGender), 
    AB_Column_kPostalAddress = AB_Attrib_AsStdColUid(AB_Attrib_kPostalAddress),
 
    AB_Column_kEmployeeId =    AB_Attrib_AsStdColUid(AB_Attrib_kEmployeeId),
    
    AB_Column_kAgent =         AB_Attrib_AsStdColUid(AB_Attrib_kAgent),
    AB_Column_kBbs =           AB_Attrib_AsStdColUid(AB_Attrib_kBbs),
    AB_Column_kBday =          AB_Attrib_AsStdColUid(AB_Attrib_kBday),
    AB_Column_kCalendar =      AB_Attrib_AsStdColUid(AB_Attrib_kCalendar),
    AB_Column_kCar =           AB_Attrib_AsStdColUid(AB_Attrib_kCar),
    AB_Column_kCarPhone =      AB_Attrib_AsStdColUid(AB_Attrib_kCarPhone),
    AB_Column_kCategories =    AB_Attrib_AsStdColUid(AB_Attrib_kCategories),
    AB_Column_kCell =          AB_Attrib_AsStdColUid(AB_Attrib_kCell),
    AB_Column_kCellPhone =     AB_Attrib_AsStdColUid(AB_Attrib_kCellPhone),
    AB_Column_kCharSet =       AB_Attrib_AsStdColUid(AB_Attrib_kCharSet),
    AB_Column_kClass =         AB_Attrib_AsStdColUid(AB_Attrib_kClass),
    AB_Column_kGeo =           AB_Attrib_AsStdColUid(AB_Attrib_kGeo),
    AB_Column_kGif =           AB_Attrib_AsStdColUid(AB_Attrib_kGif),
    AB_Column_kKey =           AB_Attrib_AsStdColUid(AB_Attrib_kKey),
    AB_Column_kLanguage =      AB_Attrib_AsStdColUid(AB_Attrib_kLanguage),
    AB_Column_kLogo =          AB_Attrib_AsStdColUid(AB_Attrib_kLogo),
    AB_Column_kModem =         AB_Attrib_AsStdColUid(AB_Attrib_kModem),
    AB_Column_kMsgPhone =      AB_Attrib_AsStdColUid(AB_Attrib_kMsgPhone),
    AB_Column_kN =             AB_Attrib_AsStdColUid(AB_Attrib_kN),
    AB_Column_kNote =          AB_Attrib_AsStdColUid(AB_Attrib_kNote),
    AB_Column_kPagerPhone =    AB_Attrib_AsStdColUid(AB_Attrib_kPagerPhone),
    AB_Column_kPgp =           AB_Attrib_AsStdColUid(AB_Attrib_kPgp),
    AB_Column_kPhoto =         AB_Attrib_AsStdColUid(AB_Attrib_kPhoto),
    AB_Column_kRev =           AB_Attrib_AsStdColUid(AB_Attrib_kRev),
    AB_Column_kRole =          AB_Attrib_AsStdColUid(AB_Attrib_kRole),
    AB_Column_kSound =         AB_Attrib_AsStdColUid(AB_Attrib_kSound),
    AB_Column_kSortString =    AB_Attrib_AsStdColUid(AB_Attrib_kSortString),
    AB_Column_kTiff =          AB_Attrib_AsStdColUid(AB_Attrib_kTiff),
    AB_Column_kTz =            AB_Attrib_AsStdColUid(AB_Attrib_kTz),
    AB_Column_kUid =           AB_Attrib_AsStdColUid(AB_Attrib_kUid),
    AB_Column_kVersion =       AB_Attrib_AsStdColUid(AB_Attrib_kVersion),
    AB_Column_kVoice =         AB_Attrib_AsStdColUid(AB_Attrib_kVoice)
   
} AB_Column_eUid;


AB_PUBLIC_API(const char*) /* abcolumn.c */
AB_ColumnUid_AsString(ab_column_uid inStandardColumnUid, AB_Env* ev);

AB_PUBLIC_API(ab_column_uid) /* abcolumn.c */
AB_String_AsStandardColumnUid(const char* inStandardColumnName, AB_Env* ev);

AB_PUBLIC_API(ab_column_count) /* abtable.cpp */
AB_Table_GetColumnLayout(const AB_Table* self, AB_Env* ev,
          AB_Column* outVector, ab_column_count inSize,
          ab_column_count* outLength);
    /*- GetColumnLayout fills the vector of AB_Column instances with
    descriptions of the column layout for this table. (If this table does not
    override settings from the address book, then this is the same as the
    address book column layout.) 

    Note that columns described by GetColumnLayout are the default
    columns added to AB_Row instances created by
    AB_Table_MakeDefaultRow(). 

    The outVector out parameter must be an array of at least inSize
    AB_Column instances, but preferrably AB_Table_CountColumns() plus
    one, because given enough instances the last column will be null
    terminated by means of a zero pointer written in the sColumn_Name slot. 

    The actual number of columns (exactly equal to
    AB_Table_CountColumns()) is returned from the method as the function
    value. The number of columns actually described in outVector is
    returned in outLength, and this is the same as the method value only
    when inSize is greater than AB_Table_CountColumns(). If inSize is
    greater than AB_Table_CountColumns(), then the column after the last
    one described is null terminated with zero in the sColumn_Name slot. 

    Each column in the layout is described as follows. sColumn_Uid gets a
    copy of the column uid returned from AB_Table_GetColumnId() for the
    name written in sColumn_Name. 

    sColumn_Name gets a copy of the column name returned from
    AB_Table_GetColumnName(). Callers must not modify or delete these
    name strings, because these pointers are aliases to strings stored in a
    dictionary within the self table instance, and modifying the strings
    might have catastrophic effect. 

    sColumn_PrettyName gets either a null pointer (when the pretty name
    and the real name are identical), or a string different from sColumn_Name
    which the user prefers to see in the table display. Like sColumn_Name,
    the storage for this string also belongs to self and must not be
    modified, on pain of potential catastrophic effect. 

    sColumn_CanSort gets a boolean indicating whether the table can be
    sorted by the column named by sColumn_Name. -*/



AB_PUBLIC_API(ab_column_count) /* abtable.cpp */
AB_Table_GetDefaultLayout(const AB_Table* self, AB_Env* ev,
          AB_Column* outVector, ab_column_count inSize,
          ab_column_count* outLength);
    /*- GetDefaultLayout is nearly the same as
    AB_Table_GetColumnLayout() except it returns the default layout used
    for a new address book table, as opposed to a specific layour currently
    in use by either this table or the actual address book table containing this
    table. -*/

AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_ChangeColumnLayout(AB_Table* self, AB_Env* ev,
          const AB_Column* inVector, ab_bool changeIndex);

    /*- ChangeColumnLayout modifies the column layout of this table to be
    equal to whatever is described in inVector, which must be null
    terminated by means of a zero pointer in the final sColumn_Name slot of
    the last column instance. 

    Note changing table columns will change the default set of columns
    added to AB_Row instances created by subsequent calls to
    AB_Table_MakeDefaultRow(). 

    If changing the column layout implies a change of address book indexes,
    then changeIndex must be true in order to actually cause this change to
    occur, because removing an index can be a very expensive (slow)
    operation. (We'll add asynchronous versions of this later.) -*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_GetColumnAt(const AB_Table* self, AB_Env* ev,
          AB_Column* outColumn, ab_column_pos inColPos);

    /*- GetColumnAt gets the table's column layout information from column
    position pos, which must be a one-based index from one to
    AB_Table_CountColumns() (or otherwise false is returned and nothing
    happens to the col parameter). -*/

AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_PutColumnAt(const AB_Table* self, AB_Env* ev,
          const AB_Column* col, ab_column_pos pos, ab_bool changeIndex);

    /*- PutColumnAt overwrites the table's column layout information at
    column position pos, which must be a one-based index from one to
    AB_Table_CountColumns() (or otherwise false is returned and nothing
    happens to the table's column layout). 

    If the column named (sColumnName) already occurs at some other
    position in the layout, then an error occurs. Otherwise the existing
    column at pos is changed to col. 

    If the value of sColumn_CanSort implies a change of address book
    indexes, then changeIndex must be true in order to actually cause this
    change to occur, because adding or removing an index can be a very
    expensive (slow) operation. (We'll add asynchronous versions of this
    later.) -*/

AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_AddColumnAt(const AB_Table* self, AB_Env* ev,
          const AB_Column* col, ab_column_pos pos, ab_bool changeIndex);
    /*- AddColumnAt inserts new table column layout information at column
    position pos, which must be a one-based index from one to
    AB_Table_CountColumns() plus one (or otherwise false is returned and
    nothing happens to the table's column layout). 

    If the column named (sColumnName) already occurs at some other
    position in the layout, then an error occurs. Otherwise a new column at
    pos is inserted in the layout. 

    If the value of sColumn_CanSort implies a change of address book
    indexes, then changeIndex must be true in order to actually cause this
    change to occur, because adding an index can be a very expensive
    (slow) operation. (We'll add asynchronous versions of this later.) -*/

AB_PUBLIC_API(ab_column_pos) /* abtable.cpp */
AB_Table_CutColumn(const AB_Table* self, AB_Env* ev,
          ab_column_uid col, ab_bool changeIndex);
    /*- CutColumn removes an old column from the table column layout. The
    col parameter must name a column currently in the layout (or otherwise
    false is returned and nothing happens to the table's column layout). 

    If removing the column implies a change of address book indexes, then
    changeIndex must be true in order to actually cause this change to
    occur, because removing an index can be a very expensive (slow)
    operation. (We'll add asynchronous versions of this later.) -*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_CanSortByUid(const AB_Table* self, AB_Env* ev, ab_column_uid col);
    /*- CanSortByUid indicates whether this table can sort rows by the
    specified column uid col. Frontends might prefer to call
    CanSortByName. -*/

AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_CanSortByName(const AB_Table* self, AB_Env* ev, const char* name);
    /*- CanSortByName indicates whether this table can sort rows by the
    specified column name. 

    CanSortByName is implemented by calling 
    AB_Table_CanSortByUid(self, ev,
    AB_Table_GetColumnId(self, ev, name)); -*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_SortByUid(const AB_Table* self, AB_Env* ev, ab_column_uid col);
    /*- SortByUid sorts this table by the specified column col. If col is zero
    this means make the table unsorted, which causes the table to order rows
    in the most convenient internal form (which is by row uid for address
    books, and some user specified order for mailing lists). -*/

AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_SortByName(const AB_Table* self, AB_Env* ev, const char* name);
    /*- SortByName sorts this table by the specified column name. The
    name parameter can be null, and this means make the table unsorted
    (with respect to specific cell values). Making a table unsorted makes
    particular sense for mailing lists. 

    SortByName is implemented by calling 
    AB_Table_SortByUid(self, ev, AB_Table_GetColumnId(self,
    ev, name)); -*/

AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_SortFoward(const AB_Table* self, AB_Env* ev, ab_bool inAscend);
    /*- If inAscend is true, arrange sorted rows in ascending order,
    and otherwise arrange sorted rows in descending order -*/

AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_GetSortFoward(const AB_Table* self, AB_Env* ev);
    /*- Return whether rows are arranged in ascending order -*/

AB_PUBLIC_API(ab_column_uid) /* abtable.cpp */
AB_Table_GetSortColumn(const AB_Table* self, AB_Env* ev);
    /*- GetSortColumn returns the column currently used by the table for
    sorting. Zero is returned when this table is currently unsorted (and also
    when any error occurs). Unsorted tables might be common for mailing
    lists. -*/

AB_PUBLIC_API(AB_Table*) /* abtable.cpp */
AB_Table_AcquireSortedTable(AB_Table* self, AB_Env* ev,
          ab_column_uid newSortColumn);
    /*- AcquireSortedTable returns a new table instance that has the same
    content as the old table, but sorts on a different column. This is similar
    to AB_Table_SortByUid(), but lets one have two different views of the
    same table with different sortings. 

    If newSortColumn is zero this means return an unsorted table. If
    newSortColumn is the same value as the current sorting returned by
    AB_Table_GetSortColumn(), this means the returned table might be
    exactly the same AB_Table instance as self. However, it will have been
    acquired one more time, so self and the returned table act as if refcounted
    separately even if they are the same table instance. 

    The caller must eventually release the returned table by calling
    AB_Table_Release(), and then make sure not to refer to this table again
    after released because it might be destroyed. -*/

#define AB_Table_AcquireUnsortedTable(table,ev) \
    AB_Table_AcquireSortedTable(table,ev, /*unsorted*/ 0)


/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */
                          
/* ----- ----- ----- ----- Search ----- ----- ----- ----- */


AB_PUBLIC_API(ab_row_uid) /* abtable.cpp */
AB_Table_FindFirstRowWithPrefix(AB_Table* self, AB_Env* ev,
          const char* cellValuePrefix, ab_column_uid inColumnUid);

AB_PUBLIC_API(AB_Table*) /* abtable.cpp */
AB_Table_AcquireSearchTable(AB_Table* self, AB_Env* ev,
          const char* cellValue, const ab_column_uid* inColumnVector);
    /*- AcquireSearchTable returns a table giving the non-persistent results of
    a search for cellValue in one or more columns of table self. The
    columns to search are given in the array specified by inColumnVector
    which should be null-terminated with a zero ab_column_uid value after
    the last column to be searched. 

    The returned table will contain no duplicates, even if cellValue causes a
    match in more than one column. The table content is virtual (not
    persistent, and possibly listed only by indirect reference) and will not
    exist after the table is released down to a zero refcount. (If the caller
    wants a persistent table, the caller can make a new mailing list and iterate
    over this table, adding all rows to the new mailing list.) 

    The caller must eventually release the returned table by calling
    AB_Table_Release(), and then make sure not to refer to this table again
    after released because it might be destroyed. (Debug builds might keep
    around "destroyed" tables for a time to detect invalid access after ref
    counts reach zero.) -*/

                          
/* ----- ----- ----- ----- Rows ----- ----- ----- ----- */


/*- (The notion of row replaces both the old notions of entry and tuple. In
more detail, ab_row_uid replaces ABID, and AB_Row replaces
HgAbTuple. Nothing replaces HgAbSchema which simply goes away
because this behavior is folded into AB_Row. We could introduce the idea
of a meta-row to replace schema, but this is really just an performance
issue and we can hide these details from the FEs.) 

A persistent row is identified by its uid, ab_row_uid, but callers can
only read and write such persistent objects by means of a runtime object
named AB_Row which can describe the content of a persisent row.
AB_Row denotes a transient session object and not a persistent object.
Any given instance of AB_Row has no associated ab_row_uid because
the runtime object is not intended to have a close association with a
single persisent row. 

However, each AB_Row instance is associated with a specific instance of
AB_Table (even though this could be avoided) because this runtime
relationship happens to be convenient, and causes no awkwardness in
the interfaces. AB_Row can effectively be considered a view onto the
collection of persisent rows inside a given table, except AB_Row is not
focused on a specific row in the table. -*/

#ifndef AB_Row_typedef
typedef struct AB_Row AB_Row;
#define AB_Row_typedef 1
#endif

    /*- AB_Row is an abstract data type. You don't know its internal structure.
    All that is known about this class is the methods that use this type as an
    argument or return value. -*/

enum {
    AB_Row_kFaultNullMethods = /*i*/ AB_Fault_kRow,   /* 300 */
    AB_Row_kFaultWrongMethodTag,                /*i*/ /* 301 */
    AB_Row_kFaultNotRowUid,                     /*i*/ /* 302 */
    AB_Row_kFaultNullTable,                     /*i*/ /* 303 */
    AB_Row_kFaultOutOfMemory,                   /*i*/ /* 304 */
    AB_Row_kFaultNonStandardColumn,             /*i*/ /* 305 */
    AB_Row_kFaultCountNotUnderSize,             /*i*/ /* 306 */
    AB_Row_kFaultNotOpen                        /*i*/ /* 307 */
};

AB_PUBLIC_API(ab_row_count) /* abtable.cpp */
AB_Table_AddAllRows(AB_Table* self, AB_Env* ev, 
            const AB_Table* other);
    /*- AddAllRows adds all the rows in other to self, as if by iterating over
    all rows in other and adding them one at a time with
    AB_Table_AddRow(). -*/


AB_PUBLIC_API(ab_row_uid)
AB_Table_CopyRow(AB_Table* self, AB_Env* cev,
     const AB_Table* inOther, ab_row_uid inRowUid);
    /*- copy row inRowUid from inOther to this table -*/

AB_PUBLIC_API(ab_row_pos) /* abtable.cpp */
AB_Table_AddRow(AB_Table* self, AB_Env* ev, ab_row_uid row);
    /*- AddRow aliases an existing row in the address book containing table
    self (see AB_Table_GetBookTable()) so this table also contains this
    row. If self already contains this row, nothing happens and false is
    returned. If row does not denote a row in the address book, an error
    occurs and zero is returned. (The error is indicated by
    AB_Env_GetError().) 

    If self did not previously contain row, it is added and true is returned.
    This is a way to alias an existing row into a new location, rather than
    creaing a new persistent row. (New persistent rows are created with
    AB_Row_NewTableRowAt().) -*/

AB_PUBLIC_API(ab_row_pos) /* abtable.cpp */
AB_Table_CutRow(AB_Table* self, AB_Env* ev, ab_row_uid row);
    /*- CutRow removes the indicated row from the table. This does not
    actually destroy the row unless this was the last reference to the row.
    False is returned when the table did not previously contain row (but it's
    okay to attempt cutting it again because the same desired result obtains
    when the row is not in the table afterwards). -*/

AB_PUBLIC_API(ab_row_count) /* abtable.cpp */
AB_Table_CutRowRange(AB_Table* self, AB_Env* ev, ab_row_pos p, ab_row_count c);
    /*- CutRow removes the indicated set of c rows from the table, starting
    at one-based position p. This does not actually destroy the rows unless
    they were the last references to the rows. -*/

AB_PUBLIC_API(ab_row_pos) /* abtable.cpp */
AB_Table_DestroyRow(AB_Table* self, AB_Env* ev, ab_row_uid row);
    /*- DestroyRow removes the indicated row from this table and from
    every other table containing the same row. False is returned when the
    containing address book did not previously contain row anywhare (but
    it's okay to attempt destroying it again because the same desired result
    obtains when the row is not in the table afterwards). An error might
    occur if the table can determine that row was never a valid row inside
    the address book (perhaps the size of the uid indicates it has never been
    assigned). -*/

AB_PUBLIC_API(ab_row_count) /* abtable.cpp */
AB_Table_CountRows(const AB_Table* self, AB_Env* ev);
    /*- CountRows returns the number of rows in the table. (In other words,
    how many entries does this address book or mailing list contain?) -*/

AB_PUBLIC_API(AB_Table*) /* abtable.cpp */
AB_Table_AcquireListsTable(AB_Table* self, AB_Env* ev);
    /*- AcquireListsTable -*/



AB_PUBLIC_API(ab_row_count) /* abtable.cpp */
AB_Table_CountRowParents(const AB_Table* self, AB_Env* ev,
          ab_row_uid id);
    /*- CountRowParents returns the number of parent tables that contain the
    row known by id. If zero, this means id does not exist because no
    row corresponds to this uid. If more than one, this means that more than
    one table contains an alias to this row. The value returned by
    CountRowParents is effectively the persistent reference count for row
    id. -*/

AB_PUBLIC_API(AB_Table*) /* abtable.cpp */
AB_Table_AcquireRowParentsTable(const AB_Table* self, AB_Env* ev,
          ab_row_uid id);
    /*- AcquireRowParentsTable returns a table representing the collection of
    tables that contain the row known by id. A null pointer is returned
    when any problem occurs (such as id not existing (i.e. zero
    AB_Table_CountRowParents())). 

    The caller must eventually release the table by calling
    AB_Table_Release(), and then make sure not to refer to this table again
    after released because it might be destroyed. (Debug builds might keep
    around "destroyed" tables for a time to detect invalid access after ref
    counts reach zero.) -*/


/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

AB_PUBLIC_API(ab_row_count) /* abtable.cpp */
AB_Table_CountRowChildren(const AB_Table* self, AB_Env* ev,
          ab_row_uid id);
    /*- CountRowChildren returns the number of children rows in the row
    known by id. When this number is positive the row is a table.
    Otherwise when zero the row is simply a row. -*/

#define AB_Table_IsRowTable(t,ev,row) \
             (AB_Table_CountRowChildren(t,ev,row) != 0)
    /*- IsRowTable returns whether the row known by row is a table.-*/ 

AB_PUBLIC_API(AB_Table*) /* abtable.cpp */
AB_Table_AcquireRowChildrenTable(const AB_Table* self, AB_Env* ev,
          ab_row_uid tableId);
    /*- AcquireRowChildrenTable returns a table representing the set of rows
    inside the table known by tableId. Typically tableId is a mailing list
    and the returned table shows the content of this mailing list. A null
    pointer is returned when any problem occurs. 

    If tableId was previously not a table (because it had no children and so
    AB_Table_IsRowTable() returns false), then the table returned is
    empty. However, when the caller adds any row to this table, then
    tableId becomes a table as a result. In other words, any row can be
    made a mailing list by adding children, and this is done by acquiring the
    row's children table and adding some children. Presto, changeo, now
    the row is a table. 

    The caller must eventually release the table by calling
    AB_Table_Release(), and then make sure not to refer to this table again
    after released because it might be destroyed. (Debug builds might keep
    around "destroyed" tables for a time to detect invalid access after ref
    counts reach zero.) -*/

AB_PUBLIC_API(ab_row_uid) /* abtable.cpp */
AB_Table_GetRowAt(const AB_Table* self, AB_Env* ev, ab_row_pos p);
    /*- GetRowAt returns the uid of the row at position p, where each row in
    a table has a position from one to AB_Table_RowCount(). (One-based
    indexes are used rather than zero-based indexes, because zero is
    convenient for indicating when a row is not inside a table.) -*/

AB_PUBLIC_API(ab_row_count) /* abtable.cpp */
AB_Table_GetRows(const AB_Table* self, AB_Env* ev,
       ab_row_uid* outVector, ab_row_count inSize, ab_row_pos pos);
    /*- GetRows is roughly equivalent to calling AB_Table_GetRowAt()
    inSize times. It is a good way to read a contiguous sequence of row
    uids starting at position pos inside the table. 

    At most inSize uids are written to outVector, and the actual number
    written there is returned as the value of function. The only reason why
    fewer might be written is if fewer than inSize rows are in the table
    starting at position pos. 

    Remember that pos is one-based, so one is the position of the first row
    and AB_Table_CountRows() is the position of the last. -*/

AB_PUBLIC_API(ab_bool) /* abtable.cpp */
AB_Table_DoesSortRows(const AB_Table* self, AB_Env* ev);
    /*- DoesSortRows returns whether table self maintains its row collection
    in sorted order. Presumably this is always true for address books
    (AB_Type_kAddressBookTable) but always false for mailing lists
    (AB_Type_kMailingListTable). At least this would be consistent with
    4.0 behavior. 

    DoesSortRows is equivalent to the expression
    (AB_Table_GetSortColumn()!=0). 

    We might want to permit mailing lists to sort rows by cell values, just
    like address books do. But in that case an unsorted representation should
    be kept so users can keep list recipients in a preferred order. (A user
    specified ordering might be important for social constraints in showing
    proper acknowledgement to individuals according to some ranking
    scheme.) 

    If we decide to sort mailing lists, the sorting will likely be maintained in
    memory, as opposed to persistently. (Users with huge mailing lists
    should use a separate address book for this purpose.) This would let
    users easily revert to the original unsorted view of a list. 

    Getting a sorted interface to a list is done with
    AB_Table_AcquireSortedTable(), and getting the unsorted version of
    a list is done with AB_Table_AcquireUnsortedTable(). -*/

AB_PUBLIC_API(ab_row_pos) /* abtable.cpp */
AB_Table_RowPos(const AB_Table* self, AB_Env* ev, ab_row_uid id);
    /*- RowPos returns the position of the row known by id, where each row
    in a table has a position from one to AB_Table_RowCount(). Zero is
    returned when this row is not inside the table. -*/

#define AB_Table_HasRow(t,ev,row) (AB_Table_RowPos(t,ev,row) != 0)
    /*- HasRow returns whether table self contains the indicated row.-*/ 

AB_PUBLIC_API(ab_row_pos) /* abtable.cpp */
AB_Table_ChangeRowPos(const AB_Table* self, AB_Env* ev,
      ab_row_uid existingRow, ab_row_pos toNewPos);
    /*- ChangeRowPos moves the row known by existingRow to new
    position toNewPos, provided AB_Table_DoesSortRows() is true.
    Otherwise ChangeRowPos does nothing (and returns false) if the table
    is in sorted order, or if existingRow is not in the table
    (AB_Table_HasRow() returns false). 

    Specifically, ChangeRowPos is expected to be useful for mailing list
    tables (AB_Type_kMailingListTable), but not useful for address book
    tables (AB_Type_kAddressBookTable) nor useful for some other table
    types (e.g. AB_Type_kSearchResultTable). 

    If FEs wish, they need not worry about this restriction, but simply let
    users see that dragging rows in mailing lists is useful, but dragging rows
    in address books has no effect. It does not seem feasible to prevent users
    from attempting to drag within an address book, because FEs will not
    know whether the user intends to drop elsewhere, say in another address
    book. FEs might want to figure out whether a drop makes sense in the
    same table, but they can simply call ChangeRowPos and find out
    (through both the return value and notifications) whether it has any
    effect. -*/

AB_PUBLIC_API(ab_column_count) /* abtable.cpp */
AB_Table_CountRowCells(const AB_Table* self, AB_Env* ev,
             ab_row_uid id);
    /*- CountRowCells returns the number of cells in the row known by id,
    which means the number of columns that have non-empty values in the
    row. (In other words, how many attributes does this entry have?) -*/

AB_PUBLIC_API(AB_Row*) /* abtable.cpp */
AB_Table_MakeDefaultRow(AB_Table* self, AB_Env* ev);
    /*- MakeDefaultRow creates a runtime description of a row (but not a
    persistent row). (Creating a persistent table row is done with
    AB_Row_NewTableRowAt().) This instance must be deallocated later with
    AB_Row_Free(). 

    This instance of AB_Row will already have cells corresponding to the
    columns used by the table (see AB_Table_GetColumnLayout()), so
    frontends that intend to use this row to display columns from the table
    will no need not do any more work before calling
    AB_Row_ReadTableRow() to read row content from the table. -*/

AB_PUBLIC_API(AB_Row*) /* abtable.cpp */
AB_Table_MakeRow(AB_Table* self, AB_Env* ev, const AB_Cell* inVector);
    /*- MakeRow creates a runtime description of a row (but not a persistent
    row) with N cells as described by the array of (at least) N+1 cells
    pointed to by inVector. Only the sCell_Column and sCell_Size slots
    matter. Other cell slots are ignored. The length of the inVector array is
    inferred by the first cell that contains zero in the sCell_Column slot,
    which is used for null termination. 

    If the first N cells of inVector have nonzero sCell_Column slots, then
    those first N cells must also have nonzero sCell_Size slots, or else an
    error occurs. 

    The sCell_Column slot values should be all distinct, without duplicate
    columns. If inVector has duplicate columns, the last one wins and no
    error occurs (so callers should check before calling if they care). 

    After a row is created, more cells can be added with
    AB_Row_AddCells(). -*/

AB_PUBLIC_API(AB_Row*) /* abtable.cpp */
AB_Table_MakeRowFromColumns(AB_Table* self, AB_Env* ev,
       const AB_Column* inColumns);
    /*- MakeRowFromColumns is just like AB_Table_MakeRow() except that
    sColumn_Uid and sColumn_CellSize slots are used (instead of
    sCell_Column and sCell_Size) and the vector is null terminated with a
    zero in sColumn_Uid. -*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

AB_PUBLIC_API(AB_Row*) /* abtable.cpp */
AB_Table_MakePersonRow(AB_Table* self, AB_Env* ev);
    /*- MakePersonRow creates a runtime description of a row (but not a
    persistent row) that seems most suited for editing a description of a
    person. This is like calling AB_Table_MakeRow() with the cell vector
    returned by AB_Table_GetPersonCells(). -*/

AB_PUBLIC_API(AB_Row*) /* abtable.cpp */
AB_Table_MakePersonList(AB_Table* self, AB_Env* ev);
    /*- MakePersonList creates a runtime description of a row (but not a
    persistent row) that seems most suited for editing a description of a
    mailing list. This is like calling AB_Table_MakeRow() with the cell vector
    returned by AB_Table_GetListCells(). -*/



AB_PUBLIC_API(ab_cell_count) /* abtable.cpp */
AB_Table_GetPersonCells(const AB_Table* self, AB_Env* ev,
          AB_Cell* outVector, ab_cell_count inSize, ab_cell_count* outLength);
    /*- GetPersonCells returns a copy of the standard cells for a person in
    cell array outVector. This cell vector is address book specific because
    column uids in the sCell_Column slots have address book scope. The
    actual number of standard cells is returned as the function value, but the
    size of outVector is assumed to be inSize, so no more than inSize
    cells will be written to outVector. The actual number of cells written is
    returned in outLength (unless outLength is a null pointer to suppress
    this output). 

    If inSize is greater than the number of cells, N, then outVector[N] will
    be null terminated by placing all zero values in all the cell slots. 

    All the sCell_Content slots will be null pointers because these cells
    will not represent actual storage in a row. GetPersonCells is expected
    to be used in the implementation of methods like
    AB_Table_MakePersonRow(). -*/

AB_PUBLIC_API(ab_cell_count) /* abtable.cpp */
AB_Table_GetListCells(const AB_Table* self, AB_Env* ev,
          AB_Cell* outVector, ab_cell_count inSize, ab_cell_count* outLength);
    /*- GetListCells returns a copy of the standard cells for a mailing list 
    in cell array outVector. This cell vector is address book specific because
    column uids in the sCell_Column slots have address book scope. The
    actual number of standard cells is returned as the function value, but the
    size of outVector is assumed to be inSize, so no more than inSize
    cells will be written to outVector. The actual number of cells written is
    returned in outLength (unless outLength is a null pointer to suppress
    this output). 

    If inSize is greater than the number of cells, N, then outVector[N] will
    be null terminated by placing all zero values in all the cell slots. 

    All the sCell_Content slots will be null pointers because these cells
    will not represent actual storage in a row. GetListCells is expected to be
    used in the implementation of methods like
    AB_Table_MakePersonList(). -*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_PUBLIC_API(AB_Row*) /* abrow.c */
AB_Row_MakeRowClone(const AB_Row* self, AB_Env* ev);
    /*- MakeRowClone allocates a duplicate of row self with exactly the same
    table, cell structure, and cell content. A caller might use this method to
    create a copy of a row prior to making changes when the original row
    must be kept intact. -*/

AB_PUBLIC_API(ab_ref_count) /* abrow.c */
AB_Row_Acquire(AB_Row* self, AB_Env* ev);

AB_PUBLIC_API(ab_ref_count) /* abrow.c */
AB_Row_Release(AB_Row* self, AB_Env* ev);

AB_PUBLIC_API(ab_bool) /* abrow.c */
AB_Row_CopyRowContent(AB_Row* self, AB_Env* ev, const AB_Row* other);
    /*- CopyRowContent makes all cells in self contain the same content as
    corresponding cells (with the same sCell_Column) in other. This
    affects cell content only, and does not change the cell structure of self at
    all, so number and size of cells does not change. If a cell in self is too
    small to receive all content in other, the content is truncated rather than
    enlarging the cell (in contrast to AB_Row_BecomeRow()). -*/

AB_PUBLIC_API(ab_bool) /* abrow.c */
AB_Row_BecomeRowClone(AB_Row* self, AB_Env* ev, const AB_Row* other);
    /*- BecomeRowClone causes self to be a clone of other, with exactly the
    same cell structure and cell content. (If self and other belong to
    different tables, then self changes its table to that of other.) -*/

AB_PUBLIC_API(AB_Table*) /* abrow.c */
AB_Row_GetTable(const AB_Row* self, AB_Env* ev);
    /*- GetTable returns the table that created this row.
    (Null returns on error.)-*/

AB_PUBLIC_API(ab_bool) /* abrow.c */
AB_Row_ChangeTable(AB_Row* self, AB_Env* ev, AB_Table* table);
    /*- ChangeTable sets the table for this row to table (and this does any
    necessary table reference counting). -*/

AB_PUBLIC_API(ab_row_uid) /* abrow.c */
AB_Row_NewTableRowAt(const AB_Row* self, AB_Env* ev, ab_row_pos pos);
    /*- New creates a new row in the table at position pos by writing the 
    cells specified by this row. Zero is returned on error (zero is never a 
    valid uid), and the specific error is indicated by AB_Env_GetError(). 

    The position pos can be any value, not just one to
    AB_Table_CountRows(). If zero, this means put the new row wherever
    seems best. If greater than the number of rows, it means append to the
    end. If the table is currently sorted in some order, pos is ignored and
    the new row is goes in sorted position. -*/

#define AB_Row_NewTableRow(table,ev) AB_Row_NewTableRowAt(table,ev,0)
    /*- NewTableRow is just like AB_Row_NewTableRowAt() except with the
    position argument curried with the constant value zero, which is ignored
    anyway by address books, but for mailing lists might cause the new row
    to be, say, appended after the last existing row. -*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

AB_PUBLIC_API(ab_bool) /* abrow.c */
AB_Row_ReadTableRowAt(AB_Row* self, AB_Env* ev, ab_row_pos pos);
    /*- ReadTableRowAt reads the row in the table at postion pos, filling cell
    values from the persistent content for the row found in the table. All cells
    are given values read from row id in the table, and this includes making
    cells empty when the persistent row has no such cell value. -*/

AB_PUBLIC_API(ab_bool) /* abrow.c */
AB_Row_ReadTableRow(AB_Row* self, AB_Env* ev, ab_row_uid id);
    /*- ReadTableRow reads the row in the table known by id, filling cell
    values from the persistent content for the row found in the table. All cells
    are given values read from row id in the table, and this includes making
    cells empty when the persistent row has no such cell value. If cells in
    self are too small to read the entire content from the table, only the -*/

AB_PUBLIC_API(ab_bool) /* abrow.c */
AB_Row_GrowToReadEntireTableRow(AB_Row* self, AB_Env* ev,
         ab_row_uid id, ab_cell_size maxCellSize);
    /*- GrowToReadEntireTableRow reads the row in the table known by id,
    filling cell values from the persistent content for the row found in the
    table. All cells are given values read from row id in the table, and this
    includes making cells empty when the persistent row has no such cell
    value. 

    In contrast to AB_Row_ReadTableRow() which does not change cell
    structure to read content from the table, GrowToReadEntireTableRow
    will add cells as needed and grow the size of cells as needed in order to
    hold every byte of every persistent cell value. In other words,
    GrowToReadEntireTableRow will read the entire persistent content of
    the table row while growing to the extent necessary to capture all this
    content. 

    The maxCellSize parameter can be zero to cause it to be ignored, but if
    nonzero, maxCellSize is used to cap cell growth so to no more than
    maxCellSize. This lets callers attempt to accomodate all persistent
    content up to some reasonable threshold over which the caller might no
    longer care whether more cell content is present. So if a persistent value
    is too big to fit in a cell, and is also bigger than maxCellSize, then the
    cell has its size increased only to maxCellSize before reading the cell
    value. -*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_PUBLIC_API(ab_row_pos) /* abrow.c */
AB_Row_UpdateTableRow(const AB_Row* self, AB_Env* ev, ab_row_uid id);
    /*- UpdateTableRow writes part of the row known by id in the table,
    updating all cells with non-empty content in self, so that empty cells in
    self have no effect on the persistent row in the table. Also, cells not
    described at all in self are not touched at all in the table. -*/

AB_PUBLIC_API(ab_row_pos) /* abrow.c */
AB_Row_ResetTableRow(const AB_Row* self, AB_Env* ev, ab_row_uid id);
    /*- ResetTableRow writes all of the row known by id in the table, updating
    all of the table row cells, whether they are described by self or not.
    Cells not in self and empty cells in self are removed from row id.
    Non-empty cells in self have their values written to row id so that each
    table row cell contains only the content described by self. -*/


/* ----- ----- ----- ----- Cells ----- ----- ----- ----- */


#ifndef AB_Cell_typedef
typedef struct AB_Cell AB_Cell;
#define AB_Cell_typedef 1
#endif

struct AB_Cell {         /* interface to buffer a single row attribute */
  ab_column_uid   sCell_Column;   /* the column associated with this row cell */
  ab_cell_size    sCell_Size;     /* the size of sCell_Content in bytes */
  ab_cell_length  sCell_Length;   /* the length of content in sCell_Content */
  ab_cell_extent  sCell_Extent;   /* the amount of persistent cell content */
  char*           sCell_Content;  /* buffer to hold cell content */
};


enum AB_Cell_eError { /* AB_Cell errors */
    AB_Cell_kFaultOutOfMemory = /*i*/ AB_Fault_kCell, /* 500 */
    AB_Cell_kFaultBadColumnUid,                 /*i*/ /* 501 */
    AB_Cell_kFaultNullContent,                  /*i*/ /* 502 */
    AB_Cell_kFaultZeroCellSize,                 /*i*/ /* 503 */
    AB_Cell_kFaultSizeExceedsMax,               /*i*/ /* 504 */
    AB_Cell_kFaultSizeTooSmall,                 /*i*/ /* 505 */
    AB_Cell_kFaultLengthExceedsSize             /*i*/ /* 506 */
};

AB_PUBLIC_API(ab_bool) /* abrow.c */
AB_Row_ClearAllCells(AB_Row* self, AB_Env* ev);
    /*- ClearAllCells makes all the cells in this row empty of content. -*/

AB_PUBLIC_API(ab_bool) /* abrow.c */
AB_Row_WriteCell(AB_Row* self, AB_Env* ev, 
          const char* content, ab_column_uid col);
    /*- WriteCell ensures that row self has a cell with column col and sets
    the content of this cell to content, with length strlen(content). If
    the row did not previously have such a cell, it gets one just as if
    AB_Row_AddCell() had been called. Also, if the length of content is
    greater than the old size of the cell, the cell's size is increased to make it
    big enough to hold all the content (also just as if AB_Row_AddCell() had
    been called). 

    (WriteCell is implemented by calling AB_Row_PutCell().) -*/

AB_PUBLIC_API(ab_bool) /* abrow.c */
AB_Row_PutCell(AB_Row* self, AB_Env* ev, const AB_Cell* c);
    /*- PutCell is the internal form of WriteCell which assumes the length of
    the content to write is already known. Only the sCell_Content,
    sCell_Length, and sCell_Column slots are used from c. -*/

AB_PUBLIC_API(ab_cell_count) /* abrow.c */
AB_Row_CountCells(const AB_Row* self, AB_Env* ev);
    /*- CountCells returns the number of cells in this row. -*/

AB_PUBLIC_API(ab_cell_count) /* abrow.c */
AB_Row_GetCells(const AB_Row* self, AB_Env* ev,
          AB_Cell* outVector, ab_cell_count inSize, ab_cell_count* outLength);
    /*- GetCells returns a copy of the row's cells in outVector. The actual
    number of row cells is returned as the function value, but the size of
    outVector is assumed to be inSize, so no more than inSize cells will
    be written to outVector. The actual number of cells written is returned
    in outLength (unless outLength is a null pointer to suppress this
    output). 

    If inSize is greater than the number of cells, N, then outVector[N] will
    be null terminated by placing all zero values in all the cell slots. 

    Each sCell_Content slot points to space owned by the row. This space
    might change whenever the row changes, so callers must not cause the
    row to change while cell content is still being accessed. -*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_PUBLIC_API(const AB_Cell*) /* abrow.c */
AB_Row_GetCellAt(const AB_Row* self, AB_Env* ev, ab_cell_pos pos);
    /*- GetCellAt returns a pointer to a AB_Cell inside the self instance. This
    is a potentially dangerous exposure of internal structure for the sake of
    performance. Callers are expected to use this cell only long enough to
    see cell slots like sCell_Content that might be needed, and then forget
    this pointer as soon as possible. 

    Whenever the row is changed so that cell structure is modified, this
    AB_Cell instance might no longer exist afterwards. So using this cell
    must be only of transient nature. Clients should call GetCellAt again
    later, each time a cell is desired. 

    GetCellAt returns a null pointer if pos is zero, or if pos is greater
    than the number of cells (AB_Row_CountCells()). The cells have
    one-based positions numbered from one to the number of cells. 

    The cell returned must not be modified, on pain of undefined behavior.
    Callers must not assume anything about adjacency of other cells in the
    row. Cells might be stored discontiguously. -*/

AB_PUBLIC_API(const AB_Cell*) /* abrow.c */
AB_Row_GetColumnCell(const AB_Row* self, AB_Env* ev, ab_column_uid id);
    /*- GetColumnCell is just like AB_Row_GetCellAt() except that the cell is
    found by matching the column uid. If the row has no such column, then
    a null pointer is returned. -*/

AB_PUBLIC_API(ab_bool) /* abrow.c */
AB_Row_AddCells(AB_Row* self, AB_Env* ev, const AB_Cell* inVector);
    /*- AddCells changes the row's cell structure to include the cells described
    in inVector which must be null terminated by a zero value in the
    sCell_Column slot after the last cell to be added. Only the
    sCell_Column and sCell_Size slots matter and other slots are ignored.

    The sCell_Column slot values should be all distinct, without duplicate
    columns. If inVector has duplicate columns, the last one wins and no
    error occurs (so callers should check before calling if they care). 

    AddCells is implemented by calling AB_Row_AddCell(). -*/

#define AB_Cell_kMaxCellSize (32 * 1024)
    /*- kMaxCellSize is the maximum size cell. It's main function is to sanity
    check reasonable values for cell size passed to AB_Row_AddCell(), but
    this is also effectively the largest piece of contiguous memory we want
    to guarantee we can allocate in cross platform code. -*/

AB_PUBLIC_API(AB_Cell*) /* abrow.c */
AB_Row_AddCell(AB_Row* self, AB_Env* ev, ab_column_uid id, ab_cell_size size);
    /*- AddCell gives the row another cell for column id which can hold size
    substract one content bytes. Both id and size must be nonzero, and
    id must denote a valid column in the address book, and size must be no
    greater than AB_Cell_kMaxCellSize. If the cell known by id
    previously existed, then it's size is changed to equal the maximum of
    either the old size or the new size. (To shrink the size of a cell, remove
    it first with AB_Row_CutCell() and then re-add the cell. The specified
    size behavior here is most convenient for upgrading rows to hold bigger
    content.) -*/

AB_PUBLIC_API(ab_bool) /* abrow.c */
AB_Row_CutCell(AB_Row* self, AB_Env* ev, ab_column_uid id);
	/*- CutCell removes any cell identified by column uid id. True is returned
	only if such a cell existed (and was removed). False returns when no
	such cell was found (or if an error occurs). No error occurs from cutting
	a cell which does not exist, because the desired end result -*/

AB_PUBLIC_API(char *) /* abrow.c */
AB_Row_AsVCardString(AB_Row* self, AB_Env* ev);
    /*- returned string allocated by ab_Env::CopyString() which advertises
    itself as using XP_ALLOC for space allocation.  A null pointer is
    typically returned in case of error.  (It would not be that safe
    to return an empty static string which could not be deallocated.) -*/

/* ----- ----- ----- ----- Stores ----- ----- ----- ----- */

#define AB_Store_kMinFootprint /*i*/ (48 * 1024)
#define AB_Store_kMinFootprintGrowth /*i*/ (32 * 1024)

#define AB_Store_kGoodFootprintSpace /*i*/ (512 * 1024)
#define AB_Store_kGoodFootprintGrowth /*i*/ (1024 * 1024)

#define AB_Store_kBigStartingFootprint /*i*/ (8 * 1024 * 1024)
#define AB_Store_kBigFootprintGrowth /*i*/ (4 * 1024 * 1024)

/* ````` making a new store instance (need AB_Env instance) ````` */

AB_PUBLIC_API(AB_Store*) /* abstore.cpp */
AB_Env_NewStore(AB_Env* self, const char* inFileName,
    ab_num inTargetFootprint);
    /*- Open and/or create a database named inFileName (which must
    yield type AB_StoreType_kTable from AB_FileName_GetStoreType(),
    or else an error occurs). -*/

/* ````` regarding file names for stores ````` */

typedef enum AB_Store_eType /*d*/ { 

  AB_StoreType_kUnknown = 0, /* unknown: cannot be opened or imported */ 
  AB_StoreType_kTable = 1,   /* use AB_Env_NewStore() to access store */ 
  AB_StoreType_kImport = 2,  /* import only using AB_Store_NewImportFile() */ 
  AB_StoreType_kStale = 3,   /* unsupported transient development format */ 
  AB_StoreType_kFuture = 4   /* unsupported furture final format */ 

} AB_Store_eType;

AB_PUBLIC_API(AB_Store_eType) /* abstore.cpp */
AB_FileName_GetStoreType(const char* inFileName, AB_Env* ev);
	/*- GetStoreType() returns an enum value that describes how the file
	can be handled.  Currently this is based on the file name itself, but
	later this might also involve looking inside the file to examine the
	bytes to determine correct usage.  The file can only be opened and
	used as an instance of AB_Store (using AB_Env_NewStore()) if the type
	equals AB_StoreType_kTable, which means the store supports the table
	interface.  Otherwise, if the file type is AB_StoreType_kImport, this
	means one should create a new database with an appropriate name (see
	AB_FileName_MakeNativeName()), and import this file.  (Or the new file
	might already exist from earlier, openable with AB_Env_NewStore().)
	Formats which cannot opened nor imported might be either "unknown"
	or "stale" (AB_StoreType_kUnknown or AB_StoreType_kStale), where the
	stale formats are recognizably retired transient development formats,
	while unknown formats might still be importable by other means.  When
	a "future" format is detected which has not yet been implemented, the
	type returned is AB_StoreType_kFuture, and presumably this means that
	neither opening nor importing is feasible (like stale formats) but
	that products with later versions should read the format.
	
	The native format will be ".na2", but we can use transient development
	formats before the final format with different suffixes like ".na0".
	Later a ".na0" format will return AB_StoreType_kStale.
	
	The imported formats will include ".ldi" and ".ldif" for LDIF, with
	".htm" and ".html" for HTML.  Later we'll support more formats.
	
	The unknown file formats might still be importable using more general
	purpose import code that can recognize a broader range of formats. -*/

AB_PUBLIC_API(void) /* abstore.cpp */
AB_FileName_GetNativeSuffix(char* outFileNameSuffix8, AB_Env* ev);
	/*- GetNativeSuffix() writes into outFileNameSuffix8 the file name 
	extension suffix (for example, ".na2") that is preferred for the
	current native database format created for new database files.  The
	space should be at least eight characters in length for safety, though
	in practice five bytes will be written: a period, followed by three
	alphanumeric characters, followed by a null byte.  Whenever one wishes
	to make a new database, the filename for the database should end with
	this string suffix.  (Note that implementations might return more than
	one string suffix, assuming the environment has access to prefs or some
	other indication of which format to choose among possibilities.) -*/

AB_PUBLIC_API(ab_bool) /* abstore.cpp */
AB_FileName_HasNativeSuffix(const char* inFileName, AB_Env* ev);
	/*- HasNativeSuffix() returns true only if inFileName ends with a
	suffix equal to that returned by AB_FileName_GetNativeSuffix(). -*/

AB_PUBLIC_API(const char*) /* abstore.cpp */
AB_FileName_FindSuffix(const char* inFileName);
	/*- FindSuffix() returns the address of the last period "." in the
	string inFileName, or null if no period is found at all.  This method
	is used to locate the suffix for comparison in HasNativeSuffix(). 
	Typically file format judgments will be made on the three characters
	that follow the final period in the filename. -*/

AB_PUBLIC_API(char*) /* abstore.cpp */
AB_FileName_MakeNativeName(const char* inFileName, AB_Env* ev);
	/*- MakeNativeName() allocates a new string (which must be freed
	later using XP_FREE()) for which AB_FileName_HasNativeSuffix()
	is true.  If inFileName already has the native suffix, the string
	returned is a simple copy; otherwise the returned string appends
	the value of AB_FileName_GetNativeSuffix() to the end. (Note that
	the environment might select one of several format choices.) -*/

/* ````` destroying an old store instance (need AB_Env instance) ````` */

AB_API_IMPL(void) /* abstore.cpp */
AB_Env_DestroyStoreWithFileName(AB_Env* self, const char* inFileName);
    /* DestroyStoreWithFileName() deletes the file from the file system
     containing the store that would be opened if a store instance was
     created using inFileName for the file name. This method is provided
     to keep file destruction equally as abstract as the implicit file
     creation occuring when a new store is created.  Otherwise it would
     be difficult to discard databases as easily as they are created. But
     this method is dangerous in the sense that destruction of data is
     definitely intended, so don't call this method unless the total
     destruction of data in the store is what you have in mind.  This
     method does not attempt to verify the file contains a database before
     destroying the file with name inFileName in whatever directory is
     used to host the database files.  Make sure you name the right file. */

    
/* ````` opening store CONTENT ````` */

AB_PUBLIC_API(void) /* abstore.cpp */
AB_Store_OpenStoreContent(AB_Store* self, AB_Env* ev);

/* ````` closing store CONTENT (aborts unsaved changes) ````` */

AB_PUBLIC_API(void) /* abstore.cpp */
AB_Store_CloseStoreContent(AB_Store* self, AB_Env* ev); /* abort */

/* ````` saving store CONTENT (commits unsaved changes) ````` */

AB_PUBLIC_API(void) /* abstore.cpp */
AB_Store_SaveStoreContent(AB_Store* self, AB_Env* ev); /* commit */

/* ````` importing store CONTENT ````` */

AB_PUBLIC_API(void) /* abstore.cpp */
AB_Store_ImportWithPromptForFileName(AB_Store* self, AB_Env* ev, MWContext* ioContext);

AB_PUBLIC_API(void) /* abstore.cpp */
AB_Store_ImportEntireFileByName(AB_Store* self, AB_Env* ev, const char* inFileName);
 /* ImportEntireFileByName() just calls ImportFileByName() with a
	thumb instance intended to convey "all the file content". */

/* ````` exporting store CONTENT ````` */

AB_PUBLIC_API(void) /* abstore.cpp */
AB_Store_ExportWithPromptForFileName(AB_Store* self, AB_Env* ev, MWContext* ioContext);


/* ````` thumbs ````` */

AB_PUBLIC_API(AB_Thumb*) /* abthumb.cpp */
AB_Store_NewThumb(AB_Store* self, AB_Env* ev, ab_row_count inRowLimit,
	ab_num inFileByteCountLimit);

AB_PUBLIC_API(ab_bool) /* abthumb.cpp */
AB_Thumb_IsProgressFinished(const AB_Thumb* self, AB_Env* ev);

AB_PUBLIC_API(void) /* abthumb.cpp */
AB_Thumb_SetPortingLimits(AB_Thumb* self, AB_Env* ev, ab_row_count inRowLimit,
	ab_num inFileByteCountLimit);

AB_PUBLIC_API(ab_ref_count) /* abthumb.cpp */
AB_Thumb_Acquire(AB_Thumb* self, AB_Env* ev);

AB_PUBLIC_API(ab_ref_count) /* abthumb.cpp */
AB_Thumb_Release(AB_Thumb* self, AB_Env* ev);

/*| ImportProgress: return the current position in the import file, and also
**| the actual length of the import file in outFileLength, since this can be
**| used to show percentage progress through the import file.  However, we
**| can only show progress through the current pass through the file, which
**| is returned in outPass (typically either 1 or 2 for first or second pass).
**|
**|| Note that when outPass turns over from 1 to 2, the progress through the
**| file will revert back to a smaller percentage of the file, so a progress
**| bar might want to reveal to users which pass is currently in progress.
|*/
AB_PUBLIC_API(ab_pos) /* abthumb.cpp */
AB_Thumb_ImportProgress(AB_Thumb* self, AB_Env* ev, ab_pos* outFileLength,
	 ab_count* outPass);

/*| ExportProgress: return the row position of the row that was last exported,
**| and also the total number of rows to be exported in outTotalRows, since this
**| can be used to show percentage progress through the export task.
|*/
AB_PUBLIC_API(ab_row_pos) /* abthumb.cpp */
AB_Thumb_ExportProgress(AB_Thumb* self, AB_Env* ev, ab_row_count* outTotalRows);

typedef enum AB_File_eFormat {
	AB_File_kUnknownFormat = 0x3F3F3F3F,   /* '????' */
	AB_File_kLdifFormat = 0x6C646966,      /* 'ldif' */
	AB_File_kHtmlFormat = 0x68746D6C,      /* 'hmtl' */
	AB_File_kXmlFormat = 0x61786D6C,       /* 'axml' */
	AB_File_kVCardFormat = 0x76637264,     /* 'vcrd' */
	AB_File_kBinaryFormat = 0x626E7279,    /* 'bnry' */
	AB_File_kNativeFormat = 0x6E617476     /* 'natv' current native format */
} AB_File_eFormat;

/*| ImportFileFormat: describe the current file format being imported
|*/
AB_PUBLIC_API(AB_File_eFormat) /* abthumb.cpp */
AB_Thumb_ImportFileFormat(AB_Thumb* self, AB_Env* ev);

/* ````` file-and-thumb based import and export ````` */

AB_PUBLIC_API(AB_File*) /* abstore.cpp */
AB_Store_NewImportFile(AB_Store* self, AB_Env* ev, const char* inFileName);

AB_PUBLIC_API(ab_bool) /* abstore.cpp, true iff !AB_Thumb_IsProgressFinished() */
AB_Store_ContinueImport(AB_Store* self, AB_Env* ev, AB_File* ioFile,
	 AB_Thumb* ioThumb);

AB_PUBLIC_API(AB_File*) /* abstore.cpp */
AB_Store_NewExportFile(AB_Store* self, AB_Env* ev,
	const char* inFileName);

AB_PUBLIC_API(ab_bool) /* abstore.cpp, true iff !AB_Thumb_IsProgressFinished() */
AB_Store_ContinueExport(AB_Store* self, AB_Env* ev, AB_File* ioFile,
	 AB_Thumb* ioThumb);

AB_PUBLIC_API(ab_ref_count) /* abfile.cpp */
AB_File_Acquire(AB_File* self, AB_Env* ev);

AB_PUBLIC_API(ab_ref_count) /* abfile.cpp */
AB_File_Release(AB_File* self, AB_Env* ev);


/* ````` batching changes (these calls should nest okay) ````` */

AB_PUBLIC_API(void) /* abstore.cpp, commit every inEventThreshold changes */
AB_Store_StartBatchMode(AB_Store* self, AB_Env* ev,
	ab_num inEventThreshold);

AB_PUBLIC_API(void) /* abstore.cpp */
AB_Store_EndBatchMode(AB_Store* self, AB_Env* ev);


/* ````` open/close content status querying ````` */

AB_PUBLIC_API(ab_bool) /* abstore.cpp */
AB_Store_IsOpenStoreContent(AB_Store* self);

AB_PUBLIC_API(ab_bool) /* abstore.cpp */
AB_Store_IsShutStoreContent(AB_Store* self);
    
/* ````` closing store INSTANCE ````` */

AB_PUBLIC_API(void) /* abstore.cpp */
AB_Store_CloseObject(AB_Store* self, AB_Env* ev);
    
/* ````` store refcounting ````` */

AB_PUBLIC_API(ab_ref_count) /* abstore.cpp */
AB_Store_Acquire(AB_Store* self, AB_Env* ev);

AB_PUBLIC_API(ab_ref_count) /* abstore.cpp */
AB_Store_Release(AB_Store* self, AB_Env* ev);

/* ````` accessing top store table (with or without acquire)  ````` */

AB_PUBLIC_API(AB_Table*) /* abstore.cpp */
AB_Store_GetTopStoreTable(AB_Store* self, AB_Env* ev);

AB_PUBLIC_API(AB_Table*) /* abstore.cpp */
AB_Store_AcquireTopStoreTable(AB_Store* self, AB_Env* ev);


/* `````` `````` protos `````` `````` */
AB_END_C_PROTOS
/* `````` `````` protos `````` `````` */


#endif /* _ABTABLE_ */
