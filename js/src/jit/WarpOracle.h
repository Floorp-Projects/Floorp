/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_WarpOracle_h
#define jit_WarpOracle_h

#include "mozilla/LinkedList.h"

#include "jit/JitAllocPolicy.h"
#include "jit/JitContext.h"

namespace js {

class ModuleEnvironmentObject;

namespace jit {

class MIRGenerator;

#define WARP_OP_SNAPSHOT_LIST(_) \
  _(WarpArguments)               \
  _(WarpRegExp)                  \
  _(WarpBuiltinProto)            \
  _(WarpGetIntrinsic)            \
  _(WarpGetImport)               \
  _(WarpLambda)

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

#ifdef DEBUG
  Kind kind_;
#endif

 protected:
  WarpOpSnapshot(Kind kind, uint32_t offset) : offset_(offset) {
#ifdef DEBUG
    kind_ = kind;
#endif
  }

 public:
  uint32_t offset() const { return offset_; }

  template <typename T>
  const T* as() const {
    MOZ_ASSERT(kind_ == T::ThisKind);
    return static_cast<const T*>(this);
  }
};

using WarpOpSnapshotList = mozilla::LinkedList<WarpOpSnapshot>;

// Template object for JSOp::Arguments.
class WarpArguments : public WarpOpSnapshot {
  // TODO: trace this when we can trace snapshots.
  ArgumentsObject* templateObj_;

 public:
  static constexpr Kind ThisKind = Kind::WarpArguments;

  WarpArguments(uint32_t offset, ArgumentsObject* templateObj)
      : WarpOpSnapshot(ThisKind, offset), templateObj_(templateObj) {}
  ArgumentsObject* templateObj() const { return templateObj_; }
};

// The "has RegExpShared" state for JSOp::RegExp's template object.
class WarpRegExp : public WarpOpSnapshot {
  bool hasShared_;

 public:
  static constexpr Kind ThisKind = Kind::WarpRegExp;

  WarpRegExp(uint32_t offset, bool hasShared)
      : WarpOpSnapshot(ThisKind, offset), hasShared_(hasShared) {}
  bool hasShared() const { return hasShared_; }
};

// The proto for JSOp::BuiltinProto if it exists at compile-time.
class WarpBuiltinProto : public WarpOpSnapshot {
  // TODO: trace this.
  JSObject* proto_;

 public:
  static constexpr Kind ThisKind = Kind::WarpBuiltinProto;

  WarpBuiltinProto(uint32_t offset, JSObject* proto)
      : WarpOpSnapshot(ThisKind, offset), proto_(proto) {
    MOZ_ASSERT(proto);
  }
  JSObject* proto() const { return proto_; }
};

// The intrinsic for JSOp::GetIntrinsic if it exists at compile-time.
class WarpGetIntrinsic : public WarpOpSnapshot {
  // TODO: trace this.
  Value intrinsic_;

 public:
  static constexpr Kind ThisKind = Kind::WarpGetIntrinsic;

  WarpGetIntrinsic(uint32_t offset, const Value& intrinsic)
      : WarpOpSnapshot(ThisKind, offset), intrinsic_(intrinsic) {}
  Value intrinsic() const { return intrinsic_; }
};

// Target module environment and slot information for JSOp::GetImport.
class WarpGetImport : public WarpOpSnapshot {
  // TODO: trace this.
  ModuleEnvironmentObject* targetEnv_;
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
};

// JSFunction info we don't want to read off-thread for JSOp::Lambda and
// JSOp::LambdaArrow.
class WarpLambda : public WarpOpSnapshot {
  // TODO: trace this
  BaseScript* baseScript_;
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
};

// Snapshot data for the environment object(s) created in the script's prologue.
// TODO: trace object pointers.
class WarpEnvironment {
 public:
  enum class Kind {
    // No environment object should be set. Leave the slot initialized to
    // |undefined|.
    None,

    // Use constantObject_ as the environment object.
    ConstantObject,

    // Use the callee's environment chain. Optionally allocate a new
    // NamedLambdaObject and/or CallObject based on namedLambdaTemplate_ and
    // callObjectTemplate_.
    Function,
  };

 private:
  Kind kind_ = Kind::None;

  union {
    // For Kind::ConstantObject, the object.
    JSObject* constantObject_;
    // For Kind::Function, the template objects for CallObject and NamedLambda
    // if needed. nullptr if the environment object is not needed.
    struct {
      CallObject* callObjectTemplate_;
      LexicalEnvironmentObject* namedLambdaTemplate_;
    } fun;
  };

 public:
  void initConstantObject(JSObject* obj) {
    kind_ = Kind::ConstantObject;
    MOZ_ASSERT(obj);
    constantObject_ = obj;
  }
  void initFunction(CallObject* callObjectTemplate,
                    LexicalEnvironmentObject* namedLambdaTemplate) {
    kind_ = Kind::Function;
    fun.callObjectTemplate_ = callObjectTemplate;
    fun.namedLambdaTemplate_ = namedLambdaTemplate;
  }

  Kind kind() const { return kind_; }

  JSObject* constantObject() const {
    MOZ_ASSERT(kind_ == Kind::ConstantObject);
    return constantObject_;
  }
  CallObject* maybeCallObjectTemplate() const {
    MOZ_ASSERT(kind_ == Kind::Function);
    return fun.callObjectTemplate_;
  }
  LexicalEnvironmentObject* maybeNamedLambdaTemplate() const {
    MOZ_ASSERT(kind_ == Kind::Function);
    return fun.namedLambdaTemplate_;
  }
};

// Snapshot data for a single JSScript.
class WarpScriptSnapshot : public TempObject {
  JSScript* script_;
  WarpEnvironment environment_;
  WarpOpSnapshotList opSnapshots_;

  // If the script has a JSOp::ImportMeta op, this is the module to bake in.
  // TODO: trace this
  ModuleObject* moduleObject_;

  // Constants pushed by JSOp::Instrumentation* ops in the script.
  // TODO: trace this
  JSObject* instrumentationCallback_;
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
};

// Data allocated by WarpOracle on the main thread that's used off-thread by
// WarpBuilder to build the MIR graph.
//
// TODO: trace op snapshots in IonCompileTask::trace (like MRootList for
// non-Warp).
// TODO: add logging to dump the full WarpSnapshot.
class WarpSnapshot : public TempObject {
  // The script to compile.
  WarpScriptSnapshot* script_;

  // The global lexical environment and its thisValue(). We don't inline
  // cross-realm calls so this can be stored once per snapshot.
  // TODO: trace these values.
  LexicalEnvironmentObject* globalLexicalEnv_;
  Value globalLexicalEnvThis_;

 public:
  explicit WarpSnapshot(JSContext* cx, WarpScriptSnapshot* script);

  WarpScriptSnapshot* script() const { return script_; }

  LexicalEnvironmentObject* globalLexicalEnv() const {
    return globalLexicalEnv_;
  }
  Value globalLexicalEnvThis() const { return globalLexicalEnvThis_; }
};

// WarpOracle creates a WarpSnapshot data structure that's used by WarpBuilder
// to generate the MIR graph off-thread.
class MOZ_STACK_CLASS WarpOracle {
  JSContext* cx_;
  MIRGenerator& mirGen_;
  TempAllocator& alloc_;
  HandleScript script_;

  mozilla::GenericErrorResult<AbortReason> abort(AbortReason r);
  mozilla::GenericErrorResult<AbortReason> abort(AbortReason r,
                                                 const char* message, ...);

  AbortReasonOr<WarpEnvironment> createEnvironment(HandleScript script);
  AbortReasonOr<WarpScriptSnapshot*> createScriptSnapshot(HandleScript script);

 public:
  WarpOracle(JSContext* cx, MIRGenerator& mirGen, HandleScript script);

  AbortReasonOr<WarpSnapshot*> createSnapshot();
};

}  // namespace jit
}  // namespace js

#endif /* jit_WarpOracle_h */
