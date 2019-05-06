/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineInspector_h
#define jit_BaselineInspector_h

#include "jit/BaselineIC.h"
#include "jit/BaselineJIT.h"
#include "jit/MIR.h"

namespace js {
namespace jit {

class BaselineInspector;

class ICInspector {
 protected:
  BaselineInspector* inspector_;
  jsbytecode* pc_;
  ICEntry* icEntry_;

  ICInspector(BaselineInspector* inspector, jsbytecode* pc, ICEntry* icEntry)
      : inspector_(inspector), pc_(pc), icEntry_(icEntry) {}
};

class SetElemICInspector : public ICInspector {
 public:
  SetElemICInspector(BaselineInspector* inspector, jsbytecode* pc,
                     ICEntry* icEntry)
      : ICInspector(inspector, pc, icEntry) {}

  bool sawOOBDenseWrite() const;
  bool sawOOBTypedArrayWrite() const;
};

class BaselineInspector {
 private:
  JSScript* script;
  ICEntry* prevLookedUpEntry;

 public:
  explicit BaselineInspector(JSScript* script)
      : script(script), prevLookedUpEntry(nullptr) {
    MOZ_ASSERT(script);
  }

  bool hasICScript() const { return script->hasICScript(); }

  ICScript* icScript() const;

 private:
#ifdef DEBUG
  bool isValidPC(jsbytecode* pc) { return script->containsPC(pc); }
#endif

  ICEntry& icEntryFromPC(jsbytecode* pc);
  ICEntry* maybeICEntryFromPC(jsbytecode* pc);

  template <typename ICInspectorType>
  ICInspectorType makeICInspector(jsbytecode* pc,
                                  ICStub::Kind expectedFallbackKind) {
    ICEntry* ent = nullptr;
    if (hasICScript()) {
      ent = &icEntryFromPC(pc);
      MOZ_ASSERT(ent->fallbackStub()->kind() == expectedFallbackKind);
    }
    return ICInspectorType(this, pc, ent);
  }

  ICStub* monomorphicStub(jsbytecode* pc);
  MOZ_MUST_USE bool dimorphicStub(jsbytecode* pc, ICStub** pfirst,
                                  ICStub** psecond);

 public:
  typedef Vector<ReceiverGuard, 4, JitAllocPolicy> ReceiverVector;
  typedef Vector<ObjectGroup*, 4, JitAllocPolicy> ObjectGroupVector;
  MOZ_MUST_USE bool maybeInfoForPropertyOp(jsbytecode* pc,
                                           ReceiverVector& receivers);

  MOZ_MUST_USE bool maybeInfoForProtoReadSlot(jsbytecode* pc,
                                              ReceiverVector& receivers,
                                              JSObject** holder);

  SetElemICInspector setElemICInspector(jsbytecode* pc) {
    return makeICInspector<SetElemICInspector>(pc, ICStub::SetElem_Fallback);
  }

  MIRType expectedResultType(jsbytecode* pc);
  MCompare::CompareType expectedCompareType(jsbytecode* pc);
  MIRType expectedBinaryArithSpecialization(jsbytecode* pc);
  MIRType expectedPropertyAccessInputType(jsbytecode* pc);

  bool hasSeenNegativeIndexGetElement(jsbytecode* pc);
  bool hasSeenNonIntegerIndex(jsbytecode* pc);
  bool hasSeenAccessedGetter(jsbytecode* pc);
  bool hasSeenDoubleResult(jsbytecode* pc);

  JSObject* getTemplateObject(jsbytecode* pc);
  JSObject* getTemplateObjectForNative(jsbytecode* pc, Native native);
  JSObject* getTemplateObjectForClassHook(jsbytecode* pc, const Class* clasp);

  // Sometimes the group a template object will have is known, even if the
  // object itself isn't.
  ObjectGroup* getTemplateObjectGroup(jsbytecode* pc);

  JSFunction* getSingleCallee(jsbytecode* pc);

  LexicalEnvironmentObject* templateNamedLambdaObject();
  CallObject* templateCallObject();

  // If |innerized| is true, we're doing a GETPROP on a WindowProxy and
  // IonBuilder unwrapped/innerized it to do the lookup on the Window (the
  // global object) instead. In this case we should only look for Baseline
  // stubs that performed the same optimization.
  MOZ_MUST_USE bool commonGetPropFunction(
      jsbytecode* pc, jsid id, bool innerized, JSObject** holder,
      Shape** holderShape, JSFunction** commonGetter, Shape** globalShape,
      bool* isOwnProperty, ReceiverVector& receivers);

  MOZ_MUST_USE bool megamorphicGetterSetterFunction(
      jsbytecode* pc, jsid id, bool isGetter, JSFunction** getterOrSetter);

  MOZ_MUST_USE bool commonSetPropFunction(jsbytecode* pc, JSObject** holder,
                                          Shape** holderShape,
                                          JSFunction** commonSetter,
                                          bool* isOwnProperty,
                                          ReceiverVector& receivers);

  MOZ_MUST_USE bool instanceOfData(jsbytecode* pc, Shape** shape,
                                   uint32_t* slot, JSObject** prototypeObject);
};

}  // namespace jit
}  // namespace js

#endif /* jit_BaselineInspector_h */
