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

#ifndef _TDBexperimental_h_
#define _TDBexperimental_h_ 1

/* These are experimental APIs.  You should not use them in real code; you
   should only use them if you fully understand the fragility of the APIs,
   etc., etc.*/



/* TDBGetTripleID() gets a unique identifier that represents a triple in the
   database.  You can later call TDBFindTripleFromID() to get back the
   same triple.  This will return TDB_FAILURE if the given triple is not
   actually stored in the database.

   The top four bits of the returned ID is guaranteed to be zero. */

TDB_EXTERN(TDBStatus) TDBGetTripleID(TDB* database, TDBNodePtr triple[3],
                                   TDBInt64* id);



/* TDBFindTripleFromID() returns the triple that corresponds to the given ID.
   It is potentially fatal to call this with an illegal ID (though we try real
   hard to detect that case and return NULL instead).  The returned triple
   must be free'd with TDBFreeTriple().
  */

TDB_EXTERN(TDBTriple*) TDBFindTripleFromID(TDB* database, TDBInt64 id);


/* TDBFreeTriple() is used to free a triple returned from
   TDBFindTripleFromID().  It must *not* be used on a triple returned from
   TDBGetResult(); the TDB code is responsible for freeing those.  */

TDB_EXTERN(void) TDBFreeTriple(TDBTriple* triple);



#endif /* _TDBexperimental_h_ */
