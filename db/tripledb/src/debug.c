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
 * Copyright (C) 1999 Geocast Network Systems. All
 * Rights Reserved.
 *
 * Contributor(s): Terry Weissman <terry@mozilla.org>
 */

/* Various debugging routines. */

#include "tdbtypes.h"
#include "tdbdebug.h"
#include "prprf.h"

void TDBDumpNode(PRFileDesc* fid, TDBNode* node)
{
    switch(node->type) {
    case TDBTYPE_STRING:
        PR_fprintf(fid, "%s", node->d.str.string);
        break;
    default:
        PR_fprintf(fid, "%ld", node->d.i);
    }
}


void TDBDumpRecord(PRFileDesc* fid, TDB* db, TDBPtr ptr, PRInt32 tree, int indent, const char* kidname)
{
    TDBRecord* record;
    int i;
    TDBPtr kid0;
    TDBPtr kid1;
    if (ptr == 0) return;
    record = tdbLoadRecord(db, ptr);
    for (i=0 ; i<indent ; i++) {
        PR_fprintf(fid, " ");
    }
    PR_fprintf(fid, "[%s] pos=%d, len=%d, bal=%d  ", kidname, record->position,
               record->length, record->entry[tree].balance);
    TDBDumpNode(fid, record->data[0]);
    PR_fprintf(fid, ",");
    TDBDumpNode(fid, record->data[1]);
    PR_fprintf(fid, ",");
    TDBDumpNode(fid, record->data[2]);
    PR_fprintf(fid, "\n");
    kid0 = record->entry[tree].child[0];
    kid1 = record->entry[tree].child[1];
    tdbFlush(db);
    TDBDumpRecord(fid, db, kid0, tree, indent + 1, "0");
    TDBDumpRecord(fid, db, kid1, tree, indent + 1, "1");

}


void TDBDumpTree(PRFileDesc* fid, TDB* db, PRInt32 tree)
{
    TDBPtr root;
    if (tree < 0) {
        root = db->freeroot;
        tree = 0;
    } else {
        root = db->roots[tree];
    }
    TDBDumpRecord(fid, db, root, tree, 0, "root");
}



typedef struct _TDBRangeSet {
    PRInt32 position;
    PRInt32 length;
    struct _TDBRangeSet* next;
    struct _TDBRangeSet* prev;
} TDBRangeSet;


static PRStatus checkMerges(TDBRangeSet* set, TDBRangeSet* tmp)
{
    while (tmp->next != set &&
           tmp->position + tmp->length == tmp->next->position) {
        TDBRangeSet* t = tmp->next;
        tmp->next = t->next;
        tmp->next->prev = tmp;
        tmp->length += t->length;
        PR_Free(t);
    }
    while (tmp->prev != set &&
           tmp->position == tmp->prev->position + tmp->prev->length) {
        TDBRangeSet* t = tmp->prev;
        tmp->prev = t->prev;
        tmp->prev->next = tmp;
        tmp->position = t->position;
        t->length += t->length;
        PR_Free(t);
    }
    return PR_SUCCESS;
}


static PRStatus addRange(TDBRangeSet* set, PRInt32 position, PRInt32 length)
{
    TDBRangeSet* tmp;
    TDBRangeSet* this;
    for (tmp = set->next ; tmp != set ; tmp = tmp->next) {
        if ((position >= tmp->position &&
             position < tmp->position + tmp->length) ||
            (tmp->position >= position &&
             tmp->position < position + length)) {
            PR_ASSERT(0);       /* Collision! */
            return PR_FAILURE;
        }
        if (position + length == tmp->position) {
            tmp->position = position;
            tmp->length += length;
            return checkMerges(set, tmp);
        } else if (tmp->position + tmp->length == position) {
            tmp->length += length;
            return checkMerges(set, tmp);
        }
        if (tmp->position > position + length) {
            break;
        }
    }
    this = PR_NEWZAP(TDBRangeSet);
    if (!this) return PR_FAILURE;
    this->next = tmp;
    this->prev = tmp->prev;
    this->next->prev = this;
    this->prev->next = this;
    this->position = position;
    this->length = length;
    return PR_SUCCESS;
}


typedef struct _TDBTreeInfo {
    TDB* db;
    PRInt32 tree;               /* Which tree we're analyzing */
    PRInt32 comparerule;
    PRInt32 count;              /* Number of nodes in tree. */
    TDBRangeSet set;            /* Range of bytes used by tree. */
    PRInt32 maxdepth;           /* Maximum depth of tree. */
} TDBTreeInfo;


static PRStatus checkTree(TDBTreeInfo* info, TDBPtr ptr, int depth,
                          int* maxdepth, TDBRecord* parent, int kid)
{
    TDBRecord* record;
    PRStatus status;
    TDBPtr kid0;
    TDBPtr kid1;
    int d0 = depth;
    int d1 = depth;
    PRInt8 bal;
    TDBRecord keep;
    PRInt64 cmp;
    int i;
    if (ptr == 0) return PR_SUCCESS;
    info->count++;
    if (*maxdepth < depth) {
        *maxdepth = depth;
    }
    record = tdbLoadRecord(info->db, ptr);
    if (parent) {
	cmp = tdbCompareRecords(record, parent, info->comparerule);
	PR_ASSERT((kid == 0 && cmp < 0) || (kid == 1 && cmp > 0));
	if (! ((kid == 0 && cmp < 0) || (kid == 1 && cmp > 0))) {
	    return PR_FAILURE;
	}
    }
    status = addRange(&(info->set), record->position, record->length);
    if (status != PR_SUCCESS) return status;
    bal = record->entry[info->tree].balance;
    if (bal < -1 || bal > 1) {
        PR_ASSERT(0);
        return PR_FAILURE;
    }
    kid0 = record->entry[info->tree].child[0];
    kid1 = record->entry[info->tree].child[1];
    memcpy(&keep, record, sizeof(TDBRecord));
    for (i=0 ; i<3 ; i++) {
	keep.data[i] = tdbNodeDup(keep.data[i]);
    }
    tdbFlush(info->db);
    status = checkTree(info, kid0, depth + 1, &d0, &keep, 0);
    if (status != PR_SUCCESS) return status;
    status = checkTree(info, kid1, depth + 1, &d1, &keep, 1);
    if (status != PR_SUCCESS) return status;
    for (i=0 ; i<3 ; i++) {
	TDBFreeNode(keep.data[i]);
    }
    if (d1 - d0 != bal) {
        PR_ASSERT(0);
        return PR_FAILURE;
    }
    if (*maxdepth < d0) {
        *maxdepth = d0;
    }
    if (*maxdepth < d1) {
        *maxdepth = d1;
    }

    return PR_SUCCESS;
}


PRStatus TDBSanityCheck(TDB* db, PRFileDesc* fid)
{
    TDBTreeInfo info;
    TDBRangeSet* tmp;
    int i;
    for (i=-1 ; i<4 ; i++) {
        info.set.next = &(info.set);
        info.set.prev = &(info.set);
        info.db = db;
        info.tree = (i < 0 ? 0 : i);
        info.comparerule = i;
        info.maxdepth = 0;
        info.count = 0;
        PR_fprintf(fid, "Checking tree %d...\n", i);
        if (checkTree(&info, i < 0 ? db->freeroot : db->roots[i], 0,
                      &(info.maxdepth), NULL, 0) != PR_SUCCESS) {
            PR_fprintf(fid, "Problem found!\n");
            return PR_FAILURE;
        }
        for (tmp = info.set.next; tmp != &(info.set) ; tmp = tmp->next) {
            PR_fprintf(fid, "  %d - %d   (%d)\n",
                       tmp->position, tmp->position + tmp->length,
                       tmp->length);
        }
        PR_fprintf(fid, "   maxdepth: %d   count %d\n", info.maxdepth,
                   info.count);
    }
    return PR_SUCCESS;
}


extern void TDBGetCursorStats(TDBCursor* cursor,
                              PRInt32* hits,
                              PRInt32* misses)
{
    *hits = cursor->hits;
    *misses = cursor->misses;
}



void makeDotEntry(TDB* db, PRFileDesc* fid, TDBPtr ptr, PRInt32 tree)
{
    TDBRecord* record;
    TDBPtr kid[2];
    int i;
    record = tdbLoadRecord(db, ptr);
    PR_fprintf(fid, "%d [label=\"%d\\n", record->position, record->position);
    for (i=0 ; i<3 ; i++) {
	TDBDumpNode(fid, record->data[i]);
	if (i<2) PR_fprintf(fid, ",");
    }
    PR_fprintf(fid, "\\nbal=%d\"]\n", record->entry[tree].balance);
    kid[0] = record->entry[tree].child[0];
    kid[1] = record->entry[tree].child[1];
    /* tdbFlush(db); */
    for (i=0 ; i<2 ; i++) {
	if (kid[i] != 0) {
	    makeDotEntry(db, fid, kid[i], tree);
	    PR_fprintf(fid, "%d -> %d [label=\"%d\"]\n", ptr, kid[i], i);
	}
    }
}

void TDBMakeDotGraph(TDB* db, const char* filename, PRInt32 tree)
{
    TDBPtr root;
    PRFileDesc* fid;
    fid = PR_Open(filename, PR_WRONLY | PR_CREATE_FILE, 0666);
    if (tree == -1) {
	root = db->freeroot;
	tree = 0;
    } else {
	root = db->roots[tree];
    }
    PR_fprintf(fid, "digraph G {\n");
    makeDotEntry(db, fid, root, tree);
    PR_fprintf(fid, "}\n");
    PR_Close(fid);
}
