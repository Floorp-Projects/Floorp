/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dbg_DebugScript_h
#define dbg_DebugScript_h

#include <stddef.h>  // for offsetof
#include <stddef.h>  // for size_t
#include <stdint.h>  // for uint32_t

#include "jsapi.h"

namespace JS {
class Realm;
}

namespace js {

class BreakpointSite;
class Debugger;
class FreeOp;

// DebugScript manages the internal debugger state for a JSScript, which may be
// associated with multiple Debuggers.
class DebugScript {
  friend class DebugAPI;

  /*
   * If this is a generator script, this is the number of Debugger.Frames
   * referring to calls to this generator, whether live or suspended. Closed
   * generators do not contribute a count.
   *
   * When greater than zero, this script should be compiled with debug
   * instrumentation to call Debugger::onResumeFrame at each resumption site, so
   * that Debugger can reconnect any extant Debugger.Frames with the new
   * concrete frame.
   */
  uint32_t generatorObserverCount;

  /*
   * The number of Debugger.Frame objects that refer to frames running this
   * script and that have onStep handlers. When nonzero, the interpreter and JIT
   * must arrange to call Debugger::onSingleStep before each bytecode, or at
   * least at some useful granularity.
   */
  uint32_t stepperCount;

  /*
   * Number of breakpoint sites at opcodes in the script. This is the number
   * of populated entries in DebugScript::breakpoints, below.
   */
  uint32_t numSites;

  /*
   * Breakpoints set in our script. For speed and simplicity, this array is
   * parallel to script->code(): the BreakpointSite for the opcode at
   * script->code()[offset] is debugScript->breakpoints[offset]. Naturally,
   * this array's true length is script->length().
   */
  BreakpointSite* breakpoints[1];

  /*
   * True if this DebugScript carries any useful information. If false, it
   * should be removed from its JSScript.
   */
  bool needed() const {
    return generatorObserverCount > 0 || stepperCount > 0 || numSites > 0;
  }

  static size_t allocSize(size_t codeLength) {
    return offsetof(DebugScript, breakpoints) +
           codeLength * sizeof(BreakpointSite*);
  }

  static DebugScript* get(JSScript* script);
  static DebugScript* getOrCreate(JSContext* cx, JSScript* script);

 public:
  static BreakpointSite* getBreakpointSite(JSScript* script, jsbytecode* pc);
  static BreakpointSite* getOrCreateBreakpointSite(JSContext* cx,
                                                   JSScript* script,
                                                   jsbytecode* pc);
  static void destroyBreakpointSite(FreeOp* fop, JSScript* script,
                                    jsbytecode* pc);

  static void clearBreakpointsIn(FreeOp* fop, JS::Realm* realm, Debugger* dbg,
                                 JSObject* handler);
  static void clearBreakpointsIn(FreeOp* fop, JSScript* script,
                                 Debugger* dbg, JSObject* handler);

#ifdef DEBUG
  static uint32_t getStepperCount(JSScript* script);
#endif

  /*
   * Increment or decrement the single-step count. If the count is non-zero
   * then the script is in single-step mode.
   *
   * Only incrementing is fallible, as it could allocate a DebugScript.
   */
  static bool incrementStepperCount(JSContext* cx, JSScript* script);
  static void decrementStepperCount(FreeOp* fop, JSScript* script);

  /*
   * Increment or decrement the generator observer count. If the count is
   * non-zero then the script reports resumptions to the debugger.
   *
   * Only incrementing is fallible, as it could allocate a DebugScript.
   */
  static bool incrementGeneratorObserverCount(JSContext* cx, JSScript* script);
  static void decrementGeneratorObserverCount(FreeOp* fop, JSScript* script);
};

} /* namespace js */

#endif /* dbg_DebugScript_h */
