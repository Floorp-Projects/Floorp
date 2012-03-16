/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLLIBRARYLOADER_H_
#define GLLIBRARYLOADER_H_

#include <stdio.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "GLDefs.h"
#include "mozilla/Util.h"
#include "nscore.h"
#include "prlink.h"

namespace mozilla {
namespace gl {

class GLLibraryLoader
{
public:
    bool OpenLibrary(const char *library);

    typedef PRFuncPtr (GLAPIENTRY * PlatformLookupFunction) (const char *);

    enum {
        MAX_SYMBOL_NAMES = 5,
        MAX_SYMBOL_LENGTH = 128
    };

    typedef struct {
        PRFuncPtr *symPointer;
        const char *symNames[MAX_SYMBOL_NAMES];
    } SymLoadStruct;

    bool LoadSymbols(SymLoadStruct *firstStruct,
                       bool tryplatform = false,
                       const char *prefix = nsnull);

    /*
     * Static version of the functions in this class
     */
    static PRFuncPtr LookupSymbol(PRLibrary *lib,
                                  const char *symname,
                                  PlatformLookupFunction lookupFunction = nsnull);
    static bool LoadSymbols(PRLibrary *lib,
                              SymLoadStruct *firstStruct,
                              PlatformLookupFunction lookupFunction = nsnull,
                              const char *prefix = nsnull);
protected:
    GLLibraryLoader() {
        mLibrary = nsnull;
        mLookupFunc = nsnull;
    }

    PRLibrary *mLibrary;
    PlatformLookupFunction mLookupFunc;
};

} /* namespace gl */
} /* namespace mozilla */

#endif /* GLLIBRARYLOADER_H_ */
