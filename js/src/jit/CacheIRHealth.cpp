/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef JS_CACHEIR_SPEW

#  include "jit/CacheIRHealth.h"

#  include "mozilla/Maybe.h"

#  include "gc/Zone.h"
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
    AutoStructuredSpewer& spew, ICCacheIRStub* stub) {
  const CacheIRStubInfo* stubInfo = stub->stubInfo();
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
      uint32_t count = stub->enteredCount();
      Happiness stubHappiness = spewStubHealth(spew, stub->toCacheIRStub());
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
    stub = stub->toCacheIRStub()->next();
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
  spew->property("op", CodeName(op));

  // TODO: If a perf issue arises, look into improving the SrcNotes
  // API call below.
  unsigned column;
  spew->property("lineno", PCToLineNumber(script, pc, &column));
  spew->property("column", column);

  return spewHealthForStubsInCacheIREntry(spew, entry);
}

void CacheIRHealth::spewScriptFinalWarmUpCount(JSContext* cx,
                                               const char* filename,
                                               JSScript* script,
                                               uint32_t warmUpCount) {
  AutoStructuredSpewer spew(cx, SpewChannel::RateMyCacheIR, nullptr);
  if (!spew) {
    return;
  }

  spew->property("filename", filename);
  spew->property("line", script->lineno());
  spew->property("column", script->column());
  spew->property("finalWarmUpCount", warmUpCount);
}

static bool addScriptToFinalWarmUpCountMap(JSContext* cx, HandleScript script) {
  // Create Zone::scriptFilenameMap if necessary.
  JS::Zone* zone = script->zone();
  if (!zone->scriptFinalWarmUpCountMap) {
    auto map = MakeUnique<ScriptFinalWarmUpCountMap>();
    if (!map) {
      return false;
    }

    zone->scriptFinalWarmUpCountMap = std::move(map);
  }

  auto* filename = js_pod_malloc<char>(strlen(script->filename()) + 1);
  if (!filename) {
    return false;
  }
  strcpy(filename, script->filename());

  if (!zone->scriptFinalWarmUpCountMap->put(
          script, mozilla::MakeTuple(uint32_t(0), filename))) {
    js_free(filename);
    return false;
  }

  script->setNeedsFinalWarmUpCount();
  return true;
}

void CacheIRHealth::rateIC(JSContext* cx, ICEntry* entry, HandleScript script,
                           SpewContext context) {
  AutoStructuredSpewer spew(cx, SpewChannel::RateMyCacheIR, script);
  if (!spew) {
    return;
  }

  if (!addScriptToFinalWarmUpCountMap(cx, script)) {
    cx->recoverFromOutOfMemory();
    return;
  }
  spew->property("spewContext", uint8_t(context));

  jsbytecode* op = entry->pc(script);
  JSOp jsOp = JSOp(*op);

  spewJSOpAndCacheIRHealth(spew, script, entry, op, jsOp);
}

void CacheIRHealth::rateScript(JSContext* cx, HandleScript script,
                               SpewContext context) {
  jit::JitScript* jitScript = script->maybeJitScript();
  if (!jitScript) {
    return;
  }

  AutoStructuredSpewer spew(cx, SpewChannel::RateMyCacheIR, script);
  if (!spew) {
    return;
  }

  if (!addScriptToFinalWarmUpCountMap(cx, script)) {
    cx->recoverFromOutOfMemory();
    return;
  }

  spew->property("spewContext", uint8_t(context));

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
      spew->beginObject();
      Happiness entryHappiness =
          spewJSOpAndCacheIRHealth(spew, script, entry, next, op);

      if (entryHappiness < scriptHappiness) {
        scriptHappiness = entryHappiness;
      }
      spew->endObject();
    }

    next += len;
  }
  spew->endList();  // entries

  spew->property("scriptHappiness", uint8_t(scriptHappiness));
}

#endif /* JS_CACHEIR_SPEW */
