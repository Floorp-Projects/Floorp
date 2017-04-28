/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MemMoveAnnotation_h__
#define MemMoveAnnotation_h__

#include "CustomTypeAnnotation.h"
#include "CustomMatchers.h"
#include "Utils.h"

class MemMoveAnnotation final : public CustomTypeAnnotation {
public:
  MemMoveAnnotation()
      : CustomTypeAnnotation("moz_non_memmovable", "non-memmove()able") {}

  virtual ~MemMoveAnnotation() {}

protected:
  std::string getImplicitReason(const TagDecl *D) const override {
    // Annotate everything in ::std, with a few exceptions; see bug
    // 1201314 for discussion.
    if (getDeclarationNamespace(D) == "std") {
      // This doesn't check that it's really ::std::pair and not
      // ::std::something_else::pair, but should be good enough.
      StringRef Name = getNameChecked(D);
      if (Name == "pair" ||
          Name == "atomic" ||
          // libstdc++ specific names
          Name == "__atomic_base" ||
          Name == "atomic_bool" ||
          // MSVCRT specific names
          Name == "_Atomic_impl" ||
          Name == "_Atomic_base" ||
          Name == "_Atomic_bool" ||
          Name == "_Atomic_char" ||
          Name == "_Atomic_schar" ||
          Name == "_Atomic_uchar" ||
          Name == "_Atomic_char16_t" ||
          Name == "_Atomic_char32_t" ||
          Name == "_Atomic_wchar_t" ||
          Name == "_Atomic_short" ||
          Name == "_Atomic_ushort" ||
          Name == "_Atomic_int" ||
          Name == "_Atomic_uint" ||
          Name == "_Atomic_long" ||
          Name == "_Atomic_ulong" ||
          Name == "_Atomic_llong" ||
          Name == "_Atomic_ullong" ||
          Name == "_Atomic_address") {
        return "";
      }
      return "it is an stl-provided type not guaranteed to be memmove-able";
    }
    return "";
  }
};

extern MemMoveAnnotation NonMemMovable;

#endif
