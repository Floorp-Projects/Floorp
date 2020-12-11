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

// TODO: Refine how we assign happiness based on total health score.
CacheIRHealth::Happiness CacheIRHealth::determineStubHappiness(
    uint32_t stubHealthScore) {
  if (stubHealthScore >= 30) {
    return Sad;
  }

  if (stubHealthScore >= 20) {
    return MediumSad;
  }

  if (stubHealthScore >= 10) {
    return MediumHappy;
  }

  return Happy;
}

CacheIRHealth::Happiness CacheIRHealth::spewStubHealth(
    AutoStructuredSpewer& spew, ICStub* stub) {
  const CacheIRStubInfo* stubInfo = stub->cacheIRStubInfo();
  CacheIRReader stubReader(stubInfo);
  uint32_t totalStubHealth = 0;

  spew->beginListProperty("cacheIROps");
  while (stubReader.more()) {
    CacheOp op = stubReader.readOp();
    uint32_t opHealth = CacheIROpHealth[size_t(op)];
    uint32_t argLength = CacheIROpInfos[size_t(op)].argLength;
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

  Happiness stubHappiness = determineStubHappiness(totalStubHealth);
  spew->property("stubHappiness", stubHappiness);

  return stubHappiness;
}

CacheIRHealth::Happiness CacheIRHealth::spewHealthForStubsInCacheIREntry(
    AutoStructuredSpewer& spew, ICEntry* entry) {
  jit::ICStub* stub = entry->firstStub();

  spew->beginListProperty("stubs");

  Happiness entryHappiness = Happy;
  bool sawNonZeroCount = false;

  while (stub && !stub->isFallback()) {
    spew->beginObject();
    {
      uint32_t count = stub->getEnteredCount();
      Happiness stubHappiness = spewStubHealth(spew, stub);
      if (stubHappiness < entryHappiness) {
        entryHappiness = stubHappiness;
      }

      if (count > 0 && sawNonZeroCount) {
        // More than one stub has a hit count greater than zero.
        // This is sad because we do not Warp transpile in this case.
        entryHappiness = Sad;
      } else if (count > 0 && !sawNonZeroCount) {
        sawNonZeroCount = true;
      }

      spew->property("hitCount", count);
    }

    spew->endObject();
    stub = stub->next();
  }
  spew->endList();  // stubs

  if (entry->fallbackStub()->state().mode() != ICState::Mode::Specialized) {
    entryHappiness = Sad;
  }

  spew->property("entryHappiness", uint8_t(entryHappiness));

  spew->property("mode", uint8_t(entry->fallbackStub()->state().mode()));

  spew->property("fallbackCount", entry->fallbackStub()->enteredCount());

  return entryHappiness;
}

CacheIRHealth::Happiness CacheIRHealth::spewJSOpAndCacheIRHealth(
    AutoStructuredSpewer& spew, HandleScript script, jit::ICEntry* entry,
    jsbytecode* pc, JSOp op) {
  spew->beginObject();
  spew->property("op", CodeName(op));

  if (pc == script->main()) {
    spew->property("main", true);
  }

  // TODO: If a perf issue arises, look into improving the SrcNotes
  // API call below.
  unsigned column;
  spew->property("lineno", PCToLineNumber(script, pc, &column));
  spew->property("column", column);

  Happiness entryHappiness = spewHealthForStubsInCacheIREntry(spew, entry);
  spew->endObject();

  return entryHappiness;
}

void CacheIRHealth::rateMyCacheIR(JSContext* cx, HandleScript script) {
  jit::JitScript* jitScript = script->maybeJitScript();
  if (!jitScript) {
    return;
  }

  AutoStructuredSpewer spew(cx, SpewChannel::RateMyCacheIR, script);
  if (!spew) {
    return;
  }

  jsbytecode* next = script->code();
  jsbytecode* end = script->codeEnd();

  spew->beginListProperty("entries");
  ICEntry* prevEntry = nullptr;
  Happiness scriptHappiness = Happy;
  while (next < end) {
    uint32_t len = 0;
    uint32_t pcOffset = script->pcToOffset(next);

    jit::ICEntry* entry =
        jitScript->maybeICEntryFromPCOffset(pcOffset, prevEntry);
    if (entry) {
      prevEntry = entry;
    }

    JSOp op = JSOp(*next);
    const JSCodeSpec& cs = CodeSpec(op);
    len = cs.length;
    MOZ_ASSERT(len);

    if (entry) {
      Happiness entryHappiness =
          spewJSOpAndCacheIRHealth(spew, script, entry, next, op);

      if (entryHappiness < scriptHappiness) {
        scriptHappiness = entryHappiness;
      }
    }

    next += len;
  }
  spew->endList();  // entries

  spew->property("scriptHappiness", uint8_t(scriptHappiness));
}

#endif /* JS_CACHEIR_SPEW */
