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

#include "tdbtypes.h"

#include "node.h"
#include "rtype.h"
#include "tdb.h"
#include "util.h"
#include "vector.h"

struct _TDBRType {
    TDBBase* base;
    TDBRType* next;
    char* name;
    TDBUint8 code;

    TDBInt32 numfields;
    TDBInt32 fieldnum[TDB_MAXTYPES];

    DB* db;
};




TDBRType*
tdbRTypeNew(TDBBase* base, const char* name, TDBUint8 code,
            TDBInt32 numfields, TDBInt32* fieldnum)
{
    TDBInt32 i;
    TDBRType* result;
    TDBRType* first;
    int dbstatus;
    first = tdbGetFirstRType(base);
    if (numfields <= 0 || numfields > TDB_MAXTYPES || fieldnum == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    
    for (result = first ; result ; result = result->next) {
        if (result->code == code) {
            tdb_ASSERT(0);
            return NULL;
        }
    }

    result = tdb_NEWZAP(TDBRType);
    if (!result) return NULL;
    result->base = base;
    result->name = tdb_strdup(name);
    if (!result->name) {
        tdb_Free(result);
        return NULL;
    }
    result->code = code;
    result->numfields = numfields;
    result->next = first;
    tdbSetFirstRType(base, result);

    for (i=0 ; i<numfields ; i++) {
        result->fieldnum[i] = fieldnum[i];
    }

    dbstatus = db_create(&(result->db), tdbGetDBEnv(base), 0);
    if (dbstatus != DB_OK) goto FAIL;
    dbstatus = result->db->open(result->db, name, NULL, DB_BTREE,
                                DB_CREATE, 0666);
    if (dbstatus != DB_OK) goto FAIL;


    return result;

 FAIL:
    tdbRTypeFree(result);
    return NULL;
}


void tdbRTypeFree(TDBRType* rtype)
{
    TDBRType* first;
    if (!rtype) {
        tdb_ASSERT(0);
        return;
    }
    first = tdbGetFirstRType(rtype->base);
    if (first == rtype) {
        tdbSetFirstRType(rtype->base, first->next);
    } else {
        for (; first ; first = first->next) {
            if (first->next == rtype) {
                first->next = rtype->next;
                break;
            }
        }
    }
    if (rtype->name) tdb_Free(rtype->name);
    if (rtype->db) {
        rtype->db->close(rtype->db, 0);
    }
    tdb_Free(rtype);
}


TDBBase*
tdbRTypeGetBase(TDBRType* rtype)
{
    if (!rtype) {
        tdb_ASSERT(0);
        return NULL;
    }
    return rtype->base;
}


TDBUint8
tdbRTypeGetCode(TDBRType* rtype)
{
    if (!rtype) {
        tdb_ASSERT(0);
        return 0;
    }
    return rtype->code;
}


const char*
tdbRTypeGetName(TDBRType* rtype)
{
    if (!rtype) {
        tdb_ASSERT(0);
        return 0;
    }
    return rtype->name;
}


TDBInt32
tdbRTypeGetNumFields(TDBRType* rtype)
{
    if (!rtype) {
        tdb_ASSERT(0);
        return 0;
    }
    return rtype->numfields;
}


TDBStatus
tdbRTypeIndexGetField(TDBRType* rtype, TDBInt32 which, TDBInt32* fieldnum)
{
    if (!rtype || which < 0 || which >= rtype->numfields) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    if (fieldnum) *fieldnum = rtype->fieldnum[which];
    return TDB_SUCCESS;
}


TDBRType*
tdbRTypeFromCode(TDBBase* base, TDBUint8 code)
{
    TDBRType* result;
    tdb_ASSERT((code & TDB_FREERECORD) == 0);
    for (result = tdbGetFirstRType(base); result ; result = result->next) {
        if (result->code == code) break;
    }
    return result;
}


TDBRType* tdbRTypeGetNextRType(TDBRType* rtype)
{
    if (rtype == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return rtype->next;
}


#define FIELDTYPE(i) ((i)==0 ? TDBTYPE_INT64 : ((i)==1 ? TDBTYPE_INTERNED : 0))

static TDBStatus
loadKeyData(TDBRType* rtype, DBT* key, TDBVector* vector)
{
    char* keydata;
    TDBNodePtr node;
    TDBInt32 i;
    TDBInt32 w;
    char* ptr;
    char* endptr;
    if (rtype == NULL || vector == NULL ||
        tdbVectorGetNumFields(vector) != rtype->numfields) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    keydata = tdbGrowTmpBuf(rtype->base, 512);
    if (keydata == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    ptr = keydata;
    endptr = ptr + tdbGetTmpBufSize(rtype->base);
    for (i=0 ; i<rtype->numfields ; i++) {
        w = rtype->fieldnum[i];
        node = tdbVectorGetInternedNode(vector, w);
        switch (w) {
        case 1:
            if (node->type != TDBTYPE_INTERNED) {
                tdb_ASSERT(0);
                return TDB_FAILURE;
            }
            tdbPutUInt32(&ptr, node->d.i, endptr);
            break;
        default:
            tdb_ASSERT(w == 0 || w == 2);
            if (tdbPutNode(&ptr, node, endptr) != TDB_SUCCESS) {
                ptr = endptr;
            }
            break;
        }
    }
    tdbPutUInt8(&ptr, tdbVectorGetLayer(vector), endptr);
    tdbPutUInt8(&ptr, tdbVectorGetFlags(vector), endptr);
    if (ptr >= endptr) {
        if (tdbGrowTmpBuf(rtype->base, sizeof(TDBUint64) + sizeof(TDBUint32) +
                          tdbNodeSize(tdbVectorGetInternedNode(vector, 2))
                          + 10) != TDB_SUCCESS) return TDB_FAILURE;
        return loadKeyData(rtype, key, vector);
    }

    key->data = keydata;
    key->size = ptr - keydata;

    return TDB_SUCCESS;
}


TDBStatus
tdbRTypeAdd(TDBRType* rtype, TDBVector* vector)
{
    DBT key;
    DBT data;
    int dbstatus;
    TDBUint32 recordnum;
    char buf[10];
    char* ptr;
    char* endptr;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    if (loadKeyData(rtype, &key, vector) != TDB_SUCCESS) {
        return TDB_FAILURE;
    }
    recordnum = tdbVectorGetRecordNumber(vector);
    tdb_ASSERT(recordnum > 0);
    ptr = buf;
    endptr = buf + sizeof(buf);
    tdbPutUInt32(&ptr, recordnum, endptr);
    data.data = buf;
    data.size = ptr - buf;
    /*     tdb_ASSERT(rtype->tdb->transaction != NULL); */
    dbstatus = rtype->db->put(rtype->db, tdbGetTransaction(rtype->base),
                              &key, &data, 0);
    if (dbstatus != DB_OK) return TDB_FAILURE;
    return TDB_SUCCESS;
}


TDBStatus
tdbRTypeRemove(TDBRType* rtype, TDBVector* vector)
{
    DBT key;
    int dbstatus;
    memset(&key, 0, sizeof(key));
    if (loadKeyData(rtype, &key, vector) != TDB_SUCCESS) {
        return TDB_FAILURE;
    }
    dbstatus = rtype->db->del(rtype->db, tdbGetTransaction(rtype->base),
                              &key, 0);
    if (dbstatus != DB_OK) return TDB_FAILURE;
    return TDB_SUCCESS;
}


DBC*
tdbRTypeGetCursor(TDBRType* rtype, TDBVector* vector)
{
    DBT key;
    DBT data;
    int dbstatus;
    DBC* cursor;
    if (rtype == NULL || vector == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    if (loadKeyData(rtype, &key, vector) != TDB_SUCCESS) {
        return NULL;
    }
    dbstatus = rtype->db->cursor(rtype->db, NULL,
                                 &cursor, 0);
    if (dbstatus != DB_OK) return NULL;
    dbstatus = cursor->c_get(cursor, &key, &data, DB_SET_RANGE);
    if (dbstatus != DB_OK) {
        cursor->c_close(cursor);
        return NULL;
    }

    return cursor;
}




TDBVector*
tdbRTypeGetCursorValue(TDBRType* rtype, DBC* cursor, int dbflags)
{
    DBT key;
    DBT data;
    TDBNodePtr nodes[20];
    int dbstatus;
    TDBInt32 i;
    TDBInt32 w;
    char* ptr;
    char* endptr;
    TDBUint8 layer;
    TDBUint8 flags;
    TDBUint32 recordnum;
    TDBVector* result;
    if (rtype == NULL || cursor == NULL ||
          rtype->numfields > sizeof(nodes) / sizeof(nodes[0])) {
        tdb_ASSERT(0);
        return NULL;
    }
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    dbstatus = cursor->c_get(cursor, &key, &data, dbflags);
    if (dbstatus == DB_NOTFOUND) return NULL;
    if (dbstatus != DB_OK) return NULL;
    ptr = key.data;
    endptr = ptr + key.size;
    for (i=0 ; i<rtype->numfields ; i++) {
        w = rtype->fieldnum[i];
        switch (w) {
        case 1:
            nodes[w] = TDBCreateIntNode(tdbGetUInt32(&ptr, endptr),
                                        TDBTYPE_INTERNED);
            break;
        default:
            nodes[w] = tdbGetNode(&ptr, endptr);
            break;
        }
        if (nodes[w] == NULL) {
            for (i-- ; i>=0 ; i--) {
                w = rtype->fieldnum[i];
                TDBFreeNode(nodes[w]);
            }
            return NULL;
        }
    }
    layer = tdbGetUInt8(&ptr, endptr);
    flags = tdbGetUInt8(&ptr, endptr);
    tdb_ASSERT(ptr == endptr);
    ptr = data.data;
    endptr = ptr + data.size;
    recordnum = tdbGetUInt32(&ptr, endptr);
    result = tdbVectorNewFromNodes(rtype->base, nodes, layer, flags,
                                   recordnum);
    for (i=0 ; i<rtype->numfields ; i++) {
        w = rtype->fieldnum[i];
        TDBFreeNode(nodes[w]);
    }
    return result;
}


TDBStatus
tdbRTypeSync(TDBRType* rtype)
{
    if (rtype == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    if (rtype->db->sync(rtype->db, 0) != DB_OK) return TDB_FAILURE;
    return TDB_SUCCESS;
}


extern TDBBool
tdbRTypeProbe(TDBRType* rtype, TDBVector* vector)
{
    TDBBool result = TDB_FALSE;
    DBC* cursor;
    TDBVector* v;
    cursor = tdbRTypeGetCursor(rtype, vector);
    if (!cursor) {
        return TDB_FALSE;
    }
    v = tdbRTypeGetCursorValue(rtype, cursor, DB_CURRENT);
    cursor->c_close(cursor);
    if (v) {
        if (tdbVectorEqual(vector, v)) {
            result = TDB_TRUE;
        }
        tdbVectorFree(v);
    }
    return result;
}
