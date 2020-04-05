/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLLIBRARYLOADER_H_
#define GLLIBRARYLOADER_H_

#include <array>
#include <stdio.h>

#include "GLDefs.h"
#include "nscore.h"
#include "mozilla/Assertions.h"
#include "mozilla/SharedLibrary.h"

namespace mozilla {
namespace gl {

struct SymLoadStruct final {
  PRFuncPtr* symPointer;
  std::array<const char*, 6> symNames;
};

void ClearSymbols(const SymLoadStruct* firstStruct);

class SymbolLoader final {
 public:
  typedef PRFuncPtr(GLAPIENTRY* GetProcAddressT)(const char*);

  GetProcAddressT mPfn = nullptr;  // Try this first, if not null.
  PRLibrary* mLib = nullptr;

  explicit SymbolLoader(void*(GLAPIENTRY* pfn)(const char*))
      : mPfn(GetProcAddressT(pfn)) {
    MOZ_ASSERT(mPfn);
  }

  explicit SymbolLoader(const GetProcAddressT pfn) : mPfn(pfn) {
    MOZ_ASSERT(mPfn);
  }

  explicit SymbolLoader(PRLibrary& lib) : mLib(&lib) { MOZ_ASSERT(mLib); }

  PRFuncPtr GetProcAddress(const char*) const;
  bool LoadSymbols(const SymLoadStruct* firstStruct,
                   bool warnOnFailures = true) const;
};

}  // namespace gl
}  // namespace mozilla

#endif  // GLLIBRARYLOADER_H_
