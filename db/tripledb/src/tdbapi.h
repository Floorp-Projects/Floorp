/* -*- Mode: C; indent-tabs-mode: nil; -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is the TripleDB database library.
 * 
 * The Initial Developer of the Original Code is Geocast Network Systems.
 * Portions created by Geocast are Copyright (C) 1999 Geocast Network Systems.
 * All Rights Reserved.
 * 
 * Contributor(s): Terry Weissman <terry@mozilla.org>
 */

#ifndef _TDBapi_h_
#define _TDBapi_h_ 1

/* All things are prefixed with TDB, which stands for "Triples
   DataBase".  Suggestions for better names are always welcome. */

/* A TDBNode contains one of the three items in a triple.  This is a
   structure that defines a very basic type, strings or ints or dates.
   If we decide to add other kinds of basic types (floats? bools? unsigned?),
   then this is where we muck things.

   It is important that all nodes be strictly ordered.  All the integer values
   sort together in the obvious way.  PRTimes get sorted with them by treating
   them as if they were PRInt64's.  All strings are considered to be greater
   than all integers. */

#define TDBTYPE_INT8    1       /* 8-bit signed integer */
#define TDBTYPE_INT16   2       /* 16-bit signed integer */
#define TDBTYPE_INT32   3       /* 32-bit signed integer */
#define TDBTYPE_INT64   4       /* 64-bit signed integer */
#define TDBTYPE_TIME    5       /* NSPR date&time stamp (PRTime) */
#define TDBTYPE_STRING  6       /* A string (up to 65535 chars long) */

typedef struct {
    PRInt8 type;
    union {
        PRInt64 i;  /* All the int types are stored here, as an
                       Int64.  The type just indicates how it is to be
                       stored in the database. */

        struct {
            PRUint16 length;
            char string[1];
        } str;
        PRTime time;
    } d;
} TDBNode, *TDBNodePtr;



/* A TDBTriple specifies one entry in the database.  This is generally thought
   of as (subject, verb, object). */

typedef struct {
    TDBNodePtr data[3];
} TDBTriple;


/* A TDBNodeRange specifies a range of values for a node.  If min is
   not NULL, then this matches only nodes that are greater than or
   equal to min.  If max is not NULL, then this matches only nodes
   that are less than or equal to max.  Therefore, if both min and
   max are NULL, then this specifies the range of all possible nodes.
   If min and max are not NULL, and are the same value, then it specifies
   exactly that value.  */

typedef struct {
    TDBNodePtr min;
    TDBNodePtr max;
} TDBNodeRange;



/* A TDBSortSpecification specifies what order results from a request should 
   come in.  I suspect that there will someday be much more to it than this. */

typedef struct {
    PRBool reverse;             /* If true, then return biggest results
                                   first.  Otherwise, the smallest
                                   stuff comes first. */
    PRInt16 keyorder[3];        /* Specify which keys to sort in.  If you use
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

                                   Practically speaking, the only reason to
                                   ever set the keyorder is if your request
                                   only gives values for one of the ranges,
                                   and you want to specify which order the
                                   other fields should be sorted in.  And
                                   keyorder[0] had better specify which
                                   entry is the non-NULL one, or things will
                                   be very slow.
                                */
} TDBSortSpecification;


/* A TDB* is an opaque pointer that represents an entire database. */

typedef struct _TDB TDB;



/* A TDBCursor* is an opaque pointer that represents a query that you 
   are getting results for. */

typedef struct _TDBCursor TDBCursor;


/* TDBCallbackFunction is for callbacks notifying of certain database
   changes. */

typedef void (*TDBCallbackFunction)(TDB* database, void* closure,
                                    TDBTriple* triple,
                                    PRInt32 action); /* One of the below
                                                        TDBACTION_* values. */

#define TDBACTION_ADDED    1    /* The given triple has been added to the
                                   database. */
#define TDBACTION_REMOVED  2    /* The given triple has been removed from the
                                   database. */




PR_BEGIN_EXTERN_C



/* Open a database (creating it if non-existant).  Returns NULL on failure. */

PR_EXTERN(TDB*) TDBOpen(const char* filename);


/* Close an opened database. Frees the storage for TDB; you may not use
   that pointer after that call. Will flush out any changes that have
   been made.  This call will fail if you have not freed up all of the
   cursors that were created with TDBQuery. */

PR_EXTERN(PRStatus) TDBClose(TDB* database);




/* TDBQuery() returns a cursor that you can use with TDBNextResult() to get 
   the results of a query.  It will return NULL on failure.  If the query
   is legal, but there are no matching results, this will *not* return 
   NULL; it will return a cursor that will have no results.

   If a single value is specified for both range[0] and range[1], then the
   results are guaranteed to be sorted by range[2].  If a single value is
   specified for both range[1] and range[2], then the results are guaranteed
   to be sorted by range[0].  No other sorting rules are promised (at least,
   not yet.)

   A NULL TDBSortSpecification can be provided, which will sort in the
   default manner. */

PR_EXTERN(TDBCursor*) TDBQuery(TDB* database, TDBNodeRange range[3],
                               TDBSortSpecification* sortspec);


/* TDBGetResult returns the next result that matches the cursor, and
   advances the cursor to the next matching entry.  It will return NULL
   when there are no more matching entries.  The returned triple must
   not be modified in any way by the caller, and is valid only until
   the next call to TDBGetResult(), or until the cursor is freed. */

PR_EXTERN(TDBTriple*) TDBGetResult(TDBCursor* cursor);


/* TDBFreeCursor frees the cursor. */

PR_EXTERN(PRStatus) TDBFreeCursor(TDBCursor* cursor);


/* TDBAddCallback calls the given function whenever a change to the database
   is made that matches the given range.  Note that no guarantees are made
   about which thread this call will be done in.  It is also possible (though
   not very likely) that by the time the callback gets called, further changes
   may have been made to the database, and the action being notified here has
   already been undone.  (If that happens, though, you will get another
   callback soon.)

   It is legal for the receiver of the callback to make any queries or
   modifications it wishes to the database.  It is probably not a good idea
   to undo the action being notified about; this kind of policy leads to
   infinite loops.

   The receiver should not take unduly long before returning from the callback;
   until the callback returns, no other callbacks will occur.
 */

PR_EXTERN(PRStatus) TDBAddCallback(TDB* database, TDBNodeRange range[3],
                                   TDBCallbackFunction func, void* closure);

/* TDBRemoveCallback will remove a callback that was earlier registered with a
   call to TDBAddCallback.  */

PR_EXTERN(PRStatus) TDBRemoveCallback(TDB* database, TDBNodeRange range[3],
                                      TDBCallbackFunction func, void* closure);


/* TDBRemove() removes all entries matching the given parameters from 
   the database.  */

PR_EXTERN(PRStatus) TDBRemove(TDB* database, TDBNodeRange range[3]);


/* TDBAdd() adds a triple into the database. */

PR_EXTERN(PRStatus) TDBAdd(TDB* database, TDBNodePtr triple[3]);


/* TDBReplace() looks for an existing entry that matches triple[0] and
   triple[1].  It deletes the first such entry found (if any), and
   then inserts a new entry.  The intention is to replace the "object"
   part of an existing triple with a new value.  It really only makes
   sense if you know up-front that there is no more than one existing
   triple with the given "subject" and "verb". */

PR_EXTERN(PRStatus) TDBReplace(TDB* database, TDBNodePtr triple[3]);



/* TDBCreateStringNode() is just a utility routine that correctly
   allocates a new TDBNode that represents the given string.  The
   TDBNode can be free'd using TDBFreeNode(). */

PR_EXTERN(TDBNodePtr) TDBCreateStringNode(const char* string);


/* TDBCreateIntNode() is just a utility routine that correctly
   allocates a new TDBNode that represents the given integer.  The
   TDBNode can be free'd using TDBFreeNode().  You must specify
   the correct TDBTYPE_* value for it. */

PR_EXTERN(TDBNodePtr) TDBCreateIntNode(PRInt64 value, PRInt8 type);

/* Free up a node created with TDBCreateStringNode or TDBCreateIntNode. */

PR_EXTERN(void) TDBFreeNode(TDBNodePtr node);


PR_END_EXTERN_C

#endif /* _TDBapi_h_ */
