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
#include "util.h"

TDBNodePtr TDBCreateStringNode(const char* string)
{
    TDBNode* result;
    TDBInt32 length;
    tdb_ASSERT(string != NULL);
    if (string == NULL) return NULL;
    length = strlen(string);
    if (length > 65535) {
        tdb_ASSERT(0);          /* Don't call us with huge strings! */
        return NULL;
    }
    result = (TDBNode*) tdb_Malloc(sizeof(TDBNode) + length + 1);

    if (result == NULL) return NULL;
    result->type = TDBTYPE_STRING;
    result->d.str.length = length;
    strcpy(result->d.str.string, string);
    return result;
}


TDBNodePtr TDBCreateIntNode(TDBInt64 value, TDBInt8 type)
{
    TDBNode* result;
    if ((type < TDBTYPE_INT32 || type >= TDBTYPE_STRING) && type !=
          TDBTYPE_INTERNED) {
        tdb_ASSERT(0);
        return NULL;
    }
    result = tdb_NEWZAP(TDBNode);
    if (result == NULL) return NULL;
    result->type = type;
    result->d.i = value;
    return result;
}


TDBNodePtr tdbCreateSpecialNode(TDBInt8 type)
{
    TDBNodePtr result;
    tdb_ASSERT(type == TDBTYPE_MINNODE || type == TDBTYPE_MAXNODE);
    result = tdb_NEWZAP(TDBNode);
    if (result == NULL) return NULL;
    result->type = type;
    return result;
}

void TDBFreeNode(TDBNode* node)
{
    tdb_Free(node);
}

TDBNode* TDBNodeDup(TDBNode* node)
{
    TDBNode* result;
    TDBInt32 length;
    tdb_ASSERT(node != NULL);
    if (node == NULL) return NULL;
    if (node->type == TDBTYPE_STRING) {
        length = node->d.str.length;
        result = tdb_Malloc(sizeof(TDBNode) + length + 1);
        if (result == NULL) return NULL;
        result->type = TDBTYPE_STRING;
        result->d.str.length = length;
        memcpy(result->d.str.string, node->d.str.string, length);
        result->d.str.string[length] = '\0';
        return result;
    }
    if (node->type == TDBTYPE_MINNODE || node->type == TDBTYPE_MAXNODE) {
        return tdbCreateSpecialNode(node->type);
    }
    return TDBCreateIntNode(node->d.i, node->type);
}


TDBInt32 tdbNodeSize(TDBNode* node)
{
    tdb_ASSERT(node != NULL);
    if (node == NULL) return 0;
    switch (node->type) {
    case TDBTYPE_INT32:
        return 1 + sizeof(TDBInt32);
    case TDBTYPE_INT64:
        return 1 + sizeof(TDBInt64);
    case TDBTYPE_ID:
        return 1 + sizeof(TDBUint64);
    case TDBTYPE_TIME:
        return 1 + sizeof(TDBTime);
    case TDBTYPE_STRING:
        return 1 + node->d.str.length + 1;
    case TDBTYPE_BLOB:
        return 1 + sizeof(TDBUint16) + node->d.str.length;
    case TDBTYPE_INTERNED:
        return 1 + sizeof(TDBUint32);
    default:
        tdb_ASSERT(0);
        return 0;
    }
}


TDBStatus tdbPutNode(char** ptr, TDBNode* node, char* endptr)
{
    if (tdbPutInt8(ptr, node->type, endptr) != TDB_SUCCESS) {
        return TDB_FAILURE;
    }
    switch (node->type) {
    case TDBTYPE_STRING:
        if (endptr - *ptr <= node->d.str.length) return TDB_FAILURE;
        memcpy(*ptr, node->d.str.string, node->d.str.length);
        *ptr += node->d.str.length;
        *(*ptr)++ = '\0';
        return TDB_SUCCESS;
    case TDBTYPE_BLOB:
        tdbPutUInt16(ptr, node->d.str.length, endptr);
        if (endptr - *ptr < node->d.str.length) {
            return TDB_FAILURE;
        }
        memcpy(*ptr, node->d.str.string, node->d.str.length);
        *ptr += node->d.str.length;
        return TDB_SUCCESS;
    case TDBTYPE_INT32:
        return tdbPutInt32(ptr, (TDBInt32) node->d.i, endptr);
    case TDBTYPE_INT64:
    case TDBTYPE_TIME:
        return tdbPutInt64(ptr, node->d.i, endptr);
    case TDBTYPE_ID:
        return tdbPutUInt64(ptr, node->d.i, endptr);
    case TDBTYPE_INTERNED:
        return tdbPutUInt32(ptr, node->d.i, endptr);
    case TDBTYPE_MINNODE:
    case TDBTYPE_MAXNODE:
        return TDB_SUCCESS;
    default:
        tdb_ASSERT(0);
        return TDB_FAILURE;
    }
}


TDBNode* tdbGetNode(char** ptr, char* endptr)
{
    TDBNode* result;
    TDBInt8 type;
    TDBInt64 i;
    TDBUint16 length;
    char* p;
    type = tdbGetInt8(ptr, endptr);
    if (*ptr >= endptr) return NULL;
    switch (type) {
    case TDBTYPE_STRING:
        for (p = *ptr ; p <= endptr && *p ; p++) {}
        if (p > endptr) return NULL;
        length = p - *ptr;
        result = (TDBNode*) tdb_Malloc(sizeof(TDBNode) + length + 1);
        if (result == NULL) {
            return NULL;
        }
        result->type = type;
        result->d.str.length = length;
        memcpy(result->d.str.string, *ptr, length + 1);
        *ptr = p + 1;
        return result;
    case TDBTYPE_BLOB:
        length = tdbGetUInt16(ptr, endptr);
        if (*ptr > endptr) return NULL;
        if (*ptr + length > endptr) return NULL;
        result = (TDBNode*) tdb_Malloc(sizeof(TDBNode) + length + 1);
        if (result == NULL) {
            return NULL;
        }
        result->type = type;
        result->d.str.length = length;
        memcpy(result->d.str.string, *ptr, length);
        result->d.str.string[length] = '\0';
        (*ptr) += length;
        return result;
    case TDBTYPE_INT32:
        i = tdbGetInt32(ptr, endptr);
        break;
    case TDBTYPE_INT64:
    case TDBTYPE_TIME:
        i = tdbGetInt64(ptr, endptr);
        break;
    case TDBTYPE_ID:
        i = tdbGetUInt64(ptr, endptr);
        break;
    case TDBTYPE_INTERNED:
        i = tdbGetUInt32(ptr, endptr);
        break;
    default:
        return NULL;
    }
    return TDBCreateIntNode(i, type);
}


TDBInt64 TDBCompareNodes(TDBNode* n1, TDBNode* n2)
{
    TDBInt64 result;
    if (n1->type != n2->type) {
        return n1->type - n2->type;
    } else if (n1->type == TDBTYPE_STRING || n1->type == TDBTYPE_BLOB) {
        result = memcmp(n1->d.str.string, n2->d.str.string,
                        n1->d.str.length < n2->d.str.length ?
                        n1->d.str.length : n2->d.str.length);
        if (result != 0) return result;
        return ((TDBInt64) n1->d.str.length) - ((TDBInt64) n2->d.str.length);
    } else {
        return n1->d.i - n2->d.i;
    }
}
