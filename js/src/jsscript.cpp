/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS script operations.
 */

#include <string.h>
#include "jstypes.h"
#include "jsutil.h"
#include "jscrashreport.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"

#include "gc/Marking.h"
#include "frontend/BytecodeEmitter.h"
#include "frontend/Parser.h"
#include "js/MemoryMetrics.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/Retcon.h"
#include "vm/Debugger.h"
#include "vm/Xdr.h"

#include "jsinferinlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "frontend/TreeContext-inl.h"
#include "vm/RegExpObject-inl.h"

#include "frontend/TreeContext-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::frontend;

namespace js {

BindingKind
Bindings::lookup(JSContext *cx, JSAtom *name, unsigned *indexp) const
{
    if (!lastBinding)
        return NONE;

    Shape **spp;
    Shape *shape = Shape::search(cx, lastBinding, AtomToId(name), &spp);
    if (!shape)
        return NONE;

    if (indexp)
        *indexp = shape->shortid();

    if (shape->setter() == CallObject::setArgOp)
        return ARGUMENT;

    return shape->writable() ? VARIABLE : CONSTANT;
}

bool
Bindings::add(JSContext *cx, HandleAtom name, BindingKind kind)
{
    if (!ensureShape(cx))
        return false;

    if (nargs + nvars == BINDING_COUNT_LIMIT) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             (kind == ARGUMENT)
                             ? JSMSG_TOO_MANY_FUN_ARGS
                             : JSMSG_TOO_MANY_LOCALS);
        return false;
    }

    /*
     * We still follow 10.2.3 of ES3 and make argument and variable properties
     * of the Call objects enumerable. ES5 reformulated all of its Clause 10 to
     * avoid objects as activations, something we should do too.
     */
    unsigned attrs = JSPROP_ENUMERATE | JSPROP_PERMANENT;

    uint16_t *indexp;
    PropertyOp getter;
    StrictPropertyOp setter;
    uint32_t slot = CallObject::RESERVED_SLOTS;

    if (kind == ARGUMENT) {
        JS_ASSERT(nvars == 0);
        indexp = &nargs;
        getter = NULL;
        setter = CallObject::setArgOp;
        slot += nargs;
    } else {
        JS_ASSERT(kind == VARIABLE || kind == CONSTANT);

        indexp = &nvars;
        getter = NULL;
        setter = CallObject::setVarOp;
        if (kind == CONSTANT)
            attrs |= JSPROP_READONLY;
        slot += nargs + nvars;
    }

    RootedId id(cx);
    if (!name) {
        JS_ASSERT(kind == ARGUMENT); /* destructuring */
        id = INT_TO_JSID(nargs);
    } else {
        id = AtomToId(name);
    }

    StackBaseShape base(&CallClass, NULL, BaseShape::VAROBJ);
    base.updateGetterSetter(attrs, getter, setter);

    UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
    if (!nbase)
        return false;

    StackShape child(nbase, id, slot, 0, attrs, Shape::HAS_SHORTID, *indexp);

    /* Shapes in bindings cannot be dictionaries. */
    Shape *shape = lastBinding->getChildBinding(cx, child);
    if (!shape)
        return false;

    lastBinding = shape;
    ++*indexp;
    return true;
}

Shape *
Bindings::callObjectShape(JSContext *cx) const
{
    if (!hasDup())
        return lastShape();

    /*
     * Build a vector of non-duplicate properties in order from last added
     * to first (i.e., the order we normally have iterate over Shapes). Choose
     * the last added property in each set of dups.
     */
    Vector<const Shape *> shapes(cx);
    HashSet<jsid> seen(cx);
    if (!seen.init())
        return NULL;

    for (Shape::Range r = lastShape()->all(); !r.empty(); r.popFront()) {
        const Shape &s = r.front();
        HashSet<jsid>::AddPtr p = seen.lookupForAdd(s.propid());
        if (!p) {
            if (!seen.add(p, s.propid()))
                return NULL;
            if (!shapes.append(&s))
                return NULL;
        }
    }

    /*
     * Now build the Shape without duplicate properties.
     */
    RootedShape shape(cx);
    shape = initialShape(cx);
    for (int i = shapes.length() - 1; i >= 0; --i) {
        shape = shape->getChildBinding(cx, shapes[i]);
        if (!shape)
            return NULL;
    }

    return shape;
}

bool
Bindings::getLocalNameArray(JSContext *cx, BindingNames *namesp)
{
    JS_ASSERT(lastBinding);
    if (count() == 0)
        return true;

    BindingNames &names = *namesp;
    JS_ASSERT(names.empty());

    unsigned n = count();
    if (!names.growByUninitialized(n))
        return false;

#ifdef DEBUG
    JSAtom * const POISON = reinterpret_cast<JSAtom *>(0xdeadbeef);
    for (unsigned i = 0; i < n; i++)
        names[i].maybeAtom = POISON;
#endif

    for (Shape::Range r = lastBinding->all(); !r.empty(); r.popFront()) {
        const Shape &shape = r.front();
        unsigned index = uint16_t(shape.shortid());

        if (shape.setter() == CallObject::setArgOp) {
            JS_ASSERT(index < nargs);
            names[index].kind = ARGUMENT;
        } else {
            JS_ASSERT(index < nvars);
            index += nargs;
            names[index].kind = shape.writable() ? VARIABLE : CONSTANT;
        }

        if (JSID_IS_ATOM(shape.propid())) {
            names[index].maybeAtom = JSID_TO_ATOM(shape.propid());
        } else {
            JS_ASSERT(JSID_IS_INT(shape.propid()));
            JS_ASSERT(shape.setter() == CallObject::setArgOp);
            names[index].maybeAtom = NULL;
        }
    }

#ifdef DEBUG
    for (unsigned i = 0; i < n; i++)
        JS_ASSERT(names[i].maybeAtom != POISON);
#endif

    return true;
}

const Shape *
Bindings::lastArgument() const
{
    JS_ASSERT(lastBinding);

    const js::Shape *shape = lastVariable();
    if (nvars > 0) {
        while (shape->previous() && shape->setter() != CallObject::setArgOp)
            shape = shape->previous();
    }
    return shape;
}

const Shape *
Bindings::lastVariable() const
{
    JS_ASSERT(lastBinding);
    return lastBinding;
}

void
Bindings::makeImmutable()
{
    JS_ASSERT(lastBinding);
    JS_ASSERT(!lastBinding->inDictionary());
}

void
Bindings::trace(JSTracer *trc)
{
    if (lastBinding)
        MarkShape(trc, &lastBinding, "shape");
}

} /* namespace js */

template<XDRMode mode>
static bool
XDRScriptConst(XDRState<mode> *xdr, HeapValue *vp)
{
    /*
     * A script constant can be an arbitrary primitive value as they are used
     * to implement JSOP_LOOKUPSWITCH. But they cannot be objects, see
     * bug 407186.
     */
    enum ConstTag {
        SCRIPT_INT     = 0,
        SCRIPT_DOUBLE  = 1,
        SCRIPT_ATOM    = 2,
        SCRIPT_TRUE    = 3,
        SCRIPT_FALSE   = 4,
        SCRIPT_NULL    = 5,
        SCRIPT_VOID    = 6
    };

    uint32_t tag;
    if (mode == XDR_ENCODE) {
        if (vp->isInt32()) {
            tag = SCRIPT_INT;
        } else if (vp->isDouble()) {
            tag = SCRIPT_DOUBLE;
        } else if (vp->isString()) {
            tag = SCRIPT_ATOM;
        } else if (vp->isTrue()) {
            tag = SCRIPT_TRUE;
        } else if (vp->isFalse()) {
            tag = SCRIPT_FALSE;
        } else if (vp->isNull()) {
            tag = SCRIPT_NULL;
        } else {
            JS_ASSERT(vp->isUndefined());
            tag = SCRIPT_VOID;
        }
    }

    if (!xdr->codeUint32(&tag))
        return false;

    switch (tag) {
      case SCRIPT_INT: {
        uint32_t i;
        if (mode == XDR_ENCODE)
            i = uint32_t(vp->toInt32());
        if (!xdr->codeUint32(&i))
            return JS_FALSE;
        if (mode == XDR_DECODE)
            vp->init(Int32Value(int32_t(i)));
        break;
      }
      case SCRIPT_DOUBLE: {
        double d;
        if (mode == XDR_ENCODE)
            d = vp->toDouble();
        if (!xdr->codeDouble(&d))
            return false;
        if (mode == XDR_DECODE)
            vp->init(DoubleValue(d));
        break;
      }
      case SCRIPT_ATOM: {
        JSAtom *atom;
        if (mode == XDR_ENCODE)
            atom = &vp->toString()->asAtom();
        if (!XDRAtom(xdr, &atom))
            return false;
        if (mode == XDR_DECODE)
            vp->init(StringValue(atom));
        break;
      }
      case SCRIPT_TRUE:
        if (mode == XDR_DECODE)
            vp->init(BooleanValue(true));
        break;
      case SCRIPT_FALSE:
        if (mode == XDR_DECODE)
            vp->init(BooleanValue(false));
        break;
      case SCRIPT_NULL:
        if (mode == XDR_DECODE)
            vp->init(NullValue());
        break;
      case SCRIPT_VOID:
        if (mode == XDR_DECODE)
            vp->init(UndefinedValue());
        break;
    }
    return true;
}

template<XDRMode mode>
bool
js::XDRScript(XDRState<mode> *xdr, JSScript **scriptp, JSScript *parentScript)
{
    /* NB: Keep this in sync with CloneScript. */

    enum ScriptBits {
        NoScriptRval,
        SavedCallerFun,
        StrictModeCode,
        ContainsDynamicNameAccess,
        FunHasExtensibleScope,
        ArgumentsHasLocalBinding,
        NeedsArgsObj,
        OwnFilename,
        ParentFilename,
        IsGenerator
    };

    uint32_t length, lineno, nslots;
    uint32_t natoms, nsrcnotes, ntrynotes, nobjects, nregexps, nconsts, nClosedArgs, nClosedVars, i;
    uint32_t prologLength, version;
    uint32_t nTypeSets = 0;
    uint32_t scriptBits = 0;

    JSContext *cx = xdr->cx();
    JSScript *script;
    nsrcnotes = ntrynotes = natoms = nobjects = nregexps = nconsts = nClosedArgs = nClosedVars = 0;
    jssrcnote *notes = NULL;

    /* XDR arguments, local vars, and upvars. */
    uint16_t nargs, nvars;
#if defined(DEBUG) || defined(__GNUC__) /* quell GCC overwarning */
    script = NULL;
    nargs = nvars = Bindings::BINDING_COUNT_LIMIT;
#endif
    uint32_t argsVars;
    if (mode == XDR_ENCODE) {
        script = *scriptp;
        JS_ASSERT_IF(parentScript, parentScript->compartment() == script->compartment());

        nargs = script->bindings.numArgs();
        nvars = script->bindings.numVars();
        argsVars = (nargs << 16) | nvars;
    }
    if (!xdr->codeUint32(&argsVars))
        return false;
    if (mode == XDR_DECODE) {
        nargs = argsVars >> 16;
        nvars = argsVars & 0xFFFF;
    }
    JS_ASSERT(nargs != Bindings::BINDING_COUNT_LIMIT);
    JS_ASSERT(nvars != Bindings::BINDING_COUNT_LIMIT);

    Bindings bindings(cx);
    Bindings::AutoRooter bindingsRoot(cx, &bindings);

    uint32_t nameCount = nargs + nvars;
    if (nameCount > 0) {
        LifoAllocScope las(&cx->tempLifoAlloc());

        /*
         * To xdr the names we prefix the names with a bitmap descriptor and
         * then xdr the names as strings. For argument names (indexes below
         * nargs) the corresponding bit in the bitmap is unset when the name
         * is null. Such null names are not encoded or decoded. For variable
         * names (indexes starting from nargs) bitmap's bit is set when the
         * name is declared as const, not as ordinary var.
         * */
        unsigned bitmapLength = JS_HOWMANY(nameCount, JS_BITS_PER_UINT32);
        uint32_t *bitmap = cx->tempLifoAlloc().newArray<uint32_t>(bitmapLength);
        if (!bitmap) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        BindingNames names(cx);
        if (mode == XDR_ENCODE) {
            if (!script->bindings.getLocalNameArray(cx, &names))
                return false;
            PodZero(bitmap, bitmapLength);
            for (unsigned i = 0; i < nameCount; i++) {
                if (i < nargs && names[i].maybeAtom)
                    bitmap[i >> JS_BITS_PER_UINT32_LOG2] |= JS_BIT(i & (JS_BITS_PER_UINT32 - 1));
            }
        }
        for (unsigned i = 0; i < bitmapLength; ++i) {
            if (!xdr->codeUint32(&bitmap[i]))
                return false;
        }

        for (unsigned i = 0; i < nameCount; i++) {
            if (i < nargs &&
                !(bitmap[i >> JS_BITS_PER_UINT32_LOG2] & JS_BIT(i & (JS_BITS_PER_UINT32 - 1))))
            {
                if (mode == XDR_DECODE) {
                    uint16_t dummy;
                    if (!bindings.addDestructuring(cx, &dummy))
                        return false;
                } else {
                    JS_ASSERT(!names[i].maybeAtom);
                }
                continue;
            }

            RootedAtom name(cx);
            if (mode == XDR_ENCODE)
                name = names[i].maybeAtom;
            if (!XDRAtom(xdr, name.address()))
                return false;
            if (mode == XDR_DECODE) {
                BindingKind kind = (i < nargs)
                                   ? ARGUMENT
                                   : (bitmap[i >> JS_BITS_PER_UINT32_LOG2] &
                                      JS_BIT(i & (JS_BITS_PER_UINT32 - 1))
                                     ? CONSTANT
                                     : VARIABLE);
                if (!bindings.add(cx, name, kind))
                    return false;
            }
        }
    }

    if (mode == XDR_DECODE) {
        if (!bindings.ensureShape(cx))
            return false;
        bindings.makeImmutable();
    }

    if (mode == XDR_ENCODE)
        length = script->length;
    if (!xdr->codeUint32(&length))
        return JS_FALSE;

    if (mode == XDR_ENCODE) {
        prologLength = script->mainOffset;
        JS_ASSERT(script->getVersion() != JSVERSION_UNKNOWN);
        version = (uint32_t)script->getVersion() | (script->nfixed << 16);
        lineno = script->lineno;
        nslots = (uint32_t)script->nslots;
        nslots = (uint32_t)((script->staticLevel << 16) | script->nslots);
        natoms = script->natoms;

        notes = script->notes();
        nsrcnotes = script->numNotes();

        if (script->hasConsts())
            nconsts = script->consts()->length;
        if (script->hasObjects())
            nobjects = script->objects()->length;
        if (script->hasRegexps())
            nregexps = script->regexps()->length;
        if (script->hasTrynotes())
            ntrynotes = script->trynotes()->length;
        nClosedArgs = script->numClosedArgs();
        nClosedVars = script->numClosedVars();

        nTypeSets = script->nTypeSets;

        if (script->noScriptRval)
            scriptBits |= (1 << NoScriptRval);
        if (script->savedCallerFun)
            scriptBits |= (1 << SavedCallerFun);
        if (script->strictModeCode)
            scriptBits |= (1 << StrictModeCode);
        if (script->bindingsAccessedDynamically)
            scriptBits |= (1 << ContainsDynamicNameAccess);
        if (script->funHasExtensibleScope)
            scriptBits |= (1 << FunHasExtensibleScope);
        if (script->argumentsHasLocalBinding())
            scriptBits |= (1 << ArgumentsHasLocalBinding);
        if (script->analyzedArgsUsage() && script->needsArgsObj())
            scriptBits |= (1 << NeedsArgsObj);
        if (script->filename) {
            scriptBits |= (parentScript && parentScript->filename == script->filename)
                          ? (1 << ParentFilename)
                          : (1 << OwnFilename);
        }
        if (script->isGenerator)
            scriptBits |= (1 << IsGenerator);

        JS_ASSERT(!script->compileAndGo);
        JS_ASSERT(!script->hasSingletons);
    }

    if (!xdr->codeUint32(&prologLength))
        return JS_FALSE;
    if (!xdr->codeUint32(&version))
        return JS_FALSE;

    /*
     * To fuse allocations, we need srcnote, atom, objects, regexp, and trynote
     * counts early.
     */
    if (!xdr->codeUint32(&natoms))
        return JS_FALSE;
    if (!xdr->codeUint32(&nsrcnotes))
        return JS_FALSE;
    if (!xdr->codeUint32(&ntrynotes))
        return JS_FALSE;
    if (!xdr->codeUint32(&nobjects))
        return JS_FALSE;
    if (!xdr->codeUint32(&nregexps))
        return JS_FALSE;
    if (!xdr->codeUint32(&nconsts))
        return JS_FALSE;
    if (!xdr->codeUint32(&nClosedArgs))
        return JS_FALSE;
    if (!xdr->codeUint32(&nClosedVars))
        return JS_FALSE;
    if (!xdr->codeUint32(&nTypeSets))
        return JS_FALSE;
    if (!xdr->codeUint32(&scriptBits))
        return JS_FALSE;

    if (mode == XDR_DECODE) {
        /* Note: version is packed into the 32b space with another 16b value. */
        JSVersion version_ = JSVersion(version & JS_BITMASK(16));
        JS_ASSERT((version_ & VersionFlags::FULL_MASK) == unsigned(version_));

        // principals and originPrincipals are set with xdr->initScriptPrincipals(script) below.
        // staticLevel is set below.
        script = JSScript::Create(cx,
                                  !!(scriptBits & (1 << SavedCallerFun)),
                                  /* principals = */ NULL,
                                  /* originPrincipals = */ NULL,
                                  /* compileAndGo = */ false,
                                  !!(scriptBits & (1 << NoScriptRval)),
                                  /* globalObject = */ NULL,
                                  version_,
                                  /* staticLevel = */ 0);
        if (!script || !script->partiallyInit(cx, length, nsrcnotes, natoms, nobjects,
                                              nregexps, ntrynotes, nconsts, nClosedArgs,
                                              nClosedVars, nTypeSets))
            return JS_FALSE;

        script->bindings.transfer(cx, &bindings);
        JS_ASSERT(!script->mainOffset);
        script->mainOffset = prologLength;
        script->nfixed = uint16_t(version >> 16);

        /* If we know nsrcnotes, we allocated space for notes in script. */
        notes = script->notes();
        *scriptp = script;

        if (scriptBits & (1 << StrictModeCode))
            script->strictModeCode = true;
        if (scriptBits & (1 << ContainsDynamicNameAccess))
            script->bindingsAccessedDynamically = true;
        if (scriptBits & (1 << FunHasExtensibleScope))
            script->funHasExtensibleScope = true;
        if (scriptBits & (1 << ArgumentsHasLocalBinding)) {
            PropertyName *arguments = cx->runtime->atomState.argumentsAtom;
            unsigned local;
            DebugOnly<BindingKind> kind = script->bindings.lookup(cx, arguments, &local);
            JS_ASSERT(kind == VARIABLE || kind == CONSTANT);
            script->setArgumentsHasLocalBinding(local);
        }
        if (scriptBits & (1 << NeedsArgsObj))
            script->setNeedsArgsObj(true);
        if (scriptBits & (1 << IsGenerator))
            script->isGenerator = true;
    }

    JS_STATIC_ASSERT(sizeof(jsbytecode) == 1);
    JS_STATIC_ASSERT(sizeof(jssrcnote) == 1);
    if (!xdr->codeBytes(script->code, length) ||
        !xdr->codeBytes(notes, nsrcnotes) ||
        !xdr->codeUint32(&lineno) ||
        !xdr->codeUint32(&nslots)) {
        return false;
    }

    if (scriptBits & (1 << OwnFilename)) {
        const char *filename;
        if (mode == XDR_ENCODE)
            filename = script->filename;
        if (!xdr->codeCString(&filename))
            return false;
        if (mode == XDR_DECODE) {
            script->filename = SaveScriptFilename(cx, filename);
            if (!script->filename)
                return false;
        }
    } else if (scriptBits & (1 << ParentFilename)) {
        JS_ASSERT(parentScript);
        if (mode == XDR_DECODE)
            script->filename = parentScript->filename;
    }

    if (mode == XDR_DECODE) {
        script->lineno = lineno;
        script->nslots = uint16_t(nslots);
        script->staticLevel = uint16_t(nslots >> 16);
        xdr->initScriptPrincipals(script);
    }

    for (i = 0; i != natoms; ++i) {
        if (mode == XDR_DECODE) {
            JSAtom *tmp = NULL;
            if (!XDRAtom(xdr, &tmp))
                return false;
            script->atoms[i].init(tmp);
        } else {
            JSAtom *tmp = script->atoms[i];
            if (!XDRAtom(xdr, &tmp))
                return false;
        }
    }

    /*
     * Here looping from 0-to-length to xdr objects is essential. It ensures
     * that block objects from the script->objects array will be written and
     * restored in the outer-to-inner order. js_XDRBlockObject relies on this
     * to restore the parent chain.
     */
    for (i = 0; i != nobjects; ++i) {
        HeapPtr<JSObject> *objp = &script->objects()->vector[i];
        uint32_t isBlock;
        if (mode == XDR_ENCODE) {
            JSObject *obj = *objp;
            JS_ASSERT(obj->isFunction() || obj->isStaticBlock());
            isBlock = obj->isBlock() ? 1 : 0;
        }
        if (!xdr->codeUint32(&isBlock))
            return false;
        if (isBlock == 0) {
            JSObject *tmp = *objp;
            if (!XDRInterpretedFunction(xdr, &tmp, parentScript))
                return false;
            *objp = tmp;
        } else {
            JS_ASSERT(isBlock == 1);
            StaticBlockObject *tmp = static_cast<StaticBlockObject *>(objp->get());
            if (!XDRStaticBlockObject(xdr, script, &tmp))
                return false;
            *objp = tmp;
        }
    }
    for (i = 0; i != nregexps; ++i) {
        if (!XDRScriptRegExpObject(xdr, &script->regexps()->vector[i]))
            return false;
    }
    for (i = 0; i != nClosedArgs; ++i) {
        if (!xdr->codeUint32(&script->closedArgs()->vector[i]))
            return false;
    }
    for (i = 0; i != nClosedVars; ++i) {
        if (!xdr->codeUint32(&script->closedVars()->vector[i]))
            return false;
    }

    if (ntrynotes != 0) {
        /*
         * We combine tn->kind and tn->stackDepth when serializing as XDR is not
         * efficient when serializing small integer types.
         */
        JSTryNote *tn, *tnfirst;
        uint32_t kindAndDepth;
        JS_STATIC_ASSERT(sizeof(tn->kind) == sizeof(uint8_t));
        JS_STATIC_ASSERT(sizeof(tn->stackDepth) == sizeof(uint16_t));

        tnfirst = script->trynotes()->vector;
        JS_ASSERT(script->trynotes()->length == ntrynotes);
        tn = tnfirst + ntrynotes;
        do {
            --tn;
            if (mode == XDR_ENCODE) {
                kindAndDepth = (uint32_t(tn->kind) << 16)
                               | uint32_t(tn->stackDepth);
            }
            if (!xdr->codeUint32(&kindAndDepth) ||
                !xdr->codeUint32(&tn->start) ||
                !xdr->codeUint32(&tn->length)) {
                return false;
            }
            if (mode == XDR_DECODE) {
                tn->kind = uint8_t(kindAndDepth >> 16);
                tn->stackDepth = uint16_t(kindAndDepth);
            }
        } while (tn != tnfirst);
    }

    if (nconsts) {
        HeapValue *vector = script->consts()->vector;
        for (i = 0; i != nconsts; ++i) {
            if (!XDRScriptConst(xdr, &vector[i]))
                return false;
        }
    }

    if (mode == XDR_DECODE) {
        if (cx->hasRunOption(JSOPTION_PCCOUNT))
            (void) script->initScriptCounts(cx);
        *scriptp = script;
    }

    return true;
}

template bool
js::XDRScript(XDRState<XDR_ENCODE> *xdr, JSScript **scriptp, JSScript *parentScript);

template bool
js::XDRScript(XDRState<XDR_DECODE> *xdr, JSScript **scriptp, JSScript *parentScript);

bool
JSScript::initScriptCounts(JSContext *cx)
{
    JS_ASSERT(!hasScriptCounts);

    size_t n = 0;

    jsbytecode *pc, *next;
    for (pc = code; pc < code + length; pc = next) {
        n += PCCounts::numCounts(JSOp(*pc));
        next = pc + GetBytecodeLength(pc);
    }

    size_t bytes = (length * sizeof(PCCounts)) + (n * sizeof(double));
    char *cursor = (char *) cx->calloc_(bytes);
    if (!cursor)
        return false;

    /* Create compartment's scriptCountsMap if necessary. */
    ScriptCountsMap *map = compartment()->scriptCountsMap;
    if (!map) {
        map = cx->new_<ScriptCountsMap>();
        if (!map || !map->init()) {
            cx->free_(cursor);
            cx->delete_(map);
            return false;
        }
        compartment()->scriptCountsMap = map;
    }

    DebugOnly<char *> base = cursor;

    ScriptCounts scriptCounts;
    scriptCounts.pcCountsVector = (PCCounts *) cursor;
    cursor += length * sizeof(PCCounts);

    for (pc = code; pc < code + length; pc = next) {
        scriptCounts.pcCountsVector[pc - code].counts = (double *) cursor;
        size_t capacity = PCCounts::numCounts(JSOp(*pc));
#ifdef DEBUG
        scriptCounts.pcCountsVector[pc - code].capacity = capacity;
#endif
        cursor += capacity * sizeof(double);
        next = pc + GetBytecodeLength(pc);
    }

    if (!map->putNew(this, scriptCounts)) {
        cx->free_(cursor);
        cx->delete_(map);
        return false;
    }
    hasScriptCounts = true; // safe to set this;  we can't fail after this point

    JS_ASSERT(size_t(cursor - base) == bytes);

    /* Enable interrupts in any interpreter frames running on this script. */
    InterpreterFrames *frames;
    for (frames = cx->runtime->interpreterFrames; frames; frames = frames->older)
        frames->enableInterruptsIfRunning(this);

    return true;
}

js::PCCounts
JSScript::getPCCounts(jsbytecode *pc) {
    JS_ASSERT(hasScriptCounts);
    JS_ASSERT(size_t(pc - code) < length);
    ScriptCountsMap *map = compartment()->scriptCountsMap;
    JS_ASSERT(map);
    ScriptCountsMap::Ptr p = map->lookup(this);
    JS_ASSERT(p);
    return p->value.pcCountsVector[pc - code];
}

ScriptCounts
JSScript::releaseScriptCounts()
{
    JS_ASSERT(hasScriptCounts);
    ScriptCountsMap *map = compartment()->scriptCountsMap;
    JS_ASSERT(map);
    ScriptCountsMap::Ptr p = map->lookup(this);
    JS_ASSERT(p);
    ScriptCounts counts = p->value;
    map->remove(p);
    hasScriptCounts = false;
    return counts;
}

void
JSScript::destroyScriptCounts(FreeOp *fop)
{
    if (hasScriptCounts) {
        ScriptCounts scriptCounts = releaseScriptCounts();
        fop->free_(scriptCounts.pcCountsVector);
    }
}

bool
JSScript::setSourceMap(JSContext *cx, jschar *sourceMap)
{
    JS_ASSERT(!hasSourceMap);

    /* Create compartment's sourceMapMap if necessary. */
    SourceMapMap *map = compartment()->sourceMapMap;
    if (!map) {
        map = cx->new_<SourceMapMap>();
        if (!map || !map->init()) {
            cx->delete_(map);
            return false;
        }
        compartment()->sourceMapMap = map;
    }

    if (!map->putNew(this, sourceMap)) {
        cx->delete_(map);
        return false;
    }
    hasSourceMap = true; // safe to set this;  we can't fail after this point

    return true;
}

jschar *
JSScript::getSourceMap() {
    JS_ASSERT(hasSourceMap);
    SourceMapMap *map = compartment()->sourceMapMap;
    JS_ASSERT(map);
    SourceMapMap::Ptr p = map->lookup(this);
    JS_ASSERT(p);
    return p->value;
}

jschar *
JSScript::releaseSourceMap()
{
    JS_ASSERT(hasSourceMap);
    SourceMapMap *map = compartment()->sourceMapMap;
    JS_ASSERT(map);
    SourceMapMap::Ptr p = map->lookup(this);
    JS_ASSERT(p);
    jschar *sourceMap = p->value;
    map->remove(p);
    hasSourceMap = false;
    return sourceMap;
}

void
JSScript::destroySourceMap(FreeOp *fop)
{
    if (hasSourceMap)
        fop->free_(releaseSourceMap());
}

/*
 * Shared script filename management.
 */

const char *
js::SaveScriptFilename(JSContext *cx, const char *filename)
{
    if (!filename)
        return NULL;

    JSRuntime *rt = cx->runtime;

    ScriptFilenameTable::AddPtr p = rt->scriptFilenameTable.lookupForAdd(filename);
    if (!p) {
        size_t size = offsetof(ScriptFilenameEntry, filename) + strlen(filename) + 1;
        ScriptFilenameEntry *entry = (ScriptFilenameEntry *) cx->malloc_(size);
        if (!entry)
            return NULL;
        entry->marked = false;
        strcpy(entry->filename, filename);

        if (!rt->scriptFilenameTable.add(p, entry)) {
            Foreground::free_(entry);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
    }

    ScriptFilenameEntry *sfe = *p;
#ifdef JSGC_INCREMENTAL
    /*
     * During the IGC we need to ensure that filename is marked whenever it is
     * accessed even if the name was already in the table. At this point old
     * scripts or exceptions pointing to the filename may no longer be
     * reachable.
     */
    if (rt->gcIncrementalState == MARK && rt->gcIsFull)
        sfe->marked = true;
#endif

    return sfe->filename;
}

void
js::SweepScriptFilenames(JSRuntime *rt)
{
    JS_ASSERT(rt->gcIsFull);
    ScriptFilenameTable &table = rt->scriptFilenameTable;
    for (ScriptFilenameTable::Enum e(table); !e.empty(); e.popFront()) {
        ScriptFilenameEntry *entry = e.front();
        if (entry->marked) {
            entry->marked = false;
        } else if (!rt->gcKeepAtoms) {
            Foreground::free_(entry);
            e.removeFront();
        }
    }
}

void
js::FreeScriptFilenames(JSRuntime *rt)
{
    ScriptFilenameTable &table = rt->scriptFilenameTable;
    for (ScriptFilenameTable::Enum e(table); !e.empty(); e.popFront())
        Foreground::free_(e.front());

    table.clear();
}

/*
 * JSScript::data has a complex, manually-controlled, memory layout.
 *
 * First are some optional array headers.  They are optional because they
 * often aren't needed, i.e. the corresponding arrays often have zero elements.
 * Each header has a bit in JSScript::hasArrayBits that indicates if it's
 * present within |data|;  from this the offset of each present array header
 * can be computed.  Each header has an accessor function in JSScript that
 * encapsulates this offset computation.
 *
 * Array type       Array elements  Accessor
 * ----------       --------------  --------
 * ConstArray       Consts          consts()
 * ObjectArray      Objects         objects()
 * ObjectArray      Regexps         regexps()
 * TryNoteArray     Try notes       trynotes()
 * ClosedSlotArray  ClosedArgs      closedArgs()
 * ClosedSlotArray  ClosedVars      closedVars()
 *
 * Then are the elements of several arrays.
 * - Most of these arrays have headers listed above (if present).  For each of
 *   these, the array pointer and the array length is stored in the header.
 * - The remaining arrays have pointers and lengths that are stored directly in
 *   JSScript.  This is because, unlike the others, they are nearly always
 *   non-zero length and so the optional-header space optimization isn't
 *   worthwhile.
 *
 * Array elements   Pointed to by         Length
 * --------------   -------------         ------
 * Consts           consts()->vector      consts()->length
 * Atoms            atoms                 natoms
 * Objects          objects()->vector     objects()->length
 * Regexps          regexps()->vector     regexps()->length
 * Try notes        trynotes()->vector    trynotes()->length
 * Closed args      closedArgs()->vector  closedArgs()->length
 * Closed vars      closedVars()->vector  closedVars()->length
 * Bytecodes        code                  length
 * Source notes     notes()               numNotes() * sizeof(jssrcnote)
 *
 * IMPORTANT: This layout has two key properties.
 * - It ensures that everything has sufficient alignment;  in particular, the
 *   consts() elements need jsval alignment.
 * - It ensures there are no gaps between elements, which saves space and makes
 *   manual layout easy.  In particular, in the second part, arrays with larger
 *   elements precede arrays with smaller elements.
 *
 * The following static assertions check these properties.
 */

#define KEEPS_JSVAL_ALIGNMENT(T) \
    (JS_ALIGNMENT_OF(jsval) % JS_ALIGNMENT_OF(T) == 0 && \
     sizeof(T) % sizeof(jsval) == 0)

#define HAS_JSVAL_ALIGNMENT(T) \
    (JS_ALIGNMENT_OF(jsval) == JS_ALIGNMENT_OF(T) && \
     sizeof(T) == sizeof(jsval))

#define NO_PADDING_BETWEEN_ENTRIES(T1, T2) \
    (JS_ALIGNMENT_OF(T1) % JS_ALIGNMENT_OF(T2) == 0)

/*
 * These assertions ensure that there is no padding between the array headers,
 * and also that the consts() elements (which follow immediately afterward) are
 * jsval-aligned.  (There is an assumption that |data| itself is jsval-aligned;
 * we check this below).
 */
JS_STATIC_ASSERT(KEEPS_JSVAL_ALIGNMENT(ConstArray));
JS_STATIC_ASSERT(KEEPS_JSVAL_ALIGNMENT(ObjectArray));       /* there are two of these */
JS_STATIC_ASSERT(KEEPS_JSVAL_ALIGNMENT(TryNoteArray));
JS_STATIC_ASSERT(KEEPS_JSVAL_ALIGNMENT(ClosedSlotArray));   /* there are two of these */

/* These assertions ensure there is no padding required between array elements. */
JS_STATIC_ASSERT(HAS_JSVAL_ALIGNMENT(HeapValue));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(HeapValue, JSAtom *));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(JSAtom *, HeapPtrObject));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(HeapPtrObject, HeapPtrObject));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(HeapPtrObject, JSTryNote));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(JSTryNote, uint32_t));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(uint32_t, uint32_t));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(uint32_t, jsbytecode));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(jsbytecode, jssrcnote));

static inline size_t
ScriptDataSize(uint32_t length, uint32_t nsrcnotes, uint32_t natoms,
               uint32_t nobjects, uint32_t nregexps, uint32_t ntrynotes, uint32_t nconsts,
               uint16_t nClosedArgs, uint16_t nClosedVars)
{
    size_t size = 0;

    if (nconsts != 0)
        size += sizeof(ConstArray) + nconsts * sizeof(Value);
    size += sizeof(JSAtom *) * natoms;
    if (nobjects != 0)
        size += sizeof(ObjectArray) + nobjects * sizeof(JSObject *);
    if (nregexps != 0)
        size += sizeof(ObjectArray) + nregexps * sizeof(JSObject *);
    if (ntrynotes != 0)
        size += sizeof(TryNoteArray) + ntrynotes * sizeof(JSTryNote);
    if (nClosedArgs != 0)
        size += sizeof(ClosedSlotArray) + nClosedArgs * sizeof(uint32_t);
    if (nClosedVars != 0)
        size += sizeof(ClosedSlotArray) + nClosedVars * sizeof(uint32_t);

    size += length * sizeof(jsbytecode);
    size += nsrcnotes * sizeof(jssrcnote);
    return size;
}

JSScript *
JSScript::Create(JSContext *cx, bool savedCallerFun, JSPrincipals *principals,
                 JSPrincipals *originPrincipals, bool compileAndGo, bool noScriptRval,
                 GlobalObject *globalObject, JSVersion version, unsigned staticLevel)
{
    JSScript *script = js_NewGCScript(cx);
    if (!script)
        return NULL;

    PodZero(script);

    script->savedCallerFun = savedCallerFun;

    /* Establish invariant: principals implies originPrincipals. */
    if (principals) {
        script->principals = principals;
        script->originPrincipals = originPrincipals ? originPrincipals : principals;
        JS_HoldPrincipals(script->principals);
        JS_HoldPrincipals(script->originPrincipals);
    } else if (originPrincipals) {
        script->originPrincipals = originPrincipals;
        JS_HoldPrincipals(script->originPrincipals);
    }

    script->compileAndGo = compileAndGo;
    script->noScriptRval = noScriptRval;
 
    script->globalObject = globalObject;

    script->version = version;
    JS_ASSERT(script->getVersion() == version);     // assert that no overflow occurred

    // This is an unsigned-to-uint16_t conversion, test for too-high values.
    // In practice, recursion in Parser and/or BytecodeEmitter will blow the
    // stack if we nest functions more than a few hundred deep, so this will
    // never trigger.  Oh well.
    if (staticLevel > UINT16_MAX) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TOO_DEEP, js_function_str);
        return NULL;
    }
    script->staticLevel = uint16_t(staticLevel);

    return script;
}

static inline uint8_t *
AllocScriptData(JSContext *cx, size_t size)
{
    uint8_t *data = static_cast<uint8_t *>(cx->calloc_(JS_ROUNDUP(size, sizeof(Value))));
    if (!data)
        return NULL;

    JS_ASSERT(size_t(data) % sizeof(Value) == 0);
    return data;
}

bool
JSScript::partiallyInit(JSContext *cx, uint32_t length, uint32_t nsrcnotes, uint32_t natoms,
                        uint32_t nobjects, uint32_t nregexps, uint32_t ntrynotes, uint32_t nconsts,
                        uint16_t nClosedArgs, uint16_t nClosedVars, uint32_t nTypeSets)
{
    JSScript *script = this;

    size_t size = ScriptDataSize(length, nsrcnotes, natoms, nobjects, nregexps,
                                 ntrynotes, nconsts, nClosedArgs, nClosedVars);
    script->data = AllocScriptData(cx, size);
    if (!script->data)
        return false;

    script->length = length;

    new (&script->bindings) Bindings(cx);

    uint8_t *cursor = data;
    if (nconsts != 0) {
        script->setHasArray(CONSTS);
        cursor += sizeof(ConstArray);
    }
    if (nobjects != 0) {
        script->setHasArray(OBJECTS);
        cursor += sizeof(ObjectArray);
    }
    if (nregexps != 0) {
        script->setHasArray(REGEXPS);
        cursor += sizeof(ObjectArray);
    }
    if (ntrynotes != 0) {
        script->setHasArray(TRYNOTES);
        cursor += sizeof(TryNoteArray);
    }
    if (nClosedArgs != 0) {
        script->setHasArray(CLOSED_ARGS);
        cursor += sizeof(ClosedSlotArray);
    }
    if (nClosedVars != 0) {
        script->setHasArray(CLOSED_VARS);
        cursor += sizeof(ClosedSlotArray);
    }

    if (nconsts != 0) {
        JS_ASSERT(reinterpret_cast<uintptr_t>(cursor) % sizeof(jsval) == 0);
        script->consts()->length = nconsts;
        script->consts()->vector = (HeapValue *)cursor;
        cursor += nconsts * sizeof(script->consts()->vector[0]);
    }

    if (natoms != 0) {
        script->natoms = natoms;
        script->atoms = reinterpret_cast<HeapPtrAtom *>(cursor);
        cursor += natoms * sizeof(script->atoms[0]);
    }

    if (nobjects != 0) {
        script->objects()->length = nobjects;
        script->objects()->vector = (HeapPtr<JSObject> *)cursor;
        cursor += nobjects * sizeof(script->objects()->vector[0]);
    }

    if (nregexps != 0) {
        script->regexps()->length = nregexps;
        script->regexps()->vector = (HeapPtr<JSObject> *)cursor;
        cursor += nregexps * sizeof(script->regexps()->vector[0]);
    }

    if (ntrynotes != 0) {
        script->trynotes()->length = ntrynotes;
        script->trynotes()->vector = reinterpret_cast<JSTryNote *>(cursor);
        size_t vectorSize = ntrynotes * sizeof(script->trynotes()->vector[0]);
#ifdef DEBUG
        memset(cursor, 0, vectorSize);
#endif
        cursor += vectorSize;
    }

    if (nClosedArgs != 0) {
        script->closedArgs()->length = nClosedArgs;
        script->closedArgs()->vector = reinterpret_cast<uint32_t *>(cursor);
        cursor += nClosedArgs * sizeof(script->closedArgs()->vector[0]);
    }

    if (nClosedVars != 0) {
        script->closedVars()->length = nClosedVars;
        script->closedVars()->vector = reinterpret_cast<uint32_t *>(cursor);
        cursor += nClosedVars * sizeof(script->closedVars()->vector[0]);
    }

    JS_ASSERT(nTypeSets <= UINT16_MAX);
    script->nTypeSets = uint16_t(nTypeSets);

    script->code = (jsbytecode *)cursor;
    JS_ASSERT(cursor + length * sizeof(jsbytecode) + nsrcnotes * sizeof(jssrcnote) == data + size);

    return true;
}

bool
JSScript::fullyInitTrivial(JSContext *cx)
{
    JSScript *script = this;

    if (!script->partiallyInit(cx, /* length = */ 1, /* nsrcnotes = */ 1, 0, 0, 0, 0, 0, 0, 0, 0))
        return false;

    script->code[0] = JSOP_STOP;
    script->notes()[0] = SRC_NULL;

    return true;
}

bool
JSScript::fullyInitFromEmitter(JSContext *cx, BytecodeEmitter *bce)
{
    JSScript *script = this;

    /* The counts of indexed things must be checked during code generation. */
    JS_ASSERT(bce->atomIndices->count() <= INDEX_LIMIT);
    JS_ASSERT(bce->objectList.length <= INDEX_LIMIT);
    JS_ASSERT(bce->regexpList.length <= INDEX_LIMIT);

    uint32_t mainLength = bce->offset();
    uint32_t prologLength = bce->prologOffset();

    if (!bce->sc->bindings.ensureShape(cx))
        return false;

    uint32_t nsrcnotes = uint32_t(bce->countFinalSourceNotes());
    uint16_t nClosedArgs = uint16_t(bce->closedArgs.length());
    JS_ASSERT(nClosedArgs == bce->closedArgs.length());
    uint16_t nClosedVars = uint16_t(bce->closedVars.length());
    JS_ASSERT(nClosedVars == bce->closedVars.length());
    if (!script->partiallyInit(cx, prologLength + mainLength, nsrcnotes, bce->atomIndices->count(),
                               bce->objectList.length, bce->regexpList.length, bce->ntrynotes,
                               bce->constList.length(), nClosedArgs, nClosedVars,
                               bce->typesetCount))
        return false;

    bce->sc->bindings.makeImmutable();

    JS_ASSERT(script->mainOffset == 0);
    script->mainOffset = prologLength;
    PodCopy<jsbytecode>(script->code, bce->prologBase(), prologLength);
    PodCopy<jsbytecode>(script->main(), bce->base(), mainLength);
    uint32_t nfixed = bce->sc->inFunction() ? bce->sc->bindings.numVars() : 0;
    JS_ASSERT(nfixed < SLOTNO_LIMIT);
    script->nfixed = uint16_t(nfixed);
    InitAtomMap(cx, bce->atomIndices.getMap(), script->atoms);

    const char *filename = bce->parser->tokenStream.getFilename();
    if (filename) {
        script->filename = SaveScriptFilename(cx, filename);
        if (!script->filename)
            return false;
    }
    script->lineno = bce->firstLine;
    if (script->nfixed + bce->maxStackDepth >= JS_BIT(16)) {
        ReportCompileErrorNumber(cx, bce->tokenStream(), NULL, JSREPORT_ERROR, JSMSG_NEED_DIET,
                                 "script");
        return false;
    }
    script->nslots = script->nfixed + bce->maxStackDepth;

    jschar *sourceMap = (jschar *) bce->parser->tokenStream.releaseSourceMap();
    if (sourceMap) {
        if (!script->setSourceMap(cx, sourceMap)) {
            cx->free_(sourceMap);
            return false;
        }
    }

    if (!FinishTakingSrcNotes(cx, bce, script->notes()))
        return false;
    if (bce->ntrynotes != 0)
        FinishTakingTryNotes(bce, script->trynotes());
    if (bce->objectList.length != 0)
        bce->objectList.finish(script->objects());
    if (bce->regexpList.length != 0)
        bce->regexpList.finish(script->regexps());
    if (bce->constList.length() != 0)
        bce->constList.finish(script->consts());
    script->strictModeCode = bce->sc->inStrictMode();
    script->bindingsAccessedDynamically = bce->sc->bindingsAccessedDynamically();
    script->funHasExtensibleScope = bce->sc->funHasExtensibleScope();
    script->hasSingletons = bce->hasSingletons;
#ifdef JS_METHODJIT
    if (cx->compartment->debugMode())
        script->debugMode = true;
#endif

    if (bce->sc->inFunction()) {
        if (bce->sc->funArgumentsHasLocalBinding()) {
            // This must precede the script->bindings.transfer() call below
            script->setArgumentsHasLocalBinding(bce->sc->argumentsLocal());
            if (bce->sc->funDefinitelyNeedsArgsObj())        
                script->setNeedsArgsObj(true);
        } else {
            JS_ASSERT(!bce->sc->funDefinitelyNeedsArgsObj());
        }
    }

    if (nClosedArgs)
        PodCopy<uint32_t>(script->closedArgs()->vector, &bce->closedArgs[0], nClosedArgs);
    if (nClosedVars)
        PodCopy<uint32_t>(script->closedVars()->vector, &bce->closedVars[0], nClosedVars);

    script->bindings.transfer(cx, &bce->sc->bindings);

    JSFunction *fun = NULL;
    if (bce->sc->inFunction()) {
        JS_ASSERT(!bce->script->noScriptRval);

        script->isGenerator = bce->sc->funIsGenerator();

        /*
         * We initialize fun->script() to be the script constructed above
         * so that the debugger has a valid fun->script().
         */
        fun = bce->sc->fun();
        JS_ASSERT(fun->isInterpreted());
        JS_ASSERT(!fun->script());
        if (bce->sc->funIsHeavyweight())
            fun->flags |= JSFUN_HEAVYWEIGHT;

        /* Mark functions which will only be executed once as singletons. */
        bool singleton =
            cx->typeInferenceEnabled() &&
            bce->parent &&
            bce->parent->checkSingletonContext();

        if (!script->typeSetFunction(cx, fun, singleton))
            return false;

        fun->setScript(script);
    }

    /* Tell the debugger about this compiled script. */
    js_CallNewScriptHook(cx, script, fun);
    if (!bce->parent) {
        GlobalObject *compileAndGoGlobal = NULL;
        if (script->compileAndGo) {
            compileAndGoGlobal = script->globalObject;
            if (!compileAndGoGlobal)
                compileAndGoGlobal = &bce->sc->scopeChain()->global();
        }
        Debugger::onNewScript(cx, script, compileAndGoGlobal);
    }

    /*
     * initScriptCounts updates scriptCountsMap if necessary. The other script
     * maps in JSCompartment are populated lazily.
     */
    if (cx->hasRunOption(JSOPTION_PCCOUNT))
        (void) script->initScriptCounts(cx);

    return true;
}

size_t
JSScript::computedSizeOfData()
{
    uint8_t *dataEnd = code + length * sizeof(jsbytecode) + numNotes() * sizeof(jssrcnote);
    JS_ASSERT(dataEnd >= data);
    return dataEnd - data;
}

size_t
JSScript::sizeOfData(JSMallocSizeOfFun mallocSizeOf)
{
    return mallocSizeOf(data);
}

/*
 * Nb: srcnotes are variable-length.  This function computes the number of
 * srcnote *slots*, which may be greater than the number of srcnotes.
 */
uint32_t
JSScript::numNotes()
{
    jssrcnote *sn;
    jssrcnote *notes_ = notes();
    for (sn = notes_; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn))
        continue;
    return sn - notes_ + 1;    /* +1 for the terminator */
}

JS_FRIEND_API(void)
js_CallNewScriptHook(JSContext *cx, JSScript *script, JSFunction *fun)
{
    JS_ASSERT(!script->callDestroyHook);
    JS_ASSERT(!script->isActiveEval);
    if (JSNewScriptHook hook = cx->runtime->debugHooks.newScriptHook) {
        AutoKeepAtoms keep(cx->runtime);
        hook(cx, script->filename, script->lineno, script, fun,
             cx->runtime->debugHooks.newScriptHookData);
    }
    script->callDestroyHook = true;
}

void
js::CallDestroyScriptHook(FreeOp *fop, JSScript *script)
{
    if (!script->callDestroyHook)
        return;

    if (JSDestroyScriptHook hook = fop->runtime()->debugHooks.destroyScriptHook)
        hook(fop, script, fop->runtime()->debugHooks.destroyScriptHookData);
    script->callDestroyHook = false;
    script->clearTraps(fop);
}

void
JSScript::finalize(FreeOp *fop)
{
    // NOTE: this JSScript may be partially initialized at this point.  E.g. we
    // may have created it and partially initialized it with
    // JSScript::Create(), but not yet finished initializing it with
    // fullyInitFromEmitter() or fullyInitTrivial().

    CallDestroyScriptHook(fop, this);

    JS_ASSERT_IF(principals, originPrincipals);
    if (principals)
        JS_DropPrincipals(fop->runtime(), principals);
    if (originPrincipals)
        JS_DropPrincipals(fop->runtime(), originPrincipals);

    if (types)
        types->destroy();

#ifdef JS_METHODJIT
    mjit::ReleaseScriptCode(fop, this);
#endif

    destroyScriptCounts(fop);
    destroySourceMap(fop);
    destroyDebugScript(fop);

    if (data) {
        JS_POISON(data, 0xdb, computedSizeOfData());
        fop->free_(data);
    }
}

namespace js {

static const uint32_t GSN_CACHE_THRESHOLD = 100;
static const uint32_t GSN_CACHE_MAP_INIT_SIZE = 20;

void
GSNCache::purge()
{
    code = NULL;
    if (map.initialized())
        map.finish();
}

} /* namespace js */

jssrcnote *
js_GetSrcNoteCached(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    size_t target = pc - script->code;
    if (target >= size_t(script->length))
        return NULL;

    GSNCache *cache = GetGSNCache(cx);
    if (cache->code == script->code) {
        JS_ASSERT(cache->map.initialized());
        GSNCache::Map::Ptr p = cache->map.lookup(pc);
        return p ? p->value : NULL;
    }

    size_t offset = 0;
    jssrcnote *result;
    for (jssrcnote *sn = script->notes(); ; sn = SN_NEXT(sn)) {
        if (SN_IS_TERMINATOR(sn)) {
            result = NULL;
            break;
        }
        offset += SN_DELTA(sn);
        if (offset == target && SN_IS_GETTABLE(sn)) {
            result = sn;
            break;
        }
    }

    if (cache->code != script->code && script->length >= GSN_CACHE_THRESHOLD) {
        unsigned nsrcnotes = 0;
        for (jssrcnote *sn = script->notes(); !SN_IS_TERMINATOR(sn);
             sn = SN_NEXT(sn)) {
            if (SN_IS_GETTABLE(sn))
                ++nsrcnotes;
        }
        if (cache->code) {
            JS_ASSERT(cache->map.initialized());
            cache->map.finish();
            cache->code = NULL;
        }
        if (cache->map.init(nsrcnotes)) {
            pc = script->code;
            for (jssrcnote *sn = script->notes(); !SN_IS_TERMINATOR(sn);
                 sn = SN_NEXT(sn)) {
                pc += SN_DELTA(sn);
                if (SN_IS_GETTABLE(sn))
                    JS_ALWAYS_TRUE(cache->map.put(pc, sn));
            }
            cache->code = script->code;
        }
    }

    return result;
}

unsigned
js::PCToLineNumber(unsigned startLine, jssrcnote *notes, jsbytecode *code, jsbytecode *pc)
{
    unsigned lineno = startLine;

    /*
     * Walk through source notes accumulating their deltas, keeping track of
     * line-number notes, until we pass the note for pc's offset within
     * script->code.
     */
    ptrdiff_t offset = 0;
    ptrdiff_t target = pc - code;
    for (jssrcnote *sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        offset += SN_DELTA(sn);
        SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            if (offset <= target)
                lineno = (unsigned) js_GetSrcNoteOffset(sn, 0);
        } else if (type == SRC_NEWLINE) {
            if (offset <= target)
                lineno++;
        }
        if (offset > target)
            break;
    }

    return lineno;
}

unsigned
js::PCToLineNumber(JSScript *script, jsbytecode *pc)
{
    /* Cope with StackFrame.pc value prior to entering js_Interpret. */
    if (!pc)
        return 0;

    return PCToLineNumber(script->lineno, script->notes(), script->code, pc);
}

/* The line number limit is the same as the jssrcnote offset limit. */
#define SN_LINE_LIMIT   (SN_3BYTE_OFFSET_FLAG << 16)

jsbytecode *
js_LineNumberToPC(JSScript *script, unsigned target)
{
    ptrdiff_t offset = 0;
    ptrdiff_t best = -1;
    unsigned lineno = script->lineno;
    unsigned bestdiff = SN_LINE_LIMIT;
    for (jssrcnote *sn = script->notes(); !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        /*
         * Exact-match only if offset is not in the prolog; otherwise use
         * nearest greater-or-equal line number match.
         */
        if (lineno == target && offset >= ptrdiff_t(script->mainOffset))
            goto out;
        if (lineno >= target) {
            unsigned diff = lineno - target;
            if (diff < bestdiff) {
                bestdiff = diff;
                best = offset;
            }
        }
        offset += SN_DELTA(sn);
        SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            lineno = (unsigned) js_GetSrcNoteOffset(sn, 0);
        } else if (type == SRC_NEWLINE) {
            lineno++;
        }
    }
    if (best >= 0)
        offset = best;
out:
    return script->code + offset;
}

JS_FRIEND_API(unsigned)
js_GetScriptLineExtent(JSScript *script)
{
    unsigned lineno = script->lineno;
    unsigned maxLineNo = 0;
    bool counting = true;
    for (jssrcnote *sn = script->notes(); !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            if (maxLineNo < lineno)
                maxLineNo = lineno;
            lineno = (unsigned) js_GetSrcNoteOffset(sn, 0);
            counting = true;
            if (maxLineNo < lineno)
                maxLineNo = lineno;
            else
                counting = false;
        } else if (type == SRC_NEWLINE) {
            if (counting)
                lineno++;
        }
    }

    if (maxLineNo > lineno)
        lineno = maxLineNo;

    return 1 + lineno - script->lineno;
}

namespace js {

unsigned
CurrentLine(JSContext *cx)
{
    return PCToLineNumber(cx->fp()->script(), cx->regs().pc);
}

void
CurrentScriptFileLineOriginSlow(JSContext *cx, const char **file, unsigned *linenop,
                                JSPrincipals **origin)
{
    ScriptFrameIter iter(cx);

    if (iter.done()) {
        *file = NULL;
        *linenop = 0;
        *origin = NULL;
        return;
    }

    JSScript *script = iter.script();
    *file = script->filename;
    *linenop = PCToLineNumber(iter.script(), iter.pc());
    *origin = script->originPrincipals;
}

}  /* namespace js */

template <class T>
static inline T *
Rebase(JSScript *dst, JSScript *src, T *srcp)
{
    size_t off = reinterpret_cast<uint8_t *>(srcp) - src->data;
    return reinterpret_cast<T *>(dst->data + off);
}

JSScript *
js::CloneScript(JSContext *cx, HandleScript src)
{
    /* NB: Keep this in sync with XDRScript. */

    uint32_t nconsts   = src->hasConsts()   ? src->consts()->length   : 0;
    uint32_t nobjects  = src->hasObjects()  ? src->objects()->length  : 0;
    uint32_t nregexps  = src->hasRegexps()  ? src->regexps()->length  : 0;
    uint32_t ntrynotes = src->hasTrynotes() ? src->trynotes()->length : 0;
    uint32_t nClosedArgs = src->numClosedArgs();
    uint32_t nClosedVars = src->numClosedVars();

    /* Script data */

    size_t size = ScriptDataSize(src->length, src->numNotes(), src->natoms,
                                 nobjects, nregexps, ntrynotes, nconsts, nClosedArgs, nClosedVars);

    uint8_t *data = AllocScriptData(cx, size);
    if (!data)
        return NULL;

    /* Bindings */

    Bindings bindings(cx);
    Bindings::AutoRooter bindingsRoot(cx, &bindings);
    BindingNames names(cx);
    if (!src->bindings.getLocalNameArray(cx, &names))
        return NULL;

    for (unsigned i = 0; i < names.length(); ++i) {
        if (JSAtom *atom = names[i].maybeAtom) {
            Rooted<JSAtom*> root(cx, atom);
            if (!bindings.add(cx, root, names[i].kind))
                return NULL;
        } else {
            uint16_t _;
            if (!bindings.addDestructuring(cx, &_))
                return NULL;
        }
    }

    if (!bindings.ensureShape(cx))
        return NULL;
    bindings.makeImmutable();

    /* Objects */

    AutoObjectVector objects(cx);
    if (nobjects != 0) {
        HeapPtrObject *vector = src->objects()->vector;
        for (unsigned i = 0; i < nobjects; i++) {
            JSObject *clone = vector[i]->isStaticBlock()
                              ? CloneStaticBlockObject(cx, vector[i]->asStaticBlock(), objects, src)
                              : CloneInterpretedFunction(cx, vector[i]->toFunction());
            if (!clone || !objects.append(clone))
                return NULL;
        }
    }

    /* RegExps */

    AutoObjectVector regexps(cx);
    for (unsigned i = 0; i < nregexps; i++) {
        HeapPtrObject *vector = src->regexps()->vector;
        for (unsigned i = 0; i < nregexps; i++) {
            JSObject *clone = CloneScriptRegExpObject(cx, vector[i]->asRegExp());
            if (!clone || !regexps.append(clone))
                return NULL;
        }
    }

    /* Now that all fallible allocation is complete, create the GC thing. */

    JSScript *dst = JSScript::Create(cx, src->savedCallerFun,
                                     cx->compartment->principals, src->originPrincipals,
                                     src->compileAndGo, src->noScriptRval,
                                     /* globalObject = */ NULL, src->getVersion(),
                                     src->staticLevel);
    if (!dst) {
        Foreground::free_(data);
        return NULL;
    }

    new (&dst->bindings) Bindings(cx);
    dst->bindings.transfer(cx, &bindings);

    /* This assignment must occur before all the Rebase calls. */
    dst->data = data;
    memcpy(data, src->data, size);

    dst->code = Rebase<jsbytecode>(dst, src, src->code);

    /* Script filenames are runtime-wide. */
    dst->filename = src->filename;

    /* Atoms are runtime-wide. */
    if (src->natoms != 0)
        dst->atoms = Rebase<HeapPtrAtom>(dst, src, src->atoms);

    dst->length = src->length;
    dst->lineno = src->lineno;
    dst->mainOffset = src->mainOffset;
    dst->natoms = src->natoms;
    dst->nfixed = src->nfixed;
    dst->nTypeSets = src->nTypeSets;
    dst->nslots = src->nslots;
    if (src->argumentsHasLocalBinding()) {
        dst->setArgumentsHasLocalBinding(src->argumentsLocal());
        if (src->analyzedArgsUsage())
            dst->setNeedsArgsObj(src->needsArgsObj());
    }
    dst->cloneHasArray(src);
    dst->strictModeCode = src->strictModeCode;
    dst->bindingsAccessedDynamically = src->bindingsAccessedDynamically;
    dst->funHasExtensibleScope = src->funHasExtensibleScope;
    dst->hasSingletons = src->hasSingletons;
    dst->isGenerator = src->isGenerator;

    /*
     * initScriptCounts updates scriptCountsMap if necessary. The other script
     * maps in JSCompartment are populated lazily.
     */
    if (cx->hasRunOption(JSOPTION_PCCOUNT))
        (void) dst->initScriptCounts(cx);

    if (nconsts != 0) {
        HeapValue *vector = Rebase<HeapValue>(dst, src, src->consts()->vector);
        dst->consts()->vector = vector;
        for (unsigned i = 0; i < nconsts; ++i)
            JS_ASSERT_IF(vector[i].isMarkable(), vector[i].toString()->isAtom());
    }
    if (nobjects != 0) {
        HeapPtrObject *vector = Rebase<HeapPtr<JSObject> >(dst, src, src->objects()->vector);
        dst->objects()->vector = vector;
        for (unsigned i = 0; i < nobjects; ++i)
            vector[i].init(objects[i]);
    }
    if (nregexps != 0) {
        HeapPtrObject *vector = Rebase<HeapPtr<JSObject> >(dst, src, src->regexps()->vector);
        dst->regexps()->vector = vector;
        for (unsigned i = 0; i < nregexps; ++i)
            vector[i].init(regexps[i]);
    }
    if (ntrynotes != 0)
        dst->trynotes()->vector = Rebase<JSTryNote>(dst, src, src->trynotes()->vector);
    if (nClosedArgs != 0)
        dst->closedArgs()->vector = Rebase<uint32_t>(dst, src, src->closedArgs()->vector);
    if (nClosedVars != 0)
        dst->closedVars()->vector = Rebase<uint32_t>(dst, src, src->closedVars()->vector);

    return dst;
}

DebugScript *
JSScript::debugScript()
{
    JS_ASSERT(hasDebugScript);
    DebugScriptMap *map = compartment()->debugScriptMap;
    JS_ASSERT(map);
    DebugScriptMap::Ptr p = map->lookup(this);
    JS_ASSERT(p);
    return p->value;
}

DebugScript *
JSScript::releaseDebugScript()
{
    JS_ASSERT(hasDebugScript);
    DebugScriptMap *map = compartment()->debugScriptMap;
    JS_ASSERT(map);
    DebugScriptMap::Ptr p = map->lookup(this);
    JS_ASSERT(p);
    DebugScript *debug = p->value;
    map->remove(p);
    hasDebugScript = false;
    return debug;
}

void
JSScript::destroyDebugScript(FreeOp *fop)
{
    if (hasDebugScript) {
        jsbytecode *end = code + length;
        for (jsbytecode *pc = code; pc < end; pc++) {
            if (BreakpointSite *site = getBreakpointSite(pc)) {
                /* Breakpoints are swept before finalization. */
                JS_ASSERT(site->firstBreakpoint() == NULL);
                site->clearTrap(fop, NULL, NULL);
                JS_ASSERT(getBreakpointSite(pc) == NULL);
            }
        }
        fop->free_(releaseDebugScript());
    }
}

bool
JSScript::ensureHasDebugScript(JSContext *cx)
{
    if (hasDebugScript)
        return true;

    size_t nbytes = offsetof(DebugScript, breakpoints) + length * sizeof(BreakpointSite*);
    DebugScript *debug = (DebugScript *) cx->calloc_(nbytes);
    if (!debug)
        return false;

    /* Create compartment's debugScriptMap if necessary. */
    DebugScriptMap *map = compartment()->debugScriptMap;
    if (!map) {
        map = cx->new_<DebugScriptMap>();
        if (!map || !map->init()) {
            cx->free_(debug);
            cx->delete_(map);
            return false;
        }
        compartment()->debugScriptMap = map;
    }

    if (!map->putNew(this, debug)) {
        cx->free_(debug);
        cx->delete_(map);
        return false;
    }
    hasDebugScript = true; // safe to set this;  we can't fail after this point

    /*
     * Ensure that any Interpret() instances running on this script have
     * interrupts enabled. The interrupts must stay enabled until the
     * debug state is destroyed.
     */
    InterpreterFrames *frames;
    for (frames = cx->runtime->interpreterFrames; frames; frames = frames->older)
        frames->enableInterruptsIfRunning(this);

    return true;
}

void
JSScript::recompileForStepMode(FreeOp *fop)
{
#ifdef JS_METHODJIT
    if (hasJITInfo()) {
        mjit::Recompiler::clearStackReferences(fop, this);
        mjit::ReleaseScriptCode(fop, this);
    }
#endif
}

bool
JSScript::tryNewStepMode(JSContext *cx, uint32_t newValue)
{
    JS_ASSERT(hasDebugScript);

    DebugScript *debug = debugScript();
    uint32_t prior = debug->stepMode;
    debug->stepMode = newValue;

    if (!prior != !newValue) {
        /* Step mode has been enabled or disabled. Alert the methodjit. */
        recompileForStepMode(cx->runtime->defaultFreeOp());

        if (!stepModeEnabled() && !debug->numSites)
            cx->free_(releaseDebugScript());
    }

    return true;
}

bool
JSScript::setStepModeFlag(JSContext *cx, bool step)
{
    if (!ensureHasDebugScript(cx))
        return false;

    return tryNewStepMode(cx, (debugScript()->stepMode & stepCountMask) |
                               (step ? stepFlagMask : 0));
}

bool
JSScript::changeStepModeCount(JSContext *cx, int delta)
{
    if (!ensureHasDebugScript(cx))
        return false;

    assertSameCompartment(cx, this);
    JS_ASSERT_IF(delta > 0, cx->compartment->debugMode());

    DebugScript *debug = debugScript();
    uint32_t count = debug->stepMode & stepCountMask;
    JS_ASSERT(((count + delta) & stepCountMask) == count + delta);
    return tryNewStepMode(cx,
                          (debug->stepMode & stepFlagMask) |
                          ((count + delta) & stepCountMask));
}

BreakpointSite *
JSScript::getOrCreateBreakpointSite(JSContext *cx, jsbytecode *pc,
                                    GlobalObject *scriptGlobal)
{
    JS_ASSERT(size_t(pc - code) < length);

    if (!ensureHasDebugScript(cx))
        return NULL;

    DebugScript *debug = debugScript();
    BreakpointSite *&site = debug->breakpoints[pc - code];

    if (!site) {
        site = cx->runtime->new_<BreakpointSite>(this, pc);
        if (!site) {
            js_ReportOutOfMemory(cx);
            return NULL;
        }
        debug->numSites++;
    }

    if (site->scriptGlobal)
        JS_ASSERT_IF(scriptGlobal, site->scriptGlobal == scriptGlobal);
    else
        site->scriptGlobal = scriptGlobal;

    return site;
}

void
JSScript::destroyBreakpointSite(FreeOp *fop, jsbytecode *pc)
{
    JS_ASSERT(unsigned(pc - code) < length);

    DebugScript *debug = debugScript();
    BreakpointSite *&site = debug->breakpoints[pc - code];
    JS_ASSERT(site);

    fop->delete_(site);
    site = NULL;

    if (--debug->numSites == 0 && !stepModeEnabled())
        fop->free_(releaseDebugScript());
}

void
JSScript::clearBreakpointsIn(FreeOp *fop, js::Debugger *dbg, JSObject *handler)
{
    if (!hasAnyBreakpointsOrStepMode())
        return;

    jsbytecode *end = code + length;
    for (jsbytecode *pc = code; pc < end; pc++) {
        BreakpointSite *site = getBreakpointSite(pc);
        if (site) {
            Breakpoint *nextbp;
            for (Breakpoint *bp = site->firstBreakpoint(); bp; bp = nextbp) {
                nextbp = bp->nextInSite();
                if ((!dbg || bp->debugger == dbg) && (!handler || bp->getHandler() == handler))
                    bp->destroy(fop);
            }
        }
    }
}

void
JSScript::clearTraps(FreeOp *fop)
{
    if (!hasAnyBreakpointsOrStepMode())
        return;

    jsbytecode *end = code + length;
    for (jsbytecode *pc = code; pc < end; pc++) {
        BreakpointSite *site = getBreakpointSite(pc);
        if (site)
            site->clearTrap(fop);
    }
}

void
JSScript::markChildren(JSTracer *trc)
{
    // NOTE: this JSScript may be partially initialized at this point.  E.g. we
    // may have created it and partially initialized it with
    // JSScript::Create(), but not yet finished initializing it with
    // fullyInitFromEmitter() or fullyInitTrivial().

    JS_ASSERT_IF(trc->runtime->gcStrictCompartmentChecking, compartment()->isCollecting());

    for (uint32_t i = 0; i < natoms; ++i) {
        if (atoms[i])
            MarkString(trc, &atoms[i], "atom");
    }

    if (hasObjects()) {
        ObjectArray *objarray = objects();
        MarkObjectRange(trc, objarray->length, objarray->vector, "objects");
    }

    if (hasRegexps()) {
        ObjectArray *objarray = regexps();
        MarkObjectRange(trc, objarray->length, objarray->vector, "objects");
    }

    if (hasConsts()) {
        ConstArray *constarray = consts();
        MarkValueRange(trc, constarray->length, constarray->vector, "consts");
    }

    if (function())
        MarkObject(trc, &function_, "function");

    if (!isCachedEval && globalObject)
        MarkObject(trc, &globalObject, "object");

    if (IS_GC_MARKING_TRACER(trc) && filename)
        MarkScriptFilename(trc->runtime, filename);

    bindings.trace(trc);

    if (types)
        types->trace(trc);

#ifdef JS_METHODJIT
    for (int constructing = 0; constructing <= 1; constructing++) {
        for (int barriers = 0; barriers <= 1; barriers++) {
            mjit::JITScript *jit = getJIT((bool) constructing, (bool) barriers);
            if (jit)
                jit->trace(trc);
        }
    }
#endif

    if (hasAnyBreakpointsOrStepMode()) {
        for (unsigned i = 0; i < length; i++) {
            BreakpointSite *site = debugScript()->breakpoints[i];
            if (site && site->trapHandler)
                MarkValue(trc, &site->trapClosure, "trap closure");
        }
    }
}

void
JSScript::setArgumentsHasLocalBinding(uint16_t local)
{
    argsHasLocalBinding_ = true;
    argsLocal_ = local;
    needsArgsAnalysis_ = true;
}

void
JSScript::setNeedsArgsObj(bool needsArgsObj)
{
    JS_ASSERT(!analyzedArgsUsage());
    JS_ASSERT_IF(needsArgsObj, argumentsHasLocalBinding());
    needsArgsAnalysis_ = false;
    needsArgsObj_ = needsArgsObj;
}

/* static */ bool
JSScript::applySpeculationFailed(JSContext *cx, JSScript *script_)
{
    Rooted<JSScript*> script(cx, script_);

    JS_ASSERT(script->analyzedArgsUsage());
    JS_ASSERT(script->argumentsHasLocalBinding());

    /*
     * It is possible that the apply speculation has already failed, everything
     * has been fixed up, but there was an outstanding magic value on the
     * stack that has just now flowed into an apply. In this case, there is
     * nothing to do; GuardFunApplySpeculation will patch in the real argsobj.
     */
    if (script->needsArgsObj())
        return true;

    script->needsArgsObj_ = true;

    const unsigned local = script->argumentsLocal();

    /*
     * By design, the apply-arguments optimization is only made when there
     * are no outstanding cases of MagicValue(JS_OPTIMIZED_ARGUMENTS) other
     * than this particular invocation of 'f.apply(x, arguments)'. Thus, there
     * are no outstanding values of MagicValue(JS_OPTIMIZED_ARGUMENTS) on the
     * stack. However, there are three things that need fixup:
     *  - there may be any number of activations of this script that don't have
     *    an argsObj that now need one.
     *  - jit code compiled (and possible active on the stack) with the static
     *    assumption of !script->needsArgsObj();
     *  - type inference data for the script assuming script->needsArgsObj; and
     */
    for (AllFramesIter i(cx->stack.space()); !i.done(); ++i) {
        StackFrame *fp = i.fp();
        if (fp->isFunctionFrame() && fp->script() == script) {
            ArgumentsObject *argsobj = ArgumentsObject::createExpected(cx, fp);
            if (!argsobj) {
                /*
                 * We can't leave stack frames with script->needsArgsObj but no
                 * arguments object. It is, however, safe to leave frames with
                 * an arguments object but !script->needsArgsObj.
                 */
                script->needsArgsObj_ = false;
                return false;
            }

            /* Note: 'arguments' may have already been overwritten. */
            if (fp->unaliasedLocal(local).isMagic(JS_OPTIMIZED_ARGUMENTS))
                fp->unaliasedLocal(local) = ObjectValue(*argsobj);
        }
    }

#ifdef JS_METHODJIT
    if (script->hasJITInfo()) {
        mjit::Recompiler::clearStackReferences(cx->runtime->defaultFreeOp(), script);
        mjit::ReleaseScriptCode(cx->runtime->defaultFreeOp(), script);
    }
#endif

    if (script->hasAnalysis() && script->analysis()->ranInference()) {
        types::AutoEnterTypeInference enter(cx);
        types::TypeScript::MonitorUnknown(cx, script, script->argumentsBytecode());
    }

    return true;
}

bool
JSScript::varIsAliased(unsigned varSlot)
{
    if (bindingsAccessedDynamically)
        return true;

    for (uint32_t i = 0; i < numClosedVars(); ++i) {
        if (closedVars()->vector[i] == varSlot) {
            JS_ASSERT(function()->isHeavyweight());
            return true;
        }
    }

    return false;
}

bool
JSScript::formalIsAliased(unsigned argSlot)
{
    return formalLivesInCallObject(argSlot) || argsObjAliasesFormals();
}

bool
JSScript::formalLivesInArgumentsObject(unsigned argSlot)
{
    return argsObjAliasesFormals() && !formalLivesInCallObject(argSlot);
}

bool
JSScript::formalLivesInCallObject(unsigned argSlot)
{
    if (bindingsAccessedDynamically)
        return true;

    for (uint32_t i = 0; i < numClosedArgs(); ++i) {
        if (closedArgs()->vector[i] == argSlot) {
            JS_ASSERT(function()->isHeavyweight());
            return true;
        }
    }

    return false;
}
