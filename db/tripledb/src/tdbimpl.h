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
