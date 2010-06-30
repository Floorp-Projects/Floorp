/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *   David Mandelin <dmandelin@mozilla.com>
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

#define __STDC_LIMIT_MACROS

#include "jscntxt.h"
#include "jsscope.h"
#include "jsobj.h"
#include "jslibmath.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsxml.h"
#include "jsstaticcheck.h"
#include "jsbool.h"
#include "assembler/assembler/MacroAssemblerCodeRef.h"
#include "jsiter.h"
#include "jstypes.h"
#include "methodjit/StubCalls.h"
#include "jstracer.h"
#include "jspropertycache.h"
#include "jspropertycacheinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jsstrinlines.h"
#include "jsobjinlines.h"
#include "jscntxtinlines.h"
#include "jsatominlines.h"
#include "StubCalls-inl.h"
#ifdef XP_WIN
# include "jswin.h"
#endif

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;
using namespace JSC;

void JS_FASTCALL
mjit::stubs::BindName(VMFrame &f)
{
    PropertyCacheEntry *entry;

    /* Fast-path should have caught this. See comment in interpreter. */
    JS_ASSERT(f.fp->scopeChainObj()->getParent());

    JSAtom *atom;
    JSObject *obj2;
    JSContext *cx = f.cx;
    JSObject *obj = f.fp->scopeChainObj();
    JS_PROPERTY_CACHE(cx).test(cx, f.regs.pc, obj, obj2, entry, atom);
    if (atom) {
        jsid id = ATOM_TO_JSID(atom);
        obj = js_FindIdentifierBase(cx, f.fp->scopeChainObj(), id);
        if (!obj)
            THROW();
    }
    f.regs.sp++;
    f.regs.sp[-1].setObject(*obj);
}

JSObject * JS_FASTCALL
mjit::stubs::BindGlobalName(VMFrame &f)
{
    return f.fp->scopeChainObj()->getGlobal();
}

void JS_FASTCALL
mjit::stubs::SetName(VMFrame &f, JSAtom *origAtom)
{
    JSContext *cx = f.cx;

    Value &rref = f.regs.sp[-1];
    Value &lref = f.regs.sp[-2];
    JSObject *obj = ValueToObject(cx, &lref);
    if (!obj)
        THROW();

    do {
        PropertyCache *cache = &JS_PROPERTY_CACHE(cx);

        /*
         * Probe the property cache, specializing for two important
         * set-property cases. First:
         *
         *   function f(a, b, c) {
         *     var o = {p:a, q:b, r:c};
         *     return o;
         *   }
         *
         * or similar real-world cases, which evolve a newborn native
         * object predicatably through some bounded number of property
         * additions. And second:
         *
         *   o.p = x;
         *
         * in a frequently executed method or loop body, where p will
         * (possibly after the first iteration) always exist in native
         * object o.
         */
        PropertyCacheEntry *entry;
        JSObject *obj2;
        JSAtom *atom;
        if (cache->testForSet(cx, f.regs.pc, obj, &entry, &obj2, &atom)) {
            /*
             * Fast property cache hit, only partially confirmed by
             * testForSet. We know that the entry applies to regs.pc and
             * that obj's shape matches.
             *
             * The entry predicts either a new property to be added
             * directly to obj by this set, or on an existing "own"
             * property, or on a prototype property that has a setter.
             */
            JS_ASSERT(entry->vword.isSprop());
            JSScopeProperty *sprop = entry->vword.toSprop();
            JS_ASSERT_IF(sprop->isDataDescriptor(), sprop->writable());
            JS_ASSERT_IF(sprop->hasSlot(), entry->vcapTag() == 0);

            JSScope *scope = obj->scope();
            JS_ASSERT(!scope->sealed());

            /*
             * Fastest path: check whether the cached sprop is already
             * in scope and call NATIVE_SET and break to get out of the
             * do-while(0). But we can call NATIVE_SET only if obj owns
             * scope or sprop is shared.
             */
            bool checkForAdd;
            if (!sprop->hasSlot()) {
                if (entry->vcapTag() == 0 ||
                    ((obj2 = obj->getProto()) &&
                     obj2->isNative() &&
                     obj2->shape() == entry->vshape())) {
                    goto fast_set_propcache_hit;
                }

                /* The cache entry doesn't apply. vshape mismatch. */
                checkForAdd = false;
            } else if (!scope->isSharedEmpty()) {
                if (sprop == scope->lastProperty() || scope->hasProperty(sprop)) {
                  fast_set_propcache_hit:
                    PCMETER(cache->pchits++);
                    PCMETER(cache->setpchits++);
                    NATIVE_SET(cx, obj, sprop, entry, &rref);
                    break;
                }
                checkForAdd = sprop->hasSlot() && sprop->parent == scope->lastProperty();
            } else {
                /*
                 * We check that cx own obj here and will continue to
                 * own it after js_GetMutableScope returns so we can
                 * continue to skip JS_UNLOCK_OBJ calls.
                 */
                JS_ASSERT(CX_OWNS_OBJECT_TITLE(cx, obj));
                scope = js_GetMutableScope(cx, obj);
                JS_ASSERT(CX_OWNS_OBJECT_TITLE(cx, obj));
                if (!scope)
                    THROW();
                checkForAdd = !sprop->parent;
            }

            uint32 slot;
            if (checkForAdd &&
                entry->vshape() == cx->runtime->protoHazardShape &&
                sprop->hasDefaultSetter() &&
                (slot = sprop->slot) == scope->freeslot) {
                /*
                 * Fast path: adding a plain old property that was once
                 * at the frontier of the property tree, whose slot is
                 * next to claim among the allocated slots in obj,
                 * where scope->table has not been created yet.
                 *
                 * We may want to remove hazard conditions above and
                 * inline compensation code here, depending on
                 * real-world workloads.
                 */
                PCMETER(cache->pchits++);
                PCMETER(cache->addpchits++);

                /*
                 * Beware classes such as Function that use the
                 * reserveSlots hook to allocate a number of reserved
                 * slots that may vary with obj.
                 */
                if (slot < obj->numSlots() &&
                    !obj->getClass()->reserveSlots) {
                    ++scope->freeslot;
                } else {
                    if (!js_AllocSlot(cx, obj, &slot))
                        THROW();
                }

                /*
                 * If this obj's number of reserved slots differed, or
                 * if something created a hash table for scope, we must
                 * pay the price of JSScope::putProperty.
                 *
                 * (A reserveSlots hook can cause scopes of the same
                 * shape to have different freeslot values. This is
                 * what causes the slot != sprop->slot case. See
                 * js_GetMutableScope.)
                 */
                if (slot != sprop->slot || scope->table) {
                    JSScopeProperty *sprop2 =
                        scope->putProperty(cx, sprop->id,
                                           sprop->getter(), sprop->setter(),
                                           slot, sprop->attributes(),
                                           sprop->getFlags(), sprop->shortid);
                    if (!sprop2) {
                        js_FreeSlot(cx, obj, slot);
                        THROW();
                    }
                    sprop = sprop2;
                } else {
                    scope->extend(cx, sprop);
                }

                /*
                 * No method change check here because here we are
                 * adding a new property, not updating an existing
                 * slot's value that might contain a method of a
                 * branded scope.
                 */
                obj->lockedSetSlot(slot, rref);

                /*
                 * Purge the property cache of the id we may have just
                 * shadowed in obj's scope and proto chains. We do this
                 * after unlocking obj's scope to avoid lock nesting.
                 */
                js_PurgeScopeChain(cx, obj, sprop->id);
                break;
            }
            PCMETER(cache->setpcmisses++);
            atom = NULL;
        } else if (!atom) {
            /*
             * Slower property cache hit, fully confirmed by testForSet (in
             * the slow path, via fullTest).
             */
            JSScopeProperty *sprop = NULL;
            if (obj == obj2) {
                sprop = entry->vword.toSprop();
                JS_ASSERT(sprop->writable());
                JS_ASSERT(!obj2->scope()->sealed());
                NATIVE_SET(cx, obj, sprop, entry, &rref);
            }
            if (sprop)
                break;
        }

        if (!atom)
            atom = origAtom;
        jsid id = ATOM_TO_JSID(atom);
        if (entry && JS_LIKELY(obj->map->ops->setProperty == js_SetProperty)) {
            uintN defineHow;
            JSOp op = JSOp(*f.regs.pc);
            if (op == JSOP_SETMETHOD)
                defineHow = JSDNP_CACHE_RESULT | JSDNP_SET_METHOD;
            else if (op == JSOP_SETNAME)
                defineHow = JSDNP_CACHE_RESULT | JSDNP_UNQUALIFIED;
            else
                defineHow = JSDNP_CACHE_RESULT;
            if (!js_SetPropertyHelper(cx, obj, id, defineHow, &rref))
                THROW();
        } else {
            if (!obj->setProperty(cx, id, &rref))
                THROW();
        }
    } while (0);

    f.regs.sp[-2] = f.regs.sp[-1];
}

void JS_FASTCALL
stubs::SetGlobalName(VMFrame &f, JSAtom *atom)
{
    SetName(f, atom);
}

static void
ReportAtomNotDefined(JSContext *cx, JSAtom *atom)
{
    const char *printable = js_AtomToPrintableString(cx, atom);
    if (printable)
        js_ReportIsNotDefined(cx, printable);
}

static JSObject *
NameOp(VMFrame &f, JSObject *obj, bool callname = false)
{
    JSContext *cx = f.cx;

    JSScopeProperty *sprop;
    Value rval;

    PropertyCacheEntry *entry;
    JSObject *obj2;
    JSAtom *atom;
    JS_PROPERTY_CACHE(cx).test(cx, f.regs.pc, obj, obj2, entry, atom);
    if (!atom) {
        if (entry->vword.isFunObj()) {
            f.regs.sp++;
            f.regs.sp[-1].setObject(entry->vword.toFunObj());
        } else if (entry->vword.isSlot()) {
            uintN slot = entry->vword.toSlot();
            JS_ASSERT(slot < obj2->scope()->freeslot);
            f.regs.sp++;
            f.regs.sp[-1] = obj2->lockedGetSlot(slot);
        } else {
            JS_ASSERT(entry->vword.isSprop());
            sprop = entry->vword.toSprop();
            NATIVE_GET(cx, obj, obj2, sprop, JSGET_METHOD_BARRIER, &rval, return NULL);
            obj2->dropProperty(cx, (JSProperty *) sprop);
            f.regs.sp++;
            f.regs.sp[-1] = rval;
        }

        /*
         * Push results, same as below, but with a prop$ hit there is
         * no need to test for the unusual and uncacheable case where the caller
         * determines |this|.
         */
#if DEBUG
        Class *clasp;
        JS_ASSERT(!obj->getParent() ||
                  (clasp = obj->getClass()) == &js_CallClass ||
                  clasp == &js_BlockClass ||
                  clasp == &js_DeclEnvClass);
#endif
        if (callname) {
            f.regs.sp++;
            f.regs.sp[-1].setNull();
        }
        return obj;
    }

    jsid id;
    id = ATOM_TO_JSID(atom);
    JSProperty *prop;
    if (!js_FindPropertyHelper(cx, id, true, &obj, &obj2, &prop))
        return NULL;
    if (!prop) {
        /* Kludge to allow (typeof foo == "undefined") tests. */
        JSOp op2 = js_GetOpcode(cx, f.fp->script, f.regs.pc + JSOP_NAME_LENGTH);
        if (op2 == JSOP_TYPEOF) {
            f.regs.sp++;
            f.regs.sp[-1].setUndefined();
            return obj;
        }
        ReportAtomNotDefined(cx, atom);
        return NULL;
    }

    /* Take the slow path if prop was not found in a native object. */
    if (!obj->isNative() || !obj2->isNative()) {
        obj2->dropProperty(cx, prop);
        if (!obj->getProperty(cx, id, &rval))
            return NULL;
    } else {
        sprop = (JSScopeProperty *)prop;
        NATIVE_GET(cx, obj, obj2, sprop, JSGET_METHOD_BARRIER, &rval, return NULL);
        obj2->dropProperty(cx, (JSProperty *) sprop);
    }

    f.regs.sp++;
    f.regs.sp[-1] = rval;
    if (callname) {
        Class *clasp;
        JSObject *thisp = obj;
        if (!thisp->getParent() ||
            (clasp = thisp->getClass()) == &js_CallClass ||
            clasp == &js_BlockClass ||
            clasp == &js_DeclEnvClass) {
            thisp = NULL;
        } else {
            thisp = thisp->thisObject(cx);
            if (!thisp)
                return NULL;
        }
        f.regs.sp++;
        f.regs.sp[-1].setObjectOrNull(thisp);
    }
    return obj;
}

void JS_FASTCALL
stubs::Name(VMFrame &f)
{
    if (!NameOp(f, f.fp->scopeChainObj()))
        THROW();
}

void JS_FASTCALL
stubs::GetGlobalName(VMFrame &f)
{
    JSObject *globalObj = f.fp->scopeChainObj()->getGlobal();
    if (!NameOp(f, globalObj))
         THROW();
}

static inline bool
IteratorNext(JSContext *cx, JSObject *iterobj, Value *rval)
{
    if (iterobj->getClass() == &js_IteratorClass.base) {
        NativeIterator *ni = (NativeIterator *) iterobj->getPrivate();
        JS_ASSERT(ni->props_cursor < ni->props_end);
        if ((ni->flags & JSITER_FOREACH) == 0) {
            jsid id = ni->currentId();
            if (JSID_IS_ATOM(id)) {
                rval->setString(JSID_TO_STRING(id));
                ni->incIdCursor();
                return true;
            }
            /* Take the slow path if we have to stringify a numeric property name. */
        } else {
            *rval = ni->currentValue();
            ni->incValueCursor();
            return true;
        }
    }
    return js_IteratorNext(cx, iterobj, rval);
}

void JS_FASTCALL
stubs::ForName(VMFrame &f, JSAtom *atom)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    JS_ASSERT(regs.sp - 1 >= f.fp->base());
    jsid id = ATOM_TO_JSID(atom);
    JSObject *obj, *obj2;
    JSProperty *prop;
    if (!js_FindProperty(cx, id, &obj, &obj2, &prop))
        THROW();
    if (prop)
        obj2->dropProperty(cx, prop);
    {
        AutoValueRooter tvr(cx);
        JS_ASSERT(regs.sp[-1].isObject());
        if (!IteratorNext(cx, &regs.sp[-1].asObject(), tvr.addr()))
            THROW();
        if (!obj->setProperty(cx, id, tvr.addr()))
            THROW();
    }
}

void JS_FASTCALL
stubs::GetElem(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    Value lval = regs.sp[-2];
    Value rval = regs.sp[-1];
    const Value *copyFrom;

    JSObject *obj;
    jsid id;
    int i;

    if (lval.isString() && rval.isInt32()) {
        Value retval;
        JSString *str = lval.asString();
        i = rval.asInt32();

        if ((size_t)i >= str->length())
            THROW();

        str = JSString::getUnitString(cx, str, (size_t)i);
        if (!str)
            THROW();
        f.regs.sp[-2].setString(str);
        return;
    }

    obj = ValueToObject(cx, &lval);
    if (!obj)
        THROW();

    if (rval.isInt32()) {
        if (obj->isDenseArray()) {
            jsuint idx = jsuint(rval.asInt32());
            
            if (idx < obj->getArrayLength() &&
                idx < obj->getDenseArrayCapacity()) {
                copyFrom = obj->addressOfDenseArrayElement(idx);
                if (!copyFrom->isMagic())
                    goto end_getelem;
                /* Otherwise, fall to getProperty(). */
            }
        } else if (obj->isArguments()
#if 0 /* def JS_TRACER */
                   && !GetArgsPrivateNative(obj)
#endif
                  ) {
            uint32 arg = uint32(rval.asInt32());

            if (arg < obj->getArgsLength()) {
                JSStackFrame *afp = (JSStackFrame *) obj->getPrivate();
                if (afp) {
                    copyFrom = &afp->argv[arg];
                    goto end_getelem;
                }

                copyFrom = obj->addressOfArgsElement(arg);
                if (!copyFrom->isMagic())
                    goto end_getelem;
                /* Otherwise, fall to getProperty(). */
            }
        }
        id = INT_TO_JSID(rval.asInt32());

    } else {
        if (!js_InternNonIntElementId(cx, obj, rval, &id))
            THROW();
    }

    if (!obj->getProperty(cx, id, &rval))
        THROW();
    copyFrom = &rval;

  end_getelem:
    f.regs.sp[-2] = *copyFrom;
}

static inline bool
FetchElementId(VMFrame &f, JSObject *obj, const Value &idval, jsid &id, Value *vp)
{
    int32_t i_;
    if (ValueFitsInInt32(idval, &i_)) {
        id = INT_TO_JSID(i_);
        return true;
    }
    return !!js_InternNonIntElementId(f.cx, obj, idval, &id, vp);
}

void JS_FASTCALL
stubs::CallElem(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    /* Fetch the left part and resolve it to a non-null object. */
    JSObject *obj = ValueToObject(cx, &regs.sp[-2]);
    if (!obj)
        THROW();

    /* Fetch index and convert it to id suitable for use with obj. */
    jsid id;
    if (!FetchElementId(f, obj, regs.sp[-1], id, &regs.sp[-2]))
        THROW();

    /* Get or set the element. */
    if (!js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, &regs.sp[-2]))
        THROW();

#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(regs.sp[-2].isUndefined())) {
        regs.sp[-2] = regs.sp[-1];
        regs.sp[-1].setObject(*obj);
        if (!js_OnUnknownMethod(cx, regs.sp - 2))
            THROW();
    } else
#endif
    {
        regs.sp[-1].setObject(*obj);
    }
}

void JS_FASTCALL
stubs::SetElem(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    Value &objval = regs.sp[-3];
    Value &idval  = regs.sp[-2];
    Value retval  = regs.sp[-1];
    
    JSObject *obj;
    jsid id;

    obj = ValueToObject(cx, &objval);
    if (!obj)
        THROW();

    if (!FetchElementId(f, obj, idval, id, &regs.sp[-2]))
        THROW();

    if (obj->isDenseArray() && JSID_IS_INT(id)) {
        jsuint length = obj->getDenseArrayCapacity();
        jsint i = JSID_TO_INT(id);
        
        if ((jsuint)i < length) {
            if (obj->getDenseArrayElement(i).isMagic(JS_ARRAY_HOLE)) {
                if (js_PrototypeHasIndexedProperties(cx, obj))
                    goto mid_setelem;
                if ((jsuint)i >= obj->getArrayLength())
                    obj->setDenseArrayLength(i + 1);
                obj->incDenseArrayCountBy(1);
            }
            obj->setDenseArrayElement(i, regs.sp[-1]);
            goto end_setelem;
        }
    }

  mid_setelem:
    if (!obj->setProperty(cx, id, &regs.sp[-1]))
        THROW();

  end_setelem:
    /* :FIXME: Moving the assigned object into the lowest stack slot
     * is a temporary hack. What we actually want is an implementation
     * of popAfterSet() that allows popping more than one value;
     * this logic can then be handled in Compiler.cpp. */
    regs.sp[-3] = retval;
}

void JS_FASTCALL
stubs::CallName(VMFrame &f)
{
    JSObject *obj = NameOp(f, f.fp->scopeChainObj(), true);
    if (!obj)
        THROW();
}

void JS_FASTCALL
stubs::BitOr(VMFrame &f)
{
    int32_t i, j;

    if (!ValueToECMAInt32(f.cx, f.regs.sp[-2], &i) ||
        !ValueToECMAInt32(f.cx, f.regs.sp[-1], &j)) {
        THROW();
    }
    i = i | j;
    f.regs.sp[-2].setInt32(i);
}

void JS_FASTCALL
stubs::BitXor(VMFrame &f)
{
    int32_t i, j;

    if (!ValueToECMAInt32(f.cx, f.regs.sp[-2], &i) ||
        !ValueToECMAInt32(f.cx, f.regs.sp[-1], &j)) {
        THROW();
    }
    i = i ^ j;
    f.regs.sp[-2].setInt32(i);
}

void JS_FASTCALL
stubs::BitAnd(VMFrame &f)
{
    int32_t i, j;

    if (!ValueToECMAInt32(f.cx, f.regs.sp[-2], &i) ||
        !ValueToECMAInt32(f.cx, f.regs.sp[-1], &j)) {
        THROW();
    }
    i = i & j;
    f.regs.sp[-2].setInt32(i);
}

void JS_FASTCALL
stubs::BitNot(VMFrame &f)
{
    int32_t i;

    if (!ValueToECMAInt32(f.cx, f.regs.sp[-1], &i))
        THROW();
    i = ~i;
    f.regs.sp[-1].setInt32(i);
}

void JS_FASTCALL
stubs::Lsh(VMFrame &f)
{
    int32_t i, j;
    if (!ValueToECMAInt32(f.cx, f.regs.sp[-2], &i))
        THROW();
    if (!ValueToECMAInt32(f.cx, f.regs.sp[-1], &j))
        THROW();
    i = i << (j & 31);
    f.regs.sp[-2].setInt32(i);
}

void JS_FASTCALL
stubs::Rsh(VMFrame &f)
{
    int32_t i, j;
    if (!ValueToECMAInt32(f.cx, f.regs.sp[-2], &i))
        THROW();
    if (!ValueToECMAInt32(f.cx, f.regs.sp[-1], &j))
        THROW();
    i = i >> (j & 31);
    f.regs.sp[-2].setInt32(i);
}

void JS_FASTCALL
stubs::Ursh(VMFrame &f)
{
    uint32_t u;
    if (!ValueToECMAUint32(f.cx, f.regs.sp[-2], &u))
        THROW();
    int32_t j;
    if (!ValueToECMAInt32(f.cx, f.regs.sp[-1], &j))
        THROW();

    u >>= (j & 31);

	f.regs.sp[-2].setNumber(uint32(u));
}

template <int32 N>
static inline bool
PostInc(VMFrame &f, Value *vp)
{
    double d;
    if (!ValueToNumber(f.cx, *vp, &d))
        return false;
    f.regs.sp++;
    f.regs.sp[-1].setDouble(d);
    d += N;
    vp->setDouble(d);
    return true;
}

template <int32 N>
static inline bool
PreInc(VMFrame &f, Value *vp)
{
    double d;
    if (!ValueToNumber(f.cx, *vp, &d))
        return false;
    d += N;
    vp->setDouble(d);
    f.regs.sp++;
    f.regs.sp[-1].setDouble(d);
    return true;
}

void JS_FASTCALL
stubs::VpInc(VMFrame &f, Value *vp)
{
    if (!PostInc<1>(f, vp))
        THROW();
}

void JS_FASTCALL
stubs::VpDec(VMFrame &f, Value *vp)
{
    if (!PostInc<-1>(f, vp))
        THROW();
}

void JS_FASTCALL
stubs::DecVp(VMFrame &f, Value *vp)
{
    if (!PreInc<-1>(f, vp))
        THROW();
}

void JS_FASTCALL
stubs::IncVp(VMFrame &f, Value *vp)
{
    if (!PreInc<1>(f, vp))
        THROW();
}

void JS_FASTCALL
stubs::DefFun(VMFrame &f, uint32 index)
{
    bool doSet;
    JSObject *pobj, *obj2;
    JSProperty *prop;
    uint32 old;

    JSContext *cx = f.cx;
    JSStackFrame *fp = f.fp;
    JSScript *script = fp->script;

    /*
     * A top-level function defined in Global or Eval code (see ECMA-262
     * Ed. 3), or else a SpiderMonkey extension: a named function statement in
     * a compound statement (not at the top statement level of global code, or
     * at the top level of a function body).
     */
    JSFunction *fun = script->getFunction(index);
    JSObject *obj = FUN_OBJECT(fun);

    if (FUN_NULL_CLOSURE(fun)) {
        /*
         * Even a null closure needs a parent for principals finding.
         * FIXME: bug 476950, although debugger users may also demand some kind
         * of scope link for debugger-assisted eval-in-frame.
         */
        obj2 = fp->scopeChainObj();
    } else {
        JS_ASSERT(!FUN_FLAT_CLOSURE(fun));

        /*
         * Inline js_GetScopeChain a bit to optimize for the case of a
         * top-level function.
         */
        if (!fp->blockChain) {
            obj2 = fp->scopeChainObj();
        } else {
            obj2 = js_GetScopeChain(cx, fp);
            if (!obj2)
                THROW();
        }
    }

    /*
     * If static link is not current scope, clone fun's object to link to the
     * current scope via parent. We do this to enable sharing of compiled
     * functions among multiple equivalent scopes, amortizing the cost of
     * compilation over a number of executions.  Examples include XUL scripts
     * and event handlers shared among Firefox or other Mozilla app chrome
     * windows, and user-defined JS functions precompiled and then shared among
     * requests in server-side JS.
     */
    if (obj->getParent() != obj2) {
        obj = CloneFunctionObject(cx, fun, obj2);
        if (!obj)
            THROW();
    }

    /*
     * Protect obj from any GC hiding below JSObject::setProperty or
     * JSObject::defineProperty.  All paths from here must flow through the
     * fp->scopeChain code below the parent->defineProperty call.
     */
    MUST_FLOW_THROUGH("restore_scope");
    fp->setScopeChainObj(obj);

    Value rval;
    rval.setObject(*obj);

    /*
     * ECMA requires functions defined when entering Eval code to be
     * impermanent.
     */
    uintN attrs;
    attrs = (fp->flags & JSFRAME_EVAL)
            ? JSPROP_ENUMERATE
            : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    /*
     * Load function flags that are also property attributes.  Getters and
     * setters do not need a slot, their value is stored elsewhere in the
     * property itself, not in obj slots.
     */
    PropertyOp getter, setter;
    uintN flags;
    
    getter = setter = PropertyStub;
    flags = JSFUN_GSFLAG2ATTR(fun->flags);
    if (flags) {
        /* Function cannot be both getter a setter. */
        JS_ASSERT(flags == JSPROP_GETTER || flags == JSPROP_SETTER);
        attrs |= flags | JSPROP_SHARED;
        rval.setUndefined();
        if (flags == JSPROP_GETTER)
            getter = CastAsPropertyOp(obj);
        else
            setter = CastAsPropertyOp(obj);
    }

    jsid id;
    JSBool ok;
    JSObject *parent;

    /*
     * We define the function as a property of the variable object and not the
     * current scope chain even for the case of function expression statements
     * and functions defined by eval inside let or with blocks.
     */
    parent = fp->varobj(cx);
    JS_ASSERT(parent);

    /*
     * Check for a const property of the same name -- or any kind of property
     * if executing with the strict option.  We check here at runtime as well
     * as at compile-time, to handle eval as well as multiple HTML script tags.
     */
    id = ATOM_TO_JSID(fun->atom);
    prop = NULL;
    ok = CheckRedeclaration(cx, parent, id, attrs, &pobj, &prop);
    if (!ok)
        goto restore_scope;

    /*
     * We deviate from 10.1.2 in ECMA 262 v3 and under eval use for function
     * declarations JSObject::setProperty, not JSObject::defineProperty, to
     * preserve the JSOP_PERMANENT attribute of existing properties and make
     * sure that such properties cannot be deleted.
     *
     * We also use JSObject::setProperty for the existing properties of Call
     * objects with matching attributes to preserve the native getters and
     * setters that store the value of the property in the interpreter frame,
     * see bug 467495.
     */
    doSet = (attrs == JSPROP_ENUMERATE);
    JS_ASSERT_IF(doSet, fp->flags & JSFRAME_EVAL);
    if (prop) {
        if (parent == pobj &&
            parent->getClass() == &js_CallClass &&
            (old = ((JSScopeProperty *) prop)->attributes(),
             !(old & (JSPROP_GETTER|JSPROP_SETTER)) &&
             (old & (JSPROP_ENUMERATE|JSPROP_PERMANENT)) == attrs)) {
            /*
             * js_CheckRedeclaration must reject attempts to add a getter or
             * setter to an existing property without a getter or setter.
             */
            JS_ASSERT(!(attrs & ~(JSPROP_ENUMERATE|JSPROP_PERMANENT)));
            JS_ASSERT(!(old & JSPROP_READONLY));
            doSet = JS_TRUE;
        }
        pobj->dropProperty(cx, prop);
    }
    ok = doSet
         ? parent->setProperty(cx, id, &rval)
         : parent->defineProperty(cx, id, rval, getter, setter, attrs);

  restore_scope:
    /* Restore fp->scopeChain now that obj is defined in fp->callobj. */
    fp->setScopeChainObj(obj2);
    if (!ok)
        THROW();
}

#define DEFAULT_VALUE(cx, n, hint, v)                                         \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(v.isObject());                                              \
        JS_ASSERT(v == regs.sp[n]);                                           \
        if (!v.asObject().defaultValue(cx, hint, &regs.sp[n]))                \
            THROWV(JS_FALSE);                                                 \
        v = regs.sp[n];                                                       \
    JS_END_MACRO

#define RELATIONAL(OP)                                                        \
    JS_BEGIN_MACRO                                                            \
        JSContext *cx = f.cx;                                                 \
        JSFrameRegs &regs = f.regs;                                           \
        Value rval = regs.sp[-1];                                             \
        Value lval = regs.sp[-2];                                             \
        bool cond;                                                            \
        if (lval.isObject())                                                  \
            DEFAULT_VALUE(cx, -2, JSTYPE_NUMBER, lval);                       \
        if (rval.isObject())                                                  \
            DEFAULT_VALUE(cx, -1, JSTYPE_NUMBER, rval);                       \
        if (lval.isString() && rval.isString()) {                             \
            JSString *l = lval.asString(), *r = rval.asString();              \
            cond = js_CompareStrings(l, r) OP 0;                              \
        } else {                                                              \
            double l, r;                                                      \
            if (!ValueToNumber(cx, lval, &l) ||                               \
                !ValueToNumber(cx, rval, &r)) {                               \
                THROWV(JS_FALSE);                                             \
            }                                                                 \
            cond = JSDOUBLE_COMPARE(l, OP, r, false);                         \
        }                                                                     \
        regs.sp[-2].setBoolean(cond);                                         \
        return cond;                                                          \
    JS_END_MACRO

JSBool JS_FASTCALL
stubs::LessThan(VMFrame &f)
{
    RELATIONAL(<);
}

JSBool JS_FASTCALL
stubs::LessEqual(VMFrame &f)
{
    RELATIONAL(<=);
}

JSBool JS_FASTCALL
stubs::GreaterThan(VMFrame &f)
{
    RELATIONAL(>);
}

JSBool JS_FASTCALL
stubs::GreaterEqual(VMFrame &f)
{
    RELATIONAL(>=);
}

JSBool JS_FASTCALL
stubs::ValueToBoolean(VMFrame &f)
{
    return js_ValueToBoolean(f.regs.sp[-1]);
}

void JS_FASTCALL
stubs::Not(VMFrame &f)
{
    JSBool b = !js_ValueToBoolean(f.regs.sp[-1]);
    f.regs.sp[-1].setBoolean(b);
}

template <JSBool EQ, bool IFNAN>
static inline bool
StubEqualityOp(VMFrame &f)
{
    Class *clasp;
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    Value rval = regs.sp[-1];
    Value lval = regs.sp[-2];

    JSBool cond;

#if JS_HAS_XML_SUPPORT
    if ((lval.isObject() && lval.asObject().isXML()) ||
        (rval.isObject() && rval.asObject().isXML())) {
        if (!js_TestXMLEquality(cx, lval, rval, &cond))
            return false;
        cond = cond == EQ;
    } else
#endif

    if (SameType(lval, rval)) {
        if (lval.isString()) {
            JSString *l = lval.asString();
            JSString *r = rval.asString();
            cond = js_EqualStrings(l, r) == EQ;
        } else if (lval.isDouble()) {
            double l = lval.asDouble();
            double r = rval.asDouble();
            if (EQ)
                cond = JSDOUBLE_COMPARE(l, ==, r, IFNAN);
            else
                cond = JSDOUBLE_COMPARE(l, !=, r, IFNAN);
        } else if (lval.isObject()) {
            JSObject *l = &lval.asObject(), *r = &rval.asObject();
            if (((clasp = l->getClass())->flags & JSCLASS_IS_EXTENDED) &&
                ((ExtendedClass  *)clasp)->equality) {
                if (!((ExtendedClass *)clasp)->equality(cx, l, &rval, &cond))
                    return false;
                cond = cond == EQ;
            } else {
                cond = (l == r) == EQ;
            }
        } else {
            cond = (lval.asRawUint32() == rval.asRawUint32()) == EQ;
        }
    } else {
        if (lval.isNullOrUndefined()) {
            cond = rval.isNullOrUndefined() == EQ;
        } else if (rval.isNullOrUndefined()) {
            cond = !EQ;
        } else {
            if (lval.isObject()) {
                if (!lval.asObject().defaultValue(cx, JSTYPE_VOID, &regs.sp[-2]))
                    return false;
                lval = regs.sp[-2];
            }

            if (rval.isObject()) {
                if (!rval.asObject().defaultValue(cx, JSTYPE_VOID, &regs.sp[-1]))
                    return false;
                rval = regs.sp[-1];
            }

            if (lval.isString() && rval.isString()) {
                JSString *l = lval.asString();
                JSString *r = rval.asString();
                cond = js_EqualStrings(l, r) == EQ;
            } else {
                double l, r;
                if (!ValueToNumber(cx, lval, &l) ||
                    !ValueToNumber(cx, rval, &r)) {
                    return false;
                }
                
                if (EQ)
                    cond = JSDOUBLE_COMPARE(l, ==, r, false);
                else
                    cond = JSDOUBLE_COMPARE(l, !=, r, true);
            }
        }
    }

    regs.sp[-2].setBoolean(cond);
    return true;
}

JSBool JS_FASTCALL
stubs::Equal(VMFrame &f)
{
    if (!StubEqualityOp<JS_TRUE, false>(f))
        THROWV(JS_FALSE);
    return f.regs.sp[-2].asBoolean();
}

JSBool JS_FASTCALL
stubs::NotEqual(VMFrame &f)
{
    if (!StubEqualityOp<JS_FALSE, true>(f))
        THROWV(JS_FALSE);
    return f.regs.sp[-2].asBoolean();
}

static inline bool
DefaultValue(VMFrame &f, JSType hint, Value &v, int n)
{
    JS_ASSERT(v.isObject());
    if (!v.asObject().defaultValue(f.cx, hint, &f.regs.sp[n]))
        return false;
    v = f.regs.sp[n];
    return true;
}

void JS_FASTCALL
stubs::Add(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;
    Value rval = regs.sp[-1];
    Value lval = regs.sp[-2];

    if (lval.isInt32() && rval.isInt32()) {
        int32_t l = lval.asInt32(), r = rval.asInt32();
        int32_t sum = l + r;
        regs.sp--;
        if (JS_UNLIKELY(bool((l ^ sum) & (r ^ sum) & 0x80000000)))
            regs.sp[-1].setDouble(double(l) + double(r));
        else
            regs.sp[-1].setInt32(sum);
    } else
#if JS_HAS_XML_SUPPORT
    if (lval.isObject() && lval.asObject().isXML() &&
        rval.isObject() && rval.asObject().isXML()) {
        if (!js_ConcatenateXML(cx, &lval.asObject(), &rval.asObject(), &rval))
            THROW();
        regs.sp--;
        regs.sp[-1] = rval;
    } else
#endif
    {
        if (lval.isObject() && !DefaultValue(f, JSTYPE_VOID, lval, -2))
            THROW();
        if (rval.isObject() && !DefaultValue(f, JSTYPE_VOID, rval, -1))
            THROW();
        bool lIsString, rIsString;
        if ((lIsString = lval.isString()) | (rIsString = rval.isString())) {
            JSString *lstr, *rstr;
            if (lIsString) {
                lstr = lval.asString();
            } else {
                lstr = js_ValueToString(cx, lval);
                if (!lstr)
                    THROW();
                regs.sp[-2].setString(lstr);
            }
            if (rIsString) {
                rstr = rval.asString();
            } else {
                rstr = js_ValueToString(cx, rval);
                if (!rstr)
                    THROW();
                regs.sp[-1].setString(rstr);
            }
            JSString *str = js_ConcatStrings(cx, lstr, rstr);
            if (!str)
                THROW();
            regs.sp--;
            regs.sp[-1].setString(str);
        } else {
            double l, r;
            if (!ValueToNumber(cx, lval, &l) || !ValueToNumber(cx, rval, &r))
                THROW();
            l += r;
            regs.sp--;
            regs.sp[-1].setNumber(l);
        }
    }
}


void JS_FASTCALL
stubs::Sub(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;
    double d1, d2;
    if (!ValueToNumber(cx, regs.sp[-2], &d1) ||
        !ValueToNumber(cx, regs.sp[-1], &d2)) {
        THROW();
    }
    double d = d1 - d2;
    regs.sp[-2].setNumber(d);
}

void JS_FASTCALL
stubs::Mul(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;
    double d1, d2;
    if (!ValueToNumber(cx, regs.sp[-2], &d1) ||
        !ValueToNumber(cx, regs.sp[-1], &d2)) {
        THROW();
    }
    double d = d1 * d2;
    regs.sp[-2].setNumber(d);
}

void JS_FASTCALL
stubs::Div(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSRuntime *rt = cx->runtime;
    JSFrameRegs &regs = f.regs;

    double d1, d2;
    if (!ValueToNumber(cx, regs.sp[-2], &d1) ||
        !ValueToNumber(cx, regs.sp[-1], &d2)) {
        THROW();
    }
    if (d2 == 0) {
        const Value *vp;
#ifdef XP_WIN
        /* XXX MSVC miscompiles such that (NaN == 0) */
        if (JSDOUBLE_IS_NaN(d2))
            vp = &rt->NaNValue;
        else
#endif
        if (d1 == 0 || JSDOUBLE_IS_NaN(d1))
            vp = &rt->NaNValue;
        else if (JSDOUBLE_IS_NEG(d1) != JSDOUBLE_IS_NEG(d2))
            vp = &rt->negativeInfinityValue;
        else
            vp = &rt->positiveInfinityValue;
        regs.sp[-2] = *vp;
    } else {
        d1 /= d2;
        regs.sp[-2].setNumber(d1);
    }
}

void JS_FASTCALL
stubs::Mod(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    int32_t l, r;
    if (lref.isInt32() && rref.isInt32() &&
        (l = lref.asInt32()) >= 0 && (r = rref.asInt32()) > 0) {
        int32_t mod = l % r;
        regs.sp[-2].setInt32(mod);
    } else {
        double d1, d2;
        if (!ValueToNumber(cx, regs.sp[-2], &d1) ||
            !ValueToNumber(cx, regs.sp[-1], &d2)) {
            THROW();
        }
        if (d2 == 0) {
            regs.sp[-2].setDouble(js_NaN);
        } else {
            d1 = js_fmod(d1, d2);
            regs.sp[-2].setDouble(d1);
        }
    }
}

JSObject *JS_FASTCALL
stubs::NewArray(VMFrame &f, uint32 len)
{
    JSObject *obj = js_NewArrayObject(f.cx, len, f.regs.sp - len, JS_TRUE);
    if (!obj)
        THROWV(NULL);
    return obj;
}

void JS_FASTCALL
stubs::Interrupt(VMFrame &f)
{
    if (!js_HandleExecutionInterrupt(f.cx)) {
        THROW();
    }
}

void JS_FASTCALL
stubs::This(VMFrame &f)
{
    if (!f.fp->getThisObject(f.cx))
        THROW();
    f.regs.sp[-1] = f.fp->thisv;
}

void JS_FASTCALL
stubs::ComputeThis(VMFrame &f)
{
    if (!f.fp->getThisObject(f.cx))
        THROW();
}

void JS_FASTCALL
stubs::Neg(VMFrame &f)
{
    double d;
    if (!ValueToNumber(f.cx, f.regs.sp[-1], &d))
        THROW();
    d = -d;
    f.regs.sp[-1].setNumber(d);
}

void JS_FASTCALL
stubs::ObjToStr(VMFrame &f)
{
    const Value &ref = f.regs.sp[-1];
    if (ref.isObject()) {
        JSString *str = js_ValueToString(f.cx, ref);
        if (!str)
            THROW();
        f.regs.sp[-1].setString(str);
    }
}

JSObject * JS_FASTCALL
stubs::NewInitArray(VMFrame &f)
{
    JSObject *obj = js_NewArrayObject(f.cx, 0, NULL);
    if (!obj)
        THROWV(NULL);
    return obj;
}

JSObject * JS_FASTCALL
stubs::NewInitObject(VMFrame &f, uint32 empty)
{
    JSContext *cx = f.cx;

    JSObject *obj = NewObject(cx, &js_ObjectClass, NULL, NULL);
    if (!obj)
        THROWV(NULL);

    if (!empty) {
        JS_LOCK_OBJ(cx, obj);
        JSScope *scope = js_GetMutableScope(cx, obj);
        if (!scope) {
            JS_UNLOCK_OBJ(cx, obj);
            THROWV(NULL);
        }

        /*
         * We cannot assume that js_GetMutableScope above creates a scope
         * owned by cx and skip JS_UNLOCK_SCOPE. A new object debugger
         * hook may add properties to the newly created object, suspend
         * the current request and share the object with other threads.
         */
        JS_UNLOCK_SCOPE(cx, scope);
    }

    return obj;
}

void JS_FASTCALL
stubs::InitElem(VMFrame &f, uint32 last)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    /* Pop the element's value into rval. */
    JS_ASSERT(regs.sp - f.fp->base() >= 3);
    const Value &rref = regs.sp[-1];

    /* Find the object being initialized at top of stack. */
    const Value &lref = regs.sp[-3];
    JS_ASSERT(lref.isObject());
    JSObject *obj = &lref.asObject();

    /* Fetch id now that we have obj. */
    jsid id;
    const Value &idval = regs.sp[-2];
    if (!FetchElementId(f, obj, idval, id, &regs.sp[-2]))
        THROW();

    /*
     * Check for property redeclaration strict warning (we may be in an object
     * initialiser, not an array initialiser).
     */
    if (!CheckRedeclaration(cx, obj, id, JSPROP_INITIALIZER, NULL, NULL))
        THROW();

    /*
     * If rref is a hole, do not call JSObject::defineProperty. In this case,
     * obj must be an array, so if the current op is the last element
     * initialiser, set the array length to one greater than id.
     */
    if (rref.isMagic(JS_ARRAY_HOLE)) {
        JS_ASSERT(obj->isArray());
        JS_ASSERT(JSID_IS_INT(id));
        JS_ASSERT(jsuint(JSID_TO_INT(id)) < JS_ARGS_LENGTH_MAX);
        if (last && !js_SetLengthProperty(cx, obj, (jsuint) (JSID_TO_INT(id) + 1)))
            THROW();
    } else {
        if (!obj->defineProperty(cx, id, rref, NULL, NULL, JSPROP_ENUMERATE))
            THROW();
    }
}

void JS_FASTCALL
stubs::GetUpvar(VMFrame &f, uint32 cookie)
{
    /* :FIXME: We can do better, this stub isn't needed. */
    uint32 staticLevel = f.fp->script->staticLevel;
    f.regs.sp[0] = js_GetUpvar(f.cx, staticLevel, cookie);
}

JSObject * JS_FASTCALL
stubs::DefLocalFun(VMFrame &f, JSFunction *fun)
{
    /*
     * Define a local function (i.e., one nested at the top level of another
     * function), parented by the current scope chain, stored in a local
     * variable slot that the compiler allocated.  This is an optimization over
     * JSOP_DEFFUN that avoids requiring a call object for the outer function's
     * activation.
     */
    JS_ASSERT(fun->isInterpreted());
    JS_ASSERT(!FUN_FLAT_CLOSURE(fun));
    JSObject *obj = FUN_OBJECT(fun);

    if (FUN_NULL_CLOSURE(fun)) {
        obj = CloneFunctionObject(f.cx, fun, f.fp->scopeChainObj());
        if (!obj)
            THROWV(NULL);
    } else {
        JSObject *parent = js_GetScopeChain(f.cx, f.fp);
        if (!parent)
            THROWV(NULL);

        if (obj->getParent() != parent) {
            obj = CloneFunctionObject(f.cx, fun, parent);
            if (!obj)
                THROWV(NULL);
        }
    }

    return obj;
}

JSObject * JS_FASTCALL
stubs::RegExp(VMFrame &f, JSObject *regex)
{
    /*
     * Push a regexp object cloned from the regexp literal object mapped by the
     * bytecode at pc. ES5 finally fixed this bad old ES3 design flaw which was
     * flouted by many browser-based implementations.
     *
     * We avoid the js_GetScopeChain call here and pass fp->scopeChain as
     * js_GetClassPrototype uses the latter only to locate the global.
     */
    JSObject *proto;
    if (!js_GetClassPrototype(f.cx, f.fp->scopeChainObj(), JSProto_RegExp, &proto))
        THROWV(NULL);
    JS_ASSERT(proto);
    JSObject *obj = js_CloneRegExpObject(f.cx, regex, proto);
    if (!obj)
        THROWV(NULL);
    return obj;
}

JSObject * JS_FASTCALL
stubs::Lambda(VMFrame &f, JSFunction *fun)
{
    JSObject *obj = FUN_OBJECT(fun);

    JSObject *parent;
    if (FUN_NULL_CLOSURE(fun)) {
        parent = f.fp->scopeChainObj();
    } else {
        parent = js_GetScopeChain(f.cx, f.fp);
        if (!parent)
            THROWV(NULL);
    }

    obj = CloneFunctionObject(f.cx, fun, parent);
    if (!obj)
        THROWV(NULL);

    return obj;
}

/* Test whether v is an int in the range [-2^31 + 1, 2^31 - 2] */
static JS_ALWAYS_INLINE bool
CanIncDecWithoutOverflow(int32_t i)
{
    return (i > JSVAL_INT_MIN) && (i < JSVAL_INT_MAX);
}

template <int32 N, bool POST>
static inline bool
ObjIncOp(VMFrame &f, JSObject *obj, jsid id)
{
    JSContext *cx = f.cx;
    JSStackFrame *fp = f.fp;

    f.regs.sp[0].setNull();
    f.regs.sp++;
    if (!obj->getProperty(cx, id, &f.regs.sp[-1]))
        return false;

    Value &ref = f.regs.sp[-1];
    int32_t tmp;
    if (JS_LIKELY(ref.isInt32() && CanIncDecWithoutOverflow(tmp = ref.asInt32()))) {
        if (POST)
            ref.asInt32Ref() = tmp + N;
        else
            ref.asInt32Ref() = tmp += N;
        fp->flags |= JSFRAME_ASSIGNING;
        JSBool ok = obj->setProperty(cx, id, &ref);
        fp->flags &= ~JSFRAME_ASSIGNING;
        if (!ok)
            return false;

        /*
         * We must set regs.sp[-1] to tmp for both post and pre increments
         * as the setter overwrites regs.sp[-1].
         */
        ref.setInt32(tmp);
    } else {
        Value v;
        double d;
        if (!ValueToNumber(cx, ref, &d))
            return false;
        if (POST) {
            ref.setDouble(d);
            d += N;
        } else {
            d += N;
            ref.setDouble(d);
        }
        v.setDouble(d);
        fp->flags |= JSFRAME_ASSIGNING;
        JSBool ok = obj->setProperty(cx, id, &v);
        fp->flags &= ~JSFRAME_ASSIGNING;
        if (!ok)
            return false;
    }

    return true;
}

template <int32 N, bool POST>
static inline bool
NameIncDec(VMFrame &f, JSObject *obj, JSAtom *origAtom)
{
    JSContext *cx = f.cx;

    JSAtom *atom;
    JSObject *obj2;
    JSProperty *prop;
    PropertyCacheEntry *entry;
    JS_PROPERTY_CACHE(cx).test(cx, f.regs.pc, obj, obj2, entry, atom);
    if (!atom) {
        if (obj == obj2 && entry->vword.isSlot()) {
            uint32 slot = entry->vword.toSlot();
            JS_ASSERT(slot < obj->scope()->freeslot);
            Value &rref = obj->getSlotRef(slot);
            int32_t tmp;
            if (JS_LIKELY(rref.isInt32() && CanIncDecWithoutOverflow(tmp = rref.asInt32()))) {
                int32_t inc = tmp + N;
                if (!POST)
                    tmp = inc;
                rref.asInt32Ref() = inc;
                f.regs.sp[0].setInt32(tmp);
                return true;
            }
        }
        atom = origAtom;
    }

    jsid id = ATOM_TO_JSID(atom);
    if (!js_FindPropertyHelper(cx, id, true, &obj, &obj2, &prop))
        return false;
    if (!prop) {
        ReportAtomNotDefined(cx, atom);
        return false;
    }
    obj2->dropProperty(cx, prop);
    return ObjIncOp<N, POST>(f, obj, id);
}

void JS_FASTCALL
stubs::PropInc(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-1]);
    if (!obj)
        THROW();
    if (!ObjIncOp<1, true>(f, obj, ATOM_TO_JSID(atom)))
        THROW();
    f.regs.sp[-2] = f.regs.sp[-1];
}

void JS_FASTCALL
stubs::PropDec(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-1]);
    if (!obj)
        THROW();
    if (!ObjIncOp<-1, true>(f, obj, ATOM_TO_JSID(atom)))
        THROW();
    f.regs.sp[-2] = f.regs.sp[-1];
}

void JS_FASTCALL
stubs::IncProp(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-1]);
    if (!obj)
        THROW();
    if (!ObjIncOp<1, false>(f, obj, ATOM_TO_JSID(atom)))
        THROW();
    f.regs.sp[-2] = f.regs.sp[-1];
}

void JS_FASTCALL
stubs::DecProp(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-1]);
    if (!obj)
        THROW();
    if (!ObjIncOp<-1, false>(f, obj, ATOM_TO_JSID(atom)))
        THROW();
    f.regs.sp[-2] = f.regs.sp[-1];
}

void JS_FASTCALL
stubs::ElemInc(VMFrame &f)
{
    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-2]);
    if (!obj)
        THROW();
    jsid id;
    if (!FetchElementId(f, obj, f.regs.sp[-1], id, &f.regs.sp[-1]))
        THROW();
    if (!ObjIncOp<1, true>(f, obj, id))
        THROW();
    f.regs.sp[-3] = f.regs.sp[-1];
}

void JS_FASTCALL
stubs::ElemDec(VMFrame &f)
{
    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-2]);
    if (!obj)
        THROW();
    jsid id;
    if (!FetchElementId(f, obj, f.regs.sp[-1], id, &f.regs.sp[-1]))
        THROW();
    if (!ObjIncOp<-1, true>(f, obj, id))
        THROW();
    f.regs.sp[-3] = f.regs.sp[-1];
}

void JS_FASTCALL
stubs::IncElem(VMFrame &f)
{
    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-2]);
    if (!obj)
        THROW();
    jsid id;
    if (!FetchElementId(f, obj, f.regs.sp[-1], id, &f.regs.sp[-1]))
        THROW();
    if (!ObjIncOp<1, false>(f, obj, id))
        THROW();
    f.regs.sp[-3] = f.regs.sp[-1];
}

void JS_FASTCALL
stubs::DecElem(VMFrame &f)
{
    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-2]);
    if (!obj)
        THROW();
    jsid id;
    if (!FetchElementId(f, obj, f.regs.sp[-1], id, &f.regs.sp[-1]))
        THROW();
    if (!ObjIncOp<-1, false>(f, obj, id))
        THROW();
    f.regs.sp[-3] = f.regs.sp[-1];
}

void JS_FASTCALL
stubs::NameInc(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = f.fp->scopeChainObj();
    if (!NameIncDec<1, true>(f, obj, atom))
        THROW();
}

void JS_FASTCALL
stubs::NameDec(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = f.fp->scopeChainObj();
    if (!NameIncDec<-1, true>(f, obj, atom))
        THROW();
}

void JS_FASTCALL
stubs::IncName(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = f.fp->scopeChainObj();
    if (!NameIncDec<1, false>(f, obj, atom))
        THROW();
}

void JS_FASTCALL
stubs::DecName(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = f.fp->scopeChainObj();
    if (!NameIncDec<-1, false>(f, obj, atom))
        THROW();
}

void JS_FASTCALL
stubs::GlobalNameInc(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = f.fp->scopeChainObj()->getGlobal();
    if (!NameIncDec<1, true>(f, obj, atom))
        THROW();
}

void JS_FASTCALL
stubs::GlobalNameDec(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = f.fp->scopeChainObj()->getGlobal();
    if (!NameIncDec<-1, true>(f, obj, atom))
        THROW();
}

void JS_FASTCALL
stubs::IncGlobalName(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = f.fp->scopeChainObj()->getGlobal();
    if (!NameIncDec<1, false>(f, obj, atom))
        THROW();
}

void JS_FASTCALL
stubs::DecGlobalName(VMFrame &f, JSAtom *atom)
{
    JSObject *obj = f.fp->scopeChainObj()->getGlobal();
    if (!NameIncDec<-1, false>(f, obj, atom))
        THROW();
}

static bool JS_FASTCALL
InlineGetProp(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    Value *vp = &f.regs.sp[-1];
    JSObject *obj = ValueToObject(f.cx, vp);
    if (!obj)
        return false;

    Value rval;
    do {
        /*
         * We do not impose the method read barrier if in an imacro,
         * assuming any property gets it does (e.g., for 'toString'
         * from JSOP_NEW) will not be leaked to the calling script.
         */
        JSObject *aobj = js_GetProtoIfDenseArray(obj);

        PropertyCacheEntry *entry;
        JSObject *obj2;
        JSAtom *atom;
        JS_PROPERTY_CACHE(cx).test(cx, regs.pc, aobj, obj2, entry, atom);
        if (!atom) {
            if (entry->vword.isFunObj()) {
                rval.setObject(entry->vword.toFunObj());
            } else if (entry->vword.isSlot()) {
                uint32 slot = entry->vword.toSlot();
                JS_ASSERT(slot < obj2->scope()->freeslot);
                rval = obj2->lockedGetSlot(slot);
            } else {
                JS_ASSERT(entry->vword.isSprop());
                JSScopeProperty *sprop = entry->vword.toSprop();
                NATIVE_GET(cx, obj, obj2, sprop,
                        f.fp->imacpc ? JSGET_NO_METHOD_BARRIER : JSGET_METHOD_BARRIER,
                        &rval, return false);
            }
            break;
        }

        jsid id = ATOM_TO_JSID(atom);
        if (JS_LIKELY(aobj->map->ops->getProperty == js_GetProperty)
                ? !js_GetPropertyHelper(cx, obj, id,
                    f.fp->imacpc
                    ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                    : JSGET_CACHE_RESULT | JSGET_METHOD_BARRIER,
                    &rval)
                : !obj->getProperty(cx, id, &rval)) {
            return false;
        }
    } while(0);

    regs.sp[-1] = rval;
    return true;
}

void JS_FASTCALL
stubs::GetProp(VMFrame &f)
{
    if (!InlineGetProp(f))
        THROW();
}

void JS_FASTCALL
stubs::CallProp(VMFrame &f, JSAtom *origAtom)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    Value lval;
    lval = regs.sp[-1];

    Value objv;
    if (lval.isObject()) {
        objv = lval;
    } else {
        JSProtoKey protoKey;
        if (lval.isString()) {
            protoKey = JSProto_String;
        } else if (lval.isNumber()) {
            protoKey = JSProto_Number;
        } else if (lval.isBoolean()) {
            protoKey = JSProto_Boolean;
        } else {
            JS_ASSERT(lval.isNull() || lval.isUndefined());
            js_ReportIsNullOrUndefined(cx, -1, lval, NULL);
            THROW();
        }
        JSObject *pobj;
        if (!js_GetClassPrototype(cx, NULL, protoKey, &pobj))
            THROW();
        objv.setObject(*pobj);
    }

    JSObject *aobj = js_GetProtoIfDenseArray(&objv.asObject());
    Value rval;

    PropertyCacheEntry *entry;
    JSObject *obj2;
    JSAtom *atom;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, aobj, obj2, entry, atom);
    if (!atom) {
        if (entry->vword.isFunObj()) {
            rval.setObject(entry->vword.toFunObj());
        } else if (entry->vword.isSlot()) {
            uint32 slot = entry->vword.toSlot();
            JS_ASSERT(slot < obj2->scope()->freeslot);
            rval = obj2->lockedGetSlot(slot);
        } else {
            JS_ASSERT(entry->vword.isSprop());
            JSScopeProperty *sprop = entry->vword.toSprop();
            NATIVE_GET(cx, &objv.asObject(), obj2, sprop, JSGET_NO_METHOD_BARRIER, &rval,
                       THROW());
        }
        regs.sp++;
        regs.sp[-2] = rval;
        regs.sp[-1] = lval;
        goto end_callprop;
    }

    /*
     * Cache miss: use the immediate atom that was loaded for us under
     * PropertyCache::test.
     */
    jsid id;
    id = ATOM_TO_JSID(origAtom);

    regs.sp++;
    regs.sp[-1].setNull();
    if (lval.isObject()) {
        if (!js_GetMethod(cx, &objv.asObject(), id,
                          JS_LIKELY(aobj->map->ops->getProperty == js_GetProperty)
                          ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                          : JSGET_NO_METHOD_BARRIER,
                          &rval)) {
            THROW();
        }
        regs.sp[-1] = objv;
        regs.sp[-2] = rval;
    } else {
        JS_ASSERT(objv.asObject().map->ops->getProperty == js_GetProperty);
        if (!js_GetPropertyHelper(cx, &objv.asObject(), id,
                                  JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER,
                                  &rval)) {
            THROW();
        }
        regs.sp[-1] = lval;
        regs.sp[-2] = rval;
    }

  end_callprop:
    /* Wrap primitive lval in object clothing if necessary. */
    if (lval.isPrimitive()) {
        /* FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=412571 */
        JSObject *funobj;
        if (!IsFunctionObject(rval, &funobj) ||
            !PrimitiveThisTest(GET_FUNCTION_PRIVATE(cx, funobj), lval)) {
            if (!js_PrimitiveToObject(cx, &regs.sp[-1]))
                THROW();
        }
    }
#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(rval.isUndefined())) {
        regs.sp[-2].setString(ATOM_TO_STRING(origAtom));
        if (!js_OnUnknownMethod(cx, regs.sp - 2))
            THROW();
    }
#endif
}

void JS_FASTCALL
stubs::WrapPrimitiveThis(VMFrame &f)
{
    JSContext *cx = f.cx;
    const Value &funv = f.regs.sp[-2];
    const Value &thisv = f.regs.sp[-1];

    JS_ASSERT(thisv.isPrimitive());

    /* FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=412571 */
    JSObject *funobj;
    if (!IsFunctionObject(funv, &funobj) ||
        !PrimitiveThisTest(GET_FUNCTION_PRIVATE(cx, funobj), thisv)) {
        if (!js_PrimitiveToObject(cx, &f.regs.sp[-1]))
            THROW();
    }
}

void JS_FASTCALL
stubs::Length(VMFrame &f)
{
    JSFrameRegs &regs = f.regs;
    Value *vp = &regs.sp[-1];

    if (vp->isString()) {
        vp->setInt32(vp->asString()->length());
        return;
    } else if (vp->isObject()) {
        JSObject *obj = &vp->asObject();
        if (obj->isArray()) {
            jsuint length = obj->getArrayLength();
            regs.sp[-1].setDouble(length);
            return;
        } else if (obj->isArguments() && !obj->isArgsLengthOverridden()) {
            uint32 length = obj->getArgsLength();
            JS_ASSERT(length < INT32_MAX);
            regs.sp[-1].setInt32(int32_t(length));
            return;
        }
    }

    if (!InlineGetProp(f))
        THROW();
}

void JS_FASTCALL
stubs::Iter(VMFrame &f, uint32 flags)
{
    if (!js_ValueToIterator(f.cx, flags, &f.regs.sp[-1]))
        THROW();
    JS_ASSERT(!f.regs.sp[-1].isPrimitive());
}

static void 
InitPropOrMethod(VMFrame &f, JSAtom *atom, JSOp op)
{
    JSContext *cx = f.cx;
    JSRuntime *rt = cx->runtime;
    JSFrameRegs &regs = f.regs;

    /* Load the property's initial value into rval. */
    JS_ASSERT(regs.sp - f.fp->base() >= 2);
    Value rval;
    rval = regs.sp[-1];

    /* Load the object being initialized into lval/obj. */
    JSObject *obj = &regs.sp[-2].asObject();
    JS_ASSERT(obj->isNative());
    JS_ASSERT(!obj->getClass()->reserveSlots);

    JSScope *scope = obj->scope();

    /*
     * Probe the property cache. 
     *
     * We can not assume that the object created by JSOP_NEWINIT is still
     * single-threaded as the debugger can access it from other threads.
     * So check first.
     *
     * On a hit, if the cached sprop has a non-default setter, it must be
     * __proto__. If sprop->parent != scope->lastProperty(), there is a
     * repeated property name. The fast path does not handle these two cases.
     */
    PropertyCacheEntry *entry;
    JSScopeProperty *sprop;
    if (CX_OWNS_OBJECT_TITLE(cx, obj) &&
        JS_PROPERTY_CACHE(cx).testForInit(rt, regs.pc, obj, scope, &sprop, &entry) &&
        sprop->hasDefaultSetter() &&
        sprop->parent == scope->lastProperty())
    {
        /* Fast path. Property cache hit. */
        uint32 slot = sprop->slot;
        JS_ASSERT(slot == scope->freeslot);
        if (slot < obj->numSlots()) {
            ++scope->freeslot;
        } else {
            if (!js_AllocSlot(cx, obj, &slot))
                THROW();
            JS_ASSERT(slot == sprop->slot);
        }

        JS_ASSERT(!scope->lastProperty() ||
                  scope->shape == scope->lastProperty()->shape);
        if (scope->table) {
            JSScopeProperty *sprop2 =
                scope->addProperty(cx, sprop->id, sprop->getter(), sprop->setter(), slot,
                                   sprop->attributes(), sprop->getFlags(), sprop->shortid);
            if (!sprop2) {
                js_FreeSlot(cx, obj, slot);
                THROW();
            }
            JS_ASSERT(sprop2 == sprop);
        } else {
            JS_ASSERT(!scope->isSharedEmpty());
            scope->extend(cx, sprop);
        }

        /*
         * No method change check here because here we are adding a new
         * property, not updating an existing slot's value that might
         * contain a method of a branded scope.
         */
        obj->lockedSetSlot(slot, rval);
    } else {
        PCMETER(JS_PROPERTY_CACHE(cx).inipcmisses++);

        /* Get the immediate property name into id. */
        jsid id = ATOM_TO_JSID(atom);

        /* Set the property named by obj[id] to rval. */
        if (!CheckRedeclaration(cx, obj, id, JSPROP_INITIALIZER, NULL, NULL))
            THROW();

        uintN defineHow = (op == JSOP_INITMETHOD)
                          ? JSDNP_CACHE_RESULT | JSDNP_SET_METHOD
                          : JSDNP_CACHE_RESULT;
        if (!(JS_UNLIKELY(atom == cx->runtime->atomState.protoAtom)
              ? js_SetPropertyHelper(cx, obj, id, defineHow, &rval)
              : js_DefineNativeProperty(cx, obj, id, rval, NULL, NULL,
                                        JSPROP_ENUMERATE, 0, 0, NULL,
                                        defineHow))) {
            THROW();
        }
    }
}

void JS_FASTCALL
stubs::InitProp(VMFrame &f, JSAtom *atom)
{
    InitPropOrMethod(f, atom, JSOP_INITPROP);
}

void JS_FASTCALL
stubs::InitMethod(VMFrame &f, JSAtom *atom)
{
    InitPropOrMethod(f, atom, JSOP_INITMETHOD);
}

void JS_FASTCALL
stubs::IterNext(VMFrame &f)
{
    JS_ASSERT(f.regs.sp - 1 >= f.fp->base());
    JS_ASSERT(f.regs.sp[-1].isObject());

    JSObject *iterobj = &f.regs.sp[-1].asObject();
    f.regs.sp[0].setNull();
    f.regs.sp++;
    if (!js_IteratorNext(f.cx, iterobj, &f.regs.sp[-1]))
        THROW();
}

JSBool JS_FASTCALL
stubs::IterMore(VMFrame &f)
{
    JS_ASSERT(f.regs.sp - 1 >= f.fp->base());
    JS_ASSERT(f.regs.sp[-1].isObject());

    Value v;
    JSObject *iterobj = &f.regs.sp[-1].asObject();
    if (!js_IteratorMore(f.cx, iterobj, &v))
        THROWV(JS_FALSE);

    return v.asBoolean();
}

void JS_FASTCALL
stubs::EndIter(VMFrame &f)
{
    JS_ASSERT(f.regs.sp - 1 >= f.fp->base());
    if (!js_CloseIterator(f.cx, f.regs.sp[-1]))
        THROW();
}

JSString * JS_FASTCALL
stubs::TypeOf(VMFrame &f)
{
    const Value &ref = f.regs.sp[-1];
    JSType type = JS_TypeOfValue(f.cx, Jsvalify(ref));
    JSAtom *atom = f.cx->runtime->atomState.typeAtoms[type];
    return ATOM_TO_STRING(atom);
}

JSBool JS_FASTCALL
stubs::StrictEq(VMFrame &f)
{
    const Value &rhs = f.regs.sp[-1];
    const Value &lhs = f.regs.sp[-2];
    return StrictlyEqual(f.cx, lhs, rhs) == true;
}

JSBool JS_FASTCALL
stubs::StrictNe(VMFrame &f)
{
    const Value &rhs = f.regs.sp[-1];
    const Value &lhs = f.regs.sp[-2];
    return StrictlyEqual(f.cx, lhs, rhs) != true;
}

JSString * JS_FASTCALL
stubs::ConcatN(VMFrame &f, uint32 argc)
{
    JSCharBuffer buf(f.cx);
    for (Value *vp = f.regs.sp - argc; vp < f.regs.sp; vp++) {
        JS_ASSERT(vp->isPrimitive());
        if (!js_ValueToCharBuffer(f.cx, *vp, buf))
            THROWV(NULL);
    }
    JSString *str = js_NewStringFromCharBuffer(f.cx, buf);
    if (!str)
        THROWV(NULL);
    return str;
}

void JS_FASTCALL
stubs::Throw(VMFrame &f)
{
    JSContext *cx = f.cx;

    JS_ASSERT(!cx->throwing);
    cx->throwing = JS_TRUE;
    cx->exception = f.regs.sp[-1];
    THROW();
}

JSObject * JS_FASTCALL
stubs::FlatLambda(VMFrame &f, JSFunction *fun)
{
    JSObject *obj = js_NewFlatClosure(f.cx, fun);
    if (!obj)
        THROWV(NULL);
    return obj;
}

void JS_FASTCALL
stubs::Arguments(VMFrame &f)
{
    f.regs.sp++;
    if (!js_GetArgsValue(f.cx, f.fp, &f.regs.sp[-1]))
        THROW();
}

JSBool JS_FASTCALL
stubs::InstanceOf(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    const Value &rref = regs.sp[-1];
    JSObject *obj;
    if (rref.isPrimitive() ||
        !(obj = &rref.asObject())->map->ops->hasInstance) {
        js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                            -1, rref, NULL);
        THROWV(JS_FALSE);
    }
    const Value &lref = regs.sp[-2];
    JSBool cond = JS_FALSE;
    if (!obj->map->ops->hasInstance(cx, obj, &lref, &cond))
        THROWV(JS_FALSE);
    f.regs.sp[-2].setBoolean(cond);
    return cond;
}

void JS_FASTCALL
stubs::FastInstanceOf(VMFrame &f)
{
    const Value &lref = f.regs.sp[-1];

    if (lref.isPrimitive()) {
        /*
         * Throw a runtime error if instanceof is called on a function that
         * has a non-object as its .prototype value.
         */
        js_ReportValueError(f.cx, JSMSG_BAD_PROTOTYPE, -1, f.regs.sp[-2], NULL);
        THROW();
    }

    f.regs.sp[-3].setBoolean(js_IsDelegate(f.cx, &lref.asObject(), f.regs.sp[-3]));
}

void JS_FASTCALL
stubs::ArgCnt(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSRuntime *rt = cx->runtime;
    JSStackFrame *fp = f.fp;

    jsid id = ATOM_TO_JSID(rt->atomState.lengthAtom);
    f.regs.sp++;
    if (!js_GetArgsProperty(cx, fp, id, &f.regs.sp[-1]))
        THROW();
}

void JS_FASTCALL
stubs::EnterBlock(VMFrame &f, JSObject *obj)
{
    JSFrameRegs &regs = f.regs;
    JSStackFrame *fp = f.fp;

    JS_ASSERT(!OBJ_IS_CLONED_BLOCK(obj));
    JS_ASSERT(fp->base() + OBJ_BLOCK_DEPTH(cx, obj) == regs.sp);
    Value *vp = regs.sp + OBJ_BLOCK_COUNT(cx, obj);
    JS_ASSERT(regs.sp < vp);
    JS_ASSERT(vp <= fp->slots() + fp->script->nslots);
    SetValueRangeToUndefined(regs.sp, vp);
    regs.sp = vp;

#ifdef DEBUG
    JSContext *cx = f.cx;
    JS_ASSERT(fp->blockChain == obj->getParent());

    /*
     * The young end of fp->scopeChain may omit blocks if we haven't closed
     * over them, but if there are any closure blocks on fp->scopeChain, they'd
     * better be (clones of) ancestors of the block we're entering now;
     * anything else we should have popped off fp->scopeChain when we left its
     * static scope.
     */
    JSObject *obj2 = fp->scopeChainObj();
    Class *clasp;
    while ((clasp = obj2->getClass()) == &js_WithClass)
        obj2 = obj2->getParent();
    if (clasp == &js_BlockClass &&
        obj2->getPrivate() == js_FloatingFrameIfGenerator(cx, fp)) {
        JSObject *youngestProto = obj2->getProto();
        JS_ASSERT(!OBJ_IS_CLONED_BLOCK(youngestProto));
        JSObject *parent = obj;
        while ((parent = parent->getParent()) != youngestProto)
            JS_ASSERT(parent);
    }
#endif

    fp->blockChain = obj;
}

void JS_FASTCALL
stubs::LeaveBlock(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSStackFrame *fp = f.fp;

#ifdef DEBUG
    JS_ASSERT(fp->blockChain->getClass() == &js_BlockClass);
    uintN blockDepth = OBJ_BLOCK_DEPTH(cx, fp->blockChain);

    JS_ASSERT(blockDepth <= StackDepth(fp->script));
#endif
    /*
     * If we're about to leave the dynamic scope of a block that has been
     * cloned onto fp->scopeChain, clear its private data, move its locals from
     * the stack into the clone, and pop it off the chain.
     */
    JSObject *obj = fp->scopeChainObj();
    if (obj->getProto() == fp->blockChain) {
        JS_ASSERT(obj->getClass() == &js_BlockClass);
        if (!js_PutBlockObject(cx, JS_TRUE))
            THROW();
    }

    /* Pop the block chain, too.  */
    fp->blockChain = fp->blockChain->getParent();
}

void * JS_FASTCALL
stubs::LookupSwitch(VMFrame &f, jsbytecode *pc)
{
    jsbytecode *jpc = pc;
    JSScript *script = f.fp->script;

    /* This is correct because the compiler adjusts the stack beforehand. */
    Value lval = f.regs.sp[-1];

    if (!lval.isPrimitive()) {
        ptrdiff_t offs = (pc + GET_JUMP_OFFSET(pc)) - script->code;
        JS_ASSERT(script->nmap[offs]);
        return script->nmap[offs];
    }

    JS_ASSERT(pc[0] == JSOP_LOOKUPSWITCH);
    
    pc += JUMP_OFFSET_LEN;
    uint32 npairs = GET_UINT16(pc);
    pc += UINT16_LEN;

    JS_ASSERT(npairs);

    if (lval.isString()) {
        JSString *str = lval.asString();
        for (uint32 i = 1; i <= npairs; i++) {
            Value rval = script->getConst(GET_INDEX(pc));
            pc += INDEX_LEN;
            if (rval.isString()) {
                JSString *rhs = rval.asString();
                if (rhs == str || js_EqualStrings(str, rhs)) {
                    ptrdiff_t offs = (jpc + GET_JUMP_OFFSET(pc)) - script->code;
                    JS_ASSERT(script->nmap[offs]);
                    return script->nmap[offs];
                }
            }
            pc += JUMP_OFFSET_LEN;
        }
    } else if (lval.isNumber()) {
        double d = lval.asNumber();
        for (uint32 i = 1; i <= npairs; i++) {
            Value rval = script->getConst(GET_INDEX(pc));
            pc += INDEX_LEN;
            if (rval.isNumber() && d == rval.asNumber()) {
                ptrdiff_t offs = (jpc + GET_JUMP_OFFSET(pc)) - script->code;
                JS_ASSERT(script->nmap[offs]);
                return script->nmap[offs];
            }
            pc += JUMP_OFFSET_LEN;
        }
    } else {
        for (uint32 i = 1; i <= npairs; i++) {
            Value rval = script->getConst(GET_INDEX(pc));
            pc += INDEX_LEN;
            if (lval == rval) {
                ptrdiff_t offs = (jpc + GET_JUMP_OFFSET(pc)) - script->code;
                JS_ASSERT(script->nmap[offs]);
                return script->nmap[offs];
            }
            pc += JUMP_OFFSET_LEN;
        }
    }

    ptrdiff_t offs = (jpc + GET_JUMP_OFFSET(jpc)) - script->code;
    JS_ASSERT(script->nmap[offs]);
    return script->nmap[offs];
}

void * JS_FASTCALL
stubs::TableSwitch(VMFrame &f, jsbytecode *origPc)
{
    jsbytecode * const originalPC = origPc;
    jsbytecode *pc = originalPC;
    JSScript *script = f.fp->script;
    uint32 jumpOffset = GET_JUMP_OFFSET(pc);
    pc += JUMP_OFFSET_LEN;

    /* Note: compiler adjusts the stack beforehand. */
    Value rval = f.regs.sp[-1];

    jsint tableIdx;
    if (rval.isInt32()) {
        tableIdx = rval.asInt32();
    } else if (rval.isDouble()) {
        double d = rval.asDouble();
        if (d == 0) {
            /* Treat -0 (double) as 0. */
            tableIdx = 0;
        } else if (!JSDOUBLE_IS_INT32(d, (int32_t&)tableIdx)) {
            goto finally;
        }
    } else {
        goto finally;
    }

    {
        uint32 low = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        uint32 high = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;

        tableIdx -= low;
        if ((jsuint) tableIdx < (jsuint)(high - low + 1)) {
            pc += JUMP_OFFSET_LEN * tableIdx;
            uint32 candidateOffset = GET_JUMP_OFFSET(pc);
            if (candidateOffset)
                jumpOffset = candidateOffset;
        }
    }

finally:
    /* Provide the native address. */
    ptrdiff_t offset = (originalPC + jumpOffset) - script->code;
    JS_ASSERT(script->nmap[offset]);
    return script->nmap[offset];
}

void JS_FASTCALL
stubs::Unbrand(VMFrame &f)
{
    if (!f.regs.sp[-1].asObject().unbrand(f.cx))
        THROW();
}

void JS_FASTCALL
stubs::Pos(VMFrame &f)
{
    if (!ValueToNumber(f.cx, &f.regs.sp[-1]))
        THROW();
}

