/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "Ion.h"
#include "IonCompartment.h"
#include "jsinterp.h"
#include "ion/IonFrames.h"
#include "ion/IonFrames-inl.h" // for GetTopIonJSScript

#include "jsinterpinlines.h"

using namespace js;
using namespace js::ion;

namespace js {
namespace ion {

static inline bool
ShouldMonitorReturnType(JSFunction *fun)
{
    return fun->isInterpreted() &&
           (!fun->script()->hasAnalysis() ||
            !fun->script()->analysis()->ranInference());
}

bool
InvokeFunction(JSContext *cx, JSFunction *fun, uint32 argc, Value *argv, Value *rval)
{
    Value fval = ObjectValue(*fun);

    // In order to prevent massive bouncing between Ion and JM, see if we keep
    // hitting functions that are uncompilable.

    if (fun->script()->ion == ION_DISABLED_SCRIPT) {
        JSScript *script = cx->stack.currentScript();
        if (++script->ion->slowCallCount >= js_IonOptions.slowCallLimit) {
            Vector<types::RecompileInfo> scripts(cx);
            if (!scripts.append(types::RecompileInfo(script)))
                return false;

            Invalidate(cx->runtime->defaultFreeOp(), scripts);
            // Finally, poison the script so we don't try to run it again
            script->ion = ION_DISABLED_SCRIPT;
        }
    }


    // TI will return false for monitorReturnTypes, meaning there is no
    // TypeBarrier or Monitor instruction following this. However, we need to
    // explicitly monitor if the callee has not been analyzed yet. We special
    // case this to avoid the cost of ion::GetPcScript if we must take this
    // path frequently.
    bool needsMonitor = ShouldMonitorReturnType(fun);

    // Data in the argument vector is arranged for a JIT -> JIT call.
    Value thisv = argv[0];
    Value *argvWithoutThis = argv + 1;

    // Run the function in the interpreter.
    bool ok = Invoke(cx, thisv, fval, argc, argvWithoutThis, rval);
    if (ok && needsMonitor)
        types::TypeScript::Monitor(cx, *rval);

    return ok;
}

bool
InvokeConstructor(JSContext *cx, JSObject *obj, uint32 argc, Value *argv, Value *rval)
{
    Value fval = ObjectValue(*obj);

    // See the comment in InvokeFunction.
    bool needsMonitor = !obj->isFunction() || ShouldMonitorReturnType(obj->toFunction());

    // Data in the argument vector is arranged for a JIT -> JIT call.
    Value *argvWithoutThis = argv + 1;

    bool ok = js::InvokeConstructor(cx, fval, argc, argvWithoutThis, rval);
    if (ok && needsMonitor)
        types::TypeScript::Monitor(cx, *rval);

    return ok;
}

bool
ReportOverRecursed(JSContext *cx)
{
    JS_CHECK_RECURSION(cx, return false);

    if (cx->runtime->interrupt)
        return InterruptCheck(cx);

    js_ReportOverRecursed(cx);

    // Cause an InternalError.
    return false;
}

bool
DefVarOrConst(JSContext *cx, HandlePropertyName dn, unsigned attrs, HandleObject scopeChain)
{
    // Given the ScopeChain, extract the VarObj.
    RootedObject obj(cx, scopeChain);
    while (!obj->isVarObj())
        obj = obj->enclosingScope();

    return DefVarOrConstOperation(cx, obj, dn, attrs);
}

bool
InitProp(JSContext *cx, HandleObject obj, HandlePropertyName name, const Value &value)
{
    // Copy the incoming value. This may be overwritten; the return value is discarded.
    Value rval = value;
    RootedId id(cx, NameToId(name));

    if (name == cx->runtime->atomState.protoAtom)
        return baseops::SetPropertyHelper(cx, obj, id, 0, &rval, false);
    return !!DefineNativeProperty(cx, obj, id, rval, NULL, NULL, JSPROP_ENUMERATE, 0, 0, 0);
}

template<bool Equal>
bool
LooselyEqual(JSContext *cx, const Value &lhs, const Value &rhs, JSBool *res)
{
    bool equal;
    if (!js::LooselyEqual(cx, lhs, rhs, &equal))
        return false;
    *res = (equal == Equal);
    return true;
}

template bool LooselyEqual<true>(JSContext *cx, const Value &lhs, const Value &rhs, JSBool *res);
template bool LooselyEqual<false>(JSContext *cx, const Value &lhs, const Value &rhs, JSBool *res);

template<bool Equal>
bool
StrictlyEqual(JSContext *cx, const Value &lhs, const Value &rhs, JSBool *res)
{
    bool equal;
    if (!js::StrictlyEqual(cx, lhs, rhs, &equal))
        return false;
    *res = (equal == Equal);
    return true;
}

template bool StrictlyEqual<true>(JSContext *cx, const Value &lhs, const Value &rhs, JSBool *res);
template bool StrictlyEqual<false>(JSContext *cx, const Value &lhs, const Value &rhs, JSBool *res);

bool
LessThan(JSContext *cx, const Value &lhs, const Value &rhs, JSBool *res)
{
    bool cond;
    if (!LessThanOperation(cx, lhs, rhs, &cond))
        return false;
    *res = cond;
    return true;
}

bool
LessThanOrEqual(JSContext *cx, const Value &lhs, const Value &rhs, JSBool *res)
{
    bool cond;
    if (!LessThanOrEqualOperation(cx, lhs, rhs, &cond))
        return false;
    *res = cond;
    return true;
}

bool
GreaterThan(JSContext *cx, const Value &lhs, const Value &rhs, JSBool *res)
{
    bool cond;
    if (!GreaterThanOperation(cx, lhs, rhs, &cond))
        return false;
    *res = cond;
    return true;
}

bool
GreaterThanOrEqual(JSContext *cx, const Value &lhs, const Value &rhs, JSBool *res)
{
    bool cond;
    if (!GreaterThanOrEqualOperation(cx, lhs, rhs, &cond))
        return false;
    *res = cond;
    return true;
}

template<bool Equal>
bool
StringsEqual(JSContext *cx, HandleString lhs, HandleString rhs, JSBool *res)
{
    bool equal;
    if (!js::EqualStrings(cx, lhs, rhs, &equal))
        return false;
    *res = (equal == Equal);
    return true;
}

template bool StringsEqual<true>(JSContext *cx, HandleString lhs, HandleString rhs, JSBool *res);
template bool StringsEqual<false>(JSContext *cx, HandleString lhs, HandleString rhs, JSBool *res);

bool
ValueToBooleanComplement(JSContext *cx, const Value &input, JSBool *output)
{
    *output = !js_ValueToBoolean(input);
    return true;
}

bool
IteratorMore(JSContext *cx, HandleObject obj, JSBool *res)
{
    Value tmp;
    if (!js_IteratorMore(cx, obj, &tmp))
        return false;

    *res = tmp.toBoolean();
    return true;
}

JSObject*
NewInitArray(JSContext *cx, uint32_t count, types::TypeObject *type)
{
    JSObject *obj = NewDenseAllocatedArray(cx, count);
    if (!obj)
        return NULL;

    if (!type) {
        if (!obj->setSingletonType(cx))
            return NULL;

        types::TypeScript::Monitor(cx, ObjectValue(*obj));
    } else {
        obj->setType(type);
    }

    return obj;
}

JSObject*
NewInitObject(JSContext *cx, HandleObject baseObj, types::TypeObject *type)
{
    RootedObject obj(cx);
    if (baseObj.value()) {
        // JSOP_NEWOBJECT
        obj = CopyInitializerObject(cx, baseObj);
    } else {
        // JSOP_NEWINIT
        gc::AllocKind kind = GuessObjectGCKind(0);
        obj = NewBuiltinClassInstance(cx, &ObjectClass, kind);
    }

    if (!obj)
        return NULL;

    if (!type) {
        if (!obj->setSingletonType(cx))
            return NULL;

        types::TypeScript::Monitor(cx, ObjectValue(*obj));
    } else {
        obj->setType(type);
    }

    return obj;
}

bool
ArrayPopDense(JSContext *cx, JSObject *obj, Value *rval)
{
    AutoDetectInvalidation adi(cx, rval);

    Value argv[3] = { UndefinedValue(), ObjectValue(*obj) };
    if (!js::array_pop(cx, 0, argv))
        return false;

    // If the result is |undefined|, the array was probably empty and we
    // have to monitor the return value.
    *rval = argv[0];
    if (rval->isUndefined())
        types::TypeScript::Monitor(cx, *rval);
    return true;
}

bool
ArrayPushDense(JSContext *cx, JSObject *obj, const Value &v, uint32_t *length)
{
    JS_ASSERT(obj->isDenseArray());

    Value argv[3] = { UndefinedValue(), ObjectValue(*obj), v };
    if (!js::array_push(cx, 1, argv))
        return false;

    *length = argv[0].toInt32();
    return true;
}

bool
ArrayShiftDense(JSContext *cx, JSObject *obj, Value *rval)
{
    AutoDetectInvalidation adi(cx, rval);

    Value argv[3] = { UndefinedValue(), ObjectValue(*obj) };
    if (!js::array_shift(cx, 0, argv))
        return false;

    // If the result is |undefined|, the array was probably empty and we
    // have to monitor the return value.
    *rval = argv[0];
    if (rval->isUndefined())
        types::TypeScript::Monitor(cx, *rval);
    return true;
}

bool
SetProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, HandleValue value,
            bool strict, bool isSetName)
{
    Value v = value;
    RootedId id(cx, NameToId(name));

    if (JS_LIKELY(!obj->getOps()->setProperty)) {
        unsigned defineHow = isSetName ? DNP_UNQUALIFIED : 0;
        return baseops::SetPropertyHelper(cx, obj, id, defineHow, &v, strict);
    }

    return obj->setGeneric(cx, id, &v, strict);
}

bool
InterruptCheck(JSContext *cx)
{
    gc::MaybeVerifyBarriers(cx);

    return !!js_HandleExecutionInterrupt(cx);
}

HeapSlot *
NewSlots(JSRuntime *rt, unsigned nslots)
{
    JS_STATIC_ASSERT(sizeof(Value) == sizeof(HeapSlot));

    Value *slots = reinterpret_cast<Value *>(rt->malloc_(nslots * sizeof(Value)));
    if (!slots)
        return NULL;

    for (unsigned i = 0; i < nslots; i++)
        slots[i] = UndefinedValue();

    return reinterpret_cast<HeapSlot *>(slots);
}

JSObject *
NewCallObject(JSContext *cx, HandleShape shape, HandleTypeObject type, HeapSlot *slots,
              HandleObject global)
{
    return CallObject::create(cx, shape, type, slots, global);
}

} // namespace ion
} // namespace js
