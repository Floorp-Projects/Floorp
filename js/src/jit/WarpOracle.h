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
namespace jit {

class MIRGenerator;

#define WARP_OP_SNAPSHOT_LIST(_) \
  _(WarpArguments)               \
  _(WarpRegExp)

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

// Snapshot data for a single JSScript.
class WarpScriptSnapshot : public TempObject {
  JSScript* script_;
  WarpOpSnapshotList opSnapshots_;

 public:
  WarpScriptSnapshot(JSScript* script, WarpOpSnapshotList&& opSnapshots)
      : script_(script), opSnapshots_(std::move(opSnapshots)) {}

  JSScript* script() const { return script_; }
  const WarpOpSnapshotList& opSnapshots() const { return opSnapshots_; }
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

 public:
  explicit WarpSnapshot(WarpScriptSnapshot* script) : script_(script) {}

  WarpScriptSnapshot* script() const { return script_; }
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

  AbortReasonOr<WarpScriptSnapshot*> createScriptSnapshot(HandleScript script);

 public:
  WarpOracle(JSContext* cx, MIRGenerator& mirGen, HandleScript script);

  AbortReasonOr<WarpSnapshot*> createSnapshot();
};

}  // namespace jit
}  // namespace js

#endif /* jit_WarpOracle_h */
