/* -*- Mode: C; indent-tabs-mode: nil; -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the TripleDB database library.
 *
 * The Initial Developer of the Original Code is
 * Geocast Network Systems.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Weissman <terry@mozilla.org>
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

#ifndef _TDBapi_h_
#define _TDBapi_h_ 1

/* All things are prefixed with TDB, which stands for "Triples
   DataBase". */


/* TDB can be built with or without NSPR.  If built with NSPR, then it can be 
   built with thread support.  If you want to built it with some non-NSPR
   threading package, you'll have to do some hacking. */

#define TDB_USE_NSPR 1
#define TDB_USE_THREADS 1



#ifdef TDB_USE_NSPR
#include "prtypes.h"
#include "prtime.h"
#include "prmem.h"

typedef PRInt16         TDBInt16;
typedef PRInt32         TDBInt32;
typedef PRInt64         TDBInt64;
typedef PRInt8          TDBInt8;
typedef PRTime          TDBTime;
typedef PRUint8         TDBUint8;
typedef PRUint16        TDBUint16;
typedef PRUint32        TDBUint32;
typedef PRUint64        TDBUint64;

typedef PRBool          TDBBool;
#define TDB_TRUE        PR_TRUE
#define TDB_FALSE       PR_FALSE

#define TDB_BEGIN_EXTERN_C  PR_BEGIN_EXTERN_C
#define TDB_END_EXTERN_C    PR_END_EXTERN_C
#define TDB_EXTERN(t)       PR_EXTERN(t)

typedef struct PRFileDesc       TDBFileDesc;

#define tdb_NEWZAP(x)   PR_NEWZAP(x)
#define tdb_Malloc PR_Malloc
#define tdb_Calloc(x,y) PR_Calloc(x,y)
#define tdb_Realloc(x,y) PR_Realloc(x,y)
#define tdb_Free(x)     PR_Free(x)

#else

#include <stdio.h>

typedef short           TDBInt16;
typedef int             TDBInt32;
typedef long long       TDBInt64;
typedef signed char     TDBInt8;
typedef TDBInt64        TDBTime;
typedef unsigned char   TDBUint8;
typedef unsigned short  TDBUint16;
typedef unsigned long   TDBUint32;
typedef unsigned long long TDBUint64;

typedef int             TDBBool;
#define TDB_TRUE        1
#define TDB_FALSE       0

#define TDB_BEGIN_EXTERN_C
#define TDB_END_EXTERN_C
#define TDB_EXTERN(t) extern t

#define TDBFileDesc FILE

#define tdb_NEWZAP(x) ((x*)calloc(1, sizeof(x)))
#define tdb_Malloc malloc
#define tdb_Calloc(x,y) calloc(x,y)
#define tdb_Realloc(x,y) realloc(x,y)
#define tdb_Free(x) free(x)

#endif /* TDB_USE_NSPR */


typedef enum { TDB_FAILURE = -1, TDB_SUCCESS = 0 } TDBStatus;

/* A TDBNode contains one of the three items in a triple.  This is a
   structure that defines a very basic type, strings or ints or dates.
   If we decide to add other kinds of basic types (floats? bools? unsigned?),
   then this is where we muck things.

   It is important that all nodes be strictly ordered.  All the integer values
   sort together in the obvious way.  TDBTimes get sorted with them by treating
   them as if they were TDBInt64's.  All strings are considered to be greater
   than all integers. */

#define TDBTYPE_INT32   1       /* 32-bit signed integer */
#define TDBTYPE_INT64   2       /* 64-bit signed integer */
#define TDBTYPE_ID      3       /* A 64-bit unsigned identifier, generally used
                                   to represent an RDF resource. */
#define TDBTYPE_TIME    4       /* NSPR-style date&time stamp -- number of
                                   microseconds since midnight,
                                   1/1/1970, GMT. */
#define TDBTYPE_STRING  5       /* A string (up to 65535 chars long) */
#define TDBTYPE_BLOB    6       /* A blob, which is just like a string, except
                                   it is not considered to have searchable text
                                   in it.) */

typedef struct {
    TDBInt8 type;
    union {
        TDBInt64 i;  /* All the int types are stored here, as an
                        Int64.  The type just indicates how it is to be
                        stored in the database. */
        TDBUint64 id;           /* Unsigned 64-bit identifier. */

        struct {
            TDBUint16 length;
            char string[1];
        } str;                  /* Used for both blobs and strings. */
        TDBTime time;
    } d;
} TDBNode, *TDBNodePtr;



/* A TDBTriple specifies one entry in the database.  This is generally thought
   of as (subject, verb, object). */

typedef struct {
    TDBNodePtr data[3];
    TDBBool asserted;           /* If TRUE, then this is a normal triple.
                                   If FALSE, then this is a triple that we
                                   have explicitely turned off using
                                   TDBAddFalse().  The only way to get such
                                   triples from a query is to turn on the
                                   includefalse member of the
                                   TDBSortSpecification passed to
                                   TDBQuery(). */
} TDBTriple;



/* A TDBSortSpecification specifies what order results from a request should 
   come in.  I suspect that there will someday be much more to it than this. */

typedef struct {
    TDBBool reverse;            /* If true, then return biggest results
                                   first.  Otherwise, the smallest
                                   stuff comes first. */
    TDBInt16 keyorder[3];       /* Specify which keys to sort in.  If you use
                                   this, then each of the three entries must
                                   be a unique number between 0 and 2.  For
                                   example, if:
                                        keyorder[0] == 1
                                        keyorder[1] == 2
                                        keyorder[2] == 0
                                   then results will be returned sorted
                                   primarily by the middle value of each
                                   triple, with a secondary sort by the
                                   third value and a tertiary sort by
                                   the first value.

                                   You are not guaranteed to get things in
                                   this order; it is only a request.  In
                                   particular, in the current implementation,
                                   if keyorder[2] == 1, your request will
                                   be ignored.

                                   If the values of keyorder[] are not
                                   legitimately specified, then a default
                                   order will be selected (the system will
                                   pick the order it can do most
                                   efficiently.)

                                   Practically speaking, there is currently
                                   no reason to ever set this stuff.
                                */
    TDBBool includefalse;       /* Whether this query should include entries
                                   that were added using TDBAddFalse().  If
                                   so, such entries will have the asserted
                                   field the TDBTriple structure turned off. */
} TDBSortSpecification;


/* A TDBBase* is an opaque pointer that represents an entire database. */

typedef struct _TDBBase TDBBase;


/* A TDB* is an opaque pointer that represents a view on a database. */

typedef struct _TDB TDB;



/* A TDBCursor* is an opaque pointer that represents a query that you 
   are getting results for. */

typedef struct _TDBCursor TDBCursor;






TDB_BEGIN_EXTERN_C



/* TDBOpenBase() opens a database from a file (creating it if non-existant).
   Returns NULL on failure. */

TDB_EXTERN(TDBBase*) TDBOpenBase(const char* filename);


/* TDBOpenLayers() opens a database, looking at the given layers.  The layers
   are given in priority order (earlier listed layers override later ones).
   The first layer is the one that is to be changed by any calls that
   add or remove triples.

   ### Need to add here or somewhere a lecture on what 'layers' are all
   about. */

TDB_EXTERN(TDB*) TDBOpenLayers(TDBBase* base, TDBInt32 numlayers,
                               TDBInt32* layers);



/* TDBBlowAwayDatabaseFiles() blows away all database files associated with
   the given pathname.  This very much is throwing away real data; use this
   call with care! */

TDB_EXTERN(TDBStatus) TDBBlowAwayDatabaseFiles(const char* filename);



/* TDBGetFilename() returns the filename associated with the database.  The
   returned string should not be modified in any way. */

const char* TDBGetFilename(TDB* database);



/* TDBClose() closes an opened database.  Frees the storage for TDB; you may
   not use that pointer after that call.  Will flush out any changes that have
   been made.  This call will fail if you have not freed up all of the cursors
   that were created with TDBQuery. */

TDB_EXTERN(TDBStatus) TDBClose(TDB* database);



/* TDBCloseBase() closes the base database file.  This call will fail if you
   have not freed up all of the database views that were created with
   TDBOpenLayers(). */

TDB_EXTERN(TDBStatus) TDBCloseBase(TDBBase* base);


/* TDBSync() makes sure that any changes made to the database have been written
   out to disk.  It effectively gets called by TDBClose(), and may also be
   implicitely called from time to time. */

TDB_EXTERN(TDBStatus) TDBSync(TDB* database);





#ifdef TDB_USE_THREADS

/* TDBGetBG() returns the TDBBG* object (see tdbbg.h) that represents the
   thread tripledb uses for its background operations.  If you would like
   that thread to do some other background operations, you can queue them
   up.  That can interfere with the performance of tripledb, so you may
   want to create your own TDBBG* object instead. */

TDB_EXTERN(struct _TDBBG*) TDBGetBG(TDB* database);

#endif


/* TDBQuery() returns a cursor that you can use with TDBGetResult() to get 
   the results of a query.  It will return NULL on failure.  If the query
   is legal, but there are no matching results, this will *not* return 
   NULL; it will return a cursor that will have no results.

   If a member of a the given triple is not NULL, then only triples with the
   identical value in that position will be returned.  If it is NULL, then
   all possible triples are returned.

   If the only variation in triples are values that are of type
   TDBTYPE_INT24, then the triples will be sorted by those values.
   If a non-NULL sortspec is passed in, and it has the "reverse" field set,
   then these int24's will be sorted in descending order; otherwise, they
   will be ascending.


   A NULL TDBSortSpecification can be provided, which will make the query
   behave in the default manner. */

TDB_EXTERN(TDBCursor*) TDBQuery(TDB* database, TDBNodePtr triple[3],
                                TDBSortSpecification* sortspec);


/* TDBQueryWordSubstring() returns a cursor of all triples whose last element
   is a string and matches the given string.  It will only match by full words;
   that is, if you provide a partial word, it will not match strings that
   contain that word inside another one.  It will only return strings which
   contain the given string as a substring, ignoring case. */

TDB_EXTERN(TDBCursor*) TDBQueryWordSubstring(TDB* database,
                                             const char* string);


/* TDBGetResult() returns the next result that matches the cursor, and
   advances the cursor to the next matching entry.  It will return NULL
   when there are no more matching entries.  The returned triple must
   not be modified in any way by the caller, and is valid only until
   the next call to TDBGetResult(), or until the cursor is freed. */

TDB_EXTERN(TDBTriple*) TDBGetResult(TDBCursor* cursor);


/* TDBCursorGetTDB() returns the base TDB* object that the given cursor is
   working on. */

TDB_EXTERN(TDB*) TDBCursorGetTDB(TDBCursor* cursor);


/* TDBFreeCursor frees the cursor. */

TDB_EXTERN(TDBStatus) TDBFreeCursor(TDBCursor* cursor);


/* TDBRemove() removes all entries matching the given parameters from 
   the database.  */

TDB_EXTERN(TDBStatus) TDBRemove(TDB* database, TDBNodePtr triple[3]);


/* TDBAdd() adds a triple into the database.  The "owner" of each triple
   is recorded, so that we can later remove all things owned by a given
   owner. */

TDB_EXTERN(TDBStatus) TDBAdd(TDB* database, TDBNodePtr triple[3],
                             TDBUint64 owner);


/* TDBAddFalse() adds a triple into the database that explicitely asserts that
   this triple is *not* to be considered part of the database.  It is useful
   when this database gets merged into a bigger database (using
   TDBOpenMergedDatabase()).  In that case, this triple will not be returned
   by queries, even if it appears in an earlier database on the merge list. */

TDB_EXTERN(TDBStatus) TDBAddFalse(TDB* database, TDBNodePtr triple[3],
                                  TDBUint64 owner);


/* TDBReplace() looks for an existing entry that matches triple[0] and
   triple[1].  It deletes the first such entry found (if any), and
   then inserts a new entry.  The intention is to replace the "object"
   part of an existing triple with a new value.  It really only makes
   sense if you know up-front that there is no more than one existing
   triple with the given "subject" and "verb". */

TDB_EXTERN(TDBStatus) TDBReplace(TDB* database, TDBNodePtr triple[3],
                                 TDBUint64 owner);



/* TDBCreateStringNode() is just a utility routine that correctly
   allocates a new TDBNode that represents the given string.  The
   TDBNode can be free'd using TDBFreeNode(). */

TDB_EXTERN(TDBNodePtr) TDBCreateStringNode(const char* string);


/* TDBCreateIntNode() is just a utility routine that correctly
   allocates a new TDBNode that represents the given integer.  The
   TDBNode can be free'd using TDBFreeNode().  You must specify
   the correct TDBTYPE_* value for it. */

TDB_EXTERN(TDBNodePtr) TDBCreateIntNode(TDBInt64 value, TDBInt8 type);

/* Free up a node created with TDBCreateStringNode or TDBCreateIntNode. */

TDB_EXTERN(void) TDBFreeNode(TDBNodePtr node);


/* TDBCompareNodes() compares two nodes.  Returns negative if the first is less
   than the second, zero if they are equal, positive if the first is greater.
   Note That this returns a 64-bit int; be careful! */

TDB_EXTERN(TDBInt64) TDBCompareNodes(TDBNode* n1, TDBNode* n2);


/* TDBNodeDup allocates a new node object, and initializes it to have the
   same value as the given object.  Returns NULL on failure. */

TDB_EXTERN(TDBNodePtr) TDBNodeDup(TDBNodePtr node);




TDB_END_EXTERN_C

#endif /* _TDBapi_h_ */
