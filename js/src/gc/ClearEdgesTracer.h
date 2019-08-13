/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_ClearEdgesTracer_h
#define gc_ClearEdgesTracer_h

#include "js/TracingAPI.h"

namespace js {
namespace gc {

struct ClearEdgesTracer final : public JS::CallbackTracer {
  explicit ClearEdgesTracer(JSRuntime* rt);
  ClearEdgesTracer();

#ifdef DEBUG
  TracerKind getTracerKind() const override { return TracerKind::ClearEdges; }
#endif

  template <typename T>
  inline bool clearEdge(T** thingp);

  bool onObjectEdge(JSObject** objp) override;
  bool onStringEdge(JSString** strp) override;
  bool onSymbolEdge(JS::Symbol** symp) override;
  bool onBigIntEdge(JS::BigInt** bip) override;
  bool onScriptEdge(JSScript** scriptp) override;
  bool onShapeEdge(js::Shape** shapep) override;
  bool onObjectGroupEdge(js::ObjectGroup** groupp) override;
  bool onBaseShapeEdge(js::BaseShape** basep) override;
  bool onJitCodeEdge(js::jit::JitCode** codep) override;
  bool onLazyScriptEdge(js::LazyScript** lazyp) override;
  bool onScopeEdge(js::Scope** scopep) override;
  bool onRegExpSharedEdge(js::RegExpShared** sharedp) override;
  bool onChild(const JS::GCCellPtr& thing) override;
};

}  // namespace gc
}  // namespace js

#endif  // gc_ClearEdgesTracer_h
