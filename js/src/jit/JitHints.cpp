/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/JitHints-inl.h"
#include "vm/BytecodeLocation-inl.h"
#include "vm/JSScript-inl.h"

using namespace js;
using namespace js::jit;

JitHintsMap::~JitHintsMap() {
  while (!ionHintQueue_.isEmpty()) {
    IonHint* e = ionHintQueue_.popFirst();
    js_delete(e);
  }
  ionHintMap_.clear();
}

JitHintsMap::IonHint* JitHintsMap::addIonHint(ScriptKey key,
                                              ScriptToHintMap::AddPtr& p) {
  IonHint* hint = js_new<IonHint>(key);
  if (!hint) {
    return nullptr;
  }

  if (!ionHintMap_.add(p, key, hint)) {
    return nullptr;
  }

  ionHintQueue_.insertBack(hint);

  if (ionHintMap_.count() > IonHintMaxEntries) {
    IonHint* h = ionHintQueue_.popFirst();
    ionHintMap_.remove(h->key());
    js_delete(h);
  }

  return hint;
}

void JitHintsMap::updateAsRecentlyUsed(IonHint* hint) {
  hint->remove();
  ionHintQueue_.insertBack(hint);
}

bool JitHintsMap::recordIonCompilation(JSScript* script) {
  ScriptKey key = getScriptKey(script);
  if (!key) {
    return true;
  }

  // Only add hints for scripts that will be eager baseline compiled.
  if (!baselineHintMap_.mightContain(key)) {
    return true;
  }

  auto p = ionHintMap_.lookupForAdd(key);
  IonHint* hint = nullptr;
  if (p) {
    // Don't modify existing threshold values.
    hint = p->value();
    updateAsRecentlyUsed(hint);
  } else {
    hint = addIonHint(key, p);
    if (!hint) {
      return false;
    }
  }

  hint->initThreshold(script->warmUpCountAtLastICStub());
  return true;
}

bool JitHintsMap::getIonThresholdHint(JSScript* script,
                                      uint32_t& thresholdOut) {
  ScriptKey key = getScriptKey(script);
  if (key) {
    auto p = ionHintMap_.lookup(key);
    if (p) {
      IonHint* hint = p->value();
      // If the threshold is 0, the hint only contains
      // monomorphic inlining location information and
      // may not have entered Ion before.
      if (hint->threshold() != 0) {
        updateAsRecentlyUsed(hint);
        thresholdOut = hint->threshold();
        return true;
      }
    }
  }
  return false;
}

void JitHintsMap::recordInvalidation(JSScript* script) {
  ScriptKey key = getScriptKey(script);
  if (key) {
    auto p = ionHintMap_.lookup(key);
    if (p) {
      p->value()->incThreshold(InvalidationThresholdIncrement);
    }
  }
}

bool JitHintsMap::addMonomorphicInlineLocation(JSScript* script,
                                               BytecodeLocation loc) {
  ScriptKey key = getScriptKey(script);
  if (!key) {
    return true;
  }

  // Only add inline hints for scripts that will be eager baseline compiled.
  if (!baselineHintMap_.mightContain(key)) {
    return true;
  }

  auto p = ionHintMap_.lookupForAdd(key);
  IonHint* hint = nullptr;
  if (p) {
    hint = p->value();
  } else {
    hint = addIonHint(key, p);
    if (!hint) {
      return false;
    }
  }

  if (!hint->hasSpaceForMonomorphicInlineEntry()) {
    return true;
  }

  uint32_t offset = loc.bytecodeToOffset(script);
  return hint->addMonomorphicInlineOffset(offset);
}

bool JitHintsMap::hasMonomorphicInlineHintAtOffset(JSScript* script,
                                                   uint32_t offset) {
  ScriptKey key = getScriptKey(script);
  if (!key) {
    return false;
  }

  auto p = ionHintMap_.lookup(key);
  if (p) {
    return p->value()->hasMonomorphicInlineOffset(offset);
  }

  return false;
}
