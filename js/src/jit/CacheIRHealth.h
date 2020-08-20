/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CacheIRHealth_h
#define jit_CacheIRHealth_h

#ifdef JS_CACHEIR_SPEW

#  include "mozilla/Sprintf.h"

#  include "jit/CacheIR.h"

namespace js {
namespace jit {

// [SMDOC] CacheIR Health Rating
//
// The goal of CacheIR health rating is to make the costlier
// CacheIR stubs more apparent and easier to diagnose.
// This is done by using the scores assigned to different CacheIROps in
// CacheIROps.yaml (see the description of cost_estimate in the
// aforementioned file for how these scores are determined), summing
// the score of each op generated for a particular stub together, and displaying
// this score for each stub in a CacheIR chain. The higher the total stub score
// the more expensive the stub is.
//
// To see the CacheIR health rating for a script, simply call the
// rateMyCacheIR() shell function. This function takes either a particular
// function as a parameter and then prints the health of the CacheIR generated
// for the script associated with that function, or if you call rateMyCacheIR()
// without any parameters it will take the topmost script and rate that.
//

class CacheIRHealth {
 public:
  // Health of an individual stub.
  bool spewStubHealth(AutoStructuredSpewer& spew, ICStub* stub);
  // Health of all the stubs in an individual CacheIR Entry.
  bool spewHealthForStubsInCacheIREntry(AutoStructuredSpewer& spew,
                                        ICEntry* entry);
  // Show JSOps present in the script, formatted for CacheIR
  // health report.
  void spewJSOpAndCacheIRHealth(AutoStructuredSpewer& spew, HandleScript script,
                                jit::ICEntry* entry, jsbytecode* pc, JSOp op);
  // If a JitScript exists, shows health of all ICEntries that exist
  // for the specified script.
  bool rateMyCacheIR(JSContext* cx, HandleScript script);
};

}  // namespace jit
}  // namespace js

#endif /* JS_CACHEIR_SPEW */
#endif /* jit_CacheIRHealth_h */
