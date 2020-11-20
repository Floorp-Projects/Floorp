/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Definitions related to javascript type inference. */

#ifndef vm_TypeInference_h
#define vm_TypeInference_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"

#include "jsfriendapi.h"
#include "jstypes.h"

#include "ds/LifoAlloc.h"
#include "gc/Barrier.h"
#include "jit/IonTypes.h"
#include "js/AllocPolicy.h"
#include "js/HeapAPI.h"  // js::CurrentThreadCanAccessZone
#include "js/UbiNode.h"
#include "js/Utility.h"
#include "js/Vector.h"
#include "threading/ProtectedData.h"  // js::ZoneData
#include "util/DiagnosticAssertions.h"
#include "vm/Shape.h"
#include "vm/TypeSet.h"

namespace js {

class TypeZone;
class PlainObject;

namespace jit {

class IonScript;
class JitScript;
class TempAllocator;

}  // namespace jit

/* Is this a reasonable PC to be doing inlining on? */
inline bool isInlinableCall(jsbytecode* pc);

bool ClassCanHaveExtraProperties(const JSClass* clasp);

struct AutoEnterAnalysis;

class TypeZone {
  JS::Zone* const zone_;

  // Under CodeGenerator::link, the id of the current compilation.
  ZoneData<mozilla::Maybe<IonCompilationId>> currentCompilationId_;

  TypeZone(const TypeZone&) = delete;
  void operator=(const TypeZone&) = delete;

 public:
  ZoneData<bool> keepJitScripts;

  // The topmost AutoEnterAnalysis on the stack, if there is one.
  ZoneData<AutoEnterAnalysis*> activeAnalysis;

  explicit TypeZone(JS::Zone* zone);
  ~TypeZone();

  JS::Zone* zone() const { return zone_; }

  mozilla::Maybe<IonCompilationId> currentCompilationId() const {
    return currentCompilationId_.ref();
  }
  mozilla::Maybe<IonCompilationId>& currentCompilationIdRef() {
    return currentCompilationId_.ref();
  }
};

} /* namespace js */

// JS::ubi::Nodes can point to object groups; they're js::gc::Cell instances
// with no associated compartment.
namespace JS {
namespace ubi {

template <>
class Concrete<js::ObjectGroup> : TracerConcrete<js::ObjectGroup> {
 protected:
  explicit Concrete(js::ObjectGroup* ptr)
      : TracerConcrete<js::ObjectGroup>(ptr) {}

 public:
  static void construct(void* storage, js::ObjectGroup* ptr) {
    new (storage) Concrete(ptr);
  }

  Size size(mozilla::MallocSizeOf mallocSizeOf) const override;

  const char16_t* typeName() const override { return concreteTypeName; }
  static const char16_t concreteTypeName[];
};

}  // namespace ubi
}  // namespace JS

#endif /* vm_TypeInference_h */
