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
  std::string getImplicitReason(const TagDecl *D,
                                VisitFlags &ToVisit) const override {
    // Annotate everything in ::std, with a few exceptions; see bug
    // 1201314 for discussion.
    if (getDeclarationNamespace(D) != "std") {
      return "";
    }

    StringRef Name = getNameChecked(D);

    // If the type has a trivial move constructor and destructor, it is safe to
    // memmove, and we don't need to visit any fields.
    auto RD = dyn_cast<CXXRecordDecl>(D);
    if (RD && RD->isCompleteDefinition() &&
        (RD->hasTrivialMoveConstructor() ||
         (!RD->hasMoveConstructor() && RD->hasTrivialCopyConstructor())) &&
        RD->hasTrivialDestructor()) {
      ToVisit = VISIT_NONE;
      return "";
    }

    // This doesn't check that it's really ::std::pair and not
    // ::std::something_else::pair, but should be good enough.
    if (isNameExcepted(Name.data())) {
      // If we're an excepted name, stop traversing within the type further,
      // and only check template arguments for foreign types.
      ToVisit = VISIT_TMPL_ARGS;
      return "";
    }
    return "it is an stl-provided type not guaranteed to be memmove-able";
  }

private:
  bool isNameExcepted(StringRef Name) const {
    return Name == "pair" || Name == "atomic" || Name == "tuple";
  }
};

extern MemMoveAnnotation NonMemMovable;

#endif
