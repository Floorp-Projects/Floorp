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

/* Various debugging routines. */
#include "tdbtypes.h"
#include "tdbdebug.h"

#include "tdb.h"
#include "rtype.h"
#include "vector.h"

#ifdef TDB_USE_NSPR
#include "prprf.h"
#define tdb_fprintf     PR_fprintf
#else
#define tdb_fprintf     fprintf
#endif

void TDBDumpNode(TDBFileDesc* fid, TDBNode* node)
{
#ifdef TDB_USE_NSPR
    PRExplodedTime expl;
#endif
    switch(node->type) {
    case TDBTYPE_STRING:
        tdb_fprintf(fid, "%s", node->d.str.string);
        break;
#ifdef TDB_USE_NSPR
    case TDBTYPE_TIME:
        PR_ExplodeTime(node->d.time, PR_LocalTimeParameters, &expl);
        tdb_fprintf(fid, "%02d/%02d/%04d %02d:%02d:%02d",
                   expl.tm_month+1, expl.tm_mday, expl.tm_year,
                   expl.tm_hour, expl.tm_min, expl.tm_sec);
        break;
#endif
    default:
        tdb_fprintf(fid, "%lld", node->d.i);
    }
}


void
TDBDumpTree(TDBFileDesc* fid, TDB* db, TDBInt32 tree)
{
    /* Write me!  (or not...) */
}






TDBStatus TDBSanityCheck(TDBBase* base, TDBFileDesc* fid)
{

    TDBNode minnode;
    DBC* cursor;
    TDBVector* vector;
    int m;
    int i;
    int flag;
    int count;
    int numindices;
    TDBNodePtr nodes[3];
    TDBRType* rtype;
    tdbBeginExclusiveOp(base);
    minnode.type = TDBTYPE_MINNODE;
    for (i=0 ; i<3 ; i++) {
        nodes[i] = &minnode;
    }
    numindices = tdbGetNumIndices(base);
    for (m = 0 ; m<numindices ; m++) {
        if (fid) tdb_fprintf(fid, "Checking index %d ...", m);
        rtype = tdbGetIndex(base, m);
        vector = tdbVectorNewFromNodes(base, nodes, 0, 0, 0);
        cursor = tdbRTypeGetCursor(rtype, vector);
        tdbVectorFree(vector);
        if (!cursor) continue;
        count = 0;
        flag = DB_CURRENT;
        while (NULL != (vector = tdbRTypeGetCursorValue(rtype,
                                                        cursor, flag))) {
            flag = DB_NEXT;
            count++;
            for (i=0 ; i<numindices ; i++) {
                tdb_ASSERT(i == m || tdbRTypeProbe(tdbGetIndex(base, i),
                                                   vector));
            }
            tdbVectorFree(vector);
        }
        cursor->c_close(cursor);
        if (fid) tdb_fprintf(fid, " done (%d triples found)\n", count);
    }
    tdbEndExclusiveOp(base);


    return TDB_SUCCESS;
}





void TDBMakeDotGraph(TDB* db, const char* filename, TDBInt32 tree)
{
    /* Write me!  (or not...) */
}
