/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MemMoveAnnotation_h__
#define MemMoveAnnotation_h__

#include "CustomMatchers.h"
#include "CustomTypeAnnotation.h"
#include "Utils.h"

#include <unordered_set>

class MemMoveAnnotation final : public CustomTypeAnnotation {
public:
  MemMoveAnnotation()
      : CustomTypeAnnotation(moz_non_memmovable, "non-memmove()able") {}

  virtual ~MemMoveAnnotation() {}

protected:
  std::string getImplicitReason(const TagDecl *D) const override {
    // Annotate everything in ::std, with a few exceptions; see bug
    // 1201314 for discussion.
    if (getDeclarationNamespace(D) == "std") {
      // This doesn't check that it's really ::std::pair and not
      // ::std::something_else::pair, but should be good enough.
      StringRef Name = getNameChecked(D);
      if (isNameExcepted(Name.data())) {
        return "";
      }
      return "it is an stl-provided type not guaranteed to be memmove-able";
    }
    return "";
  }

private:
  bool isNameExcepted(const char *Name) const {
    static std::unordered_set<std::string> NamesSet = {
        {"pair"},
        {"atomic"},
        // libstdc++ specific names
        {"__atomic_base"},
        {"atomic_bool"},
        {"__cxx_atomic_impl"},
        {"__cxx_atomic_base_impl"},
        {"__pair_base"},
        // MSVCRT specific names
        {"_Atomic_impl"},
        {"_Atomic_base"},
        {"_Atomic_bool"},
        {"_Atomic_char"},
        {"_Atomic_schar"},
        {"_Atomic_uchar"},
        {"_Atomic_char16_t"},
        {"_Atomic_char32_t"},
        {"_Atomic_wchar_t"},
        {"_Atomic_short"},
        {"_Atomic_ushort"},
        {"_Atomic_int"},
        {"_Atomic_uint"},
        {"_Atomic_long"},
        {"_Atomic_ulong"},
        {"_Atomic_llong"},
        {"_Atomic_ullong"},
        {"_Atomic_address"},
        // MSVCRT 2019
        {"_Atomic_integral"},
        {"_Atomic_integral_facade"},
        {"_Atomic_padded"},
        {"_Atomic_pointer"},
        {"_Atomic_storage"}};

    return NamesSet.find(Name) != NamesSet.end();
  }
};

extern MemMoveAnnotation NonMemMovable;

#endif
