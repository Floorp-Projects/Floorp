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
