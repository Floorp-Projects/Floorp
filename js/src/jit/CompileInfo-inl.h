/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CompileInfo_inl_h
#define jit_CompileInfo_inl_h

#include "jit/CompileInfo.h"

#include "vm/JSScript-inl.h"

namespace js {
namespace jit {

inline RegExpObject* CompileInfo::getRegExp(jsbytecode* pc) const {
  return script_->getRegExp(pc);
}

inline JSFunction* CompileInfo::getFunction(jsbytecode* pc) const {
  return script_->getFunction(pc);
}

static inline const char* AnalysisModeString(AnalysisMode mode) {
  switch (mode) {
    case Analysis_None:
      return "Analysis_None";
    case Analysis_DefiniteProperties:
      return "Analysis_DefiniteProperties";
    case Analysis_ArgumentsUsage:
      return "Analysis_ArgumentsUsage";
    default:
      MOZ_CRASH("Invalid AnalysisMode");
  }
}

}  // namespace jit
}  // namespace js

#endif /* jit_CompileInfo_inl_h */
