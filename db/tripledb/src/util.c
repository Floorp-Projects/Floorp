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

#include "util.h"


#ifdef TDB_USE_NSPR
/* Don't attempt to use NSPR's "ll" routines; they don't do what we want. */
#include "prnetdb.h"
#define tdb_ntohs PR_ntohs
#define tdb_ntohl PR_ntohl
#define tdb_htons PR_htons
#define tdb_htonl PR_htonl
#else
#include <sys/types.h>
#include <netinet/in.h>
#define tdb_ntohs ntohs
#define tdb_ntohl ntohl
#define tdb_htons htons
#define tdb_htonl htonl
#endif


TDBPtr
tdbGetPtr(char** ptr, char* endptr)
{
    TDBPtr result;
    if (*ptr + sizeof(result) > endptr) return -1;
    memcpy(&result, *ptr, sizeof(result));
    *ptr += sizeof(result);
    result = tdb_ntohl(result);
    return result;
}

TDBInt32
tdbGetInt32(char** ptr, char* endptr)
{
    TDBInt32 result;
    if (*ptr + sizeof(TDBInt32) > endptr) return 0;
    memcpy(&result, *ptr, sizeof(TDBInt32));
    *ptr += sizeof(TDBInt32);
    result = tdb_ntohl(result);
    return result;
}

TDBInt32
tdbGetInt16(char** ptr, char* endptr)
{
    TDBInt16 result;
    if (*ptr + sizeof(TDBInt16) > endptr) return 0;
    memcpy(&result, *ptr, sizeof(TDBInt16));
    *ptr += sizeof(TDBInt16);
    result = tdb_ntohs(result);
    return result;
}

TDBInt8
tdbGetInt8(char** ptr, char* endptr)
{
    TDBInt8 result;
    if (*ptr + sizeof(TDBInt8) > endptr) return 0;
    memcpy(&result, *ptr, sizeof(TDBInt8));
    *ptr += sizeof(TDBInt8);
    return result;
}

TDBUint8
tdbGetUInt8(char** ptr, char* endptr)
{
    TDBUint8 result;
    if (*ptr + sizeof(TDBUint8) > endptr) return 0;
    memcpy(&result, *ptr, sizeof(TDBUint8));
    *ptr += sizeof(TDBUint8);
    return result;
}

TDBInt64
tdbGetInt64(char** ptr, char* endptr)
{
    return (TDBInt64) tdbGetUInt64(ptr, endptr);
}

TDBUint16
tdbGetUInt16(char** ptr, char* endptr)
{
    TDBUint16 result;
    if (*ptr + sizeof(TDBUint16) > endptr) return 0;
    memcpy(&result, *ptr, sizeof(TDBUint16));
    *ptr += sizeof(TDBUint16);
    result = tdb_ntohs(result);
    return result;
}


TDBUint64
tdbGetUInt64(char** ptr, char* endptr)
{
    TDBUint32 hi, low;
    TDBUint64 result;
    if (*ptr + sizeof(TDBUint64) > endptr) return 0;
    hi = tdbGetUInt32(ptr, endptr);
    low = tdbGetUInt32(ptr, endptr);
    result = ((TDBUint64) hi) << 32 | low;
    return result;
}


TDBUint32
tdbGetUInt32(char** ptr, char* endptr)
{
    TDBUint32 result;
    if (*ptr + sizeof(TDBUint32) > endptr) return 0;
    memcpy(&result, *ptr, sizeof(TDBUint32));
    *ptr += sizeof(TDBUint32);
    result = tdb_ntohl(result);
    return result;
}


TDBStatus
tdbPutPtr(char** ptr, TDBPtr value, char* endptr)
{
    if (*ptr + sizeof(value) > endptr) return TDB_FAILURE;
    value = tdb_htonl(value);
    memcpy(*ptr, &value, sizeof(TDBPtr));
    *ptr += sizeof(TDBPtr);
    return TDB_SUCCESS;
}

TDBStatus
tdbPutInt32(char** ptr, TDBInt32 value, char* endptr)
{
    if (*ptr + sizeof(value) > endptr) return TDB_FAILURE;
    value = tdb_htonl(value);
    memcpy(*ptr, &value, sizeof(TDBInt32));
    *ptr += sizeof(TDBInt32);
    return TDB_SUCCESS;
}

TDBStatus
tdbPutInt16(char** ptr, TDBInt16 value, char* endptr)
{
    if (*ptr + sizeof(value) > endptr) return TDB_FAILURE;
    value = tdb_htons(value);
    memcpy(*ptr, &value, sizeof(TDBInt16));
    *ptr += sizeof(TDBInt16);
    return TDB_SUCCESS;
}

TDBStatus
tdbPutUInt16(char** ptr, TDBUint16 value, char* endptr)
{
    if (*ptr + sizeof(value) > endptr) return TDB_FAILURE;
    value = tdb_htons(value);
    memcpy(*ptr, &value, sizeof(TDBUint16));
    *ptr += sizeof(TDBUint16);
    return TDB_SUCCESS;
}

TDBStatus
tdbPutInt8(char** ptr, TDBInt8 value, char* endptr)
{
    if (*ptr + sizeof(value) > endptr) return TDB_FAILURE;
    memcpy(*ptr, &value, sizeof(TDBInt8));
    *ptr += sizeof(TDBInt8);
    return TDB_SUCCESS;
}

TDBStatus
tdbPutUInt8(char** ptr, TDBUint8 value, char* endptr)
{
    if (*ptr + sizeof(value) > endptr) return TDB_FAILURE;
    memcpy(*ptr, &value, sizeof(TDBUint8));
    *ptr += sizeof(TDBUint8);
    return TDB_SUCCESS;
}

TDBStatus
tdbPutInt64(char** ptr, TDBInt64 value, char* endptr)
{
    return tdbPutUInt64(ptr, (TDBUint64) value, endptr);
}

TDBStatus
tdbPutUInt64(char** ptr, TDBUint64 value, char* endptr)
{
    TDBUint32 hi, low;
    if (*ptr + sizeof(value) > endptr) return TDB_FAILURE;
    hi = (TDBUint32) (value >> 32);
    low = (TDBUint32) value;
    if (tdbPutUInt32(ptr, hi, endptr) != TDB_SUCCESS) return TDB_FAILURE;
    return tdbPutUInt32(ptr, low, endptr);
}

TDBStatus
tdbPutUInt32(char** ptr, TDBUint32 value, char* endptr)
{
    if (*ptr + sizeof(value) > endptr) return TDB_FAILURE;
    value = tdb_htonl(value);
    memcpy(*ptr, &value, sizeof(TDBInt32));
    *ptr += sizeof(TDBUint32);
    return TDB_SUCCESS;
}


TDBStatus
tdbPutString(char** ptr, const char* str, char* endptr)
{
    while (*str) {
        if (*ptr >= endptr) return TDB_FAILURE;
        *(*ptr)++ = *str++;
    }
    if (*ptr >= endptr) return TDB_FAILURE;
    *(*ptr)++ = '\0';
    return TDB_SUCCESS;
}
