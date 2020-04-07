/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonIC.h"

#include "jit/CacheIRCompiler.h"
#include "jit/Linker.h"
#include "util/DiagnosticAssertions.h"

#include "jit/MacroAssembler-inl.h"
#include "vm/Interpreter-inl.h"

using namespace js;
using namespace js::jit;

void IonIC::resetCodeRaw(IonScript* ionScript) {
  codeRaw_ = fallbackAddr(ionScript);
}

uint8_t* IonIC::fallbackAddr(IonScript* ionScript) const {
  return ionScript->method()->raw() + fallbackOffset_;
}

uint8_t* IonIC::rejoinAddr(IonScript* ionScript) const {
  return ionScript->method()->raw() + rejoinOffset_;
}

Register IonIC::scratchRegisterForEntryJump() {
  switch (kind_) {
    case CacheKind::GetProp:
    case CacheKind::GetElem: {
      Register temp = asGetPropertyIC()->maybeTemp();
      if (temp != InvalidReg) {
        return temp;
      }
      TypedOrValueRegister output = asGetPropertyIC()->output();
      return output.hasValue() ? output.valueReg().scratchReg()
                               : output.typedReg().gpr();
    }
    case CacheKind::GetPropSuper:
    case CacheKind::GetElemSuper: {
      TypedOrValueRegister output = asGetPropSuperIC()->output();
      return output.valueReg().scratchReg();
    }
    case CacheKind::SetProp:
    case CacheKind::SetElem:
      return asSetPropertyIC()->temp();
    case CacheKind::GetName:
      return asGetNameIC()->temp();
    case CacheKind::BindName:
      return asBindNameIC()->temp();
    case CacheKind::In:
      return asInIC()->temp();
    case CacheKind::HasOwn:
      return asHasOwnIC()->output();
    case CacheKind::GetIterator:
      return asGetIteratorIC()->temp1();
    case CacheKind::InstanceOf:
      return asInstanceOfIC()->output();
    case CacheKind::UnaryArith:
      return asUnaryArithIC()->output().scratchReg();
    case CacheKind::BinaryArith:
      return asBinaryArithIC()->output().scratchReg();
    case CacheKind::Compare:
      return asCompareIC()->output();
    case CacheKind::Call:
    case CacheKind::TypeOf:
    case CacheKind::ToBool:
    case CacheKind::GetIntrinsic:
    case CacheKind::NewObject:
      MOZ_CRASH("Unsupported IC");
  }

  MOZ_CRASH("Invalid kind");
}

void IonIC::discardStubs(Zone* zone, IonScript* ionScript) {
  if (firstStub_ && zone->needsIncrementalBarrier()) {
    // We are removing edges from IonIC to gcthings. Perform one final trace
    // of the stub for incremental GC, as it must know about those edges.
    trace(zone->barrierTracer(), ionScript);
  }

#ifdef JS_CRASH_DIAGNOSTICS
  IonICStub* stub = firstStub_;
  while (stub) {
    IonICStub* next = stub->next();
    stub->poison();
    stub = next;
  }
#endif

  firstStub_ = nullptr;
  resetCodeRaw(ionScript);
  state_.trackUnlinkedAllStubs();
}

void IonIC::reset(Zone* zone, IonScript* ionScript) {
  discardStubs(zone, ionScript);
  state_.reset();
}

void IonIC::trace(JSTracer* trc, IonScript* ionScript) {
  if (script_) {
    TraceManuallyBarrieredEdge(trc, &script_, "IonIC::script_");
  }

  uint8_t* nextCodeRaw = codeRaw_;
  for (IonICStub* stub = firstStub_; stub; stub = stub->next()) {
    JitCode* code = JitCode::FromExecutable(nextCodeRaw);
    TraceManuallyBarrieredEdge(trc, &code, "ion-ic-code");

    TraceCacheIRStub(trc, stub, stub->stubInfo());

    nextCodeRaw = stub->nextCodeRaw();
  }

  MOZ_ASSERT(nextCodeRaw == fallbackAddr(ionScript));
}

// This helper handles ICState updates/transitions while attaching CacheIR
// stubs.
template <typename IRGenerator, typename IC, typename... Args>
static void TryAttachIonStub(JSContext* cx, IC* ic, IonScript* ionScript,
                             Args&&... args) {
  if (ic->state().maybeTransition()) {
    ic->discardStubs(cx->zone(), ionScript);
  }

  if (ic->state().canAttachStub()) {
    RootedScript script(cx, ic->script());
    bool attached = false;
    IRGenerator gen(cx, script, ic->pc(), ic->state().mode(),
                    std::forward<Args>(args)...);
    switch (gen.tryAttachStub()) {
      case AttachDecision::Attach:
        ic->attachCacheIRStub(cx, gen.writerRef(), gen.cacheKind(), ionScript,
                              &attached);
        break;
      case AttachDecision::NoAction:
        break;
      case AttachDecision::TemporarilyUnoptimizable:
        attached = true;
        break;
      case AttachDecision::Deferred:
        MOZ_ASSERT_UNREACHABLE("Not expected in generic TryAttachIonStub");
        break;
    }
    if (!attached) {
      ic->state().trackNotAttached();
    }
  }
}

/* static */
bool IonGetPropertyIC::update(JSContext* cx, HandleScript outerScript,
                              IonGetPropertyIC* ic, HandleValue val,
                              HandleValue idVal, MutableHandleValue res) {
  // Override the return value if we are invalidated (bug 728188).
  IonScript* ionScript = outerScript->ionScript();
  AutoDetectInvalidation adi(cx, res, ionScript);

  // If the IC is idempotent, we will redo the op in the interpreter.
  if (ic->idempotent()) {
    adi.disable();
  }

  if (ic->state().maybeTransition()) {
    ic->discardStubs(cx->zone(), ionScript);
  }

  bool attached = false;
  if (ic->state().canAttachStub()) {
    jsbytecode* pc = ic->idempotent() ? nullptr : ic->pc();
    GetPropIRGenerator gen(cx, outerScript, pc, ic->state().mode(), ic->kind(),
                           val, idVal, val, ic->resultFlags());
    switch (ic->idempotent() ? gen.tryAttachIdempotentStub()
                             : gen.tryAttachStub()) {
      case AttachDecision::Attach:
        ic->attachCacheIRStub(cx, gen.writerRef(), gen.cacheKind(), ionScript,
                              &attached);
        break;
      case AttachDecision::NoAction:
        break;
      case AttachDecision::TemporarilyUnoptimizable:
        attached = true;
        break;
      case AttachDecision::Deferred:
        MOZ_ASSERT_UNREACHABLE("No deferred GetProp stubs");
        break;
    }
    if (!attached) {
      ic->state().trackNotAttached();
    }
  }

  if (!attached && ic->idempotent()) {
    // Invalidate the cache if the property was not found, or was found on
    // a non-native object. This ensures:
    // 1) The property read has no observable side-effects.
    // 2) There's no need to dynamically monitor the return type. This would
    //    be complicated since (due to GVN) there can be multiple pc's
    //    associated with a single idempotent cache.
    JitSpew(JitSpew_IonIC, "Invalidating from idempotent cache %s:%u:%u",
            outerScript->filename(), outerScript->lineno(),
            outerScript->column());

    outerScript->setInvalidatedIdempotentCache();

    // Do not re-invalidate if the lookup already caused invalidation.
    if (outerScript->hasIonScript()) {
      Invalidate(cx, outerScript);
    }

    // We will redo the potentially effectful lookup in Baseline.
    return true;
  }

  if (ic->kind() == CacheKind::GetProp) {
    RootedPropertyName name(cx, idVal.toString()->asAtom().asPropertyName());
    if (!GetProperty(cx, val, name, res)) {
      return false;
    }
  } else {
    MOZ_ASSERT(ic->kind() == CacheKind::GetElem);
    if (!GetElementOperation(cx, JSOp(*ic->pc()), val, idVal, res)) {
      return false;
    }
  }

  if (!ic->idempotent()) {
    // Monitor changes to cache entry.
    if (!ic->monitoredResult()) {
      JitScript::MonitorBytecodeType(cx, ic->script(), ic->pc(), res);
    }
  }

  return true;
}

/* static */
bool IonGetPropSuperIC::update(JSContext* cx, HandleScript outerScript,
                               IonGetPropSuperIC* ic, HandleObject obj,
                               HandleValue receiver, HandleValue idVal,
                               MutableHandleValue res) {
  // Override the return value if we are invalidated (bug 728188).
  IonScript* ionScript = outerScript->ionScript();
  AutoDetectInvalidation adi(cx, res, ionScript);

  if (ic->state().maybeTransition()) {
    ic->discardStubs(cx->zone(), ionScript);
  }

  RootedValue val(cx, ObjectValue(*obj));
  TryAttachIonStub<GetPropIRGenerator, IonGetPropSuperIC>(
      cx, ic, ionScript, ic->kind(), val, idVal, receiver,
      GetPropertyResultFlags::All);

  RootedId id(cx);
  if (!ValueToId<CanGC>(cx, idVal, &id)) {
    return false;
  }

  if (!GetProperty(cx, obj, receiver, id, res)) {
    return false;
  }

  // Monitor changes to cache entry.
  JitScript::MonitorBytecodeType(cx, ic->script(), ic->pc(), res);
  return true;
}

/* static */
bool IonSetPropertyIC::update(JSContext* cx, HandleScript outerScript,
                              IonSetPropertyIC* ic, HandleObject obj,
                              HandleValue idVal, HandleValue rhs) {
  using DeferType = SetPropIRGenerator::DeferType;

  RootedShape oldShape(cx);
  RootedObjectGroup oldGroup(cx);
  IonScript* ionScript = outerScript->ionScript();

  bool attached = false;
  DeferType deferType = DeferType::None;

  if (ic->state().maybeTransition()) {
    ic->discardStubs(cx->zone(), ionScript);
  }

  if (ic->state().canAttachStub()) {
    oldShape = obj->shape();
    oldGroup = JSObject::getGroup(cx, obj);
    if (!oldGroup) {
      return false;
    }

    RootedValue objv(cx, ObjectValue(*obj));
    RootedScript script(cx, ic->script());
    jsbytecode* pc = ic->pc();
    SetPropIRGenerator gen(cx, script, pc, ic->kind(), ic->state().mode(), objv,
                           idVal, rhs, ic->needsTypeBarrier(),
                           ic->guardHoles());
    switch (gen.tryAttachStub()) {
      case AttachDecision::Attach:
        ic->attachCacheIRStub(cx, gen.writerRef(), gen.cacheKind(), ionScript,
                              &attached, gen.typeCheckInfo());
        break;
      case AttachDecision::NoAction:
        break;
      case AttachDecision::TemporarilyUnoptimizable:
        attached = true;
        break;
      case AttachDecision::Deferred:
        deferType = gen.deferType();
        MOZ_ASSERT(deferType != DeferType::None);
        break;
    }
  }

  jsbytecode* pc = ic->pc();
  if (ic->kind() == CacheKind::SetElem) {
    if (JSOp(*pc) == JSOp::InitElemInc || JSOp(*pc) == JSOp::InitElemArray) {
      if (!InitArrayElemOperation(cx, pc, obj, idVal.toInt32(), rhs)) {
        return false;
      }
    } else if (IsPropertyInitOp(JSOp(*pc))) {
      if (!InitElemOperation(cx, pc, obj, idVal, rhs)) {
        return false;
      }
    } else {
      MOZ_ASSERT(IsPropertySetOp(JSOp(*pc)));
      if (!SetObjectElement(cx, obj, idVal, rhs, ic->strict())) {
        return false;
      }
    }
  } else {
    MOZ_ASSERT(ic->kind() == CacheKind::SetProp);

    if (JSOp(*pc) == JSOp::InitGLexical) {
      RootedScript script(cx, ic->script());
      MOZ_ASSERT(!script->hasNonSyntacticScope());
      InitGlobalLexicalOperation(cx, &cx->global()->lexicalEnvironment(),
                                 script, pc, rhs);
    } else if (IsPropertyInitOp(JSOp(*pc))) {
      // This might be a JSOp::InitElem op with a constant string id. We
      // can't call InitPropertyOperation here as that function is
      // specialized for JSOp::Init*Prop (it does not support arbitrary
      // objects that might show up here).
      if (!InitElemOperation(cx, pc, obj, idVal, rhs)) {
        return false;
      }
    } else {
      MOZ_ASSERT(IsPropertySetOp(JSOp(*pc)));
      RootedPropertyName name(cx, idVal.toString()->asAtom().asPropertyName());
      if (!SetProperty(cx, obj, name, rhs, ic->strict(), pc)) {
        return false;
      }
    }
  }

  if (attached) {
    return true;
  }

  // The SetProperty call might have entered this IC recursively, so try
  // to transition.
  if (ic->state().maybeTransition()) {
    ic->discardStubs(cx->zone(), ionScript);
  }

  bool canAttachStub = ic->state().canAttachStub();
  if (deferType != DeferType::None && canAttachStub) {
    RootedValue objv(cx, ObjectValue(*obj));
    RootedScript script(cx, ic->script());
    jsbytecode* pc = ic->pc();
    SetPropIRGenerator gen(cx, script, pc, ic->kind(), ic->state().mode(), objv,
                           idVal, rhs, ic->needsTypeBarrier(),
                           ic->guardHoles());
    MOZ_ASSERT(deferType == DeferType::AddSlot);
    AttachDecision decision = gen.tryAttachAddSlotStub(oldGroup, oldShape);

    switch (decision) {
      case AttachDecision::Attach:
        ic->attachCacheIRStub(cx, gen.writerRef(), gen.cacheKind(), ionScript,
                              &attached, gen.typeCheckInfo());
        break;
      case AttachDecision::NoAction:
        gen.trackAttached(IRGenerator::NotAttached);
        break;
      case AttachDecision::TemporarilyUnoptimizable:
      case AttachDecision::Deferred:
        MOZ_ASSERT_UNREACHABLE("Invalid attach result");
        break;
    }
  }
  if (!attached && canAttachStub) {
    ic->state().trackNotAttached();
  }

  return true;
}

/* static */
bool IonGetNameIC::update(JSContext* cx, HandleScript outerScript,
                          IonGetNameIC* ic, HandleObject envChain,
                          MutableHandleValue res) {
  IonScript* ionScript = outerScript->ionScript();
  jsbytecode* pc = ic->pc();
  RootedPropertyName name(cx, ic->script()->getName(pc));

  TryAttachIonStub<GetNameIRGenerator, IonGetNameIC>(cx, ic, ionScript,
                                                     envChain, name);

  RootedObject obj(cx);
  RootedObject holder(cx);
  Rooted<PropertyResult> prop(cx);
  if (!LookupName(cx, name, envChain, &obj, &holder, &prop)) {
    return false;
  }

  if (JSOp(*GetNextPc(pc)) == JSOp::Typeof) {
    if (!FetchName<GetNameMode::TypeOf>(cx, obj, holder, name, prop, res)) {
      return false;
    }
  } else {
    if (!FetchName<GetNameMode::Normal>(cx, obj, holder, name, prop, res)) {
      return false;
    }
  }

  // No need to call JitScript::Monitor, IonBuilder always inserts a type
  // barrier after GetName ICs.

  return true;
}

/* static */
JSObject* IonBindNameIC::update(JSContext* cx, HandleScript outerScript,
                                IonBindNameIC* ic, HandleObject envChain) {
  IonScript* ionScript = outerScript->ionScript();
  jsbytecode* pc = ic->pc();
  RootedPropertyName name(cx, ic->script()->getName(pc));

  TryAttachIonStub<BindNameIRGenerator, IonBindNameIC>(cx, ic, ionScript,
                                                       envChain, name);

  RootedObject holder(cx);
  if (!LookupNameUnqualified(cx, name, envChain, &holder)) {
    return nullptr;
  }

  return holder;
}

/* static */
JSObject* IonGetIteratorIC::update(JSContext* cx, HandleScript outerScript,
                                   IonGetIteratorIC* ic, HandleValue value) {
  IonScript* ionScript = outerScript->ionScript();

  TryAttachIonStub<GetIteratorIRGenerator, IonGetIteratorIC>(cx, ic, ionScript,
                                                             value);

  return ValueToIterator(cx, value);
}

/* static */
bool IonHasOwnIC::update(JSContext* cx, HandleScript outerScript,
                         IonHasOwnIC* ic, HandleValue val, HandleValue idVal,
                         int32_t* res) {
  IonScript* ionScript = outerScript->ionScript();

  TryAttachIonStub<HasPropIRGenerator, IonHasOwnIC>(
      cx, ic, ionScript, CacheKind::HasOwn, idVal, val);

  bool found;
  if (!HasOwnProperty(cx, val, idVal, &found)) {
    return false;
  }

  *res = found;
  return true;
}

/* static */
bool IonInIC::update(JSContext* cx, HandleScript outerScript, IonInIC* ic,
                     HandleValue key, HandleObject obj, bool* res) {
  IonScript* ionScript = outerScript->ionScript();
  RootedValue objV(cx, ObjectValue(*obj));

  TryAttachIonStub<HasPropIRGenerator, IonInIC>(cx, ic, ionScript,
                                                CacheKind::In, key, objV);

  return OperatorIn(cx, key, obj, res);
}
/* static */
bool IonInstanceOfIC::update(JSContext* cx, HandleScript outerScript,
                             IonInstanceOfIC* ic, HandleValue lhs,
                             HandleObject rhs, bool* res) {
  IonScript* ionScript = outerScript->ionScript();

  TryAttachIonStub<InstanceOfIRGenerator, IonInstanceOfIC>(cx, ic, ionScript,
                                                           lhs, rhs);

  return HasInstance(cx, rhs, lhs, res);
}

/*  static */
bool IonUnaryArithIC::update(JSContext* cx, HandleScript outerScript,
                             IonUnaryArithIC* ic, HandleValue val,
                             MutableHandleValue res) {
  IonScript* ionScript = outerScript->ionScript();
  RootedScript script(cx, ic->script());
  jsbytecode* pc = ic->pc();
  JSOp op = JSOp(*pc);

  switch (op) {
    case JSOp::BitNot: {
      res.set(val);
      if (!BitNot(cx, res, res)) {
        return false;
      }
      break;
    }
    case JSOp::Neg: {
      res.set(val);
      if (!NegOperation(cx, res, res)) {
        return false;
      }
      break;
    }
    case JSOp::Inc: {
      if (!IncOperation(cx, val, res)) {
        return false;
      }
      break;
    }
    case JSOp::Dec: {
      if (!DecOperation(cx, val, res)) {
        return false;
      }
      break;
    }
    default:
      MOZ_CRASH("Unexpected op");
  }
  MOZ_ASSERT(res.isNumeric());

  TryAttachIonStub<UnaryArithIRGenerator, IonUnaryArithIC>(cx, ic, ionScript,
                                                           op, val, res);

  return true;
}

/* static */
bool IonBinaryArithIC::update(JSContext* cx, HandleScript outerScript,
                              IonBinaryArithIC* ic, HandleValue lhs,
                              HandleValue rhs, MutableHandleValue ret) {
  IonScript* ionScript = outerScript->ionScript();
  RootedScript script(cx, ic->script());
  jsbytecode* pc = ic->pc();
  JSOp op = JSOp(*pc);

  // Don't pass lhs/rhs directly, we need the original values when
  // generating stubs.
  RootedValue lhsCopy(cx, lhs);
  RootedValue rhsCopy(cx, rhs);

  // Perform the compare operation.
  switch (op) {
    case JSOp::Add:
      // Do an add.
      if (!AddValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::Sub:
      if (!SubValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::Mul:
      if (!MulValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::Div:
      if (!DivValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::Mod:
      if (!ModValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::Pow:
      if (!PowValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::BitOr: {
      if (!BitOr(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOp::BitXor: {
      if (!BitXor(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOp::BitAnd: {
      if (!BitAnd(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOp::Lsh: {
      if (!BitLsh(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOp::Rsh: {
      if (!BitRsh(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOp::Ursh: {
      if (!UrshOperation(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    default:
      MOZ_CRASH("Unhandled binary arith op");
  }

  TryAttachIonStub<BinaryArithIRGenerator, IonBinaryArithIC>(cx, ic, ionScript,
                                                             op, lhs, rhs, ret);

  return true;
}

/* static */
bool IonCompareIC::update(JSContext* cx, HandleScript outerScript,
                          IonCompareIC* ic, HandleValue lhs, HandleValue rhs,
                          bool* res) {
  IonScript* ionScript = outerScript->ionScript();
  RootedScript script(cx, ic->script());
  jsbytecode* pc = ic->pc();
  JSOp op = JSOp(*pc);

  // Don't pass lhs/rhs directly, we need the original values when
  // generating stubs.
  RootedValue lhsCopy(cx, lhs);
  RootedValue rhsCopy(cx, rhs);

  // Perform the compare operation.
  switch (op) {
    case JSOp::Lt:
      if (!LessThan(cx, &lhsCopy, &rhsCopy, res)) {
        return false;
      }
      break;
    case JSOp::Le:
      if (!LessThanOrEqual(cx, &lhsCopy, &rhsCopy, res)) {
        return false;
      }
      break;
    case JSOp::Gt:
      if (!GreaterThan(cx, &lhsCopy, &rhsCopy, res)) {
        return false;
      }
      break;
    case JSOp::Ge:
      if (!GreaterThanOrEqual(cx, &lhsCopy, &rhsCopy, res)) {
        return false;
      }
      break;
    case JSOp::Eq:
      if (!LooselyEqual<EqualityKind::Equal>(cx, &lhsCopy, &rhsCopy, res)) {
        return false;
      }
      break;
    case JSOp::Ne:
      if (!LooselyEqual<EqualityKind::NotEqual>(cx, &lhsCopy, &rhsCopy, res)) {
        return false;
      }
      break;
    case JSOp::StrictEq:
      if (!StrictlyEqual<EqualityKind::Equal>(cx, &lhsCopy, &rhsCopy, res)) {
        return false;
      }
      break;
    case JSOp::StrictNe:
      if (!StrictlyEqual<EqualityKind::NotEqual>(cx, &lhsCopy, &rhsCopy, res)) {
        return false;
      }
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled ion compare op");
      return false;
  }

  TryAttachIonStub<CompareIRGenerator, IonCompareIC>(cx, ic, ionScript, op, lhs,
                                                     rhs);

  return true;
}

uint8_t* IonICStub::stubDataStart() {
  return reinterpret_cast<uint8_t*>(this) + stubInfo_->stubDataOffset();
}

void IonIC::attachStub(IonICStub* newStub, JitCode* code) {
  MOZ_ASSERT(newStub);
  MOZ_ASSERT(code);

  if (firstStub_) {
    IonICStub* last = firstStub_;
    while (IonICStub* next = last->next()) {
      last = next;
    }
    last->setNext(newStub, code);
  } else {
    firstStub_ = newStub;
    codeRaw_ = code->raw();
  }

  state_.trackAttached();
}
