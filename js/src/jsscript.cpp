/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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
 *   Nick Fitzgerald <nfitzgerald@mozilla.com>
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

/*
 * JS script operations.
 */
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jscrashreport.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jstracer.h"
#if JS_HAS_XDR
#include "jsxdrapi.h"
#endif
#include "methodjit/MethodJIT.h"
#include "methodjit/Retcon.h"
#include "vm/Debugger.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

using namespace js;
using namespace js::gc;

namespace js {

BindingKind
Bindings::lookup(JSContext *cx, JSAtom *name, uintN *indexp) const
{
    if (!lastBinding)
        return NONE;

    Shape *shape =
        SHAPE_FETCH(Shape::search(cx->runtime, const_cast<Shape **>(&lastBinding),
                    ATOM_TO_JSID(name)));
    if (!shape)
        return NONE;

    if (indexp)
        *indexp = shape->shortid;

    if (shape->getter() == GetCallArg)
        return ARGUMENT;
    if (shape->getter() == GetCallUpvar)
        return UPVAR;

    return shape->writable() ? VARIABLE : CONSTANT;
}

bool
Bindings::add(JSContext *cx, JSAtom *name, BindingKind kind)
{
    if (!ensureShape(cx))
        return false;

    /*
     * We still follow 10.2.3 of ES3 and make argument and variable properties
     * of the Call objects enumerable. ES5 reformulated all of its Clause 10 to
     * avoid objects as activations, something we should do too.
     */
    uintN attrs = JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED;

    uint16 *indexp;
    PropertyOp getter;
    StrictPropertyOp setter;
    uint32 slot = JSObject::CALL_RESERVED_SLOTS;

    if (kind == ARGUMENT) {
        JS_ASSERT(nvars == 0);
        JS_ASSERT(nupvars == 0);
        indexp = &nargs;
        getter = GetCallArg;
        setter = SetCallArg;
        slot += nargs;
    } else if (kind == UPVAR) {
        indexp = &nupvars;
        getter = GetCallUpvar;
        setter = SetCallUpvar;
        slot = SHAPE_INVALID_SLOT;
    } else {
        JS_ASSERT(kind == VARIABLE || kind == CONSTANT);
        JS_ASSERT(nupvars == 0);

        indexp = &nvars;
        getter = GetCallVar;
        setter = SetCallVar;
        if (kind == CONSTANT)
            attrs |= JSPROP_READONLY;
        slot += nargs + nvars;
    }

    if (*indexp == BINDING_COUNT_LIMIT) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             (kind == ARGUMENT)
                             ? JSMSG_TOO_MANY_FUN_ARGS
                             : JSMSG_TOO_MANY_LOCALS);
        return false;
    }

    jsid id;
    if (!name) {
        JS_ASSERT(kind == ARGUMENT); /* destructuring */
        id = INT_TO_JSID(nargs);
    } else {
        id = ATOM_TO_JSID(name);
    }

    Shape child(id, getter, setter, slot, attrs, Shape::HAS_SHORTID, *indexp);

    Shape *shape = lastBinding->getChild(cx, child, &lastBinding);
    if (!shape)
        return false;

    JS_ASSERT(lastBinding == shape);
    ++*indexp;
    return true;
}

bool
Bindings::getLocalNameArray(JSContext *cx, Vector<JSAtom *> *namesp)
{
    JS_ASSERT(lastBinding);
    JS_ASSERT(hasLocalNames());

    Vector<JSAtom *> &names = *namesp;
    JS_ASSERT(names.empty());

    uintN n = countLocalNames();
    if (!names.growByUninitialized(n))
        return false;

#ifdef DEBUG
    JSAtom * const POISON = reinterpret_cast<JSAtom *>(0xdeadbeef);
    for (uintN i = 0; i < n; i++)
        names[i] = POISON;
#endif

    for (Shape::Range r = lastBinding; !r.empty(); r.popFront()) {
        const Shape &shape = r.front();
        uintN index = uint16(shape.shortid);

        if (shape.getter() == GetCallArg) {
            JS_ASSERT(index < nargs);
        } else if (shape.getter() == GetCallUpvar) {
            JS_ASSERT(index < nupvars);
            index += nargs + nvars;
        } else {
            JS_ASSERT(index < nvars);
            index += nargs;
        }

        if (JSID_IS_ATOM(shape.propid)) {
            names[index] = JSID_TO_ATOM(shape.propid);
        } else {
            JS_ASSERT(JSID_IS_INT(shape.propid));
            JS_ASSERT(shape.getter() == GetCallArg);
            names[index] = NULL;
        }
    }

#ifdef DEBUG
    for (uintN i = 0; i < n; i++)
        JS_ASSERT(names[i] != POISON);
#endif

    return true;
}

const Shape *
Bindings::lastArgument() const
{
    JS_ASSERT(lastBinding);

    const js::Shape *shape = lastVariable();
    if (nvars > 0) {
        while (shape->previous() && shape->getter() != GetCallArg)
            shape = shape->previous();
    }
    return shape;
}

const Shape *
Bindings::lastVariable() const
{
    JS_ASSERT(lastBinding);

    const js::Shape *shape = lastUpvar();
    if (nupvars > 0) {
        while (shape->getter() == GetCallUpvar)
            shape = shape->previous();
    }
    return shape;
}

const Shape *
Bindings::lastUpvar() const
{
    JS_ASSERT(lastBinding);
    return lastBinding;
}

int
Bindings::sharpSlotBase(JSContext *cx)
{
    JS_ASSERT(lastBinding);
#if JS_HAS_SHARP_VARS
    if (JSAtom *name = js_Atomize(cx, "#array", 6)) {
        uintN index = uintN(-1);
        DebugOnly<BindingKind> kind = lookup(cx, name, &index);
        JS_ASSERT(kind == VARIABLE);
        return int(index);
    }
#endif
    return -1;
}

void
Bindings::makeImmutable()
{
    JS_ASSERT(lastBinding);
    Shape *shape = lastBinding;
    if (shape->inDictionary()) {
        do {
            JS_ASSERT(!shape->frozen());
            shape->setFrozen();
        } while ((shape = shape->parent) != NULL);
    }
}

void
Bindings::trace(JSTracer *trc)
{
    if (lastBinding)
        MarkShape(trc, lastBinding, "shape");
}

#ifdef JS_CRASH_DIAGNOSTICS

void
CheckScript(JSScript *script, JSScript *prev)
{
    if (script->cookie1[0] != JS_SCRIPT_COOKIE || script->cookie2[0] != JS_SCRIPT_COOKIE) {
        crash::StackBuffer<sizeof(JSScript), 0x87> buf1(script);
        crash::StackBuffer<sizeof(JSScript), 0x88> buf2(prev);
        JS_OPT_ASSERT(false);
    }
}

void
CheckScriptOwner(JSScript *script, JSObject *owner)
{
    JS_OPT_ASSERT(script->ownerObject == owner);
    if (owner != JS_NEW_SCRIPT && owner != JS_CACHED_SCRIPT)
        JS_OPT_ASSERT(script->compartment() == owner->compartment());
}

#endif /* JS_CRASH_DIAGNOSTICS */

} /* namespace js */

#if JS_HAS_XDR

enum ScriptBits {
    NoScriptRval,
    SavedCallerFun,
    HasSharps,
    StrictModeCode,
    UsesEval,
    UsesArguments
};

static const char *
SaveScriptFilename(JSContext *cx, const char *filename);

JSBool
js_XDRScript(JSXDRState *xdr, JSScript **scriptp)
{
    JSScript *oldscript;
    JSBool ok;
    jsbytecode *code;
    uint32 length, lineno, nslots;
    uint32 natoms, nsrcnotes, ntrynotes, nobjects, nregexps, nconsts, i;
    uint32 prologLength, version, encodedClosedCount;
    uint16 nClosedArgs = 0, nClosedVars = 0;
    uint32 nTypeSets = 0;
    JSPrincipals *principals;
    uint32 encodeable;
    JSSecurityCallbacks *callbacks;
    uint32 scriptBits = 0;

    JSContext *cx = xdr->cx;
    JSScript *script = *scriptp;
    nsrcnotes = ntrynotes = natoms = nobjects = nregexps = nconsts = 0;
    jssrcnote *notes = NULL;
    XDRScriptState *state = xdr->state;

    JS_ASSERT(state);

    /* Should not XDR scripts optimized for a single global object. */
    JS_ASSERT_IF(script, !JSScript::isValidOffset(script->globalsOffset));

    /* XDR arguments, local vars, and upvars. */
    uint16 nargs, nvars, nupvars;
#if defined(DEBUG) || defined(__GNUC__) /* quell GCC overwarning */
    nargs = nvars = nupvars = Bindings::BINDING_COUNT_LIMIT;
#endif
    uint32 argsVars, paddingUpvars;
    if (xdr->mode == JSXDR_ENCODE) {
        nargs = script->bindings.countArgs();
        nvars = script->bindings.countVars();
        nupvars = script->bindings.countUpvars();
        argsVars = (nargs << 16) | nvars;
        paddingUpvars = nupvars;
    }
    if (!JS_XDRUint32(xdr, &argsVars) || !JS_XDRUint32(xdr, &paddingUpvars))
        return false;
    if (xdr->mode == JSXDR_DECODE) {
        nargs = argsVars >> 16;
        nvars = argsVars & 0xFFFF;
        JS_ASSERT((paddingUpvars >> 16) == 0);
        nupvars = paddingUpvars & 0xFFFF;
    }
    JS_ASSERT(nargs != Bindings::BINDING_COUNT_LIMIT);
    JS_ASSERT(nvars != Bindings::BINDING_COUNT_LIMIT);
    JS_ASSERT(nupvars != Bindings::BINDING_COUNT_LIMIT);

    Bindings bindings(cx);
    uint32 nameCount = nargs + nvars + nupvars;
    if (nameCount > 0) {
        struct AutoMark {
          JSArenaPool * const pool;
          void * const mark;
          AutoMark(JSArenaPool *pool) : pool(pool), mark(JS_ARENA_MARK(pool)) { }
          ~AutoMark() {
            JS_ARENA_RELEASE(pool, mark);
          }
        } automark(&cx->tempPool);

        /*
         * To xdr the names we prefix the names with a bitmap descriptor and
         * then xdr the names as strings. For argument names (indexes below
         * nargs) the corresponding bit in the bitmap is unset when the name
         * is null. Such null names are not encoded or decoded. For variable
         * names (indexes starting from nargs) bitmap's bit is set when the
         * name is declared as const, not as ordinary var.
         * */
        uintN bitmapLength = JS_HOWMANY(nameCount, JS_BITS_PER_UINT32);
        uint32 *bitmap;
        JS_ARENA_ALLOCATE_CAST(bitmap, uint32 *, &cx->tempPool,
                               bitmapLength * sizeof *bitmap);
        if (!bitmap) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        Vector<JSAtom *> names(cx);
        if (xdr->mode == JSXDR_ENCODE) {
            if (!script->bindings.getLocalNameArray(cx, &names))
                return false;
            PodZero(bitmap, bitmapLength);
            for (uintN i = 0; i < nameCount; i++) {
                if (i < nargs && names[i])
                    bitmap[i >> JS_BITS_PER_UINT32_LOG2] |= JS_BIT(i & (JS_BITS_PER_UINT32 - 1));
            }
        }
        for (uintN i = 0; i < bitmapLength; ++i) {
            if (!JS_XDRUint32(xdr, &bitmap[i]))
                return false;
        }

        for (uintN i = 0; i < nameCount; i++) {
            if (i < nargs &&
                !(bitmap[i >> JS_BITS_PER_UINT32_LOG2] & JS_BIT(i & (JS_BITS_PER_UINT32 - 1))))
            {
                if (xdr->mode == JSXDR_DECODE) {
                    uint16 dummy;
                    if (!bindings.addDestructuring(cx, &dummy))
                        return false;
                } else {
                    JS_ASSERT(!names[i]);
                }
                continue;
            }

            JSAtom *name;
            if (xdr->mode == JSXDR_ENCODE)
                name = names[i];
            if (!js_XDRAtom(xdr, &name))
                return false;
            if (xdr->mode == JSXDR_DECODE) {
                BindingKind kind = (i < nargs)
                                   ? ARGUMENT
                                   : (i < uintN(nargs + nvars))
                                   ? (bitmap[i >> JS_BITS_PER_UINT32_LOG2] &
                                      JS_BIT(i & (JS_BITS_PER_UINT32 - 1))
                                      ? CONSTANT
                                      : VARIABLE)
                                   : UPVAR;
                if (!bindings.add(cx, name, kind))
                    return false;
            }
        }
    }

    if (xdr->mode == JSXDR_DECODE) {
        if (!bindings.ensureShape(cx))
            return false;
        bindings.makeImmutable();
    }

    if (xdr->mode == JSXDR_ENCODE)
        length = script->length;
    if (!JS_XDRUint32(xdr, &length))
        return JS_FALSE;

    if (xdr->mode == JSXDR_ENCODE) {
        prologLength = script->mainOffset;
        JS_ASSERT(script->getVersion() != JSVERSION_UNKNOWN);
        version = (uint32)script->getVersion() | (script->nfixed << 16);
        lineno = (uint32)script->lineno;
        nslots = (uint32)script->nslots;
        nslots = (uint32)((script->staticLevel << 16) | script->nslots);
        natoms = script->natoms;

        notes = script->notes();
        nsrcnotes = script->numNotes();

        if (JSScript::isValidOffset(script->objectsOffset))
            nobjects = script->objects()->length;
        if (JSScript::isValidOffset(script->upvarsOffset))
            JS_ASSERT(script->bindings.countUpvars() == script->upvars()->length);
        if (JSScript::isValidOffset(script->regexpsOffset))
            nregexps = script->regexps()->length;
        if (JSScript::isValidOffset(script->trynotesOffset))
            ntrynotes = script->trynotes()->length;
        if (JSScript::isValidOffset(script->constOffset))
            nconsts = script->consts()->length;

        nClosedArgs = script->nClosedArgs;
        nClosedVars = script->nClosedVars;
        encodedClosedCount = (nClosedArgs << 16) | nClosedVars;

        nTypeSets = script->nTypeSets;

        if (script->noScriptRval)
            scriptBits |= (1 << NoScriptRval);
        if (script->savedCallerFun)
            scriptBits |= (1 << SavedCallerFun);
        if (script->hasSharps)
            scriptBits |= (1 << HasSharps);
        if (script->strictModeCode)
            scriptBits |= (1 << StrictModeCode);
        if (script->usesEval)
            scriptBits |= (1 << UsesEval);
        if (script->usesArguments)
            scriptBits |= (1 << UsesArguments);
        JS_ASSERT(!script->compileAndGo);
        JS_ASSERT(!script->hasSingletons);
    }

    if (!JS_XDRUint32(xdr, &prologLength))
        return JS_FALSE;
    if (!JS_XDRUint32(xdr, &version))
        return JS_FALSE;

    /*
     * To fuse allocations, we need srcnote, atom, objects, regexp, and trynote
     * counts early.
     */
    if (!JS_XDRUint32(xdr, &natoms))
        return JS_FALSE;
    if (!JS_XDRUint32(xdr, &nsrcnotes))
        return JS_FALSE;
    if (!JS_XDRUint32(xdr, &ntrynotes))
        return JS_FALSE;
    if (!JS_XDRUint32(xdr, &nobjects))
        return JS_FALSE;
    if (!JS_XDRUint32(xdr, &nregexps))
        return JS_FALSE;
    if (!JS_XDRUint32(xdr, &nconsts))
        return JS_FALSE;
    if (!JS_XDRUint32(xdr, &encodedClosedCount))
        return JS_FALSE;
    if (!JS_XDRUint32(xdr, &nTypeSets))
        return JS_FALSE;
    if (!JS_XDRUint32(xdr, &scriptBits))
        return JS_FALSE;

    if (xdr->mode == JSXDR_DECODE) {
        nClosedArgs = encodedClosedCount >> 16;
        nClosedVars = encodedClosedCount & 0xFFFF;

        /* Note: version is packed into the 32b space with another 16b value. */
        JSVersion version_ = JSVersion(version & JS_BITMASK(16));
        JS_ASSERT((version_ & VersionFlags::FULL_MASK) == uintN(version_));
        script = JSScript::NewScript(cx, length, nsrcnotes, natoms, nobjects, nupvars,
                                     nregexps, ntrynotes, nconsts, 0, nClosedArgs,
                                     nClosedVars, nTypeSets, version_);
        if (!script)
            return JS_FALSE;

        script->bindings.transfer(cx, &bindings);
        JS_ASSERT(!script->mainOffset);
        script->mainOffset = prologLength;
        script->nfixed = uint16(version >> 16);

        /* If we know nsrcnotes, we allocated space for notes in script. */
        notes = script->notes();
        *scriptp = script;

        if (scriptBits & (1 << NoScriptRval))
            script->noScriptRval = true;
        if (scriptBits & (1 << SavedCallerFun))
            script->savedCallerFun = true;
        if (scriptBits & (1 << HasSharps))
            script->hasSharps = true;
        if (scriptBits & (1 << StrictModeCode))
            script->strictModeCode = true;
        if (scriptBits & (1 << UsesEval))
            script->usesEval = true;
        if (scriptBits & (1 << UsesArguments))
            script->usesArguments = true;
    }

    /*
     * Control hereafter must goto error on failure, in order for the
     * DECODE case to destroy script.
     */
    oldscript = xdr->script;
    code = script->code;
    if (xdr->mode == JSXDR_ENCODE) {
        code = js_UntrapScriptCode(cx, script);
        if (!code)
            goto error;
    }

    xdr->script = script;
    ok = JS_XDRBytes(xdr, (char *) code, length * sizeof(jsbytecode));

    if (code != script->code)
        cx->free_(code);

    if (!ok)
        goto error;

    if (!JS_XDRBytes(xdr, (char *)notes, nsrcnotes * sizeof(jssrcnote)) ||
        !JS_XDRUint32(xdr, &lineno) ||
        !JS_XDRUint32(xdr, &nslots)) {
        goto error;
    }

    if (xdr->mode == JSXDR_DECODE && state->filename) {
        if (!state->filenameSaved) {
            const char *filename = state->filename;
            filename = SaveScriptFilename(xdr->cx, filename);
            xdr->cx->free_((void *) state->filename);
            state->filename = filename;
            state->filenameSaved = true;
            if (!filename)
                goto error;
        }
        script->filename = state->filename;
    }

    JS_ASSERT_IF(xdr->mode == JSXDR_ENCODE, state->filename == script->filename);

    callbacks = JS_GetSecurityCallbacks(cx);
    if (xdr->mode == JSXDR_ENCODE) {
        principals = script->principals;
        encodeable = callbacks && callbacks->principalsTranscoder;
        if (!JS_XDRUint32(xdr, &encodeable))
            goto error;
        if (encodeable &&
            !callbacks->principalsTranscoder(xdr, &principals)) {
            goto error;
        }
    } else {
        if (!JS_XDRUint32(xdr, &encodeable))
            goto error;
        if (encodeable) {
            if (!(callbacks && callbacks->principalsTranscoder)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_CANT_DECODE_PRINCIPALS);
                goto error;
            }
            if (!callbacks->principalsTranscoder(xdr, &principals))
                goto error;
            script->principals = principals;
        }
    }

    if (xdr->mode == JSXDR_DECODE) {
        script->lineno = (uintN)lineno;
        script->nslots = (uint16)nslots;
        script->staticLevel = (uint16)(nslots >> 16);
    }

    for (i = 0; i != natoms; ++i) {
        if (!js_XDRAtom(xdr, &script->atoms[i]))
            goto error;
    }

    /*
     * Here looping from 0-to-length to xdr objects is essential. It ensures
     * that block objects from the script->objects array will be written and
     * restored in the outer-to-inner order. js_XDRBlockObject relies on this
     * to restore the parent chain.
     */
    for (i = 0; i != nobjects; ++i) {
        JSObject **objp = &script->objects()->vector[i];
        uint32 isBlock;
        if (xdr->mode == JSXDR_ENCODE) {
            Class *clasp = (*objp)->getClass();
            JS_ASSERT(clasp == &js_FunctionClass ||
                      clasp == &js_BlockClass);
            isBlock = (clasp == &js_BlockClass) ? 1 : 0;
        }
        if (!JS_XDRUint32(xdr, &isBlock))
            goto error;
        if (isBlock == 0) {
            if (!js_XDRFunctionObject(xdr, objp))
                goto error;
        } else {
            JS_ASSERT(isBlock == 1);
            if (!js_XDRBlockObject(xdr, objp))
                goto error;
        }
    }
    for (i = 0; i != nupvars; ++i) {
        if (!JS_XDRUint32(xdr, reinterpret_cast<uint32 *>(&script->upvars()->vector[i])))
            goto error;
    }
    for (i = 0; i != nregexps; ++i) {
        if (!js_XDRRegExpObject(xdr, &script->regexps()->vector[i]))
            goto error;
    }
    for (i = 0; i != nClosedArgs; ++i) {
        if (!JS_XDRUint32(xdr, &script->closedSlots[i]))
            goto error;
    }
    for (i = 0; i != nClosedVars; ++i) {
        if (!JS_XDRUint32(xdr, &script->closedSlots[nClosedArgs + i]))
            goto error;
    }

    if (ntrynotes != 0) {
        /*
         * We combine tn->kind and tn->stackDepth when serializing as XDR is not
         * efficient when serializing small integer types.
         */
        JSTryNote *tn, *tnfirst;
        uint32 kindAndDepth;
        JS_STATIC_ASSERT(sizeof(tn->kind) == sizeof(uint8));
        JS_STATIC_ASSERT(sizeof(tn->stackDepth) == sizeof(uint16));

        tnfirst = script->trynotes()->vector;
        JS_ASSERT(script->trynotes()->length == ntrynotes);
        tn = tnfirst + ntrynotes;
        do {
            --tn;
            if (xdr->mode == JSXDR_ENCODE) {
                kindAndDepth = ((uint32)tn->kind << 16)
                               | (uint32)tn->stackDepth;
            }
            if (!JS_XDRUint32(xdr, &kindAndDepth) ||
                !JS_XDRUint32(xdr, &tn->start) ||
                !JS_XDRUint32(xdr, &tn->length)) {
                goto error;
            }
            if (xdr->mode == JSXDR_DECODE) {
                tn->kind = (uint8)(kindAndDepth >> 16);
                tn->stackDepth = (uint16)kindAndDepth;
            }
        } while (tn != tnfirst);
    }

    for (i = 0; i != nconsts; ++i) {
        if (!JS_XDRValue(xdr, Jsvalify(&script->consts()->vector[i])))
            goto error;
    }

    xdr->script = oldscript;
    return JS_TRUE;

  error:
    if (xdr->mode == JSXDR_DECODE)
        *scriptp = NULL;
    xdr->script = oldscript;
    return JS_FALSE;
}

#endif /* JS_HAS_XDR */

bool
JSPCCounters::init(JSContext *cx, size_t numBytecodes)
{
    this->numBytecodes = numBytecodes;
    size_t nbytes = sizeof(*counts) * numBytecodes * NUM_COUNTERS;
    counts = (double*) cx->calloc_(nbytes);
    if (!counts)
        return false;
    return true;
}

void
JSPCCounters::destroy(JSContext *cx)
{
    if (counts) {
        cx->free_(counts);
        counts = NULL;
    }
}

static void
script_trace(JSTracer *trc, JSObject *obj)
{
    JSScript *script = (JSScript *) obj->getPrivate();
    if (script) {
        CheckScriptOwner(script, obj);
        MarkScript(trc, script, "script");
    }
}

Class js_ScriptClass = {
    "Script",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    NULL,                 /* finalize */
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    script_trace
};

/*
 * Shared script filename management.
 */

static const char *
SaveScriptFilename(JSContext *cx, const char *filename)
{
    JSCompartment *comp = cx->compartment;

    ScriptFilenameTable::AddPtr p = comp->scriptFilenameTable.lookupForAdd(filename);
    if (!p) {
        size_t size = offsetof(ScriptFilenameEntry, filename) + strlen(filename) + 1;
        ScriptFilenameEntry *entry = (ScriptFilenameEntry *) cx->malloc_(size);
        if (!entry)
            return NULL;
        entry->marked = false;
        strcpy(entry->filename, filename);

        if (!comp->scriptFilenameTable.add(p, entry)) {
            Foreground::free_(entry);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
    }

    return (*p)->filename;
}

/*
 * Back up from a saved filename by its offset within its hash table entry.
 */
#define FILENAME_TO_SFE(fn) \
    ((ScriptFilenameEntry *) ((fn) - offsetof(ScriptFilenameEntry, filename)))

void
js_MarkScriptFilename(const char *filename)
{
    ScriptFilenameEntry *sfe = FILENAME_TO_SFE(filename);
    sfe->marked = true;
}

void
js_SweepScriptFilenames(JSCompartment *comp)
{
    ScriptFilenameTable &table = comp->scriptFilenameTable;
    for (ScriptFilenameTable::Enum e(table); !e.empty(); e.popFront()) {
        ScriptFilenameEntry *entry = e.front();
        if (entry->marked) {
            entry->marked = false;
        } else if (!comp->rt->gcKeepAtoms) {
            Foreground::free_(entry);
            e.removeFront();
        }
    }
}

/*
 * JSScript data structures memory alignment:
 *
 * JSScript
 * JSObjectArray    script objects' descriptor if JSScript.objectsOffset != 0,
 *                    use script->objects() to access it.
 * JSObjectArray    script regexps' descriptor if JSScript.regexpsOffset != 0,
 *                    use script->regexps() to access it.
 * JSTryNoteArray   script try notes' descriptor if JSScript.tryNotesOffset
 *                    != 0, use script->trynotes() to access it.
 * JSAtom *a[]      array of JSScript.natoms atoms pointed by
 *                    JSScript.atoms if any.
 * JSObject *o[]    array of script->objects()->length objects if any
 *                    pointed by script->objects()->vector.
 * JSObject *r[]    array of script->regexps()->length regexps if any
 *                    pointed by script->regexps()->vector.
 * JSTryNote t[]    array of script->trynotes()->length try notes if any
 *                    pointed by script->trynotes()->vector.
 * jsbytecode b[]   script bytecode pointed by JSScript.code.
 * jssrcnote  s[]   script source notes, use script->notes() to access it
 *
 * The alignment avoids gaps between entries as alignment requirement for each
 * subsequent structure or array is the same or divides the alignment
 * requirement for the previous one.
 *
 * The followings asserts checks that assuming that the alignment requirement
 * for JSObjectArray and JSTryNoteArray are sizeof(void *) and for JSTryNote
 * it is sizeof(uint32) as the structure consists of 3 uint32 fields.
 */
JS_STATIC_ASSERT(sizeof(JSScript) % sizeof(void *) == 0);
JS_STATIC_ASSERT(sizeof(JSObjectArray) % sizeof(void *) == 0);
JS_STATIC_ASSERT(sizeof(JSTryNoteArray) == sizeof(JSObjectArray));
JS_STATIC_ASSERT(sizeof(JSAtom *) == sizeof(JSObject *));
JS_STATIC_ASSERT(sizeof(JSObject *) % sizeof(uint32) == 0);
JS_STATIC_ASSERT(sizeof(JSTryNote) == 3 * sizeof(uint32));
JS_STATIC_ASSERT(sizeof(uint32) % sizeof(jsbytecode) == 0);
JS_STATIC_ASSERT(sizeof(jsbytecode) % sizeof(jssrcnote) == 0);

/*
 * Check that uint8 offsets is enough to reach any optional array allocated
 * after JSScript. For that we check that the maximum possible offset for
 * JSConstArray, that last optional array, still fits 1 byte and do not
 * coincide with INVALID_OFFSET.
 */
JS_STATIC_ASSERT(sizeof(JSObjectArray) +
                 sizeof(JSUpvarArray) +
                 sizeof(JSObjectArray) +
                 sizeof(JSTryNoteArray) +
                 sizeof(js::GlobalSlotArray)
                 < JSScript::INVALID_OFFSET);
JS_STATIC_ASSERT(JSScript::INVALID_OFFSET <= 255);

JSScript *
JSScript::NewScript(JSContext *cx, uint32 length, uint32 nsrcnotes, uint32 natoms,
                    uint32 nobjects, uint32 nupvars, uint32 nregexps,
                    uint32 ntrynotes, uint32 nconsts, uint32 nglobals,
                    uint16 nClosedArgs, uint16 nClosedVars, uint32 nTypeSets, JSVersion version)
{
    size_t size = sizeof(JSAtom *) * natoms;
    if (nobjects != 0)
        size += sizeof(JSObjectArray) + nobjects * sizeof(JSObject *);
    if (nupvars != 0)
        size += sizeof(JSUpvarArray) + nupvars * sizeof(uint32);
    if (nregexps != 0)
        size += sizeof(JSObjectArray) + nregexps * sizeof(JSObject *);
    if (ntrynotes != 0)
        size += sizeof(JSTryNoteArray) + ntrynotes * sizeof(JSTryNote);
    if (nglobals != 0)
        size += sizeof(GlobalSlotArray) + nglobals * sizeof(GlobalSlotArray::Entry);
    uint32 totalClosed = nClosedArgs + nClosedVars;
    if (totalClosed != 0)
        size += totalClosed * sizeof(uint32);

    /*
     * To esnure jsval alignment for the const array we place it immediately
     * after JSSomethingArray structs as their sizes all divide sizeof(jsval).
     * This works as long as the data itself is allocated with proper
     * alignment which we ensure below.
     */
    JS_STATIC_ASSERT(sizeof(JSObjectArray) % sizeof(jsval) == 0);
    JS_STATIC_ASSERT(sizeof(JSUpvarArray) % sizeof(jsval) == 0);
    JS_STATIC_ASSERT(sizeof(JSTryNoteArray) % sizeof(jsval) == 0);
    JS_STATIC_ASSERT(sizeof(GlobalSlotArray) % sizeof(jsval) == 0);
    JS_STATIC_ASSERT(sizeof(JSConstArray) % sizeof(jsval) == 0);
    if (nconsts != 0)
        size += sizeof(JSConstArray) + nconsts * sizeof(Value);

    size += length * sizeof(jsbytecode) + nsrcnotes * sizeof(jssrcnote);

    uint8 *data = NULL;
#if JS_SCRIPT_INLINE_DATA_LIMIT
    if (size <= JS_SCRIPT_INLINE_DATA_LIMIT) {
        /*
         * Check that if inlineData is big enough to store const values, we
         * can do that without any special alignment requirements given that
         * the script as a GC thing is always aligned on Cell::CellSize.
         */
        JS_STATIC_ASSERT(Cell::CellSize % sizeof(Value) == 0);
        JS_STATIC_ASSERT(JS_SCRIPT_INLINE_DATA_LIMIT < sizeof(Value) ||
                         offsetof(JSScript, inlineData) % sizeof(Value) == 0);
    } else
#endif
    {
        /*
         * We assume that calloc aligns on sizeof(Value) if the size we ask to
         * allocate divides sizeof(Value).
         */
        JS_STATIC_ASSERT(sizeof(Value) == sizeof(jsdouble));
        data = static_cast<uint8 *>(cx->calloc_(JS_ROUNDUP(size, sizeof(Value))));
        if (!data)
            return NULL;
    }

    JSScript *script = js_NewGCScript(cx);
    if (!script) {
        Foreground::free_(data);
        return NULL;
    }

    PodZero(script);
#ifdef JS_CRASH_DIAGNOSTICS
    script->cookie1[0] = script->cookie2[0] = JS_SCRIPT_COOKIE;
    script->ownerObject = JS_NEW_SCRIPT;
#endif
#if JS_SCRIPT_INLINE_DATA_LIMIT
    if (!data)
        data = script->inlineData;
#endif
    script->data  = data;
    script->length = length;
    script->version = version;
    new (&script->bindings) Bindings(cx);

    if (cx->hasRunOption(JSOPTION_PCCOUNT))
        (void) script->pcCounters.init(cx, length);

    uint8 *cursor = data;
    if (nobjects != 0) {
        script->objectsOffset = (uint8)(cursor - data);
        cursor += sizeof(JSObjectArray);
    } else {
        script->objectsOffset = JSScript::INVALID_OFFSET;
    }
    if (nupvars != 0) {
        script->upvarsOffset = (uint8)(cursor - data);
        cursor += sizeof(JSUpvarArray);
    } else {
        script->upvarsOffset = JSScript::INVALID_OFFSET;
    }
    if (nregexps != 0) {
        script->regexpsOffset = (uint8)(cursor - data);
        cursor += sizeof(JSObjectArray);
    } else {
        script->regexpsOffset = JSScript::INVALID_OFFSET;
    }
    if (ntrynotes != 0) {
        script->trynotesOffset = (uint8)(cursor - data);
        cursor += sizeof(JSTryNoteArray);
    } else {
        script->trynotesOffset = JSScript::INVALID_OFFSET;
    }
    if (nglobals != 0) {
        script->globalsOffset = (uint8)(cursor - data);
        cursor += sizeof(GlobalSlotArray);
    } else {
        script->globalsOffset = JSScript::INVALID_OFFSET;
    }
    JS_ASSERT(cursor - data < 0xFF);
    if (nconsts != 0) {
        script->constOffset = (uint8)(cursor - data);
        cursor += sizeof(JSConstArray);
    } else {
        script->constOffset = JSScript::INVALID_OFFSET;
    }

    JS_STATIC_ASSERT(sizeof(JSObjectArray) +
                     sizeof(JSUpvarArray) +
                     sizeof(JSObjectArray) +
                     sizeof(JSTryNoteArray) +
                     sizeof(GlobalSlotArray) < 0xFF);


    if (nconsts != 0) {
        JS_ASSERT(reinterpret_cast<jsuword>(cursor) % sizeof(jsval) == 0);
        script->consts()->length = nconsts;
        script->consts()->vector = reinterpret_cast<Value *>(cursor);
        cursor += nconsts * sizeof(script->consts()->vector[0]);
    }

    if (natoms != 0) {
        script->natoms = natoms;
        script->atoms = reinterpret_cast<JSAtom **>(cursor);
        cursor += natoms * sizeof(script->atoms[0]);
    }

    if (nobjects != 0) {
        script->objects()->length = nobjects;
        script->objects()->vector = reinterpret_cast<JSObject **>(cursor);
        cursor += nobjects * sizeof(script->objects()->vector[0]);
    }

    if (nregexps != 0) {
        script->regexps()->length = nregexps;
        script->regexps()->vector = reinterpret_cast<JSObject **>(cursor);
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

    if (nglobals != 0) {
        script->globals()->length = nglobals;
        script->globals()->vector = reinterpret_cast<GlobalSlotArray::Entry *>(cursor);
        cursor += nglobals * sizeof(script->globals()->vector[0]);
    }

    if (totalClosed != 0) {
        script->nClosedArgs = nClosedArgs;
        script->nClosedVars = nClosedVars;
        script->closedSlots = reinterpret_cast<uint32 *>(cursor);
        cursor += totalClosed * sizeof(uint32);
    }

    JS_ASSERT(nTypeSets <= UINT16_MAX);
    script->nTypeSets = uint16(nTypeSets);

    /*
     * NB: We allocate the vector of uint32 upvar cookies after all vectors of
     * pointers, to avoid misalignment on 64-bit platforms. See bug 514645.
     */
    if (nupvars != 0) {
        script->upvars()->length = nupvars;
        script->upvars()->vector = reinterpret_cast<UpvarCookie *>(cursor);
        cursor += nupvars * sizeof(script->upvars()->vector[0]);
    }

    script->code = (jsbytecode *)cursor;
    JS_ASSERT(cursor + length * sizeof(jsbytecode) + nsrcnotes * sizeof(jssrcnote) == data + size);

#ifdef DEBUG
    script->id_ = ++cx->compartment->types.scriptCount;
#endif

    JS_ASSERT(script->getVersion() == version);
    return script;
}

JSScript *
JSScript::NewScriptFromCG(JSContext *cx, JSCodeGenerator *cg)
{
    uint32 mainLength, prologLength, nsrcnotes, nfixed;
    JSScript *script;
    const char *filename;
    JSFunction *fun;

    /* The counts of indexed things must be checked during code generation. */
    JS_ASSERT(cg->atomIndices->count() <= INDEX_LIMIT);
    JS_ASSERT(cg->objectList.length <= INDEX_LIMIT);
    JS_ASSERT(cg->regexpList.length <= INDEX_LIMIT);

    mainLength = CG_OFFSET(cg);
    prologLength = CG_PROLOG_OFFSET(cg);

    if (!cg->bindings.ensureShape(cx))
        return NULL;

    CG_COUNT_FINAL_SRCNOTES(cg, nsrcnotes);
    uint16 nClosedArgs = uint16(cg->closedArgs.length());
    JS_ASSERT(nClosedArgs == cg->closedArgs.length());
    uint16 nClosedVars = uint16(cg->closedVars.length());
    JS_ASSERT(nClosedVars == cg->closedVars.length());
    size_t upvarIndexCount = cg->upvarIndices.hasMap() ? cg->upvarIndices->count() : 0;
    script = NewScript(cx, prologLength + mainLength, nsrcnotes,
                       cg->atomIndices->count(), cg->objectList.length,
                       upvarIndexCount, cg->regexpList.length,
                       cg->ntrynotes, cg->constList.length(),
                       cg->globalUses.length(), nClosedArgs, nClosedVars,
                       cg->typesetCount, cg->version());
    if (!script)
        return NULL;

    cg->bindings.makeImmutable();

    JS_ASSERT(script->mainOffset == 0);
    script->mainOffset = prologLength;
    memcpy(script->code, CG_PROLOG_BASE(cg), prologLength * sizeof(jsbytecode));
    memcpy(script->main(), CG_BASE(cg), mainLength * sizeof(jsbytecode));
    nfixed = cg->inFunction()
             ? cg->bindings.countVars()
             : cg->sharpSlots();
    JS_ASSERT(nfixed < SLOTNO_LIMIT);
    script->nfixed = (uint16) nfixed;
    js_InitAtomMap(cx, cg->atomIndices.getMap(), script->atoms);

    filename = cg->parser->tokenStream.getFilename();
    if (filename) {
        script->filename = SaveScriptFilename(cx, filename);
        if (!script->filename)
            return NULL;
    }
    script->lineno = cg->firstLine;
    if (script->nfixed + cg->maxStackDepth >= JS_BIT(16)) {
        ReportCompileErrorNumber(cx, CG_TS(cg), NULL, JSREPORT_ERROR, JSMSG_NEED_DIET, "script");
        return NULL;
    }
    script->nslots = script->nfixed + cg->maxStackDepth;
    script->staticLevel = uint16(cg->staticLevel);
    script->principals = cg->parser->principals;
    if (script->principals)
        JSPRINCIPALS_HOLD(cx, script->principals);

    script->sourceMap = (jschar *) cg->parser->tokenStream.releaseSourceMap();

    if (!js_FinishTakingSrcNotes(cx, cg, script->notes()))
        return NULL;
    if (cg->ntrynotes != 0)
        js_FinishTakingTryNotes(cg, script->trynotes());
    if (cg->objectList.length != 0)
        cg->objectList.finish(script->objects());
    if (cg->regexpList.length != 0)
        cg->regexpList.finish(script->regexps());
    if (cg->constList.length() != 0)
        cg->constList.finish(script->consts());
    if (cg->flags & TCF_NO_SCRIPT_RVAL)
        script->noScriptRval = true;
    if (cg->hasSharps())
        script->hasSharps = true;
    if (cg->flags & TCF_STRICT_MODE_CODE)
        script->strictModeCode = true;
    if (cg->flags & TCF_COMPILE_N_GO) {
        script->compileAndGo = true;
        const StackFrame *fp = cg->parser->callerFrame;
        if (fp && fp->isFunctionFrame())
            script->savedCallerFun = true;
    }
    if (cg->callsEval())
        script->usesEval = true;
    if (cg->flags & TCF_FUN_USES_ARGUMENTS)
        script->usesArguments = true;
    if (cg->flags & TCF_HAS_SINGLETONS)
        script->hasSingletons = true;

    if (cg->hasUpvarIndices()) {
        JS_ASSERT(cg->upvarIndices->count() <= cg->upvarMap.length());
        memcpy(script->upvars()->vector, cg->upvarMap.begin(),
               cg->upvarIndices->count() * sizeof(cg->upvarMap[0]));
        cg->upvarIndices->clear();
        cg->upvarMap.clear();
    }

    if (cg->globalUses.length()) {
        memcpy(script->globals()->vector, &cg->globalUses[0],
               cg->globalUses.length() * sizeof(GlobalSlotArray::Entry));
    }

    if (script->nClosedArgs)
        memcpy(script->closedSlots, &cg->closedArgs[0], script->nClosedArgs * sizeof(uint32));
    if (script->nClosedVars) {
        memcpy(&script->closedSlots[script->nClosedArgs], &cg->closedVars[0],
               script->nClosedVars * sizeof(uint32));
    }

    script->bindings.transfer(cx, &cg->bindings);

    fun = NULL;
    if (cg->inFunction()) {
        /*
         * We initialize fun->u.i.script to be the script constructed above
         * so that the debugger has a valid fun->script().
         */
        fun = cg->fun();
        JS_ASSERT(fun->isInterpreted());
        JS_ASSERT(!fun->script());
#ifdef DEBUG
        if (JSScript::isValidOffset(script->upvarsOffset))
            JS_ASSERT(script->upvars()->length == script->bindings.countUpvars());
        else
            JS_ASSERT(script->bindings.countUpvars() == 0);
#endif
        if (cg->flags & TCF_FUN_HEAVYWEIGHT)
            fun->flags |= JSFUN_HEAVYWEIGHT;

        /* Watch for scripts whose functions will not be cloned. These are singletons. */
        bool singleton =
            cx->typeInferenceEnabled() && cg->parent && cg->parent->compiling() &&
            cg->parent->asCodeGenerator()->checkSingletonContext();

        if (!script->typeSetFunction(cx, fun, singleton))
            return NULL;

        fun->u.i.script = script;
        script->setOwnerObject(fun);
    } else {
        /*
         * Initialize script->object, if necessary, so that the debugger has a
         * valid holder object.
         */
        if ((cg->flags & TCF_NEED_SCRIPT_OBJECT) && !js_NewScriptObject(cx, script))
            return NULL;
    }

    /* Tell the debugger about this compiled script. */
    js_CallNewScriptHook(cx, script, fun);
    if (!cg->parent) {
        Debugger::onNewScript(cx, script,
                              fun ? fun : (script->u.object ? script->u.object : cg->scopeChain()),
                              (fun || script->u.object)
                              ? Debugger::NewHeldScript
                              : Debugger::NewNonHeldScript);
    }

    return script;
}

size_t
JSScript::dataSize()
{
#if JS_SCRIPT_INLINE_DATA_LIMIT
    if (data == inlineData)
        return 0;
#endif

    uint8 *dataEnd = code + length * sizeof(jsbytecode) + numNotes() * sizeof(jssrcnote);
    JS_ASSERT(dataEnd >= data);
    return dataEnd - data;
}

void
JSScript::setOwnerObject(JSObject *owner)
{
#ifdef JS_CRASH_DIAGNOSTICS
    CheckScriptOwner(this, JS_NEW_SCRIPT);
    ownerObject = owner;
#endif
}

/*
 * Nb: srcnotes are variable-length.  This function computes the number of
 * srcnote *slots*, which may be greater than the number of srcnotes.
 */
uint32
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
    if (JSNewScriptHook hook = cx->debugHooks->newScriptHook) {
        AutoKeepAtoms keep(cx->runtime);
        hook(cx, script->filename, script->lineno, script, fun,
             cx->debugHooks->newScriptHookData);
    }
    script->callDestroyHook = true;
}

void
js_CallDestroyScriptHook(JSContext *cx, JSScript *script)
{
    if (!script->callDestroyHook)
        return;

    if (JSDestroyScriptHook hook = cx->debugHooks->destroyScriptHook)
        hook(cx, script, cx->debugHooks->destroyScriptHookData);
    script->callDestroyHook = false;
    Debugger::onDestroyScript(script);
    JS_ClearScriptTraps(cx, script);
}

void
JSScript::finalize(JSContext *cx)
{
    CheckScript(this, NULL);

    js_CallDestroyScriptHook(cx, this);

    if (principals)
        JSPRINCIPALS_DROP(cx, principals);

#ifdef JS_TRACER
    if (compartment()->hasTraceMonitor())
        PurgeScriptFragments(compartment()->traceMonitor(), this);
#endif

    if (types)
        types->destroy();

#ifdef JS_METHODJIT
    mjit::ReleaseScriptCode(cx, this);
#endif

    pcCounters.destroy(cx);

    if (sourceMap)
        cx->free_(sourceMap);

#if JS_SCRIPT_INLINE_DATA_LIMIT
    if (data != inlineData)
#endif
    {
        JS_POISON(data, 0xdb, dataSize());
        cx->free_(data);
    }
}

JSObject *
js_NewScriptObject(JSContext *cx, JSScript *script)
{
    JS_ASSERT(!script->u.object);

    JSObject *obj = NewNonFunction<WithProto::Class>(cx, &js_ScriptClass, NULL, NULL);
    if (!obj)
        return NULL;
    obj->setPrivate(script);
    script->u.object = obj;
    script->setOwnerObject(obj);

    /*
     * Clear the object's type/proto, to avoid entraining stuff. Once we no longer use the parent
     * for security checks, then we can clear the parent, too.
     */
    obj->clearType();

    return obj;
}

namespace js {

static const uint32 GSN_CACHE_THRESHOLD = 100;
static const uint32 GSN_CACHE_MAP_INIT_SIZE = 20;

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
        uintN nsrcnotes = 0;
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

uintN
js_FramePCToLineNumber(JSContext *cx, StackFrame *fp, jsbytecode *pc)
{
    return js_PCToLineNumber(cx, fp->script(), fp->hasImacropc() ? fp->imacropc() : pc);
}

uintN
js_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    JSOp op;
    JSFunction *fun;
    uintN lineno;
    ptrdiff_t offset, target;
    jssrcnote *sn;
    JSSrcNoteType type;

    /* Cope with StackFrame.pc value prior to entering js_Interpret. */
    if (!pc)
        return 0;

    /*
     * Special case: function definition needs no line number note because
     * the function's script contains its starting line number.
     */
    op = js_GetOpcode(cx, script, pc);
    if (js_CodeSpec[op].format & JOF_INDEXBASE)
        pc += js_CodeSpec[op].length;
    if (*pc == JSOP_DEFFUN) {
        GET_FUNCTION_FROM_BYTECODE(script, pc, 0, fun);
        return fun->script()->lineno;
    }

    /*
     * General case: walk through source notes accumulating their deltas,
     * keeping track of line-number notes, until we pass the note for pc's
     * offset within script->code.
     */
    lineno = script->lineno;
    offset = 0;
    target = pc - script->code;
    for (sn = script->notes(); !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        offset += SN_DELTA(sn);
        type = (JSSrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            if (offset <= target)
                lineno = (uintN) js_GetSrcNoteOffset(sn, 0);
        } else if (type == SRC_NEWLINE) {
            if (offset <= target)
                lineno++;
        }
        if (offset > target)
            break;
    }
    return lineno;
}

/* The line number limit is the same as the jssrcnote offset limit. */
#define SN_LINE_LIMIT   (SN_3BYTE_OFFSET_FLAG << 16)

jsbytecode *
js_LineNumberToPC(JSScript *script, uintN target)
{
    ptrdiff_t offset, best;
    uintN lineno, bestdiff, diff;
    jssrcnote *sn;
    JSSrcNoteType type;

    offset = 0;
    best = -1;
    lineno = script->lineno;
    bestdiff = SN_LINE_LIMIT;
    for (sn = script->notes(); !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        /*
         * Exact-match only if offset is not in the prolog; otherwise use
         * nearest greater-or-equal line number match.
         */
        if (lineno == target && offset >= ptrdiff_t(script->mainOffset))
            goto out;
        if (lineno >= target) {
            diff = lineno - target;
            if (diff < bestdiff) {
                bestdiff = diff;
                best = offset;
            }
        }
        offset += SN_DELTA(sn);
        type = (JSSrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            lineno = (uintN) js_GetSrcNoteOffset(sn, 0);
        } else if (type == SRC_NEWLINE) {
            lineno++;
        }
    }
    if (best >= 0)
        offset = best;
out:
    return script->code + offset;
}

JS_FRIEND_API(uintN)
js_GetScriptLineExtent(JSScript *script)
{

    bool counting;
    uintN lineno;
    uintN maxLineNo;
    jssrcnote *sn;
    JSSrcNoteType type;

    lineno = script->lineno;
    maxLineNo = 0;
    counting = true;
    for (sn = script->notes(); !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        type = (JSSrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            if (maxLineNo < lineno)
                maxLineNo = lineno;
            lineno = (uintN) js_GetSrcNoteOffset(sn, 0);
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

uintN
CurrentLine(JSContext *cx)
{
    return js_FramePCToLineNumber(cx, cx->fp(), cx->regs().pc);
}

const char *
CurrentScriptFileAndLineSlow(JSContext *cx, uintN *linenop)
{
    FrameRegsIter iter(cx);
    while (!iter.done() && !iter.fp()->isScriptFrame())
        ++iter;

    if (iter.done()) {
        *linenop = 0;
        return NULL;
    }

    *linenop = js_FramePCToLineNumber(cx, iter.fp(), iter.pc());
    return iter.fp()->script()->filename;
}

}  /* namespace js */

class DisablePrincipalsTranscoding {
    JSSecurityCallbacks *callbacks;
    JSPrincipalsTranscoder temp;

  public:
    DisablePrincipalsTranscoding(JSContext *cx)
      : callbacks(JS_GetRuntimeSecurityCallbacks(cx->runtime)),
        temp(NULL)
    {
        if (callbacks) {
            temp = callbacks->principalsTranscoder;
            callbacks->principalsTranscoder = NULL;
        }
    }

    ~DisablePrincipalsTranscoding() {
        if (callbacks)
            callbacks->principalsTranscoder = temp;
    }
};

class AutoJSXDRState {
public:
    AutoJSXDRState(JSXDRState *x
                   JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : xdr(x)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoJSXDRState()
    {
        JS_XDRDestroy(xdr);
    }

    operator JSXDRState*() const
    {
        return xdr;
    }

private:
    JSXDRState *const xdr;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

JSScript *
js_CloneScript(JSContext *cx, JSScript *script)
{
    JS_ASSERT(cx->compartment != script->compartment());

    // serialize script
    AutoJSXDRState w(JS_XDRNewMem(cx, JSXDR_ENCODE));
    if (!w)
        return NULL;

    // we don't want gecko to transcribe our principals for us
    DisablePrincipalsTranscoding disable(cx);

    XDRScriptState wstate(w);
#ifdef DEBUG
    wstate.filename = script->filename;
#endif
    if (!js_XDRScript(w, &script))
        return NULL;

    uint32 nbytes;
    void *p = JS_XDRMemGetData(w, &nbytes);
    if (!p)
        return NULL;

    // de-serialize script
    AutoJSXDRState r(JS_XDRNewMem(cx, JSXDR_DECODE));
    if (!r)
        return NULL;

    // Hand p off from w to r.  Don't want them to share the data
    // mem, lest they both try to free it in JS_XDRDestroy
    JS_XDRMemSetData(r, p, nbytes);
    JS_XDRMemSetData(w, NULL, 0);

    XDRScriptState rstate(r);
    rstate.filename = script->filename;
    rstate.filenameSaved = true;

    if (!js_XDRScript(r, &script))
        return NULL;

    // set the proper principals for the script
    script->principals = script->compartment()->principals;
    if (script->principals)
        JSPRINCIPALS_HOLD(cx, script->principals);

    return script;
}

void
JSScript::copyClosedSlotsTo(JSScript *other)
{
    memcpy(other->closedSlots, closedSlots, nClosedArgs + nClosedVars);
}

bool
JSScript::recompileForStepMode(JSContext *cx)
{
#ifdef JS_METHODJIT
    js::mjit::JITScript *jit = jitNormal ? jitNormal : jitCtor;
    if (jit && stepModeEnabled() != jit->singleStepMode) {
        js::mjit::Recompiler recompiler(cx, this);
        recompiler.recompile();
    }
#endif
    return true;
}

bool
JSScript::tryNewStepMode(JSContext *cx, uint32 newValue)
{
    uint32 prior = stepMode;
    stepMode = newValue;

    if (!prior != !newValue) {
        /* Step mode has been enabled or disabled. Alert the methodjit. */
        if (!recompileForStepMode(cx)) {
            stepMode = prior;
            return false;
        }

        if (newValue) {
            /* Step mode has been enabled. Alert the interpreter. */
            InterpreterFrames *frames;
            for (frames = JS_THREAD_DATA(cx)->interpreterFrames; frames; frames = frames->older)
                frames->enableInterruptsIfRunning(this);
        }
    }
    return true;
}

bool
JSScript::setStepModeFlag(JSContext *cx, bool step)
{
    return tryNewStepMode(cx, (stepMode & stepCountMask) | (step ? stepFlagMask : 0));
}

bool
JSScript::changeStepModeCount(JSContext *cx, int delta)
{
    assertSameCompartment(cx, this);
    JS_ASSERT_IF(delta > 0, cx->compartment->debugMode());

    uint32 count = stepMode & stepCountMask;
    JS_ASSERT(((count + delta) & stepCountMask) == count + delta);
    return tryNewStepMode(cx,
                          (stepMode & stepFlagMask) |
                          ((count + delta) & stepCountMask));
}
