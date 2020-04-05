/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLLibraryLoader.h"

#include <regex>

#include "mozilla/gfx/Logging.h"
#include "nsDebug.h"

#ifdef WIN32
#  include <windows.h>
#endif

namespace mozilla {
namespace gl {

void ClearSymbols(const SymLoadStruct* const firstStruct) {
  for (auto itr = firstStruct; itr->symPointer; ++itr) {
    *itr->symPointer = nullptr;
  }
}

bool SymbolLoader::LoadSymbols(const SymLoadStruct* const firstStruct,
                               const bool warnOnFailures) const {
  bool ok = true;

  for (auto itr = firstStruct; itr->symPointer; ++itr) {
    *itr->symPointer = nullptr;

    for (const auto& s : itr->symNames) {
      if (!s) break;

      const auto p = GetProcAddress(s);
      if (p) {
        *itr->symPointer = p;
        break;
      }
    }

    if (!*itr->symPointer) {
      if (warnOnFailures) {
        printf_stderr("Can't find symbol '%s'.\n", itr->symNames[0]);
      }
      ok = false;
    }
  }

  return ok;
}

// -

PRFuncPtr SymbolLoader::GetProcAddress(const char* const name) const {
#ifdef DEBUG
  static const std::regex kRESymbol("[a-z].*");
  if (!std::regex_match(name, kRESymbol)) {
    gfxCriticalError() << "Bad symbol name : " << name;
  }
#endif

  PRFuncPtr ret = nullptr;
  if (!ret && mLib) {
    ret = PR_FindFunctionSymbol(mLib, name);
  }
  if (!ret && mPfn) {
    ret = mPfn(name);
  }
  return ret;
}

}  // namespace gl
}  // namespace mozilla
