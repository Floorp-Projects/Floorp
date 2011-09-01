/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <dvander@alliedmods.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsion_bailouts_h__
#define jsion_bailouts_h__

#include "jstypes.h"

#if defined(JS_CPU_X86)
# include "ion/x86/Bailouts-x86.h"
#elif defined(JS_CPU_X64)
# include "ion/x64/Bailouts-x64.h"
#elif defined(JS_CPU_ARM)
# include "ion/arm/Bailouts-arm.h"
#else
# error "CPU!"
#endif

namespace js {
namespace ion {

// A "bailout" is a condition in which we need to recover an interpreter frame
// from an IonFrame. Bailouts can happen for the following reasons:
//   (1) A deoptimization guard, for example, an add overflows or a type check
//       fails.
//   (2) A check or assumption held by the JIT is invalidated by the VM, and
//       JIT code must be thrown away. This includes the GC possibly deciding
//       to evict live JIT code, or a Type Inference reflow.
//
// Note that bailouts as described here do not include normal Ion frame
// inspection, for example, if an exception must be built or the GC needs to
// scan an Ion frame for gcthings.
//
// The second type of bailout needs a different name - "deoptimization" or
// "deep bailout". Here we are concerned with eager (or maybe "shallow")
// bailouts, that happen from JIT code. These happen from guards, like:
//
//  cmp [obj + shape], 0x50M37TH1NG
//  jmp _bailout
//
// The bailout target needs to somehow translate the Ion frame (whose state
// will differ at each program point) to an interpreter frame. This state is
// captured into the IonScript's snapshot buffer, and for each bailout we know
// which snapshot corresponds to its state.
//
// Roughly, the following needs to happen at the bailout target.
//   (1) Move snapshot ID into a known stack location (registers cannot be
//       mutated).
//   (2) Spill all registers to the stack.
//   (3) Call a Bailout() routine, whose argument is the stack pointer.
//   (4) Bailout() will find the IonScript on the stack, use the snapshot ID
//       to find the structure of the frame, and then use the stack and spilled
//       registers to perform frame conversion.
//   (5) Bailout() returns, and the JIT must immediately return to the
//       interpreter (all frames are converted at once).
//
// (2) and (3) are implemented by a trampoline held in the compartment.
// Naively, we could implement (1) like:
//
//   _bailout_ID_1:
//     push 1
//     jmp _global_bailout_handler
//   _bailout_ID_2:
//     push 2
//     jmp _global_bailout_handler
//
// This takes about 10 extra bytes per guard. On some platforms, we can reduce
// this overhead to 4 bytes by creating a global jump table, shared again in
// the compartment:
//
//     call _global_bailout_handler
//     call _global_bailout_handler
//     call _global_bailout_handler
//     call _global_bailout_handler
//      ...
//    _global_bailout_handler:
//
// In the bailout handler, we can recompute which entry in the table was
// selected by subtracting the return addressed pushed by the call, from the
// start of the table, and then dividing by the size of a (call X) entry in the
// table. This gives us a number in [0, TableSize), which we call a
// "BailoutId".
//
// Then, we can provide a per-script mapping from BailoutIds to snapshots,
// which takes only four bytes per entry.
//
// This strategy does not work as given, because the bailout handler has no way
// to compute the location of an IonScript. Currently, we do not use frame
// pointers. To account for this we segregate frames into a limited set of
// "frame sizes", and create a table for each frame size. We also have the
// option of not using bailout tables, for platforms or situations where the
// 10 byte cost is more optimal than a bailout table. See IonFrames.h for more
// detail.

typedef uint32 BailoutId;
static const BailoutId INVALID_BAILOUT_ID = BailoutId(-1);

// Keep this arbitrarily small for now, for testing.
static const uint32 BAILOUT_TABLE_SIZE = 16;

// Bailout return codes.
static const uint32 BAILOUT_RETURN_OK = 0;
static const uint32 BAILOUT_RETURN_FATAL_ERROR = 1;

// Attached to the compartment for easy passing through from ::Bailout to
// ::ThunkToInterpreter.
class BailoutClosure
{
    BailoutFrameGuard bfg_;
    StackFrame *entryfp_;

  public:
    BailoutFrameGuard *frameGuard() {
        return &bfg_;
    }
    StackFrame *entryfp() const {
        return entryfp_;
    }
    void setEntryFrame(StackFrame *fp) {
        entryfp_ = fp;
    }
};

// Called from a bailout thunk. Returns a BAILOUT_* error code.
uint32 Bailout(void **esp);

// Called from a bailout thunk. Interprets the frame(s) that have been bailed
// out.
JSBool ThunkToInterpreter(IonFramePrefix *top, Value *vp);

// Called when an error occurs in Ion code. Normally, exceptions are bailouts,
// and pop the frame. This is called to propagate an exception through multiple
// frames. The return value is how much stack to adjust before returning.
uint32 HandleException(IonFramePrefix *top);

}
}

#endif // jsion_bailouts_h__

