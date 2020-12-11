/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineICList_h
#define jit_BaselineICList_h

namespace js {
namespace jit {

// List of Baseline IC fallback stub kinds. The stub kind determines which
// class (deriving from ICFallbackStub) this is.
#define IC_BASELINE_STUB_KIND_LIST(_) \
  _(NewArray_Fallback)                \
  _(NewObject_Fallback)               \
                                      \
  _(ToBool_Fallback)                  \
                                      \
  _(UnaryArith_Fallback)              \
                                      \
  _(Call_Fallback)                    \
                                      \
  _(GetElem_Fallback)                 \
  _(SetElem_Fallback)                 \
                                      \
  _(In_Fallback)                      \
  _(HasOwn_Fallback)                  \
  _(CheckPrivateField_Fallback)       \
                                      \
  _(GetName_Fallback)                 \
                                      \
  _(BindName_Fallback)                \
                                      \
  _(GetIntrinsic_Fallback)            \
                                      \
  _(SetProp_Fallback)                 \
                                      \
  _(GetIterator_Fallback)             \
                                      \
  _(OptimizeSpreadCall_Fallback)      \
                                      \
  _(InstanceOf_Fallback)              \
                                      \
  _(TypeOf_Fallback)                  \
                                      \
  _(ToPropertyKey_Fallback)           \
                                      \
  _(Rest_Fallback)                    \
                                      \
  _(BinaryArith_Fallback)             \
                                      \
  _(Compare_Fallback)                 \
                                      \
  _(GetProp_Fallback)

// List of fallback trampolines. Each of these fallback trampolines exists as
// part of the JitRuntime. Note that some fallback stubs in previous list may
// have multiple trampolines in this list. For example, Call_Fallback has
// constructing/spread variants here with different calling conventions needing
// different trampolines.
#define IC_BASELINE_FALLBACK_CODE_KIND_LIST(_) \
  _(NewArray)                                  \
  _(NewObject)                                 \
  _(ToBool)                                    \
  _(UnaryArith)                                \
  _(Call)                                      \
  _(CallConstructing)                          \
  _(SpreadCall)                                \
  _(SpreadCallConstructing)                    \
  _(GetElem)                                   \
  _(GetElemSuper)                              \
  _(SetElem)                                   \
  _(In)                                        \
  _(HasOwn)                                    \
  _(CheckPrivateField)                         \
  _(GetName)                                   \
  _(BindName)                                  \
  _(GetIntrinsic)                              \
  _(SetProp)                                   \
  _(GetIterator)                               \
  _(OptimizeSpreadCall)                        \
  _(InstanceOf)                                \
  _(TypeOf)                                    \
  _(ToPropertyKey)                             \
  _(Rest)                                      \
  _(BinaryArith)                               \
  _(Compare)                                   \
  _(GetProp)                                   \
  _(GetPropSuper)

}  // namespace jit
}  // namespace js

#endif /* jit_BaselineICList_h */
