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

bool CacheIRHealth::spewStubHealth(AutoStructuredSpewer& spew, ICStub* stub) {
  const CacheIRStubInfo* stubInfo = stub->cacheIRStubInfo();
  CacheIRReader stubReader(stubInfo);
  uint32_t totalStubHealth = 0;

  spew->beginListProperty("cacheIROps");
  while (stubReader.more()) {
    CacheOp op = stubReader.readOp();
    uint32_t opHealth = CacheIROpHealth[size_t(op)];
    uint32_t argLength = CacheIROpArgLengths[size_t(op)];
    const char* opName = CacheIROpNames[size_t(op)];

    spew->beginObject();
    if (opHealth == UINT32_MAX) {
      spew->property("unscoredOp", opName);
    } else {
      spew->property("cacheIROp", opName);
      spew->property("opHealth", opHealth);
      totalStubHealth += opHealth;
    }
    spew->endObject();

    stubReader.skip(argLength);
  }
  spew->endList();  // cacheIROps

  spew->property("stubHealth", totalStubHealth);
  return true;
}

bool CacheIRHealth::spewHealthForStubsInCacheIREntry(AutoStructuredSpewer& spew,
                                                     ICEntry* entry) {
  jit::ICStub* stub = entry->firstStub();

  spew->beginListProperty("stubs");
  while (stub && !stub->isFallback()) {
    spew->beginObject();
    {
      uint32_t count;
      if (js::jit::GetStubEnteredCount(stub, &count)) {
        spewStubHealth(spew, stub);

        spew->property("hitCount", count);
      }
    }

    spew->endObject();
    stub = stub->next();
  }
  spew->endList();  // stubs

  spew->property("mode", uint8_t(entry->fallbackStub()->state().mode()));

  spew->property("fallbackCount", entry->fallbackStub()->enteredCount());
  return true;
}

uint32_t CacheIRHealth::spewJSOpForCacheIRHealth(AutoStructuredSpewer& spew,
                                                 unsigned pcOffset,
                                                 jsbytecode next) {
  JSOp op = JSOp(next);
  const JSCodeSpec& cs = CodeSpec(op);
  const uint32_t len = cs.length;
  if (!len) {
    return 0;
  }

  spew->property("op", CodeName(op));

  return len;
}

bool CacheIRHealth::rateMyCacheIR(JSContext* cx, HandleScript script) {
  AutoStructuredSpewer spew(cx, SpewChannel::RateMyCacheIR, script);
  if (!spew) {
    return true;
  }

  if (script->hasJitScript()) {
    jit::JitScript* jitScript = script->jitScript();

    jsbytecode* next = script->code();
    jsbytecode* end = script->codeEnd();
    spew->beginListProperty("entries");

    while (next < end) {
      uint32_t len = 0;
      spew->beginObject();
      {
        if (next == script->main()) {
          spew->property("main", true);
        }

        uint32_t pcOffset = script->pcToOffset(next);
        jit::ICEntry* entry = jitScript->maybeICEntryFromPCOffset(pcOffset);

        len = spewJSOpForCacheIRHealth(spew, pcOffset, *next);
        MOZ_ASSERT(len);

        if (entry && (entry->firstStub()->isFallback() ||
                      ICStub::IsCacheIRKind(entry->firstStub()->kind()))) {
          spewHealthForStubsInCacheIREntry(spew, entry);
        }
      }
      spew->endObject();

      next += len;
    }

    spew->endList();  // entries
  }

  return true;
}

#endif /* JS_CACHEIR_SPEW */
