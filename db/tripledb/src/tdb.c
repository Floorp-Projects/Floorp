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

#define USE_TRANSACTIONS


#include <unistd.h>             /* For getpid() */

#include "tdbtypes.h"
#include "tdbimpl.h"

#include "cursor.h"
#include "dbnspr.h"
#include "intern.h"
#include "rtype.h"
#include "tdb.h"
#include "util.h"
#include "vector.h"
#include "windex.h"

#ifdef TDB_USE_THREADS
#include "tdbbg.h"
#endif

struct _TDBBase {
    char* filename;

    TDBLock* mutex;              /* Used to prevent more than one thread
                                    from doing anything in DB code at the
                                    same time. */
    TDBCallbackInfo* firstcallback;
    TDBPendingCall* firstpendingcall;
    TDBPendingCall* lastpendingcall;

    TDBRType* firstrtype;

    TDBBool corrupted;
    TDBBool toobustedtorecover;

    DB_ENV* env;
    DB* miscdb;                 /* Database to store miscellaneous facts
                                   (e.g., tdb version number). */
    DB* recorddb;               /* Database that stores full records about
                                   each triple. */

    DB_TXN* transaction;

    TDBIntern* intern;

    int numindices;
    TDBRType** index;

    TDBWindex* windex;

    char* tmpbuf;
    TDBInt32 tmpbufsize;

    TDBBool dirty;              /* Whether there are changes in the db that
                                   we have not sync'd back to disk. */

    TDB* firsttdb;

#ifdef TDB_USE_THREADS
    TDBBG* bgthread;
#endif
};


struct _TDB {
    TDBBase* base;
    TDBCursor* firstcursor;
    TDBImplement* impl;
    void* impldata;
    TDB* nexttdb;
    TDBInt32 numlayers;
    TDBInt32* layers;
    TDBInt32 outlayer;
};    


void
tdbMarkCorrupted(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return;
    }
    base->corrupted = TDB_TRUE;
}


void
tdbMarkTooBustedToRecover(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return;
    }
    base->toobustedtorecover = TDB_TRUE;
    base->corrupted = TDB_TRUE;
}


TDBRType*
tdbGetFirstRType(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return base->firstrtype;
}


TDBStatus
tdbSetFirstRType(TDBBase* base, TDBRType* rtype)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    base->firstrtype = rtype;
    return TDB_SUCCESS;
}


TDBCursor*
tdbGetFirstCursor(TDB* tdb)
{
    if (tdb == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return tdb->firstcursor;
}


TDBStatus
tdbSetFirstCursor(TDB* tdb, TDBCursor* cursor)
{
    if (tdb == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    tdb->firstcursor = cursor;
    return TDB_SUCCESS;
}



static int Order[3][3] = {
    {0, 1, 2},
    {1, 2, 0},
    {2, 1, 0}
};

static void 
panicFunc(DB_ENV* env, int errval)
{
    tdb_ASSERT(0);              /* XXX Write me!!! ### */
    /*    tdbMarkTooBustedToRecover(WHO?); */
}


#ifdef TDB_USE_NSPR
#include "prprf.h"
#define tdb_fprintf     PR_fprintf
#define tdb_stderr      PR_STDERR
#else
#define tdb_fprintf     fprintf
#define tdb_stderr      stderr
#endif

static void
errorFunc(const char* prefix, char* msg)
{
    tdb_fprintf(tdb_stderr, "DB ERROR: %s %s\n", prefix, msg);
    tdb_ASSERT(0);
}

static TDBStatus realSync(TDBBase* base);

#ifdef TDB_USE_THREADS
static void
dosync(void* closure)
{
    TDBBase* base = (TDBBase*) closure;
    realSync(base);
}
#endif


void
tdbMarkDirty(TDBBase* base)
{
    if (!base->dirty) {
        base->dirty = TDB_TRUE;
#ifdef TDB_USE_THREADS
        TDBBG_AddFunction(base->bgthread, "sync", 10, TDBBG_PRIORITY_LOW,
                          dosync, base);
#endif
    }
}


#ifdef TDB_USE_NSPR
static TDBStatus
tdb_GetFileType(const char* path, TDBBool* isdir)
{
    PRFileInfo64 info;
    if (PR_GetFileInfo64(path, &info) != PR_SUCCESS) return TDB_FAILURE;
    if (isdir) {
        *isdir = (info.type == PR_FILE_DIRECTORY);
    }
    return TDB_SUCCESS;
}
#else
static TDBStatus
tdb_GetFileType(const char* path, TDBBool* isdir)
{
    struct stat sbuf;
    if (stat(path, &sbuf)) return TDB_FAILURE;
    if (isdir) {
        *isdir = S_ISDIR(sbuf.st_mode);
    }
    return TDB_SUCCESS;
}
#endif

static TDBStatus
openGuts(TDBBase* base, const char* filename)
{
    TDBInt32 i;
    TDBInt32 j;
    DBT key;
    DBT data;
    char buf[32];
    char* ptr;
    char* endptr;
    TDBUint16 major;
    TDBUint16 minor;
    int dbstatus;
    TDBBool isdir;
    TDBBool needdir;

    if (base == NULL || filename == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    base->filename = tdb_strdup(filename);
    if (base->filename == NULL) {
        return TDB_FAILURE;
    }

    needdir = TDB_FALSE;
    if (tdb_GetFileType(filename, &isdir) != TDB_SUCCESS) {
        needdir = TDB_TRUE;
    } else {
        if (!isdir) {
            /* Ooh, looky that, a file.  This is either something really
               old-fashioned, or something very busted and wrong happened.
               Either way, we cope by simply nuking it. */
            if (tdb_Delete(filename) != TDB_SUCCESS) {
                return TDB_FAILURE;
            }
            needdir = TDB_TRUE;
        }
    }
    if (needdir) {
        if (tdb_MkDir(filename, 0777) != TDB_SUCCESS) return TDB_FAILURE;
    }
            
    if (db_env_create(&(base->env), 0) != DB_OK) goto FAIL;


#ifdef TDB_USE_NSPR
    if (tdbMakeDBCompatableWithNSPR(base->env) != TDB_SUCCESS) goto FAIL;
#endif


    if (base->env->set_cachesize(base->env, 0, 5 * 1024 * 1024, 0) != DB_OK) {
        goto FAIL;
    }
    base->env->set_paniccall(base->env, panicFunc);
    base->env->set_errcall(base->env, errorFunc); /* Hack. */

/*      (void)base->env->set_verbose(base->env, DB_VERB_RECOVERY, 1); */
/*      (void)base->env->set_verbose(base->env, DB_VERB_CHKPOINT, 1); */

    dbstatus =
        base->env->open(base->env, filename,
                       DB_INIT_MPOOL |
#ifdef USE_TRANSACTIONS
                       DB_INIT_TXN | DB_INIT_LOG | DB_TXN_NOSYNC |
#endif
                       DB_RECOVER | DB_RECOVER_FATAL | DB_CREATE |
                       DB_PRIVATE /* | DB_THREAD */,
                       0666);
    if (dbstatus != DB_OK) goto FAIL;

    if (db_create(&(base->miscdb), base->env, 0) != DB_OK) goto FAIL;
    dbstatus = base->miscdb->open(base->miscdb, "misc", NULL, DB_HASH,
                                  DB_CREATE, 0666);
    if (dbstatus != DB_OK) {
        if (dbstatus != DB_OLD_VERSION) {
            goto FAIL;
        }
        tdbMarkTooBustedToRecover(base);
        return TDB_SUCCESS;
    }
    if (db_create(&(base->recorddb), base->env, 0) != DB_OK) goto FAIL;
    if (base->recorddb->open(base->recorddb, "record", NULL, DB_RECNO,
                             DB_CREATE, 0666) != DB_OK)  goto FAIL;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = "version";
    key.size = 7;
    dbstatus = base->miscdb->get(base->miscdb, NULL, &key, &data, 0);
    if (dbstatus == DB_NOTFOUND && needdir) {
        ptr = buf;
        endptr = buf + sizeof(buf);
        tdbPutUInt16(&ptr, TDB_MAJOR_VERSION, endptr);
        tdbPutUInt16(&ptr, TDB_MINOR_VERSION, endptr);
        tdb_ASSERT(ptr < endptr);
        data.data = buf;
        data.size = ptr - buf;
        dbstatus = base->miscdb->put(base->miscdb, NULL, &key, &data, 0);
        base->miscdb->sync(base->miscdb, 0);
    }

    if (dbstatus != DB_OK) {
        tdbMarkTooBustedToRecover(base);
    } else {
        ptr = data.data;
        endptr = ptr + data.size;
        major = tdbGetUInt16(&ptr, endptr);
        minor = tdbGetUInt16(&ptr, endptr);
        if (major != TDB_MAJOR_VERSION || ptr != endptr) {
            tdbMarkTooBustedToRecover(base);
        } else {
            if (minor != TDB_MINOR_VERSION) {
                tdbMarkCorrupted(base);
            }
        }
    }

    base->numindices = sizeof(Order) / sizeof(Order[0]);
    base->index = tdb_Calloc(base->numindices, sizeof(TDBRType*));
    if (base->index == NULL) goto FAIL;
    for (i=0 ; i<base->numindices ; i++) {
        char buf[20];
        memset(buf, 0, sizeof(buf));
        strcpy(buf, "index");
        for (j=0 ; j<3 ; j++) {
            buf[j+5] = Order[i][j] + '0';
        }
        base->index[i] = tdbRTypeNew(base, buf, i + 1, 3, Order[i]);
        if (base->index[i] == NULL) goto FAIL;
    }

    base->intern = tdbInternInit(base);
    if (!base->intern) goto FAIL;
    base->windex = tdbWindexNew(base);
    if (!base->windex) goto FAIL;

    return TDB_SUCCESS;

 FAIL:
    return TDB_FAILURE;
}


void
removeOldLogFiles(void* closure)
{
#ifdef USE_TRANSACTIONS
    TDBBase* base = (TDBBase*) closure;
    char** list;
    char** ptr;
    tdbBeginExclusiveOp(base);
    if (log_archive(base->env, &list, DB_ARCH_ABS, tdb_Malloc) == DB_OK) {
        if (list) {
            for (ptr = list ; *ptr ; ptr++) {
                tdb_Delete(*ptr);
            }
            tdb_Free(list);
        }
    }
#ifdef TDB_USE_THREADS
    TDBBG_AddFunction(base->bgthread, "removeOldLogFiles", 5 * 60,
                      TDBBG_PRIORITY_LOW, removeOldLogFiles, base);
#endif
    tdbEndExclusiveOp(base);
#endif
}


TDBBase*
TDBOpenBase(const char* filename)
{
    TDBBase* base;

    /* The below double-checks that whatever definition we're using for all our
       integer types really do work out to (more-or-less) the right thing. */

    tdb_ASSERT(sizeof(TDBInt8) == 1);
    tdb_ASSERT(sizeof(TDBInt16) == 2);
    tdb_ASSERT(sizeof(TDBInt32) == 4);
    tdb_ASSERT(sizeof(TDBInt64) == 8);
    tdb_ASSERT(sizeof(TDBUint8) == 1);
    tdb_ASSERT(sizeof(TDBUint16) == 2);

    tdb_ASSERT(filename != NULL);
    if (filename == NULL) return NULL;
    base = tdb_NEWZAP(TDBBase);
    if (base == NULL) return NULL;

    if (openGuts(base, filename) != TDB_SUCCESS) {
        TDBCloseBase(base);
        return NULL;
    }

#ifdef TDB_USE_THREADS
    base->mutex = PR_NewLock();
    base->bgthread = TDBBG_Open();
    if (base->mutex == NULL || base->bgthread == NULL) {
        TDBCloseBase(base);
        return NULL;
    }
#endif

    if (base->corrupted) {
        if (tdbRecover(base) != TDB_SUCCESS) {
            TDBCloseBase(base);
            return NULL;
        }
    }

    removeOldLogFiles(base);

    return base;
}


TDBStatus
TDBBlowAwayDatabaseFiles(const char* path)
{
#ifdef TDB_USE_NSPR
    PRDir* dir;
    PRDirEntry* entry;
    char* ptr;
    dir = PR_OpenDir(path);
    if (dir) {
        while (NULL != (entry = PR_ReadDir(dir, PR_SKIP_BOTH))) {
            ptr = PR_smprintf("%s/%s", path, entry->name);
            PR_Delete(ptr);
            PR_Free(ptr);
        }
        PR_CloseDir(dir);
    }
    return PR_RmDir(path);
#else
    tdb_ASSERT(0);              /* Need to write the non-NSPR version
                                   of this code! XXX*/
    return TDB_FAILURE;
#endif /* TDB_USE_NSPR */
}


static TDBStatus
closeGuts(TDBBase* base)
{
    TDB* tdb;
    if (base == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    if (base->transaction) {
        if (txn_commit(base->transaction, 0) != DB_OK) {
            tdb_ASSERT(0);
        }
        base->transaction = NULL;
    }
    for (tdb = base->firsttdb ; tdb ; tdb = tdb->nexttdb) {
        tdbCursorInvalidateCache(tdb);
    }
    /* ### Do I need to free up firstcallback and firstpendingcall stuff here?
       XXX */
    if (base->intern) {
        tdbInternFree(base->intern);
        base->intern = NULL;
    }
    if (base->windex) {
        tdbWindexFree(base->windex);
        base->windex = NULL;
    }
    while (base->firstrtype) {
        tdbRTypeFree(base->firstrtype);
    }
    if (base->filename) {
        tdb_Free(base->filename);
        base->filename = NULL;
    }
    if (base->index) {
        tdb_Free(base->index);
        base->index = NULL;
    }
    if (base->tmpbuf) {
        tdb_Free(base->tmpbuf);
        base->tmpbuf = NULL;
    }
    base->tmpbufsize = 0;
    if (base->miscdb) {
        base->miscdb->close(base->miscdb, 0);
        base->miscdb = NULL;
    }
    if (base->recorddb) {
        base->recorddb->close(base->recorddb, 0);
        base->recorddb = NULL;
    }
    if (base->env) {
        base->env->close(base->env, 0);
        base->env = NULL;
    }
    base->corrupted = base->toobustedtorecover = TDB_FALSE;
    base->dirty = TDB_FALSE;

    return TDB_SUCCESS;
}


TDBStatus
TDBCloseBase(TDBBase* base)
{
    TDBStatus status;
    if (base == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
#ifdef USE_TRANSACTIONS
    if (base->env) {
        int dbstatus;
        while ((dbstatus = txn_checkpoint(base->env,
                                          0, 0, 0)) == DB_INCOMPLETE) {}
        tdb_ASSERT(dbstatus == DB_OK);
    }
#endif
    status = closeGuts(base);
#ifdef TDB_USE_THREADS
    if (base->bgthread) {
        TDBBG_Close(base->bgthread);
        base->bgthread = NULL;
    }
    if (base->mutex) PR_DestroyLock(base->mutex);
#endif
    tdb_Free(base);
    return status;
}



static TDBStatus
realSync(TDBBase* base)
{
    TDBStatus status = TDB_FAILURE;
    int i;
    if (base == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    tdbBeginExclusiveOp(base);
#ifdef USE_TRANSACTIONS
    {
        int dbstatus;
        while ((dbstatus = txn_checkpoint(base->env, 0, 0, 0)) == DB_INCOMPLETE) {}
        if (dbstatus != DB_OK) {
            tdb_ASSERT(0);
            goto FAIL;
        }
    }
#endif
    if (base->miscdb->sync(base->miscdb, 0) != DB_OK) goto FAIL;
    if (base->recorddb->sync(base->recorddb, 0) != DB_OK) goto FAIL;
    for (i=0 ; i<base->numindices ; i++) {
        if (tdbRTypeSync(base->index[i]) != TDB_SUCCESS) goto FAIL;
    }
    if (tdbInternSync(base) != TDB_SUCCESS) goto FAIL;
    if (tdbWindexSync(base->windex) != TDB_SUCCESS) goto FAIL;
    status = TDB_SUCCESS;
    base->dirty = TDB_FALSE;

 FAIL:
    tdbEndExclusiveOp(base);
    return status;
}


TDBStatus
TDBSync(TDB* tdb)
{
    if (tdb == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    return realSync(tdb->base);
}

#ifdef TDB_USE_THREADS

TDBBG*
TDBGetBG(TDB* tdb)
{
    if (tdb == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return tdb->base->bgthread;
}


#endif /* TDB_USE_THREADS */


const char* 
TDBGetFilename(TDB* tdb)
{
    if (tdb == NULL || tdb->base == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return tdb->base->filename;
}


void
tdbBeginExclusiveOp(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return;
    }
    tdb_Lock(base->mutex);
    tdb_ASSERT(base->transaction == NULL);
#ifdef USE_TRANSACTIONS
    if (txn_begin(base->env, NULL, &(base->transaction), 0) != DB_OK) {
        tdb_ASSERT(0);
        base->transaction = NULL;
    } else {
        tdb_ASSERT(base->transaction != NULL);
    }
#endif
}

void
tdbEndExclusiveOp(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return;
    }
    if (base->transaction) {
        if (txn_commit(base->transaction, 0) != DB_OK) {
            tdb_ASSERT(0);
        }
        base->transaction = NULL;
    }
    tdb_Unlock(base->mutex);
}


static TDBStatus
doAdd(TDB* tdb, TDBNodePtr triple[3], TDBUint64 owner, TDBBool isassert)
{
    TDBVector* vector;
    TDBInt32 i;
    TDBBase* base = tdb->base;

    tdbCursorInvalidateCache(tdb);

    vector = tdbVectorNewFromNodes(base, triple, tdb->outlayer,
                                   isassert ? TDBFLAG_ISASSERT : 0, 0);
    if (!vector) return TDB_FAILURE;
    if (tdbVectorPutInNewRecord(vector, owner) != TDB_SUCCESS) {
        tdbVectorFree(vector);
        return TDB_FAILURE;
    }
    for (i=0 ; i<base->numindices ; i++) {
        if (tdbRTypeAdd(base->index[i], vector) != TDB_SUCCESS) {
            tdbVectorFree(vector);
            return TDB_FAILURE;
        }
    }
    if (tdbWindexAdd(base->windex, vector) != TDB_SUCCESS) {
        tdbVectorFree(vector);
        return TDB_FAILURE;
    }
    tdbVectorFree(vector);
    tdbMarkDirty(base);
    return TDB_SUCCESS;    
}






static TDBStatus
realAdd(TDB* tdb, TDBNodePtr triple[3], TDBUint64 owner, TDBBool assert)
{
    TDBStatus status;
    TDBBase* base;

    if (tdb == NULL || triple == NULL || triple[0] == NULL ||
        triple[1] == NULL || triple[2] == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }

    if (tdb && tdb->impl) {
        if (assert) {
            return (*(tdb->impl->add))(tdb, triple, owner);
        } else {
            return (*(tdb->impl->addfalse))(tdb, triple, owner);
        }
    }

    base = tdb->base;

    tdbBeginExclusiveOp(base);
    status = doAdd(tdb, triple, owner, assert);
    if (base->corrupted) {
        tdbRecover(base);
        status = doAdd(tdb, triple, owner, assert);
    }
    tdbEndExclusiveOp(base);

    return status;
}


TDBStatus
TDBAdd(TDB* tdb, TDBNodePtr triple[3], TDBUint64 owner)
{
    return realAdd(tdb, triple, owner, TDB_TRUE);
}


TDBStatus
TDBAddFalse(TDB* tdb, TDBNodePtr triple[3], TDBUint64 owner)
{
    return realAdd(tdb, triple, owner, TDB_FALSE);
}


static void
fillRangeScore(TDBInt32* rangescore, TDBNodePtr* triple, TDBInt32 numfields)
{
    TDBInt32 i;
    for (i=0 ; i<numfields ; i++) {
        rangescore[i] = 0;
        if (triple[i] != NULL) {
            /* Hey, some limitations were specified, we like this key
               some. */
            rangescore[i]++;
        }
    }
}



static TDBRType*
pickIndexType(TDB* tdb, TDBNodePtr* triple, TDBSortSpecification* sortspec)
{
    TDBRType* indextype;
    TDBInt32 numfields;
    TDBInt32 tree;
    TDBInt32 i;
    TDBInt32 j;
    TDBInt32 rangescore[3];
    TDBInt32 curscore;
    TDBInt32 bestscore;
    TDBBool match;
    TDBBase* base = tdb->base;

    if (sortspec) {
        for (tree=0 ; tree<base->numindices ; tree++) {
            numfields = tdbRTypeGetNumFields(base->index[tree]);
            tdb_ASSERT(numfields == 3);
            if (numfields != 3) continue;
            match = TDB_TRUE;
            for (i=0 ; i<numfields ; i++) {
                tdbRTypeIndexGetField(base->index[tree], i, &j);
                if (sortspec->keyorder[i] != j) {
                    match = TDB_FALSE;
                    break;
                }
            }
            if (match) return base->index[tree];
        }
    }

    /* The passed in keyorder was not valid (which, in fact, is the usual
       case).  Go find the best tree to use. */

    numfields = 3;

    fillRangeScore(rangescore, triple, numfields);

    indextype = NULL;
    bestscore = -1;
    for (tree=0 ; tree<base->numindices ; tree++) {
        tdb_ASSERT(tdbRTypeGetNumFields(base->index[tree]) == numfields);
        curscore = 0;
        for (i=0 ; i<numfields ; i++) {
            tdbRTypeIndexGetField(base->index[tree], i, &j);
            curscore = curscore * 10 + rangescore[j];
        }
        if (bestscore < curscore) {
            bestscore = curscore;
            indextype = base->index[tree];
        }
    }
    tdb_ASSERT(indextype != NULL);
    return indextype;
}


static TDBStatus
doRemove(TDB* tdb, TDBNodePtr* triple)
{
    TDBStatus status = TDB_FAILURE;
    TDBVector* vector;
    TDBCursor* cursor;
    TDBInt32 i;
    TDBUint32 recordnum;
    DBT key;
    int dbstatus;
    TDBBase* base = tdb->base;
    TDBInt32 realnumlayers;
    TDBInt32* reallayers;

    realnumlayers = tdb->numlayers;
    reallayers = tdb->layers;
    tdb->numlayers = 1;
    tdb->layers = &(tdb->outlayer);
    
    cursor = tdbCursorNew(tdb, pickIndexType(tdb, triple, NULL), triple,
                          TDB_TRUE, TDB_FALSE);
    if (!cursor) goto FAIL;
    while (NULL != (vector = tdbCursorGetNext(cursor))) {
        if (tdbVectorGetLayer(vector) != tdb->outlayer) continue;
        memset(&key, 0, sizeof(key));
        recordnum = tdbVectorGetRecordNumber(vector);
        key.data = &recordnum;
        key.size = sizeof(recordnum);
        dbstatus = base->recorddb->del(base->recorddb, base->transaction,
                                       &key, 0);
        if (dbstatus != DB_OK) {
            tdbMarkCorrupted(base);
        }
        for (i=0 ; i<base->numindices ; i++) {
            if (tdbRTypeRemove(base->index[i], vector) != TDB_SUCCESS) {
                tdbCursorFree(cursor);
                tdbMarkCorrupted(base);
                goto FAIL;
            }
        }
        if (tdbWindexRemove(base->windex, vector) != TDB_SUCCESS) {
            tdbCursorFree(cursor);
            goto FAIL;
        }
        tdbCursorInvalidateCache(tdb);
    }
    tdbCursorFree(cursor);
    tdbMarkDirty(base);
    status = TDB_SUCCESS;
 FAIL:
    tdb->numlayers = realnumlayers;
    tdb->layers = reallayers;
    return status;
}



TDBStatus
TDBRemove(TDB* tdb, TDBNodePtr triple[3])
{
    TDBStatus status;

    if (tdb == NULL || triple == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }

    if (tdb->impl) {
        return (*(tdb->impl->remove))(tdb, triple);
    }

    tdbBeginExclusiveOp(tdb->base);
    status = doRemove(tdb, triple);
    if (tdb->base->corrupted) {
        tdbRecover(tdb->base);
        status = doRemove(tdb, triple);
    }
    tdbEndExclusiveOp(tdb->base);

    return status;
}
    
   

    
TDBStatus
TDBReplace(TDB* tdb, TDBNodePtr triple[3], TDBUint64 owner)
{
    /* Write me correctly!!!  This works, but is inefficient. ### */

    TDBStatus status;
    TDBNodePtr tmp;
    TDBBase* base;
    if (tdb == NULL ||
          triple[0] == NULL || triple[1] == NULL || triple[2] == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    if (tdb->impl) {
        return (*(tdb->impl->replace))(tdb, triple, owner);
    }
    base = tdb->base;
    tdbBeginExclusiveOp(base);
    tmp = triple[2];
    triple[2] = NULL;
    status = doRemove(tdb, triple);
    triple[2] = tmp;
    if (status == TDB_SUCCESS) {
        status = doAdd(tdb, triple, owner, TDB_TRUE);
    }
    if (base->corrupted) {
        tdbRecover(base);
        triple[2] = NULL;
        status = doRemove(tdb, triple);
        if (status == TDB_SUCCESS) {
            triple[2] = tmp;
            status = doAdd(tdb, triple, owner, TDB_TRUE);
        }
    }
    tdbEndExclusiveOp(base);
    return status;
}

   

    
TDBCursor* TDBQuery(TDB* tdb, TDBNodePtr triple[3],
                    TDBSortSpecification* sortspec)
{
    TDBCursor* result;
    void* implcursor;
    if (tdb == NULL || triple == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    if (tdb->impl) {
        implcursor = (*(tdb->impl->query))(tdb, triple, sortspec);
        if (!implcursor) return NULL;
        return tdbCursorNewImpl(tdb, implcursor);
    } 
    tdbBeginExclusiveOp(tdb->base);
    result = tdbCursorNew(tdb, pickIndexType(tdb, triple, sortspec),
                          triple,
                          sortspec ? sortspec->includefalse : TDB_FALSE,
                          sortspec ? sortspec->reverse : TDB_FALSE);
    tdbEndExclusiveOp(tdb->base);
    return result;
}


TDBCursor* TDBQueryWordSubstring(TDB* tdb, const char* string)
{
    TDBCursor* result;
    if (tdb == NULL || string == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    tdbBeginExclusiveOp(tdb->base);
    result = tdbCursorNewWordSubstring(tdb, string);
    tdbEndExclusiveOp(tdb->base);
    return result;
}


TDBTriple* TDBGetResult(TDBCursor* cursor)
{
    TDBBase* base;
    TDB* tdb;
    TDBTriple* result = NULL;
    tdb_ASSERT(cursor);
    if (!cursor) return NULL;
    base = tdbCursorGetBase(cursor);
    tdb = tdbCursorGetTDB(cursor);
    if (tdb && tdb->impl) {
        return (*(tdb->impl->getresult))(tdbCursorGetImplcursor(cursor));
    }
    if (!base || !tdb) {
        tdb_ASSERT(0);
        return NULL;
    }
    tdbBeginExclusiveOp(base);
    if (tdbCursorGetNext(cursor)) {
        result = tdbCursorGetLastResultAsTriple(cursor);
    }
    tdbEndExclusiveOp(base);
    return result;
}


TDBStatus
TDBFreeCursor(TDBCursor* cursor)
{
    TDBStatus status;
    TDBBase* base;
    void* implcursor;
    if (cursor == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    implcursor = tdbCursorGetImplcursor(cursor);
    if (implcursor) {
        TDB* tdb;
        tdb = tdbCursorGetTDB(cursor);
        if (!tdb->impl) {
            tdb_ASSERT(0);
            return TDB_FAILURE;
        }
        status = (*tdb->impl->freecursor)(implcursor);
        if (status == TDB_SUCCESS) tdbCursorFree(cursor);
        return status;
    }
    base = tdbCursorGetBase(cursor);
    if (base == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    tdbBeginExclusiveOp(base);
    tdbCursorFree(cursor);
    tdbEndExclusiveOp(base);
    return TDB_SUCCESS;
}


TDB* TDBOpenImplementation(TDBImplement* impl)
{
    TDB* db;
    tdb_ASSERT(impl != NULL);
    if (impl == NULL) return NULL;
    db = tdb_NEWZAP(TDB);
    if (db == NULL) return NULL;
    db->impl = impl;
    return db;
}

TDBStatus TDBSetImplData(TDB* db, void* impldata)
{
    if (db == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    db->impldata = impldata;
    return TDB_SUCCESS;
}

void* TDBGetImplData(TDB* db)
{
    tdb_ASSERT(db != NULL);
    return db ? db->impldata : NULL;
}


void
TDBValidateSortOrder(TDB* tdb, TDBSortSpecification* sortspec,
                     TDBNodePtr triple[3])
{
    TDBRType* rtype;
    TDBInt32 i;
    TDBInt32 j;
    TDBInt32 k;
    TDBInt32 rangescore[3];
    TDBInt32 max;
    TDBBool seen[3];
    if (triple == NULL || sortspec == NULL) {
        tdb_ASSERT(0);
        return;
    }
    if (tdb == NULL) {
        /* Oh, well, ick.  If anything legal-looking was specified, just use
           that; otherwise, just pick the ordering with the highest score. */
        for (i=0 ; i<3 ; i++) {
            seen[i] = TDB_FALSE;
        }
        for (i=0 ; i<3 ; i++) {
            j = sortspec->keyorder[i];
            if (j >= 0 && j < 3) seen[j] = TDB_TRUE;
        }
        for (i=0 ; i<3 ; i++) {
            if (!seen[i]) break;
        }
        if (i == 3) return;     /* A legal sortspec was picked already. */
        fillRangeScore(rangescore, triple, 3);
        for (i=0 ; i<3 ; i++) {
            max = -9;
            for (j=0 ; j<3 ; j++) {
                if (max < rangescore[j]) {
                    max = rangescore[j];
                    k = j;
                }
            }
            sortspec->keyorder[i] = k;
            rangescore[k] = -99;
        }
        return;
    }
    rtype = pickIndexType(tdb, triple, sortspec);
    for (i=0 ; i<3 ; i++) {
        tdbRTypeIndexGetField(rtype, i, &j);
        sortspec->keyorder[i] = j;
    }
}


TDBStatus
TDBGetTripleID(TDB* database, TDBNodePtr triple[3], TDBInt64* id)
{
    tdb_ASSERT(0);              /* Write me!  (or nuke me from the API) */
    return TDB_FAILURE;
}


TDBTriple*
TDBFindTripleFromID(TDB* database, TDBInt64 id)
{
    tdb_ASSERT(0);              /* Write me!  (or nuke me from the API) */
    return NULL;
}


void TDBFreeTriple(TDBTriple* triple)
{
    int i;
    if (!triple) {
        tdb_ASSERT(0);
        return;
    }
    for (i=0 ; i<3 ; i++) {
        if (triple->data[i]) TDBFreeNode(triple->data[i]);
    }
    tdb_Free(triple);
}


char*
tdbGrowTmpBuf(TDBBase* base, TDBInt32 newsize)
{
    if (base == NULL || newsize <= 0) {
        tdb_ASSERT(0);
        return NULL;
    }
    if (newsize <= base->tmpbufsize) return base->tmpbuf;
    base->tmpbuf = base->tmpbuf ? tdb_Realloc(base->tmpbuf, newsize) :
        tdb_Malloc(newsize);
    if (base->tmpbuf) {
        base->tmpbufsize = newsize;
        return base->tmpbuf;
    }
    base->tmpbufsize = 0;
    return NULL;
}


TDBInt32
tdbGetTmpBufSize(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return -1;
    }
    return base->tmpbufsize;
}


DB*
tdbGetRecordDB(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return base->recorddb;
}


TDBStatus
tdbRecover(TDBBase* base)
{
    TDBStatus status;
    char* filename;
    char* newfile;
    DBC* cursor;
    TDBInt32 i;
    TDBVector* vector;
    TDBBase* outbase;
    TDB* outtdb[256];
    int dbstatus;
    DBT key;
    DBT data;
    TDBNodePtr nodes[3];
    TDBUint32 recordnum;
    TDBUint64 owner;
    TDBInt32 layer;
    if (base == NULL || base->filename == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    memset(outtdb, 0, sizeof(outtdb));
    filename = tdb_strdup(base->filename);
    if (!filename) return TDB_FAILURE;
    if (base->toobustedtorecover) {
        closeGuts(base);
        TDBBlowAwayDatabaseFiles(filename);
    } else {
        newfile = tdb_Malloc(strlen(filename) + 50);
        if (!newfile) return TDB_FAILURE;
        strcpy(newfile, filename);
        strcat(newfile, "-recover-");
        sprintf(newfile + strlen(newfile), "%d", getpid());
        outbase = TDBOpenBase(newfile);
        if (!outbase) return TDB_FAILURE;
        dbstatus = base->recorddb->cursor(base->recorddb, NULL, &cursor, 0);
        if (dbstatus != DB_OK) return TDB_FAILURE;
        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));
        while (DB_OK == cursor->c_get(cursor, &key, &data, DB_NEXT)) {
            memcpy(&recordnum, key.data, sizeof(recordnum));
            vector = tdbVectorNewFromRecord(base, recordnum);
            if (vector) {
                for (i=0 ; i<3 ; i++) {
                    nodes[i] = tdbVectorGetNonInternedNode(vector, i);
                }
                owner = tdbVectorGetOwner(vector);
                layer = tdbVectorGetLayer(vector);
                if (outtdb[layer] == NULL) {
                    outtdb[layer] = TDBOpenLayers(outbase, 1, &layer);
                }
                if (tdbVectorGetFlags(vector) & TDBFLAG_ISASSERT) {
                    TDBAdd(outtdb[layer], nodes, owner);
                } else {
                    TDBAddFalse(outtdb[layer], nodes, owner);
                }
                tdbVectorFree(vector);
            }
            memset(&key, 0, sizeof(key));
            memset(&data, 0, sizeof(data));
        }
        cursor->c_close(cursor);
        for (layer = 0 ; layer < 256 ; layer++) {
            if (outtdb[layer]) {
                TDBClose(outtdb[layer]);
                outtdb[layer] = NULL;
            }
        }
        TDBCloseBase(outbase);
        closeGuts(base);
        TDBBlowAwayDatabaseFiles(filename);
        tdb_Rename(newfile, filename);
        tdb_Free(newfile);
    }
        
    status = openGuts(base, filename);
    tdb_Free(filename);
    if (base->corrupted) {
        /* Looks like recovery didn't work after all.  Yuck! */
        status = TDB_FAILURE;
    }
    return status;
}


TDBWindex*
tdbGetWindex(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return base->windex;
}


TDBCallbackInfo*
tdbGetFirstCallback(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return base->firstcallback;
}

void
tdbSetFirstCallback(TDBBase* base, TDBCallbackInfo* call)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return;
    }
    base->firstcallback = call;
    
}

TDBPendingCall*
tdbGetFirstPendingCall(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return base->firstpendingcall;
}

void
tdbSetFirstPendingCall(TDBBase* base, TDBPendingCall* call)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return;
    }
    base->firstpendingcall = call;
}

TDBPendingCall*
tdbGetLastPendingCall(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return base->lastpendingcall;
}

void
tdbSetLastPendingCall(TDBBase* base, TDBPendingCall* call)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return;
    }
    base->lastpendingcall = call;
}

DB_ENV*
tdbGetDBEnv(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return base->env;
}

TDBIntern*
tdbGetIntern(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return base->intern;
}

DB_TXN*
tdbGetTransaction(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return base->transaction;
}



TDB*
TDBOpenLayers(TDBBase* base, TDBInt32 numlayers, TDBInt32* layers)
{
    TDB* tdb;
    TDBInt32 i;
    if (base == NULL || layers == NULL || numlayers <= 0) {
        tdb_ASSERT(0);
        return NULL;
    }
    tdb = tdb_NEWZAP(TDB);
    if (!tdb) return NULL;
    tdb->numlayers = numlayers;
    tdb->layers = tdb_Malloc(sizeof(TDBInt32) * numlayers);
    if (tdb->layers == NULL) {
        tdb_Free(tdb);
        return NULL;
    }
    for (i=0 ; i<numlayers ; i++) {
        tdb->layers[i] = layers[i];
    }
    tdb->outlayer = layers[0];
    tdb->base = base;
    tdbBeginExclusiveOp(base);
    tdb->nexttdb = base->firsttdb;
    base->firsttdb = tdb;
    tdbEndExclusiveOp(base);
    return tdb;
}

TDBStatus
TDBClose(TDB* tdb)
{
    TDBStatus status;
    TDB** ptr;
    if (tdb == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    if (tdb->impl) {
        status = tdb->impl->close(tdb);
        tdb_Free(tdb);
        return status;
    }
    tdb_ASSERT(tdb->firstcursor == NULL);
    tdbBeginExclusiveOp(tdb->base);
    for (ptr = &(tdb->base->firsttdb) ; *ptr ; ptr = &((*ptr)->nexttdb)) {
        if (*ptr == tdb) {
            *ptr = tdb->nexttdb;
            break;
        }
    }
    tdbEndExclusiveOp(tdb->base);
    tdb_Free(tdb->layers);
    tdb_Free(tdb);
    return TDB_SUCCESS;
}

TDBBase*
tdbGetBase(TDB* tdb)
{
    if (tdb == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return tdb->base;
}


TDBInt32
tdbGetNumIndices(TDBBase* base)
{
    if (base == NULL) {
        tdb_ASSERT(0);
        return -1;
    }
    return base->numindices;
}

TDBRType*
tdbGetIndex(TDBBase* base, TDBInt32 which)
{
    if (base == NULL || which < 0 || which >= base->numindices) {
        tdb_ASSERT(0);
        return NULL;
    }
    return base->index[which];
}

TDBInt32
tdbGetNumLayers(TDB* tdb)
{
    if (tdb == NULL) {
        tdb_ASSERT(0);
        return 0;
    }
    return tdb->numlayers;
}

TDBInt32
tdbGetLayer(TDB* tdb, TDBInt32 which)
{
    if (tdb == NULL || which < 0 || which >= tdb->numlayers) {
        tdb_ASSERT(0);
        return -1;
    }
    return tdb->layers[which];
}


TDBInt32
tdbGetOutLayer(TDB* tdb)
{
    if (tdb == NULL) {
        tdb_ASSERT(0);
        return -1;
    }
    return tdb->outlayer;
}

