/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CustomAttributes_h__
#define CustomAttributes_h__

#include "clang/AST/DeclBase.h"
#include "llvm/ADT/StringRef.h"

enum CustomAttributes {
#define ATTR(a) a,
#include "CustomAttributes.inc"
#undef ATTR
};

struct CustomAttributesSet {
#define ATTR(a) bool has_ ## a: 1;
#include "CustomAttributes.inc"
#undef ATTR
};

template<CustomAttributes A>
bool hasCustomAttribute(const clang::Decl* D) {
  return false;
}

extern CustomAttributesSet GetAttributes(const clang::Decl* D);

#define ATTR(name) \
  template<> \
  inline bool hasCustomAttribute<name>(const clang::Decl* D) { \
    return GetAttributes(D).has_ ## name; \
  }
#include "CustomAttributes.inc"
#undef ATTR

extern bool hasCustomAttribute(const clang::Decl* D, CustomAttributes A);

#endif /* CustomAttributes_h__ */
