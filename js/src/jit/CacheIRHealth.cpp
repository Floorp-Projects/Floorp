/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef JS_CACHEIR_SPEW

#  include "jit/CacheIRHealth.h"

#  include "mozilla/Maybe.h"

#  include "jit/JitScript.h"

using namespace js;
using namespace js::jit;

void CacheIRHealth::formatForNewStub() { output_.put("\t|\n\t\\\n\t "); }

void CacheIRHealth::spewCacheIREntryState(ICEntry* entry) {
  formatForNewStub();

  ICState& state = entry->fallbackStub()->state();
  if (state.mode() == ICState::Mode::Generic) {
    output_.put("Generic\n\t ");
  } else if (state.mode() == ICState::Mode::Megamorphic) {
    output_.put("Megamorphic\n\t ");
  }
}

static const CacheIRStubInfo* getStubInfo(ICStub* stub) {
  const CacheIRStubInfo* stubInfo = nullptr;
  switch (stub->kind()) {
    case ICStub::Kind::CacheIR_Monitored:
      stubInfo = stub->toCacheIR_Monitored()->stubInfo();
      break;
    case ICStub::Kind::CacheIR_Regular:
      stubInfo = stub->toCacheIR_Regular()->stubInfo();
      break;
    case ICStub::Kind::CacheIR_Updated:
      stubInfo = stub->toCacheIR_Updated()->stubInfo();
      break;
    default:
      MOZ_CRASH("Only cache IR stubs supported");
  }

  return stubInfo;
}

bool CacheIRHealth::spewStubHealth(ICStub* stub) {
  const CacheIRStubInfo* stubInfo = getStubInfo(stub);
  CacheIRReader stubReader(stubInfo);
  uint32_t totalStubHealth = 0;
  while (stubReader.more()) {
    CacheOp op = stubReader.readOp();
    uint32_t opHealth = CacheIROpHealth[size_t(op)];
    if (opHealth == UINT32_MAX) {
      const char* opName = CacheIROpNames[size_t(op)];
      if (!output_.jsprintf(
              "Warning:  %s does not have a cost estimate value in "
              "CacheIROps.yaml.\n\t You can find how to score a CacheIROp in "
              "CacheIROps.yaml under cost estimate.\n\t",
              opName)) {
        return false;
      }
    } else {
      totalStubHealth += opHealth;
    }
  }

  return output_.jsprintf("Health of stub: %" PRIu32 "\n\t ", totalStubHealth);
}

bool CacheIRHealth::spewHealthForStubsInCacheIREntry(ICEntry* entry) {
  jit::ICStub* stub = entry->firstStub();
  while (stub && !stub->isFallback()) {
    uint32_t count;
    if (js::jit::GetStubEnteredCount(stub, &count)) {
      formatForNewStub();
      spewStubHealth(stub);
      if (!output_.jsprintf("Stub hit count: %" PRIu32 "\n", count)) {
        return false;
      }
    }
    stub = stub->next();
  }

  spewCacheIREntryState(entry);
  return output_.jsprintf("Fallback entered count: %" PRIu32 "\n\n",
                          entry->fallbackStub()->enteredCount());
}

uint32_t CacheIRHealth::spewJSOpForCacheIRHealth(unsigned pcOffset,
                                                 jsbytecode next) {
  JSOp op = JSOp(next);
  const JSCodeSpec& cs = CodeSpec(op);
  const uint32_t len = cs.length;
  if (!len) {
    return 0;
  }

  if (!output_.jsprintf("%05u:  %s\n", pcOffset, CodeName(op))) {
    return 0;
  }

  return len;
}

bool CacheIRHealth::rateMyCacheIR(HandleScript script) {
  if (script->hasJitScript()) {
    jit::JitScript* jitScript = script->jitScript();
    if (!output_.jsprintf("%s:%u\n", script->filename(),
                          unsigned(script->lineno()))) {
      return false;
    }

    if (!output_.put("Offset   JSOp\n")) {
      return false;
    }

    if (!output_.put("--------------\n")) {
      return false;
    }

    jsbytecode* next = script->code();
    jsbytecode* end = script->codeEnd();
    while (next < end) {
      if (next == script->main()) {
        if (!output_.put("\nMain:\n")) {
          return false;
        }
      }

      uint32_t pcOffset = script->pcToOffset(next);
      jit::ICEntry* entry = jitScript->maybeICEntryFromPCOffset(pcOffset);

      const uint32_t len = spewJSOpForCacheIRHealth(pcOffset, *next);
      if (!len) {
        return false;
      }

      if (entry) {
        spewHealthForStubsInCacheIREntry(entry);
      }

      next += len;
    }
  }

  printf("%s\n", output_.string());
  return true;
}

#endif /* JS_CACHEIR_SPEW */
