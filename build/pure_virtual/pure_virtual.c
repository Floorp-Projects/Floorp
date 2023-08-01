/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mozilla/Assertions.h>

// This function is used in vtables to point at pure virtual methods.
// The implementation in the standard library usually aborts, but
// the function is normally never called (a call would be a bug).
// Each of these entries in vtables, however, require an unnecessary
// dynamic relocation. Defining our own function makes the linker
// point the vtables here instead of the standard library, replacing
// the dynamic relocations with relative relocations.
//
// On Windows, it doesn't really make a difference, but on macOS it
// can be packed better, saving about 10KB in libxul, and on 64-bits
// ELF systems, with packed relative relocations, it saves 140KB.
//
// Another advantage of having our own is that we can use MOZ_CRASH
// instead of the system's abort.
#ifdef _MSC_VER
int __cdecl _purecall() { MOZ_CRASH("pure virtual call"); }
#else
__attribute__((visibility("hidden"))) void __cxa_pure_virtual() {
  MOZ_CRASH("pure virtual call");
}
#endif
