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

#ifndef _tdb_h_
#define _tdb_h_ 1


/* tdbGetFID() gets the file descripter for the database file. */

extern TDBFileDesc* tdbGetFID(TDBBase* base);


/* tdbMarkCorrupted() marks the tdb as being corrupted. */

extern void tdbMarkCorrupted(TDBBase* base);


/* tdbMarkTooBustedToRecover() marks the tdb as being so corrupted that we
   shouldn't try to recover from it. */

extern void tdbMarkTooBustedToRecover(TDBBase* base);


/* tdbGetFileLength() gets the current length of the file. */

extern TDBPtr tdbGetFileLength(TDBBase* base);


/* tdbSetFileLength() sets in memory a new idea as to the current
   length of the file.  Should only be used by the page.c module, when
   it adds a new page to the file. */

extern TDBStatus tdbSetFileLength(TDBBase* base, TDBPtr length);



/* tdbGetFirstRType() returns the first rtype.  This should only be used by
   the rtype.c module. */

extern TDBRType* tdbGetFirstRType(TDBBase* base);


/* tdbSetFirstRType() sets the first rtype.  This should only be used by
   the rtype.c module. */

extern TDBStatus tdbSetFirstRType(TDBBase* base, TDBRType* rtype);


/* tdbGetFirstCursor() returns the first cursor.  This should only be used by
   the cursor.c module. */

extern TDBCursor* tdbGetFirstCursor(TDB* tdb);


/* tdbSetFirstCursor() sets the first cursor.  This should only be used by
   the cursor.c module. */

extern TDBStatus tdbSetFirstCursor(TDB* tdb, TDBCursor* cursor);


extern void tdbBeginExclusiveOp(TDBBase* base);

extern void tdbEndExclusiveOp(TDBBase* base);

extern char* tdbGrowTmpBuf(TDBBase* base, TDBInt32 newsize);

extern TDBInt32 tdbGetTmpBufSize(TDBBase* base);

extern DB* tdbGetRecordDB(TDBBase* base);

extern TDBBase* tdbGetBase(TDB* base);

extern TDBStatus tdbRecover(TDBBase* base);

extern TDBWindex* tdbGetWindex(TDBBase* base);

extern TDBPendingCall* tdbGetFirstPendingCall(TDBBase* base);

extern void tdbSetFirstPendingCall(TDBBase* base, TDBPendingCall* call);

extern TDBPendingCall* tdbGetLastPendingCall(TDBBase* base);

extern void tdbSetLastPendingCall(TDBBase* base, TDBPendingCall* call);

extern DB_ENV* tdbGetDBEnv(TDBBase* base);

extern TDBIntern* tdbGetIntern(TDBBase* base);

extern DB_TXN* tdbGetTransaction(TDBBase* base); 

extern TDBInt32 tdbGetNumIndices(TDBBase* base);
extern TDBRType* tdbGetIndex(TDBBase* base, TDBInt32 which);

extern TDBInt32 tdbGetNumLayers(TDB* tdb);
extern TDBInt32 tdbGetLayer(TDB* tdb, TDBInt32 which);
extern TDBInt32 tdbGetOutLayer(TDB* tdb);

#endif /* _tdb_h_ */
