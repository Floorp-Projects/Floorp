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

/* Operations that do things on nodes. */

#include "tdbtypes.h"

TDBNodePtr TDBCreateStringNode(const char* string)
{
    TDBNode* result;
    PRInt32 length;
    PR_ASSERT(string != NULL);
    if (string == NULL) return NULL;
    length = strlen(string);
    result = (TDBNode*) PR_Malloc(sizeof(TDBNode) + length + 1);

    if (result == NULL) return NULL;
    result->type = TDBTYPE_STRING;
    result->d.str.length = length;
    strcpy(result->d.str.string, string);
    return result;
}


TDBNodePtr TDBCreateIntNode(PRInt64 value, PRInt8 type)
{
    TDBNode* result;
    PR_ASSERT(type >= TDBTYPE_INT8 && type <= TDBTYPE_TIME);
    if (type < TDBTYPE_INT8 || type > TDBTYPE_TIME) {
        return NULL;
    }
    result = PR_NEW(TDBNode);
    if (result == NULL) return NULL;
    result->type = type;
    result->d.i = value;
    return result;
}

void TDBFreeNode(TDBNode* node)
{
    PR_Free(node);
}

TDBNode* tdbNodeDup(TDBNode* node)
{
    TDBNode* result;
    PRInt32 length;
    PR_ASSERT(node != NULL);
    if (node == NULL) return NULL;
    if (node->type == TDBTYPE_STRING) {
        length = node->d.str.length;
        result = PR_Malloc(sizeof(TDBNode) + length + 1);
        if (result == NULL) return NULL;
        result->type = TDBTYPE_STRING;
        result->d.str.length = length;
        memcpy(result->d.str.string, node->d.str.string, length);
        result->d.str.string[length] = '\0';
        return result;
    }
    return TDBCreateIntNode(node->d.i, node->type);
}


PRInt32 tdbNodeSize(TDBNode* node)
{
    PR_ASSERT(node != NULL);
    if (node == NULL) return 0;
    switch (node->type) {
    case TDBTYPE_INT8:
        return 1 + sizeof(PRInt8);
    case TDBTYPE_INT16:
        return 1 + sizeof(PRInt16);
    case TDBTYPE_INT32:
        return 1 + sizeof(PRInt32);
    case TDBTYPE_INT64:
        return 1 + sizeof(PRInt64);
    case TDBTYPE_TIME:
        return 1 + sizeof(PRTime);
    case TDBTYPE_STRING:
        return 1 + sizeof(PRUint16) + node->d.str.length;
    default:
        PR_ASSERT(0);
        return 0;
    }
}


PRInt64 tdbCompareNodes(TDBNode* n1, TDBNode* n2)
{
    if (n1->type == TDBTYPE_STRING && n2->type == TDBTYPE_STRING) {
        return strcmp(n1->d.str.string, n2->d.str.string);
    } else if (n1->type != TDBTYPE_STRING && n2->type != TDBTYPE_STRING) {
        return n1->d.i - n2->d.i;
    } else {
        return n1->type - n2->type;
    }
}


TDBNode* tdbGetNode(TDB* db, char** ptr)
{
    TDBNode* result;
    PRInt8 type = tdbGetInt8(ptr);
    PRInt64 i;
    if (type == TDBTYPE_STRING) {
        PRUint16 length = tdbGetUInt16(ptr);
        result = (TDBNode*) PR_Malloc(sizeof(TDBNode) + length + 1);
        if (result == NULL) {
            return NULL;
        }
        result->type = type;
        result->d.str.length = length;
        memcpy(result->d.str.string, *ptr, length);
        result->d.str.string[length] = '\0';
        (*ptr) += length;
    } else {
        switch(type) {
        case TDBTYPE_INT8:
            i = tdbGetInt8(ptr);
            break;
        case TDBTYPE_INT16:
            i = tdbGetInt16(ptr);
            break;
        case TDBTYPE_INT32:
            i = tdbGetInt32(ptr);
            break;
        case TDBTYPE_INT64:
        case TDBTYPE_TIME:
            i = tdbGetInt64(ptr);
            break;
        default:
            tdbMarkCorrupted(db);
            return NULL;
        }
        result = TDBCreateIntNode(i, type);
    }
    return result;
}


void tdbPutNode(TDB* db, char** ptr, TDBNode* node)
{
    tdbPutInt8(ptr, node->type);
    switch (node->type) {
        case TDBTYPE_STRING:
            tdbPutUInt16(ptr, node->d.str.length);
            memcpy(*ptr, node->d.str.string, node->d.str.length);
            *ptr += node->d.str.length;
            break;
        case TDBTYPE_INT8:
            tdbPutInt8(ptr, (PRInt8) node->d.i);
            break;
        case TDBTYPE_INT16:
            tdbPutInt16(ptr, (PRInt16) node->d.i);
            break;
        case TDBTYPE_INT32:
            tdbPutInt32(ptr, (PRInt32) node->d.i);
            break;
        case TDBTYPE_INT64:
        case TDBTYPE_TIME:
            tdbPutInt64(ptr, node->d.i);
            break;
        default:
            tdbMarkCorrupted(db);
            break;
    }
}
