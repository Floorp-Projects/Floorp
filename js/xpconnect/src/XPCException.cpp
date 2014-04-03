/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* An implementaion of nsIException. */

#include "xpcprivate.h"
#include "nsError.h"

/***************************************************************************/
/* Quick and dirty mapping of well known result codes to strings. We only
*  call this when building an exception object, so iterating the short array
*  is not too bad.
*
*  It sure would be nice to have exceptions declared in idl and available
*  in some more global way at runtime.
*/

static const struct ResultMap
{nsresult rv; const char* name; const char* format;} map[] = {
#define XPC_MSG_DEF(val, format) \
    {(val), #val, format},
#include "xpc.msg"
#undef XPC_MSG_DEF
    {NS_OK,0,0}   // sentinel to mark end of array
};

#define RESULT_COUNT ((sizeof(map) / sizeof(map[0]))-1)

// static
bool
nsXPCException::NameAndFormatForNSResult(nsresult rv,
                                         const char** name,
                                         const char** format)
{

    for (const ResultMap* p = map; p->name; p++) {
        if (rv == p->rv) {
            if (name) *name = p->name;
            if (format) *format = p->format;
            return true;
        }
    }
    return false;
}

// static
const void*
nsXPCException::IterateNSResults(nsresult* rv,
                                 const char** name,
                                 const char** format,
                                 const void** iterp)
{
    const ResultMap* p = (const ResultMap*) *iterp;
    if (!p)
        p = map;
    else
        p++;
    if (!p->name)
        p = nullptr;
    else {
        if (rv)
            *rv = p->rv;
        if (name)
            *name = p->name;
        if (format)
            *format = p->format;
    }
    *iterp = p;
    return p;
}

// static
uint32_t
nsXPCException::GetNSResultCount()
{
    return RESULT_COUNT;
}
