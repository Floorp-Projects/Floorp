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

namespace js {
namespace ion {

class TempAllocator;

struct IonOptions
{
    // If Ion is supported, this toggles whether Ion is used.
    //
    // Default: false
    bool enabled;

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

    // Toggles whether Linear Scan Register Allocation is used. If LSRA is not
    // used, then Greedy Register Allocation is used instead.
    //
    // Default: true
    bool lsra;

    // Toggles whether inlining is performed.
    //
    // Default: true
    bool inlining;

    // How many invocations or loop iterations are needed before functions
    // are compiled.
    //
    // Default: 40.
    uint32 usesBeforeCompile;

    // How many invocations or loop iterations are needed before calls
    // are inlined.
    //
    // Default: 10,000
    uint32 usesBeforeInlining;

    void setEagerCompilation() {
        usesBeforeCompile = 0;

        // Eagerly inline calls to improve test coverage.
        usesBeforeInlining = usesBeforeCompile;
    }

    IonOptions()
      : enabled(false),
        gvn(true),
        gvnIsOptimistic(true),
        licm(true),
        osr(true),
        lsra(true),
        inlining(true),
        usesBeforeCompile(40),
        usesBeforeInlining(10000)
    { }
};

enum MethodStatus
{
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

  private:
    IonContext *prev_;
};

extern IonOptions js_IonOptions;

// Initialize Ion statically for all JSRuntimes.
bool InitializeIon();

// Get and set the current Ion context.
IonContext *GetIonContext();
bool SetIonContext(IonContext *ctx);

MethodStatus CanEnterAtBranch(JSContext *cx, JSScript *script,
                              StackFrame *fp, jsbytecode *pc);
MethodStatus Compile(JSContext *cx, JSScript *script,
                     js::StackFrame *fp, jsbytecode *osrPc);

bool Cannon(JSContext *cx, StackFrame *fp);
bool SideCannon(JSContext *cx, StackFrame *fp, jsbytecode *pc);

// Walk the stack and invalidate active Ion frames for the invalid scripts.
void Invalidate(JSContext *cx, const Vector<JSScript *> &invalid, bool resetUses = true);

static inline bool IsEnabled()
{
    return js_IonOptions.enabled;
}

} // namespace ion
} // namespace js

#endif // jsion_ion_h__

