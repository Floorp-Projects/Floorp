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

#include "intern.h"
#include "node.h"
#include "tdb.h"
#include "util.h"
#include "vector.h"


struct _TDBVector {
    TDBBase* base;
    TDBInt32 numfields;
    TDBNodePtr* nodes;
    TDBNodePtr* interned;
    TDBUint8 layer;
    TDBUint8 flags;
    TDBUint32 recordnum;
    TDBUint64 owner;
    
};


TDBVector*
tdbVectorNewFromNodes(TDBBase* base, TDBNodePtr* nodes, TDBUint8 layer,
                      TDBUint8 flags, TDBUint32 recordnum)
{
    TDBVector* vector;
    TDBInt32 numfields = 3;     /* Sigh... */
    TDBInt32 i;
    if (base == NULL || nodes == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    vector = tdb_NEWZAP(TDBVector);
    if (!vector) return NULL;
    vector->base = base;
    vector->numfields = numfields;
    vector->nodes = tdb_Calloc(numfields, sizeof(TDBNodePtr));
    vector->interned = tdb_Calloc(numfields, sizeof(TDBNodePtr));
    if (!vector->nodes || !vector->interned) {
        tdb_Free(vector);
        return NULL;
    }
    for (i=0 ; i<numfields ; i++) {
        if (!nodes[i]) {
            tdb_ASSERT(0);
            tdbVectorFree(vector);
            return NULL;
        }
        vector->nodes[i] = TDBNodeDup(nodes[i]);
        if (!vector->nodes[i]) {
            tdbVectorFree(vector);
            return NULL;
        }
        if (vector->nodes[i]->type == TDBTYPE_INTERNED) {
            vector->interned[i] = vector->nodes[i];
            vector->nodes[i] = NULL;
        }
    }
    vector->layer = layer;
    vector->flags = flags;
    vector->recordnum = recordnum;
    return vector;
}


extern
TDBVector* tdbVectorNewFromRecord(TDBBase* base, TDBUint32 recordnum)
{
    TDBVector* vector;
    TDBInt32 numfields = 3;     /* Sigh... */
    TDBNodePtr nodes[3];
    DBT key;
    DBT data;
    DB* db;
    int dbstatus;
    char* ptr;
    char* endptr;
    TDBInt32 i;
    TDBUint8 layer;
    TDBUint8 flags;
    TDBUint64 owner;
    if (base == NULL || recordnum == 0) {
        tdb_ASSERT(0);
        return NULL;
    }
    db = tdbGetRecordDB(base);
    if (db == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = &recordnum;
    key.size = sizeof(recordnum);
    dbstatus = db->get(db, tdbGetTransaction(base), &key, &data, 0);
    if (dbstatus != DB_OK) {
        tdbMarkCorrupted(base);
        return NULL;
    }
    ptr = data.data;
    endptr = ptr + data.size;
    for (i=0 ; i<numfields ; i++) {
        nodes[i] = tdbGetNode(&ptr, endptr);
        if (nodes[i] == NULL) {
            tdbMarkCorrupted(base);
            for (i-- ; i>=0 ; i--) TDBFreeNode(nodes[i]);
            return NULL;
        }
    }
    layer = tdbGetUInt8(&ptr, endptr);
    flags = tdbGetUInt8(&ptr, endptr);
    owner = tdbGetUInt64(&ptr, endptr);
    vector = tdbVectorNewFromNodes(base, nodes, layer, flags, recordnum);
    for (i=0 ; i<numfields ; i++) TDBFreeNode(nodes[i]);
    if (vector == NULL) return NULL;
    vector->owner = owner;
    return vector;
}


extern TDBStatus
tdbVectorPutInNewRecord(TDBVector* vector, TDBUint64 owner)
{
    DBT key;
    DBT data;
    DB* db;
    TDBInt32 length;
    TDBInt32 i;
    char* tmpbuf;
    char* ptr;
    char* endptr;
    int dbstatus;
    if (vector == NULL || vector->recordnum != 0) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    db = tdbGetRecordDB(vector->base);
    if (db == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    length = sizeof(TDBUint64) + sizeof(TDBUint8) + sizeof(TDBUint8);
    for (i=0 ; i<vector->numfields ; i++) {
        length += tdbNodeSize(tdbVectorGetInternedNode(vector, i));
    }
    length += 20;               /* Just for general slop. */
    tmpbuf = tdbGrowTmpBuf(vector->base, length);
    if (tmpbuf == NULL) return TDB_FAILURE;
    data.data = tmpbuf;
    ptr = data.data;
    endptr = ptr + length;
    for (i=0 ; i<vector->numfields ; i++) {
        if (tdbPutNode(&ptr, tdbVectorGetInternedNode(vector, i),
                       endptr) != TDB_SUCCESS) return TDB_FAILURE;
    }
    tdbPutUInt8(&ptr, vector->layer, endptr);
    tdbPutUInt8(&ptr, vector->flags, endptr);
    tdbPutUInt64(&ptr, vector->owner, endptr);
    data.size = ptr - tmpbuf;
    if (data.size != length - 20) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    key.data = &(vector->recordnum);
    key.ulen = sizeof(vector->recordnum);
    key.flags = DB_DBT_USERMEM;
    dbstatus = db->put(db, tdbGetTransaction(vector->base),
                       &key, &data, DB_APPEND);
    if (dbstatus != DB_OK) return TDB_FAILURE;
    tdb_ASSERT(vector->recordnum > 0);
    return TDB_SUCCESS;
}

TDBUint32
tdbVectorGetRecordNumber(TDBVector* vector)
{
    if (vector == NULL) {
        tdb_ASSERT(0);
        return 0;
    }
    return vector->recordnum;
}


TDBUint64
tdbVectorGetOwner(TDBVector* vector)
{
    if (vector == NULL) {
        tdb_ASSERT(0);
        return 0;
    }
    return vector->owner;
}

void
tdbVectorFree(TDBVector* vector)
{
    TDBInt32 i;
    if (vector == NULL) {
        tdb_ASSERT(0);
        return;
    }
    if (vector->nodes) {
        for (i=0 ; i<vector->numfields ; i++) {
            if (vector->nodes[i]) TDBFreeNode(vector->nodes[i]);
        }
        tdb_Free(vector->nodes);
    }
    if (vector->interned) {
        for (i=0 ; i<vector->numfields ; i++) {
            if (vector->interned[i]) TDBFreeNode(vector->interned[i]);
        }
        tdb_Free(vector->interned);
    }
    tdb_Free(vector);
}


TDBNodePtr
tdbVectorGetNonInternedNode(TDBVector* vector, TDBInt32 i)
{
    if (vector == NULL || i < 0 || i >= vector->numfields) {
        tdb_ASSERT(0);
        return NULL;
    }
    if (i != 1) {
        tdb_ASSERT(vector->nodes[i] != NULL && vector->interned[i] == NULL);
        return vector->nodes[i];
    }
    if (vector->nodes[i] == NULL) {
        if (vector->interned[i] == NULL) {
            tdb_ASSERT(0);
            return NULL;
        }
        vector->nodes[i] = tdbUnintern(vector->base, vector->interned[i]);
    }
    return vector->nodes[i];
}


TDBNodePtr
tdbVectorGetInternedNode(TDBVector* vector, TDBInt32 i)
{
    if (vector == NULL || i < 0 || i >= vector->numfields) {
        tdb_ASSERT(0);
        return NULL;
    }
    if (i != 1) {
        tdb_ASSERT(vector->nodes[i] != NULL && vector->interned[i] == NULL);
        return vector->nodes[i];
    }
    if (vector->interned[i] == NULL) {
        if (vector->nodes[i] == NULL) {
            tdb_ASSERT(0);
            return NULL;
        }
        vector->interned[i] = tdbIntern(vector->base, vector->nodes[i]);
    }
    return vector->interned[i];
}


TDBUint8
tdbVectorGetLayer(TDBVector* vector)
{
    if (vector == NULL) {
        tdb_ASSERT(0);
        return 0;
    }
    return vector->layer;
}



TDBUint8
tdbVectorGetFlags(TDBVector* vector)
{
    if (vector == NULL) {
        tdb_ASSERT(0);
        return 0;
    }
    return vector->flags;
}




TDBInt32
tdbVectorGetNumFields(TDBVector* vector)
{
    if (vector == NULL) {
        tdb_ASSERT(0);
        return -1;
    }
    return vector->numfields;
}


TDBBool
tdbVectorEqual(TDBVector* v1, TDBVector* v2)
{
    TDBInt32 i;
    TDBInt64 cmp;
    if (v1 == NULL || v2 == NULL || v1->numfields != v2->numfields) {
        tdb_ASSERT(0);
        return TDB_FALSE;
    }
    for (i=0 ; i<v1->numfields ; i++) {
        if (v1->interned[i] != NULL && v2->interned[i] != NULL) {
            cmp = TDBCompareNodes(v1->interned[i], v2->interned[i]);
        } else {
            cmp = TDBCompareNodes(tdbVectorGetNonInternedNode(v1, i),
                                  tdbVectorGetNonInternedNode(v2, i));
        }
        if (cmp != 0) return TDB_FALSE;
    }
    return TDB_TRUE;
}


TDBBool
tdbVectorLayerMatches(TDBVector* vector, TDB* tdb)
{
    TDBInt32 i;
    if (vector == NULL || tdb == NULL) {
        tdb_ASSERT(0);
        return TDB_FALSE;
    }
    for (i=tdbGetNumLayers(tdb) - 1 ; i >= 0 ; i--) {
        if (vector->layer == tdbGetLayer(tdb, i)) {
           return TDB_TRUE;
        }
    }
    return TDB_FALSE;
}
