/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_WarpSnapshot_h
#define jit_WarpSnapshot_h

#include "mozilla/LinkedList.h"
#include "mozilla/Variant.h"

#include "gc/Policy.h"
#include "jit/JitAllocPolicy.h"
#include "jit/JitContext.h"
#include "vm/Printer.h"

namespace js {

class ModuleEnvironmentObject;

namespace jit {

#define WARP_OP_SNAPSHOT_LIST(_) \
  _(WarpArguments)               \
  _(WarpRegExp)                  \
  _(WarpFunctionProto)           \
  _(WarpGetIntrinsic)            \
  _(WarpGetImport)               \
  _(WarpLambda)                  \
  _(WarpRest)

// Wrapper for GC things stored in WarpSnapshot. Asserts the GC pointer is not
// nursery-allocated. These pointers must be traced using TraceWarpGCPtr.
template <typename T>
class WarpGCPtr {
  // Note: no pre-barrier is needed because this is a constant. No post-barrier
  // is needed because the value is always tenured.
  const T ptr_;

 public:
  explicit WarpGCPtr(const T& ptr) : ptr_(ptr) {
    MOZ_ASSERT(JS::GCPolicy<T>::isTenured(ptr),
               "WarpSnapshot pointers must be tenured");
  }
  WarpGCPtr(const WarpGCPtr<T>& other) = default;

  operator T() const { return static_cast<T>(ptr_); }
  T operator->() const { return static_cast<T>(ptr_); }

 private:
  WarpGCPtr() = delete;
  void operator=(WarpGCPtr<T>& other) = delete;
};

// WarpOpSnapshot is the base class for data attached to a single bytecode op by
// WarpOracle. This is typically data that WarpBuilder can't read off-thread
// without racing.
class WarpOpSnapshot : public TempObject,
                       public mozilla::LinkedListElement<WarpOpSnapshot> {
 public:
  enum class Kind : uint16_t {
#define DEF_KIND(KIND) KIND,
    WARP_OP_SNAPSHOT_LIST(DEF_KIND)
#undef DEF_KIND
  };

 private:
  // Bytecode offset.
  uint32_t offset_ = 0;

  Kind kind_;

 protected:
  WarpOpSnapshot(Kind kind, uint32_t offset) : offset_(offset), kind_(kind) {}

 public:
  uint32_t offset() const { return offset_; }

  template <typename T>
  const T* as() const {
    MOZ_ASSERT(kind_ == T::ThisKind);
    return static_cast<const T*>(this);
  }

  template <typename T>
  T* as() {
    MOZ_ASSERT(kind_ == T::ThisKind);
    return static_cast<T*>(this);
  }

  void trace(JSTracer* trc);

#ifdef JS_JITSPEW
  void dump(GenericPrinter& out, JSScript* script) const;
#endif
};

using WarpOpSnapshotList = mozilla::LinkedList<WarpOpSnapshot>;

// Template object for JSOp::Arguments.
class WarpArguments : public WarpOpSnapshot {
  // Note: this can be nullptr if the realm has no template object yet.
  WarpGCPtr<ArgumentsObject*> templateObj_;

 public:
  static constexpr Kind ThisKind = Kind::WarpArguments;

  WarpArguments(uint32_t offset, ArgumentsObject* templateObj)
      : WarpOpSnapshot(ThisKind, offset), templateObj_(templateObj) {}
  ArgumentsObject* templateObj() const { return templateObj_; }

  void traceData(JSTracer* trc);

#ifdef JS_JITSPEW
  void dumpData(GenericPrinter& out) const;
#endif
};

// The "has RegExpShared" state for JSOp::RegExp's template object.
class WarpRegExp : public WarpOpSnapshot {
  bool hasShared_;

 public:
  static constexpr Kind ThisKind = Kind::WarpRegExp;

  WarpRegExp(uint32_t offset, bool hasShared)
      : WarpOpSnapshot(ThisKind, offset), hasShared_(hasShared) {}
  bool hasShared() const { return hasShared_; }

  void traceData(JSTracer* trc);

#ifdef JS_JITSPEW
  void dumpData(GenericPrinter& out) const;
#endif
};

// The proto for JSOp::FunctionProto if it exists at compile-time.
class WarpFunctionProto : public WarpOpSnapshot {
  WarpGCPtr<JSObject*> proto_;

 public:
  static constexpr Kind ThisKind = Kind::WarpFunctionProto;

  WarpFunctionProto(uint32_t offset, JSObject* proto)
      : WarpOpSnapshot(ThisKind, offset), proto_(proto) {
    MOZ_ASSERT(proto);
  }
  JSObject* proto() const { return proto_; }

  void traceData(JSTracer* trc);

#ifdef JS_JITSPEW
  void dumpData(GenericPrinter& out) const;
#endif
};

// The intrinsic for JSOp::GetIntrinsic if it exists at compile-time.
class WarpGetIntrinsic : public WarpOpSnapshot {
  WarpGCPtr<Value> intrinsic_;

 public:
  static constexpr Kind ThisKind = Kind::WarpGetIntrinsic;

  WarpGetIntrinsic(uint32_t offset, const Value& intrinsic)
      : WarpOpSnapshot(ThisKind, offset), intrinsic_(intrinsic) {}
  Value intrinsic() const { return intrinsic_; }

  void traceData(JSTracer* trc);

#ifdef JS_JITSPEW
  void dumpData(GenericPrinter& out) const;
#endif
};

// Target module environment and slot information for JSOp::GetImport.
class WarpGetImport : public WarpOpSnapshot {
  WarpGCPtr<ModuleEnvironmentObject*> targetEnv_;
  uint32_t numFixedSlots_;
  uint32_t slot_;
  bool needsLexicalCheck_;

 public:
  static constexpr Kind ThisKind = Kind::WarpGetImport;

  WarpGetImport(uint32_t offset, ModuleEnvironmentObject* targetEnv,
                uint32_t numFixedSlots, uint32_t slot, bool needsLexicalCheck)
      : WarpOpSnapshot(ThisKind, offset),
        targetEnv_(targetEnv),
        numFixedSlots_(numFixedSlots),
        slot_(slot),
        needsLexicalCheck_(needsLexicalCheck) {}
  ModuleEnvironmentObject* targetEnv() const { return targetEnv_; }
  uint32_t numFixedSlots() const { return numFixedSlots_; }
  uint32_t slot() const { return slot_; }
  bool needsLexicalCheck() const { return needsLexicalCheck_; }

  void traceData(JSTracer* trc);

#ifdef JS_JITSPEW
  void dumpData(GenericPrinter& out) const;
#endif
};

// JSFunction info we don't want to read off-thread for JSOp::Lambda and
// JSOp::LambdaArrow.
class WarpLambda : public WarpOpSnapshot {
  WarpGCPtr<BaseScript*> baseScript_;
  FunctionFlags flags_;
  uint16_t nargs_;

 public:
  static constexpr Kind ThisKind = Kind::WarpLambda;

  WarpLambda(uint32_t offset, BaseScript* baseScript, FunctionFlags flags,
             uint16_t nargs)
      : WarpOpSnapshot(ThisKind, offset),
        baseScript_(baseScript),
        flags_(flags),
        nargs_(nargs) {}
  BaseScript* baseScript() const { return baseScript_; }
  FunctionFlags flags() const { return flags_; }
  uint16_t nargs() const { return nargs_; }

  void traceData(JSTracer* trc);

#ifdef JS_JITSPEW
  void dumpData(GenericPrinter& out) const;
#endif
};

// Template object for JSOp::Rest.
class WarpRest : public WarpOpSnapshot {
  WarpGCPtr<ArrayObject*> templateObject_;

 public:
  static constexpr Kind ThisKind = Kind::WarpRest;

  WarpRest(uint32_t offset, ArrayObject* templateObject)
      : WarpOpSnapshot(ThisKind, offset), templateObject_(templateObject) {}
  ArrayObject* templateObject() const { return templateObject_; }

  void traceData(JSTracer* trc);

#ifdef JS_JITSPEW
  void dumpData(GenericPrinter& out) const;
#endif
};

struct NoEnvironment {};
using ConstantObjectEnvironment = WarpGCPtr<JSObject*>;
struct FunctionEnvironment {
  WarpGCPtr<CallObject*> callObjectTemplate;
  WarpGCPtr<LexicalEnvironmentObject*> namedLambdaTemplate;

 public:
  FunctionEnvironment(CallObject* callObjectTemplate,
                      LexicalEnvironmentObject* namedLambdaTemplate)
      : callObjectTemplate(callObjectTemplate),
        namedLambdaTemplate(namedLambdaTemplate) {}
};

// Snapshot data for the environment object(s) created in the script's prologue.
//
// One of:
//
// * NoEnvironment: No environment object should be set. Leave the slot
//   initialized to |undefined|.
//
// * ConstantObjectEnvironment: Use this JSObject* as environment object.
//
// * FunctionEnvironment: Use the callee's environment chain. Optionally
//   allocate a new NamedLambdaObject and/or CallObject based on
//   namedLambdaTemplate and callObjectTemplate.
using WarpEnvironment =
    mozilla::Variant<NoEnvironment, ConstantObjectEnvironment,
                     FunctionEnvironment>;

// Snapshot data for a single JSScript.
class WarpScriptSnapshot : public TempObject {
  WarpGCPtr<JSScript*> script_;
  WarpEnvironment environment_;
  WarpOpSnapshotList opSnapshots_;

  // If the script has a JSOp::ImportMeta op, this is the module to bake in.
  WarpGCPtr<ModuleObject*> moduleObject_;

  // Constants pushed by JSOp::Instrumentation* ops in the script.
  WarpGCPtr<JSObject*> instrumentationCallback_;
  mozilla::Maybe<int32_t> instrumentationScriptId_;
  mozilla::Maybe<bool> instrumentationActive_;

  // Whether this script is for an arrow function.
  bool isArrowFunction_;

 public:
  WarpScriptSnapshot(JSScript* script, const WarpEnvironment& env,
                     WarpOpSnapshotList&& opSnapshots,
                     ModuleObject* moduleObject,
                     JSObject* instrumentationCallback,
                     mozilla::Maybe<int32_t> instrumentationScriptId,
                     mozilla::Maybe<bool> instrumentationActive);

  JSScript* script() const { return script_; }
  const WarpEnvironment& environment() const { return environment_; }
  const WarpOpSnapshotList& opSnapshots() const { return opSnapshots_; }
  ModuleObject* moduleObject() const { return moduleObject_; }

  JSObject* instrumentationCallback() const {
    MOZ_ASSERT(instrumentationCallback_);
    return instrumentationCallback_;
  }
  int32_t instrumentationScriptId() const { return *instrumentationScriptId_; }
  bool instrumentationActive() const { return *instrumentationActive_; }

  bool isArrowFunction() const { return isArrowFunction_; }

  void trace(JSTracer* trc);

#ifdef JS_JITSPEW
  void dump(GenericPrinter& out) const;
#endif
};

// Data allocated by WarpOracle on the main thread that's used off-thread by
// WarpBuilder to build the MIR graph.
class WarpSnapshot : public TempObject {
  // The script to compile.
  WarpScriptSnapshot* script_;

  // The global lexical environment and its thisValue(). We don't inline
  // cross-realm calls so this can be stored once per snapshot.
  WarpGCPtr<LexicalEnvironmentObject*> globalLexicalEnv_;
  WarpGCPtr<Value> globalLexicalEnvThis_;

 public:
  explicit WarpSnapshot(JSContext* cx, WarpScriptSnapshot* script);

  WarpScriptSnapshot* script() const { return script_; }

  LexicalEnvironmentObject* globalLexicalEnv() const {
    return globalLexicalEnv_;
  }
  Value globalLexicalEnvThis() const { return globalLexicalEnvThis_; }

  void trace(JSTracer* trc);

#ifdef JS_JITSPEW
  void dump() const;
  void dump(GenericPrinter& out) const;
#endif
};

}  // namespace jit
}  // namespace js

#endif /* jit_WarpSnapshot_h */
