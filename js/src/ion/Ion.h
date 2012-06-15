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
 *   David Anderson <danderson@mozilla.com>
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

#if !defined(jsion_ion_h__) && defined(JS_ION)
#define jsion_ion_h__

#include "jscntxt.h"
#include "IonCode.h"
#include "jsinfer.h"
#include "jsinterp.h"

namespace js {
namespace ion {

class TempAllocator;

struct IonOptions
{
    // Toggles whether global value numbering is used.
    //
    // Default: true
    bool gvn;

    // Toggles whether global value numbering is optimistic (true) or
    // pessimistic (false).
    //
    // Default: true
    bool gvnIsOptimistic;

    // Toggles whether loop invariant code motion is performed.
    //
    // Default: true
    bool licm;

    // Toggles whether functions may be entered at loop headers.
    //
    // Default: true
    bool osr;

    // Toggles whether large scripts are rejected.
    //
    // Default: true
    bool limitScriptSize;

    // Toggles whether Linear Scan Register Allocation is used. If LSRA is not
    // used, then Greedy Register Allocation is used instead.
    //
    // Default: true
    bool lsra;

    // Toggles whether inlining is performed.
    //
    // Default: true
    bool inlining;

    // Toggles whether Edge Case Analysis is used.
    //
    // Default: true
    bool edgeCaseAnalysis;

    // How many invocations or loop iterations are needed before functions
    // are compiled.
    //
    // Default: 10,240
    uint32 usesBeforeCompile;

    // How many invocations or loop iterations are needed before functions
    // are compiled when JM is disabled.
    //
    // Default: 40
    uint32 usesBeforeCompileNoJaeger;

    // How many invocations or loop iterations are needed before calls
    // are inlined.
    //
    // Default: 10,240
    uint32 usesBeforeInlining;

    // How many actual arguments are accepted on the C stack.
    //
    // Default: 4,096
    uint32 maxStackArgs;

    // The bytecode length limit for small function.
    //
    // The default for this was arrived at empirically via benchmarking.
    // We may want to tune it further after other optimizations have gone
    // in.
    //
    // Default: 100
    uint32 smallFunctionMaxBytecodeLength;

    // The inlining limit for small functions.
    //
    // This value has been arrived at empirically via benchmarking.
    // We may want to revisit this tuning after other optimizations have
    // gone in.
    //
    // Default: usesBeforeInlining / 4
    uint32 smallFunctionUsesBeforeInlining;

    void setEagerCompilation() {
        usesBeforeCompile = usesBeforeCompileNoJaeger = 0;

        // Eagerly inline calls to improve test coverage.
        usesBeforeInlining = 0;
    }

    IonOptions()
      : gvn(true),
        gvnIsOptimistic(true),
        licm(true),
        osr(true),
        limitScriptSize(true),
        lsra(true),
        inlining(true),
        edgeCaseAnalysis(true),
        usesBeforeCompile(10240),
        usesBeforeCompileNoJaeger(40),
        usesBeforeInlining(usesBeforeCompile),
        maxStackArgs(4096),
        smallFunctionMaxBytecodeLength(100),
        smallFunctionUsesBeforeInlining(usesBeforeInlining / 4)
    { }
};

enum MethodStatus
{
    Method_Error,
    Method_CantCompile,
    Method_Skipped,
    Method_Compiled
};

// An Ion context is needed to enter into either an Ion method or an instance
// of the Ion compiler. It points to a temporary allocator and the active
// JSContext.
class IonContext
{
  public:
    IonContext(JSContext *cx, TempAllocator *temp);
    ~IonContext();

    JSContext *cx;
    TempAllocator *temp;
    int getNextAssemblerId() {
        return assemblerCount_++;
    }
  private:
    IonContext *prev_;
    int assemblerCount_;
};

extern IonOptions js_IonOptions;

// Initialize Ion statically for all JSRuntimes.
bool InitializeIon();

// Get and set the current Ion context.
IonContext *GetIonContext();
bool SetIonContext(IonContext *ctx);

MethodStatus CanEnterAtBranch(JSContext *cx, JSScript *script,
                              StackFrame *fp, jsbytecode *pc);
MethodStatus CanEnter(JSContext *cx, JSScript *script, StackFrame *fp, bool newType);

enum IonExecStatus
{
    IonExec_Error,
    IonExec_Ok,
    IonExec_Bailout
};

IonExecStatus Cannon(JSContext *cx, StackFrame *fp);
IonExecStatus SideCannon(JSContext *cx, StackFrame *fp, jsbytecode *pc);

// Walk the stack and invalidate active Ion frames for the invalid scripts.
void Invalidate(FreeOp *fop, const Vector<types::RecompileInfo> &invalid, bool resetUses = true);
void MarkFromIon(JSCompartment *comp, Value *vp);

void ToggleBarriers(JSCompartment *comp, bool needs);

static inline bool IsEnabled(JSContext *cx)
{
    return cx->hasRunOption(JSOPTION_ION) && cx->typeInferenceEnabled();
}

} // namespace ion
} // namespace js

#endif // jsion_ion_h__

