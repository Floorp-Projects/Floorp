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

struct _TDBIntern {
    DB* nodetoatomdb;
    DB* atomtonodedb;
    char* tmpbuf;
    TDBInt32 tmpbufsize;
};



TDBIntern*
tdbInternInit(TDBBase* base)
{
    TDBIntern* intern;
    if (base == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    intern = tdb_NEWZAP(TDBIntern);
    if (!intern) return NULL;
    if (db_create(&(intern->nodetoatomdb), tdbGetDBEnv(base), 0) != DB_OK) {
        goto FAIL;
    }
    if (intern->nodetoatomdb->open(intern->nodetoatomdb, "nodetoatomdb",
                                   NULL, DB_HASH,
                                   DB_CREATE, 0666) != DB_OK) goto FAIL;
    if (db_create(&(intern->atomtonodedb), tdbGetDBEnv(base), 0) != DB_OK) {
        goto FAIL;
    }
    if (intern->atomtonodedb->open(intern->atomtonodedb, "atomtonodedb",
                                   NULL, DB_RECNO, 
                                   DB_CREATE, 0666) != DB_OK) goto FAIL;
    intern->tmpbufsize = 512;
    intern->tmpbuf = tdb_Malloc(intern->tmpbufsize);
    if (!intern->tmpbuf) goto FAIL;
    return intern;

 FAIL:
    tdbInternFree(intern);
    return NULL;
}


void
tdbInternFree(TDBIntern* intern)
{
    if (intern == NULL) {
        tdb_ASSERT(0);
        return;
    }
    if (intern->nodetoatomdb) {
        intern->nodetoatomdb->close(intern->nodetoatomdb, 0);
    }
    if (intern->atomtonodedb) {
        intern->atomtonodedb->close(intern->atomtonodedb, 0);
    }
    if (intern->tmpbuf) tdb_Free(intern->tmpbuf);
    tdb_Free(intern);
}


TDBNodePtr
tdbIntern(TDBBase* base, TDBNodePtr node)
{
    TDBIntern* intern;
    char* ptr;
    char* endptr;
    DBT key;
    DBT data;
    int dbstatus;
    TDBUint32 atom;
    TDBUint32 atom2;
    if (base == NULL || node == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    intern = tdbGetIntern(base);

    if (intern == NULL || intern->tmpbuf == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    
    if (node->type == TDBTYPE_MINNODE) {
        return TDBCreateIntNode(0, TDBTYPE_INTERNED);
    }
    if (node->type == TDBTYPE_MAXNODE) {
        return TDBCreateIntNode(0xffffffff, TDBTYPE_INTERNED);
    }
    if (node->type == TDBTYPE_INTERNED) {
        tdb_ASSERT(0);
        return NULL;
    }

    if (intern->tmpbuf) {
        ptr = intern->tmpbuf;
        endptr = ptr + intern->tmpbufsize;
        tdbPutNode(&ptr, node, endptr);
    } else {
        ptr = NULL;
    }
    if (ptr == NULL || ptr >= endptr) {
        intern->tmpbufsize = tdbNodeSize(node) + 10;
        intern->tmpbuf = tdb_Realloc(intern->tmpbuf, intern->tmpbufsize);
        if (!intern->tmpbuf) return NULL;
        ptr = intern->tmpbuf;
        endptr = ptr + intern->tmpbufsize;
        tdbPutNode(&ptr, node, endptr);
        if (ptr >= endptr) {
            tdb_ASSERT(0);
            return NULL;
        }
    }
        

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = intern->tmpbuf;
    key.size = ptr - intern->tmpbuf;
    dbstatus = intern->nodetoatomdb->get(intern->nodetoatomdb,
                                         tdbGetTransaction(base),
                                         &key, &data, 0);
    if (dbstatus == DB_NOTFOUND) {
        do {
            data.data = &atom;
            data.ulen = sizeof(atom);
            data.flags = DB_DBT_USERMEM;
            dbstatus = intern->atomtonodedb->put(intern->atomtonodedb,
                                                 tdbGetTransaction(base),
                                                 &data, &key, DB_APPEND);
            if (dbstatus != DB_OK) return NULL;
            tdb_ASSERT(data.size == sizeof(TDBUint32));
        } while (atom == 0);    /* First time, we may get an atom of zero,
                                   which I don't want, since I reserve atom
                                   of zero to mean something special. */
        data.data = &atom2;
        ptr = data.data;
        endptr = ptr + data.size;
        tdbPutUInt32(&ptr, atom, endptr);
        dbstatus = intern->nodetoatomdb->put(intern->nodetoatomdb,
                                             tdbGetTransaction(base),
                                             &key, &data, 0);
        if (dbstatus != DB_OK) return NULL;
    } else {
        if (dbstatus != DB_OK) return NULL;
        if (data.size != sizeof(TDBUint32)) {
            tdb_ASSERT(0);
            return NULL;
        }
        ptr = data.data;
        endptr = ptr + data.size;
        atom = tdbGetUInt32(&ptr, endptr);
    }
    return TDBCreateIntNode(atom, TDBTYPE_INTERNED);
}


TDBNodePtr
tdbUnintern(TDBBase* base, TDBNodePtr node)
{
    char* ptr;
    char* endptr;
    DBT key;
    DBT data;
    TDBIntern* intern;
    TDBUint32 atom;
    int dbstatus;
    if (base == NULL || node == NULL || node->type != TDBTYPE_INTERNED ||
        node->d.i == 0 || node->d.i == 0xffffffff) {
        tdb_ASSERT(0);
        return NULL;
    }
    intern = tdbGetIntern(base);
    if (intern == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    atom = node->d.i;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = &atom;
    key.size = sizeof(atom);
    dbstatus = intern->atomtonodedb->get(intern->atomtonodedb,
                                         tdbGetTransaction(base),
                                         &key, &data, 0);
    if (dbstatus != DB_OK) return NULL;
    ptr = (char*) data.data;
    endptr = ptr + data.size;
    return tdbGetNode(&ptr, endptr);
}


TDBStatus
tdbInternSync(TDBBase* base)
{
    TDBIntern* intern;
    if (base == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    intern = tdbGetIntern(base);
    if (intern->atomtonodedb->sync(intern->atomtonodedb, 0) != DB_OK) {
        return TDB_FAILURE;
    }
    if (intern->nodetoatomdb->sync(intern->nodetoatomdb, 0) != DB_OK) {
        return TDB_FAILURE;
    }
    return TDB_SUCCESS;
}
