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
