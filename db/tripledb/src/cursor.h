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

#ifndef _cursor_h_
#define _cursor_h_ 1

extern TDBCursor* tdbCursorNew(TDB* tdb, TDBRType* indextype,
                               TDBNodePtr* triple,
                               TDBBool includefalse, TDBBool reverse);


extern TDBCursor* tdbCursorNewWordSubstring(TDB* tdb, const char* string);

extern TDBCursor* tdbCursorNewImpl(TDB* tdb, void* implcursor);

extern void tdbCursorFree(TDBCursor* cursor);

extern TDB* tdbCursorGetTDB(TDBCursor* cursor);

extern TDBBase* tdbCursorGetBase(TDBCursor* cursor);

extern void* tdbCursorGetImplcursor(TDBCursor* cursor);

/* tdbCursorGetNext() returns the next matching item for this cursor.  The
   returned vector is owned by the cursor object, and must not be freed or
   modified. */

extern TDBVector* tdbCursorGetNext(TDBCursor* cursor);

/* tdbCursorGetLastResultAsTriple() is a hack that returns the
   last result as a TDBTriple*, to help us implement our published API
   (which has drifted a bit from our internal implementation.) */

extern TDBTriple* tdbCursorGetLastResultAsTriple(TDBCursor* cursor);


/* tdbCursorInvalidateCache() causes all cursors to invalidate any
   information about the tree that they may have cached.  This is called when
   a change is going to be made to the tree structure. */

extern TDBStatus tdbCursorInvalidateCache(TDB* tdb);


#endif /* _cursor_h_ */
