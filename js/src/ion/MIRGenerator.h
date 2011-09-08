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

#ifndef jsion_mirgen_h__
#define jsion_mirgen_h__

// This file declares the data structures used to build a control-flow graph
// containing MIR.

#include "jscntxt.h"
#include "jscompartment.h"
#include "IonAllocPolicy.h"
#include "IonCompartment.h"

namespace js {
namespace ion {

class MBasicBlock;
class MIRGraph;
class MStart;

static inline uint32
CountArgSlots(JSFunction *fun)
{
    return fun ? fun->nargs + 1 : 0;
}

class MIRGenerator
{
  public:
    MIRGenerator(JSContext *cx, TempAllocator &temp, JSScript *script, JSFunction *fun,
                 MIRGraph &graph);

    TempAllocator &temp() {
        return temp_;
    }
    JSFunction *fun() const {
        return fun_;
    }
    uint32 nslots() const {
        return nslots_;
    }
    uint32 nargs() const {
        return fun()->nargs;
    }
    uint32 nlocals() const {
        return script->nfixed;
    }
    uint32 thisSlot() const {
        JS_ASSERT(fun());
        return 0;
    }
    uint32 firstArgSlot() const {
        JS_ASSERT(fun());
        return 1;
    }
    uint32 argSlot(uint32 i) const {
        return firstArgSlot() + i;
    }
    uint32 firstLocalSlot() const {
        return CountArgSlots(fun());
    }
    uint32 localSlot(uint32 i) const {
        return firstLocalSlot() + i;
    }
    uint32 firstStackSlot() const {
        return firstLocalSlot() + nlocals();
    }
    uint32 stackSlot(uint32 i) const {
        return firstStackSlot() + i;
    }
    MIRGraph &graph() {
        return graph_;
    }
    bool ensureBallast() {
        return temp().ensureBallast();
    }
    IonCompartment *ionCompartment() const {
        return cx->compartment->ionCompartment();
    }

    template <typename T>
    T * allocate(size_t count = 1)
    {
        return reinterpret_cast<T *>(temp().allocate(sizeof(T) * count));
    }

    // Set an error state and prints a message. Returns false so errors can be
    // propagated up.
    bool abort(const char *message, ...);

    bool errored() const {
        return error_;
    }

  public:
    JSScript *script;
    JSContext *cx;

  protected:
    jsbytecode *pc;
    TempAllocator &temp_;
    JSFunction *fun_;
    uint32 nslots_;
    MIRGraph &graph_;
    bool error_;
};

} // namespace ion
} // namespace js

#endif // jsion_mirgen_h__

