/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLLibraryLoader.h"

#include "nsDebug.h"

namespace mozilla {
namespace gl {

bool
GLLibraryLoader::OpenLibrary(const char *library)
{
    PRLibSpec lspec;
    lspec.type = PR_LibSpec_Pathname;
    lspec.value.pathname = library;

    mLibrary = PR_LoadLibraryWithFlags(lspec, PR_LD_LAZY | PR_LD_LOCAL);
    if (!mLibrary)
        return false;

    return true;
}

bool
GLLibraryLoader::LoadSymbols(SymLoadStruct *firstStruct,
                             bool tryplatform,
                             const char *prefix,
                             bool warnOnFailure)
{
    return LoadSymbols(mLibrary,
                       firstStruct,
                       tryplatform ? mLookupFunc : nullptr,
                       prefix,
                       warnOnFailure);
}

PRFuncPtr
GLLibraryLoader::LookupSymbol(PRLibrary *lib,
                              const char *sym,
                              PlatformLookupFunction lookupFunction)
{
    PRFuncPtr res = 0;

    // try finding it in the library directly, if we have one
    if (lib) {
        res = PR_FindFunctionSymbol(lib, sym);
    }

    // then try looking it up via the lookup symbol
    if (!res && lookupFunction) {
        res = lookupFunction(sym);
    }

    // finally just try finding it in the process
    if (!res) {
        PRLibrary *leakedLibRef;
        res = PR_FindFunctionSymbolAndLibrary(sym, &leakedLibRef);
    }

    return res;
}

bool
GLLibraryLoader::LoadSymbols(PRLibrary *lib,
                             SymLoadStruct *firstStruct,
                             PlatformLookupFunction lookupFunction,
                             const char *prefix,
                             bool warnOnFailure)
{
    char sbuf[MAX_SYMBOL_LENGTH * 2];
    int failCount = 0;

    SymLoadStruct *ss = firstStruct;
    while (ss->symPointer) {
        *ss->symPointer = 0;

        for (int i = 0; i < MAX_SYMBOL_NAMES; i++) {
            if (ss->symNames[i] == nullptr)
                break;

            const char *s = ss->symNames[i];
            if (prefix && *prefix != 0) {
                strcpy(sbuf, prefix);
                strcat(sbuf, ss->symNames[i]);
                s = sbuf;
            }

            PRFuncPtr p = LookupSymbol(lib, s, lookupFunction);
            if (p) {
                *ss->symPointer = p;
                break;
            }
        }

        if (*ss->symPointer == 0) {
            if (warnOnFailure)
                printf_stderr("Can't find symbol '%s'.\n", ss->symNames[0]);

            failCount++;
        }

        ss++;
    }

    return failCount == 0 ? true : false;
}

} /* namespace gl */
} /* namespace mozilla */

