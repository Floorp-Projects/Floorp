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

#include "cursor.h"
#include "intern.h"
#include "node.h"
#include "rtype.h"
#include "tdb.h"
#include "vector.h"
#include "windex.h"

struct _TDBCursor {
    TDB* tdb;
    TDBBase* base;
    void* implcursor;
    TDBCursor* next;
    TDBNodePtr* range;
    TDBVector* vector;          /* The last vector returned from this cursor */
    TDBVector* curvector;       /* The current position being considered by
                                   the cursor. */
    TDBVector* candidate;       /* The next candidate to be returned as a
                                   match. */
    TDBWindexCursor* wcursor;
    DBC* dbcurs;
    TDBInt32 curentry;
    TDBRType* indextype;
    TDBInt32 numfields;         /* Cached from indextype. */
    TDBBool reverse;
    TDBBool includefalse;
    TDBBool alldone;
    TDBTriple triple;
    TDBInt32 hits;
    TDBInt32 misses;
};


TDBCursor*
tdbCursorNew(TDB* tdb, TDBRType* indextype, TDBNodePtr* range,
             TDBBool includefalse, TDBBool reverse)
{
    TDBInt32 i;
    TDBCursor* cursor;
    if (indextype == NULL || range == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    cursor = tdb_NEWZAP(TDBCursor);
    if (!cursor) return NULL;
    cursor->tdb = tdb;
    cursor->base = tdbGetBase(tdb);
    cursor->numfields = tdbRTypeGetNumFields(indextype);
    cursor->range = tdb_Calloc(cursor->numfields, sizeof(TDBNodePtr));
    if (!cursor->range) {
        tdb_Free(cursor);
        return NULL;
    }
    for (i=0 ; i<cursor->numfields ; i++) {
        if (range[i]) {
            if (i == 1 && range[i]->type != TDBTYPE_INTERNED) {
                cursor->range[i] = tdbIntern(cursor->base, range[i]);
            } else {
                cursor->range[i] = TDBNodeDup(range[i]);
            }
            if (!cursor->range[i]) {
                tdbCursorFree(cursor);
                return NULL;
            }
        }
    }
    cursor->indextype = indextype;
    cursor->includefalse = includefalse;
    cursor->reverse = reverse;
    cursor->next = tdbGetFirstCursor(cursor->tdb);
    tdbSetFirstCursor(cursor->tdb, cursor);
    return cursor;
}


TDBCursor*
tdbCursorNewWordSubstring(TDB* tdb, const char* string)
{
    TDBWindex* windex;
    TDBCursor* cursor;
    TDBBase* base;
    base = tdbGetBase(tdb);
    windex = tdbGetWindex(base);
    cursor = tdb_NEWZAP(TDBCursor);
    if (!cursor) return NULL;
    cursor->base = base;
    cursor->tdb = tdb;
    cursor->wcursor = tdbWindexGetCursor(windex, string);
    if (!cursor->wcursor) {
        tdb_Free(cursor);
        return NULL;
    }
    return cursor;
}


TDBCursor*
tdbCursorNewImpl(TDB* tdb, void* implcursor)
{
    TDBCursor* cursor;
    if (tdb == NULL || implcursor == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    cursor = tdb_NEWZAP(TDBCursor);
    if (!cursor) return NULL;
    cursor->tdb = tdb;
    cursor->base = tdbGetBase(tdb);
    cursor->implcursor = implcursor;
    return cursor;
}

static void
freeCurVector(TDBCursor* cursor)
{
    if (cursor->curvector) {
        if (cursor->curvector != cursor->candidate &&
              cursor->curvector != cursor->vector) {
            tdbVectorFree(cursor->curvector);
        }
        cursor->curvector = NULL;
    }
}


void
tdbCursorFree(TDBCursor* cursor)
{
    TDBInt32 i;
    TDBCursor* first;
    if (cursor == NULL) {
        tdb_ASSERT(0);
        return;
    }
    if (cursor->wcursor) tdbWindexCursorFree(cursor->wcursor);
    freeCurVector(cursor);
    if (cursor->vector) tdbVectorFree(cursor->vector);
    if (cursor->candidate) tdbVectorFree(cursor->candidate);
    if (cursor->range) {
        for (i=0 ; i<cursor->numfields ; i++) {
            if (cursor->range[i]) TDBFreeNode(cursor->range[i]);
        }
        tdb_Free(cursor->range);
    }
    if (cursor->dbcurs) cursor->dbcurs->c_close(cursor->dbcurs);
    first = tdbGetFirstCursor(cursor->tdb);
    if (first == cursor) {
        tdbSetFirstCursor(cursor->tdb, first->next);
    } else {
        for (; first ; first = first->next) {
            if (first->next == cursor) {
                first->next = cursor->next;
                break;
            }
        }
    }
    tdb_Free(cursor);
}


TDB*
tdbCursorGetTDB(TDBCursor* cursor)
{
    if (cursor == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return cursor->tdb;
}


TDBBase*
tdbCursorGetBase(TDBCursor* cursor)
{
    if (cursor == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return cursor->base;
}



void*
tdbCursorGetImplcursor(TDBCursor* cursor)
{
    if (cursor == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    return cursor->implcursor;
}


static TDBVector*
getNextCandidate(TDBCursor* cursor)
{
    TDBInt32 i;
    TDBNode minmaxnode;
    TDBNodePtr nodes[3];
    TDBBool bumpcursor = TDB_FALSE;
    TDBVector* vector;
    TDBBool inrange;
           
    if (cursor->curvector == NULL) {
        /* First time. */
        for (i=0 ; i<cursor->numfields ; i++) {
            if (cursor->range[i]) {
                nodes[i] = cursor->range[i];
            } else {
                minmaxnode.type =
                    cursor->reverse ? TDBTYPE_MAXNODE : TDBTYPE_MINNODE;
                nodes[i] = &minmaxnode;

                /* The below should be restored if we ever restore the rule
                   that the first member of a triple must be an ID. */
/*                  if (i == 0) { */
/*                      minmaxid.type = TDBTYPE_ID; */
/*                      minmaxid.d.i = cursor->reverse ? 0xffffffffffffffff : 0; */
/*                      nodes[i] = &minmaxid; */
/*                  } */

            }
        }
        cursor->curvector = tdbVectorNewFromNodes(cursor->base, nodes, 0, 0, 0);
        if (!cursor->curvector) return NULL;
    }
    while (1) {
        vector = NULL;
        if (cursor->dbcurs == NULL) {
            cursor->dbcurs = tdbRTypeGetCursor(cursor->indextype,
                                               cursor->curvector);
            if (!cursor->dbcurs) return NULL;
        } else {
            bumpcursor = TDB_TRUE;
        }
        vector = tdbRTypeGetCursorValue(cursor->indextype, cursor->dbcurs,
                                        bumpcursor ?
                                        (cursor->reverse ? DB_PREV :
                                         DB_NEXT) : DB_CURRENT);
        freeCurVector(cursor);
        cursor->curvector = vector;
        if (vector == NULL) {
            break;
        }
        inrange = TDB_TRUE;
        for (i=0 ; i<cursor->numfields ; i++) {
            if (cursor->range[i] &&
                TDBCompareNodes(cursor->range[i],
                                tdbVectorGetInternedNode(vector, i)) != 0) {
                inrange = TDB_FALSE;
                break;
            }
        }
        if (inrange) {
            if (cursor->includefalse ||
                (tdbVectorGetFlags(cursor->curvector) & TDBFLAG_ISASSERT)) {
                return cursor->curvector;
            }
        }
            
        if (!inrange) break;

        /* So, this one missed, but there might be more. */
        cursor->misses++;
    }
    freeCurVector(cursor);
    if (cursor->dbcurs) {
        cursor->dbcurs->c_close(cursor->dbcurs);
        cursor->dbcurs = NULL;
    }
    return NULL;
}



TDBVector*
tdbCursorGetNext(TDBCursor* cursor)
{
    TDBInt32 i;
    TDBVector* next;
    TDBInt32 l;
    TDBInt32 l1;
    TDBInt32 l2;
    TDBInt32 numlayers;
    if (cursor == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    if (cursor->wcursor) {
        while (1) {
            if (cursor->vector) tdbVectorFree(cursor->vector);
            cursor->vector = tdbWindexGetCursorValue(cursor->wcursor);
            if (!cursor->vector) return NULL;
            if (tdbVectorLayerMatches(cursor->vector, cursor->tdb)) {
                cursor->hits++;
                return cursor->vector;
            }
            cursor->misses++;
        }
    }
    if (cursor->alldone) return NULL;

    while (1) {
        next = getNextCandidate(cursor);
        if (next == NULL) {
            cursor->alldone = TDB_TRUE;
            if (cursor->vector) tdbVectorFree(cursor->vector);
            cursor->vector = cursor->candidate;
            cursor->candidate = NULL;
            if (cursor->vector) cursor->hits++;
            return cursor->vector;
        }
        if (!tdbVectorLayerMatches(next, cursor->tdb)) {
            cursor->misses++;
            continue;
        }
        if (cursor->candidate == NULL) {
            cursor->candidate = next;
            continue;
        }
        if (!tdbVectorEqual(cursor->candidate, next)) {
            if (cursor->vector) tdbVectorFree(cursor->vector);
            cursor->vector = cursor->candidate;
            cursor->candidate = next;
            cursor->hits++;
            return cursor->vector;
        }
        cursor->misses++;
        l1 = tdbVectorGetLayer(cursor->candidate);
        l2 = tdbVectorGetLayer(next);
        if (l1 != l2) {
            numlayers = tdbGetNumLayers(cursor->tdb);
            for (i=0 ; i<numlayers ; i++) {
                l = tdbGetLayer(cursor->tdb, i);
                if (l == l1) {
                    break;      /* Our existing candidate wins. */
                }
                if (l == l2) {
                    tdbVectorFree(cursor->candidate);
                    cursor->candidate = next;
                    next = NULL;
                }
            }
        }
    }
}


TDBTriple*
tdbCursorGetLastResultAsTriple(TDBCursor* cursor)
{
    TDBInt32 i;
    if (cursor == NULL) {
        tdb_ASSERT(0);
        return NULL;
    }
    if (cursor->vector == NULL) return NULL;
    for (i=0 ; i<3 ; i++) {
        cursor->triple.data[i] =
            tdbVectorGetNonInternedNode(cursor->vector, i);
    }
    cursor->triple.asserted =
        ((tdbVectorGetFlags(cursor->vector) & TDBFLAG_ISASSERT) != 0);
    return &(cursor->triple);
}

extern void
TDBGetCursorStats(TDBCursor* cursor,
                  TDBInt32* hits,
                  TDBInt32* misses)
{
    if (cursor == NULL || hits == NULL || misses == NULL) {
        tdb_ASSERT(0);
        return;
    }
    *hits = cursor->hits;
    *misses = cursor->misses;
}


TDBStatus
tdbCursorInvalidateCache(TDB* tdb)
{
    TDBCursor* cursor;
    for (cursor = tdbGetFirstCursor(tdb) ; cursor ; cursor = cursor->next) {
        if (cursor->dbcurs) {
            cursor->dbcurs->c_close(cursor->dbcurs);
            cursor->dbcurs = NULL;
        }
    }
    return TDB_SUCCESS;
}
