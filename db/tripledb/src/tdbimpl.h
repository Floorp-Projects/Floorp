/* -*- Mode: C; indent-tabs-mode: nil; -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the TripleDB database library.
 *
 * The Initial Developer of the Original Code is Geocast Network Systems.
 * Portions created by Geocast are
 * Copyright (C) 2000 Geocast Network Systems. All
 * Rights Reserved.
 *
 * Contributor(s): Terry Weissman <terry@mozilla.org>
 */

#ifndef _TDBimpl_h_
#define _TDBimpl_h_ 1


typedef struct {
    TDBStatus (*close)(TDB* database);
    void* (*query)(TDB* database, TDBNodePtr triple[3],
                   TDBSortSpecification* sortspec);
    TDBTriple* (*getresult)(void* cursor);
    TDBStatus (*freecursor)(void* cursor);
    TDBStatus (*remove)(TDB* database, TDBNodePtr triple[3]);
    TDBStatus (*add)(TDB* database, TDBNodePtr triple[3], TDBUint64 owner);
    TDBStatus (*addfalse)(TDB* database, TDBNodePtr triple[3],
                          TDBUint64 owner);
    TDBStatus (*replace)(TDB* database, TDBNodePtr triple[3], TDBUint64 owner);
    TDBStatus (*gettripleid)(TDB* db, TDBNodePtr triple[3], TDBInt64* id);
    TDBTriple* (*findtriplefromid)(TDB* db, TDBInt64 id);
} TDBImplement;


TDB_BEGIN_EXTERN_C


/* TDBOpenImplementation() opens a database object which supports the calls
   listed in tdbapi.h, but whose implentation is provided by some other module.
   The implementation is provided as a bunch of function pointers.  Each of
   the TDB* routines that correspond to the pointers know to implement
   themselves entirely by calling the appropriate routine.

   NONE OF THESE FUNCTION POINTERS MAY BE NULL!  We don't allow inheritence
   from the base routines.  Sorry!

   The void* that are passed to and from the query(), getresult() and
   freecursor() routines are implementation-specific instances of the
   cursor object.  It is up to the implementation how they are done,
   although they will almost certainly need to include a pointer back to the
   database object, or to the impldata.

   The query() routine returns a pointer to a TDBImplCursor object.
   The db field in that object should be a pointer back to the
   database itself (the same pointer that was passed to query), and should
   never be modified.  The data field is implementation-specific, and is where
   the implementation should hang all state relating to the cursor.
 */


TDB_EXTERN(TDB*) TDBOpenImplementation(TDBImplement* impl);


/* TDBSetImplData() sets implementation-specific data associated with the
   database. */

TDB_EXTERN(TDBStatus) TDBSetImplData(TDB* database, void* impldata);

/* TDBGetImplData() gets any implementation-specific data associated with the
   database. */

TDB_EXTERN(void*) TDBGetImplData(TDB* database);


/* TDBValidateSortOrder takes a sortspec and range, and makes sure the
   keyorder field in the sortspec is correctly filled out.
 */

TDB_EXTERN(void) TDBValidateSortOrder(TDB* tdb, TDBSortSpecification* sortspec,
                                      TDBNodePtr triple[3]);


TDB_END_EXTERN_C


#endif /* _TDBimpl_h_ */
