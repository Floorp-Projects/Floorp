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

#include <ctype.h>

#include "tdbtypes.h"

#include "tdb.h"
#include "util.h"
#include "vector.h"
#include "windex.h"

#define MAXWORDLEN 10

typedef struct {
    char word[MAXWORDLEN + 1];
    TDBInt32 length;
    TDBUint16 position;
} WordInfo;

struct _TDBWindex {
    TDBBase* base;
    DB* db;
};


struct _TDBWindexCursor {
    TDBWindex* windex;
    char* string;
    DBC* cursor;
    WordInfo* winfo;
    TDBInt32 numwinfo;
    TDBInt32 main;
    int cmdflag;
};


TDBWindex*
tdbWindexNew(TDBBase* base)
{
    TDBWindex* windex;
    int dbstatus;
    windex = tdb_NEWZAP(TDBWindex);
    if (!windex) return NULL;
    windex->base = base;
    dbstatus = db_create(&(windex->db), tdbGetDBEnv(base), 0);
    if (dbstatus != DB_OK) goto FAIL;
    dbstatus = windex->db->open(windex->db, "windex", NULL, DB_BTREE,
                                DB_CREATE, 0666);
    if (dbstatus != DB_OK) goto FAIL;
    return windex;

 FAIL:
    tdbWindexFree(windex);
    return NULL;
}

void
tdbWindexFree(TDBWindex* windex)
{
    if (windex == NULL) {
        tdb_ASSERT(0);
        return;
    }
    if (windex->db) {
        windex->db->close(windex->db, 0);
    }
    tdb_Free(windex);
}


TDBStatus
tdbWindexSync(TDBWindex* windex)
{
    if (windex == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    if (windex->db->sync(windex->db, 0) != DB_OK) return TDB_FAILURE;
    return TDB_SUCCESS;
}


static WordInfo*
splitIntoWords(char* str, WordInfo* winfobuf, TDBInt32 winfobuflen)
{
    WordInfo* winfo;
    WordInfo* tmp;
    TDBInt32 cur;
    TDBInt32 max;
    char* ptr;
    char* ptr1;
    char* ptr2;
    TDBInt32 i;
    TDBInt32 length;
    

    winfo = winfobuf;
    max = winfobuflen - 1;
    cur = 0;
    for (ptr1 = str ; *ptr1 ; ptr1++) {
        if (!isalnum(*ptr1)) continue;
        for (ptr2 = ptr1 + 1 ; *ptr2 && isalnum(*ptr2) ; ptr2++) { }
        if (ptr2 - ptr1 > 2) {
            /* Should check for stopwords here. XXX */
            if (cur >= max) {
                max = cur + 20;
                tmp = tdb_Calloc(max + 1, sizeof(WordInfo));
                for (i=0 ; i<cur ; i++) {
                    tmp[i] = winfo[i];
                }
                if (winfo != winfobuf) tdb_Free(winfo);
                winfo = tmp;
            }
            length = ptr2 - ptr1;
            if (length > MAXWORDLEN) length = MAXWORDLEN;
            winfo[cur].length = length;
            winfo[cur].position = ptr1 - str;
            for (ptr = winfo[cur].word ; length > 0 ; length--) {
                *ptr++ = tolower(*ptr1++);
            }
            *ptr = '\0';
            cur++;
        }
        ptr1 = ptr2;
        if (!*ptr1) break;
    }
    if (winfo) winfo[cur].length = 0;
    return winfo;
}


TDBStatus
tdbWindexAdd(TDBWindex* windex, TDBVector* vector)
{
    TDBNodePtr node;
    WordInfo winfobuf[20];
    WordInfo* winfo;
    WordInfo* wptr;
    char buf[100];
    DBT key;
    DBT data;
    char* ptr;
    char* endptr;
    int dbstatus;
    if (windex == NULL || vector == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    node = tdbVectorGetNonInternedNode(vector, 2 /* XXX Stupid hardcoding */);
    if (node == NULL) return TDB_FAILURE;
    if (node->type != TDBTYPE_STRING) {
        return TDB_SUCCESS;     /* Quietly do nothing if it's not a string. */
    }
    winfo = splitIntoWords(node->d.str.string, winfobuf,
                           sizeof(winfobuf) / sizeof(winfobuf[0]));
    if (!winfo) return TDB_FAILURE;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    for (wptr = winfo ; wptr->length > 0 ; wptr++) {
        ptr = buf;
        endptr = buf + sizeof(buf);
        tdbPutString(&ptr, wptr->word, endptr);
        tdbPutPtr(&ptr, tdbVectorGetRecordNumber(vector), endptr);
        tdbPutUInt16(&ptr, (TDBUint16) wptr->position, endptr);
        if (ptr >= endptr) {
            tdb_ASSERT(0);
        } else {
            key.size = ptr - buf;
            key.data = buf;
            dbstatus = windex->db->put(windex->db,
                                       tdbGetTransaction(windex->base),
                                       &key, &data, 0);
            if (dbstatus != DB_OK) return TDB_FAILURE;
        }
    }
    if (winfo != winfobuf) {
        tdb_Free(winfo);
    }
    return TDB_SUCCESS;
}


TDBStatus
tdbWindexRemove(TDBWindex* windex, TDBVector* vector)
{
    TDBNodePtr node;
    WordInfo winfobuf[20];
    WordInfo* winfo;
    WordInfo* wptr;
    DBT key;
    DBT data;
    char* ptr;
    char* endptr;
    char buf[100];
    int dbstatus;
    if (windex == NULL || vector == NULL) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    node = tdbVectorGetNonInternedNode(vector, 2 /* XXX Stupid hardcoding */);
    if (node == NULL) return TDB_FAILURE;
    if (node->type != TDBTYPE_STRING) {
        return TDB_SUCCESS;     /* Quietly do nothing if it's not a string. */
    }
    winfo = splitIntoWords(node->d.str.string, winfobuf,
                           sizeof(winfobuf) / sizeof(winfobuf[0]));
    if (!winfo) return TDB_FAILURE;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    for (wptr = winfo ; wptr->length > 0 ; wptr++) {
        ptr = buf;
        endptr = buf + sizeof(buf);
        tdbPutString(&ptr, wptr->word, endptr);
        tdbPutPtr(&ptr, tdbVectorGetRecordNumber(vector), endptr);
        tdbPutUInt16(&ptr, (TDBUint16) wptr->position, endptr);
        if (ptr >= endptr) {
            tdb_ASSERT(0);
        } else {
            key.size = ptr - buf;
            key.data = buf;
            dbstatus = windex->db->del(windex->db,
                                       tdbGetTransaction(windex->base),
                                       &key, 0);
            if (dbstatus != DB_OK) return TDB_FAILURE;
        }
    }
    if (winfo != winfobuf) {
        tdb_Free(winfo);
    }
    return TDB_SUCCESS;
    
}


TDBWindexCursor*
tdbWindexGetCursor(TDBWindex* windex, const char* string)
{
    DBT key;
    DBT data;
    int dbstatus;
    TDBWindexCursor* cursor;
    TDBInt32 i;
    TDBInt32 l;
    if (windex == NULL || string == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    cursor = tdb_NEWZAP(TDBWindexCursor);
    if (!cursor) return NULL;
    cursor->windex = windex;
    cursor->string = tdb_strdup(string);
    if (!cursor->string) goto FAIL;
    cursor->winfo = splitIntoWords(cursor->string, NULL, 0);
    if (cursor->winfo == NULL) goto FAIL;

    cursor->main = 0;
    cursor->cmdflag = DB_CURRENT;
    
    for (i=0 ; cursor->winfo[i].length > 0 ; i++) {
         l = cursor->winfo[i].length;
         if (l > cursor->winfo[cursor->main].length) {
             cursor->main = i;
         }
         cursor->numwinfo = i + 1;
    }

    if (cursor->numwinfo == 0) goto FAIL;
    

    dbstatus = windex->db->cursor(windex->db, NULL,
                                  &(cursor->cursor), 0);
    if (dbstatus != DB_OK) goto FAIL;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = cursor->winfo[cursor->main].word;
    key.size = cursor->winfo[cursor->main].length + 1;

    dbstatus = cursor->cursor->c_get(cursor->cursor, &key, &data,
                                     DB_SET_RANGE);
    if (dbstatus != DB_OK && dbstatus != DB_NOTFOUND) goto FAIL;

    return cursor;
 FAIL:
    tdbWindexCursorFree(cursor);
    return NULL;
}


TDBStatus
tdbWindexCursorFree(TDBWindexCursor* cursor)
{
    if (!cursor) {
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
    if (cursor->string) tdb_Free(cursor->string);
    if (cursor->cursor) cursor->cursor->c_close(cursor->cursor);
    if (cursor->winfo) tdb_Free(cursor->winfo);
    tdb_Free(cursor);
    return TDB_SUCCESS;
}


TDBVector*
tdbWindexGetCursorValue(TDBWindexCursor* cursor)
{
    TDBInt32 l;
    DBT key;
    DBT data;
    TDBBool match;
    TDBPtr recordnum;
    TDBInt32 position;
    TDBInt32 p;
    char* ptr;
    char* endptr;
    TDBInt32 i;
    char buf[MAXWORDLEN + 20];
    int dbstatus;
    TDBWindex* windex;
    TDBVector* vector;
    TDBNodePtr node;
    if (cursor == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    windex = cursor->windex;
    while (1) {
        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));
        dbstatus = cursor->cursor->c_get(cursor->cursor, &key, &data,
                                         cursor->cmdflag);
        cursor->cmdflag = DB_NEXT;
        if (dbstatus != DB_OK) return NULL;
        l = cursor->winfo[cursor->main].length;
        if (key.size <= l ||
              memcmp(key.data, cursor->winfo[cursor->main].word, l + 1) != 0) {
            /* We're past the end of matching things. */
            return NULL;
        }
        ptr = key.data + l + 1;
        endptr = key.data + key.size;
        recordnum = tdbGetPtr(&ptr, endptr);
        position = tdbGetUInt16(&ptr, endptr);
        position -= cursor->winfo[cursor->main].position;
        match = TDB_TRUE;
        for (i=0 ; i<cursor->numwinfo ; i++) {
            if (i == cursor->main) continue;
            p = position + cursor->winfo[i].position;
            if (p < 0) {
                match = TDB_FALSE;
                break;
            }
            ptr = buf;
            endptr = buf + sizeof(buf);
            tdbPutString(&ptr, cursor->winfo[i].word, endptr);
            tdbPutPtr(&ptr, recordnum, endptr);
            tdbPutUInt16(&ptr, p, endptr);
            key.data = buf;
            key.size = ptr - buf;
            memset(&data, 0, sizeof(data));
            dbstatus = windex->db->get(windex->db,
                                       tdbGetTransaction(windex->base),
                                       &key, &data, 0);
            if (dbstatus != DB_OK) {
                match = TDB_FALSE;
                break;
            }
        }
        if (match) {
            vector = tdbVectorNewFromRecord(windex->base, recordnum);
            node = tdbVectorGetNonInternedNode(vector, 2);
            if (node && node->type == TDBTYPE_STRING &&
                tdb_strcasestr(node->d.str.string, cursor->string) != NULL) {
                return vector;
            }
            tdbVectorFree(vector);
        }
    }
}
                        
