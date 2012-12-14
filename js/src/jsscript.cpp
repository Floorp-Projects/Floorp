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
#include "ion/IonCode.h"
#include "ion/BaselineJIT.h"
#include "methodjit/Retcon.h"
#include "vm/Debugger.h"
#include "vm/Xdr.h"

#include "jsinferinlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "frontend/SharedContext-inl.h"
#include "vm/RegExpObject-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::frontend;

/* static */ unsigned
Bindings::argumentsVarIndex(JSContext *cx, InternalBindingsHandle bindings)
{
    HandlePropertyName arguments = cx->names().arguments;
    BindingIter bi(bindings);
    while (bi->name() != arguments)
        bi++;
    return bi.frameIndex();
}

bool
Bindings::initWithTemporaryStorage(JSContext *cx, InternalBindingsHandle self,
                                   unsigned numArgs, unsigned numVars,
                                   Binding *bindingArray)
{
    JS_ASSERT(!self->callObjShape_);
    JS_ASSERT(self->bindingArrayAndFlag_ == TEMPORARY_STORAGE_BIT);

    if (numArgs > UINT16_MAX || numVars > UINT16_MAX) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             self->numArgs_ > self->numVars_ ?
                             JSMSG_TOO_MANY_FUN_ARGS :
                             JSMSG_TOO_MANY_LOCALS);
        return false;
    }

    JS_ASSERT(!(uintptr_t(bindingArray) & TEMPORARY_STORAGE_BIT));
    self->bindingArrayAndFlag_ = uintptr_t(bindingArray) | TEMPORARY_STORAGE_BIT;
    self->numArgs_ = numArgs;
    self->numVars_ = numVars;

    /*
     * Get the initial shape to use when creating CallObjects for this script.
     * Since unaliased variables are, by definition, only accessed by local
     * operations and never through the scope chain, only give shapes to
     * aliased variables. While the debugger may observe any scope object at
     * any time, such accesses are mediated by DebugScopeProxy (see
     * DebugScopeProxy::handleUnaliasedAccess).
     */

    JS_STATIC_ASSERT(CallObject::RESERVED_SLOTS == 2);
    gc::AllocKind allocKind = gc::FINALIZE_OBJECT2_BACKGROUND;
    JS_ASSERT(gc::GetGCKindSlots(allocKind) == CallObject::RESERVED_SLOTS);
    RootedShape initial(cx,
        EmptyShape::getInitialShape(cx, &CallClass, NULL, cx->global(),
                                    allocKind, BaseShape::VAROBJ | BaseShape::DELEGATE));
    if (!initial)
        return false;
    self->callObjShape_.init(initial);

#ifdef DEBUG
    HashSet<PropertyName *> added(cx);
    if (!added.init())
        return false;
#endif

    BindingIter bi(self);
    unsigned slot = CallObject::RESERVED_SLOTS;
    for (unsigned i = 0, n = self->count(); i < n; i++, bi++) {
        if (!bi->aliased())
            continue;

#ifdef DEBUG
        /* The caller ensures no duplicate aliased names. */
        JS_ASSERT(!added.has(bi->name()));
        if (!added.put(bi->name()))
            return false;
#endif

        StackBaseShape base(&CallClass, cx->global(), BaseShape::VAROBJ | BaseShape::DELEGATE);

        UnrootedUnownedBaseShape nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return false;

        RootedId id(cx, NameToId(bi->name()));
        unsigned attrs = JSPROP_PERMANENT | JSPROP_ENUMERATE |
                         (bi->kind() == CONSTANT ? JSPROP_READONLY : 0);
        unsigned frameIndex = bi.frameIndex();
        StackShape child(nbase, id, slot++, 0, attrs, Shape::HAS_SHORTID, frameIndex);
        DropUnrooted(nbase);

        UnrootedShape shape = self->callObjShape_->getChildBinding(cx, child);
        if (!shape)
            return false;

        self->callObjShape_ = shape;
    }
    JS_ASSERT(!bi);

    return true;
}

uint8_t *
Bindings::switchToScriptStorage(Binding *newBindingArray)
{
    JS_ASSERT(bindingArrayUsingTemporaryStorage());
    JS_ASSERT(!(uintptr_t(newBindingArray) & TEMPORARY_STORAGE_BIT));

    PodCopy(newBindingArray, bindingArray(), count());
    bindingArrayAndFlag_ = uintptr_t(newBindingArray);
    return reinterpret_cast<uint8_t *>(newBindingArray + count());
}

bool
Bindings::clone(JSContext *cx, InternalBindingsHandle self,
                uint8_t *dstScriptData,
                HandleScript srcScript)
{
    /* The clone has the same bindingArray_ offset as 'src'. */
    Bindings &src = srcScript->bindings;
    ptrdiff_t off = (uint8_t *)src.bindingArray() - srcScript->data;
    JS_ASSERT(off >= 0);
    JS_ASSERT(off <= (srcScript->code - srcScript->data));
    Binding *dstPackedBindings = (Binding *)(dstScriptData + off);

    /*
     * Since atoms are shareable throughout the runtime, we can simply copy
     * the source's bindingArray directly.
     */
    if (!initWithTemporaryStorage(cx, self, src.numArgs(), src.numVars(), src.bindingArray()))
        return false;
    self->switchToScriptStorage(dstPackedBindings);
    return true;
}

/* static */ Bindings
RootMethods<Bindings>::initial()
{
    return Bindings();
}

template<XDRMode mode>
static bool
XDRScriptBindings(XDRState<mode> *xdr, LifoAllocScope &las, unsigned numArgs, unsigned numVars,
                  HandleScript script)
{
    JSContext *cx = xdr->cx();

    if (mode == XDR_ENCODE) {
        for (BindingIter bi(script); bi; bi++) {
            RootedAtom atom(cx, bi->name());
            if (!XDRAtom(xdr, &atom))
                return false;
        }

        for (BindingIter bi(script); bi; bi++) {
            uint8_t u8 = (uint8_t(bi->kind()) << 1) | uint8_t(bi->aliased());
            if (!xdr->codeUint8(&u8))
                return false;
        }
    } else {
        unsigned nameCount = numArgs + numVars;

        AutoValueVector atoms(cx);
        if (!atoms.resize(nameCount))
            return false;
        for (unsigned i = 0; i < nameCount; i++) {
            RootedAtom atom(cx);
            if (!XDRAtom(xdr, &atom))
                return false;
            atoms[i] = StringValue(atom);
        }

        Binding *bindingArray = las.alloc().newArrayUninitialized<Binding>(nameCount);
        if (!bindingArray)
            return false;
        for (unsigned i = 0; i < nameCount; i++) {
            uint8_t u8;
            if (!xdr->codeUint8(&u8))
                return false;

            PropertyName *name = atoms[i].toString()->asAtom().asPropertyName();
            BindingKind kind = BindingKind(u8 >> 1);
            bool aliased = bool(u8 & 1);

            bindingArray[i] = Binding(name, kind, aliased);
        }

        InternalBindingsHandle bindings(script, &script->bindings);
        if (!Bindings::initWithTemporaryStorage(cx, bindings, numArgs, numVars, bindingArray))
            return false;
    }

    return true;
}

bool
Bindings::bindingIsAliased(unsigned bindingIndex)
{
    JS_ASSERT(bindingIndex < count());
    return bindingArray()[bindingIndex].aliased();
}

void
Bindings::trace(JSTracer *trc)
{
    if (callObjShape_)
        MarkShape(trc, &callObjShape_, "callObjShape");

    /*
     * As the comment in Bindings explains, bindingsArray may point into freed
     * storage when bindingArrayUsingTemporaryStorage so we don't mark it.
     * Note: during compilation, atoms are already kept alive by gcKeepAtoms.
     */
    if (bindingArrayUsingTemporaryStorage())
        return;

    for (Binding *b = bindingArray(), *end = b + count(); b != end; b++) {
        PropertyName *name = b->name();
        MarkStringUnbarriered(trc, &name, "bindingArray");
    }
}

bool
js::FillBindingVector(HandleScript fromScript, BindingVector *vec)
{
    for (BindingIter bi(fromScript); bi; bi++) {
        if (!vec->append(*bi))
            return false;
    }

    return true;
}

template<XDRMode mode>
static bool
XDRScriptConst(XDRState<mode> *xdr, HeapValue *vp)
{
    JSContext *cx = xdr->cx();

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
        RootedAtom atom(cx);
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

static inline uint32_t
FindBlockIndex(RawScript script, StaticBlockObject &block)
{
    ObjectArray *objects = script->objects();
    HeapPtrObject *vector = objects->vector;
    unsigned length = objects->length;
    for (unsigned i = 0; i < length; ++i) {
        if (vector[i] == &block)
            return i;
    }

    JS_NOT_REACHED("Block not found");
    return UINT32_MAX;
}

template<XDRMode mode>
bool
js::XDRScript(XDRState<mode> *xdr, HandleObject enclosingScope, HandleScript enclosingScript,
              HandleFunction fun, MutableHandleScript scriptp)
{
    /* NB: Keep this in sync with CloneScript. */

    enum ScriptBits {
        NoScriptRval,
        SavedCallerFun,
        Strict,
        ContainsDynamicNameAccess,
        FunHasExtensibleScope,
        FunHasAnyAliasedFormal,
        ArgumentsHasVarBinding,
        NeedsArgsObj,
        OwnFilename,
        ParentFilename,
        IsGenerator,
        IsGeneratorExp,
        OwnSource,
        ExplicitUseStrict
    };

    uint32_t length, lineno, nslots;
    uint32_t natoms, nsrcnotes, ntrynotes, nobjects, nregexps, nconsts, i;
    uint32_t prologLength, version;
    uint32_t ndefaults = 0;
    uint32_t nTypeSets = 0;
    uint32_t scriptBits = 0;

    JSContext *cx = xdr->cx();
    RootedScript script(cx);
    nsrcnotes = ntrynotes = natoms = nobjects = nregexps = nconsts = 0;
    jssrcnote *notes = NULL;

    /* XDR arguments and vars. */
    uint16_t nargs = 0, nvars = 0;
    uint32_t argsVars = 0;
    if (mode == XDR_ENCODE) {
        script = scriptp.get();
        JS_ASSERT_IF(enclosingScript, enclosingScript->compartment() == script->compartment());

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

        nTypeSets = script->nTypeSets;
        ndefaults = script->ndefaults;

        if (script->noScriptRval)
            scriptBits |= (1 << NoScriptRval);
        if (script->savedCallerFun)
            scriptBits |= (1 << SavedCallerFun);
        if (script->strict)
            scriptBits |= (1 << Strict);
        if (script->explicitUseStrict)
            scriptBits |= (1 << ExplicitUseStrict);
        if (script->bindingsAccessedDynamically)
            scriptBits |= (1 << ContainsDynamicNameAccess);
        if (script->funHasExtensibleScope)
            scriptBits |= (1 << FunHasExtensibleScope);
        if (script->funHasAnyAliasedFormal)
            scriptBits |= (1 << FunHasAnyAliasedFormal);
        if (script->argumentsHasVarBinding())
            scriptBits |= (1 << ArgumentsHasVarBinding);
        if (script->analyzedArgsUsage() && script->needsArgsObj())
            scriptBits |= (1 << NeedsArgsObj);
        if (script->filename) {
            scriptBits |= (enclosingScript && enclosingScript->filename == script->filename)
                          ? (1 << ParentFilename)
                          : (1 << OwnFilename);
        }
        if (!enclosingScript || enclosingScript->scriptSource() != script->scriptSource())
            scriptBits |= (1 << OwnSource);
        if (script->isGenerator)
            scriptBits |= (1 << IsGenerator);
        if (script->isGeneratorExp)
            scriptBits |= (1 << IsGeneratorExp);

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
    if (!xdr->codeUint32(&nTypeSets))
        return JS_FALSE;
    if (!xdr->codeUint32(&ndefaults))
        return JS_FALSE;
    if (!xdr->codeUint32(&scriptBits))
        return JS_FALSE;

    if (mode == XDR_DECODE) {
        /* Note: version is packed into the 32b space with another 16b value. */
        JSVersion version_ = JSVersion(version & JS_BITMASK(16));
        JS_ASSERT((version_ & VersionFlags::FULL_MASK) == unsigned(version_));

        // principals and originPrincipals are set with xdr->initScriptPrincipals(script) below.
        // staticLevel is set below.
        CompileOptions options(cx);
        options.setVersion(version_)
               .setNoScriptRval(!!(scriptBits & (1 << NoScriptRval)));
        ScriptSource *ss;
        if (scriptBits & (1 << OwnSource)) {
            ss = cx->new_<ScriptSource>();
            if (!ss)
                return false;
        } else {
            JS_ASSERT(enclosingScript);
            ss = enclosingScript->scriptSource();
        }
        ScriptSourceHolder ssh(cx->runtime, ss);
        script = JSScript::Create(cx, enclosingScope, !!(scriptBits & (1 << SavedCallerFun)),
                                  options, /* staticLevel = */ 0, ss, 0, 0);
        if (!script)
            return false;
    }

    /* JSScript::partiallyInit assumes script->bindings is fully initialized. */
    LifoAllocScope las(&cx->tempLifoAlloc());
    if (!XDRScriptBindings(xdr, las, nargs, nvars, script))
        return false;

    if (mode == XDR_DECODE) {
        if (!JSScript::partiallyInit(cx, script, length, nsrcnotes, natoms, nobjects, nregexps,
                                     ntrynotes, nconsts, nTypeSets))
            return false;

        JS_ASSERT(!script->mainOffset);
        script->mainOffset = prologLength;
        script->nfixed = uint16_t(version >> 16);
        script->ndefaults = ndefaults;

        /* If we know nsrcnotes, we allocated space for notes in script. */
        notes = script->notes();
        scriptp.set(script);

        if (scriptBits & (1 << Strict))
            script->strict = true;
        if (scriptBits & (1 << ExplicitUseStrict))
            script->explicitUseStrict = true;
        if (scriptBits & (1 << ContainsDynamicNameAccess))
            script->bindingsAccessedDynamically = true;
        if (scriptBits & (1 << FunHasExtensibleScope))
            script->funHasExtensibleScope = true;
        if (scriptBits & (1 << FunHasAnyAliasedFormal))
            script->funHasAnyAliasedFormal = true;
        if (scriptBits & (1 << ArgumentsHasVarBinding))
            script->setArgumentsHasVarBinding();
        if (scriptBits & (1 << NeedsArgsObj))
            script->setNeedsArgsObj(true);
        if (scriptBits & (1 << IsGenerator))
            script->isGenerator = true;
        if (scriptBits & (1 << IsGeneratorExp))
            script->isGeneratorExp = true;
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
        JS_ASSERT(enclosingScript);
        if (mode == XDR_DECODE)
            script->filename = enclosingScript->filename;
    }

    if (scriptBits & (1 << OwnSource)) {
        if (!script->scriptSource()->performXDR<mode>(xdr))
            return false;
    }
    if (!xdr->codeUint32(&script->sourceStart))
        return false;
    if (!xdr->codeUint32(&script->sourceEnd))
        return false;

    if (mode == XDR_DECODE) {
        script->lineno = lineno;
        script->nslots = uint16_t(nslots);
        script->staticLevel = uint16_t(nslots >> 16);
        xdr->initScriptPrincipals(script);
    }

    for (i = 0; i != natoms; ++i) {
        if (mode == XDR_DECODE) {
            RootedAtom tmp(cx);
            if (!XDRAtom(xdr, &tmp))
                return false;
            script->atoms[i].init(tmp);
        } else {
            RootedAtom tmp(cx, script->atoms[i]);
            if (!XDRAtom(xdr, &tmp))
                return false;
        }
    }

    /*
     * Here looping from 0-to-length to xdr objects is essential to ensure that
     * all references to enclosing blocks (via FindBlockIndex below) happen
     * after the enclosing block has been XDR'd.
     */
    for (i = 0; i != nobjects; ++i) {
        HeapPtr<JSObject> *objp = &script->objects()->vector[i];
        uint32_t isBlock;
        if (mode == XDR_ENCODE) {
            RawObject obj = *objp;
            JS_ASSERT(obj->isFunction() || obj->isStaticBlock());
            isBlock = obj->isBlock() ? 1 : 0;
        }
        if (!xdr->codeUint32(&isBlock))
            return false;
        if (isBlock == 0) {
            /* Code the nested function's enclosing scope. */
            uint32_t funEnclosingScopeIndex = 0;
            if (mode == XDR_ENCODE) {
                StaticScopeIter ssi((*objp)->toFunction()->nonLazyScript()->enclosingStaticScope());
                if (ssi.done() || ssi.type() == StaticScopeIter::FUNCTION) {
                    JS_ASSERT(ssi.done() == !fun);
                    funEnclosingScopeIndex = UINT32_MAX;
                } else {
                    funEnclosingScopeIndex = FindBlockIndex(script, ssi.block());
                    JS_ASSERT(funEnclosingScopeIndex < i);
                }
            }
            if (!xdr->codeUint32(&funEnclosingScopeIndex))
                return false;
            Rooted<JSObject*> funEnclosingScope(cx);
            if (mode == XDR_DECODE) {
                if (funEnclosingScopeIndex == UINT32_MAX) {
                    funEnclosingScope = fun;
                } else {
                    JS_ASSERT(funEnclosingScopeIndex < i);
                    funEnclosingScope = script->objects()->vector[funEnclosingScopeIndex];
                }
            }

            RootedObject tmp(cx, *objp);
            if (!XDRInterpretedFunction(xdr, funEnclosingScope, script, &tmp))
                return false;
            *objp = tmp;
        } else {
            /* Code the nested block's enclosing scope. */
            JS_ASSERT(isBlock == 1);
            uint32_t blockEnclosingScopeIndex = 0;
            if (mode == XDR_ENCODE) {
                if (StaticBlockObject *block = (*objp)->asStaticBlock().enclosingBlock())
                    blockEnclosingScopeIndex = FindBlockIndex(script, *block);
                else
                    blockEnclosingScopeIndex = UINT32_MAX;
            }
            if (!xdr->codeUint32(&blockEnclosingScopeIndex))
                return false;
            Rooted<JSObject*> blockEnclosingScope(cx);
            if (mode == XDR_DECODE) {
                if (blockEnclosingScopeIndex != UINT32_MAX) {
                    JS_ASSERT(blockEnclosingScopeIndex < i);
                    blockEnclosingScope = script->objects()->vector[blockEnclosingScopeIndex];
                } else {
                    blockEnclosingScope = fun;
                }
            }

            Rooted<StaticBlockObject*> tmp(cx, static_cast<StaticBlockObject *>(objp->get()));
            if (!XDRStaticBlockObject(xdr, blockEnclosingScope, script, tmp.address()))
                return false;
            *objp = tmp;
        }
    }
    for (i = 0; i != nregexps; ++i) {
        if (!XDRScriptRegExpObject(xdr, &script->regexps()->vector[i]))
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
        scriptp.set(script);
    }

    return true;
}

template bool
js::XDRScript(XDRState<XDR_ENCODE> *, HandleObject, HandleScript, HandleFunction,
              MutableHandleScript);

template bool
js::XDRScript(XDRState<XDR_DECODE> *, HandleObject, HandleScript, HandleFunction,
              MutableHandleScript);

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
    char *base = (char *) cx->calloc_(bytes);
    if (!base)
        return false;

    /* Create compartment's scriptCountsMap if necessary. */
    ScriptCountsMap *map = compartment()->scriptCountsMap;
    if (!map) {
        map = cx->new_<ScriptCountsMap>();
        if (!map || !map->init()) {
            js_free(base);
            js_delete(map);
            return false;
        }
        compartment()->scriptCountsMap = map;
    }

    char *cursor = base;

    ScriptCounts scriptCounts;
    scriptCounts.pcCountsVector = (PCCounts *) cursor;
    cursor += length * sizeof(PCCounts);

    for (pc = code; pc < code + length; pc = next) {
        JS_ASSERT(uintptr_t(cursor) % sizeof(double) == 0);
        scriptCounts.pcCountsVector[pc - code].counts = (double *) cursor;
        size_t capacity = PCCounts::numCounts(JSOp(*pc));
#ifdef DEBUG
        scriptCounts.pcCountsVector[pc - code].capacity = capacity;
#endif
        cursor += capacity * sizeof(double);
        next = pc + GetBytecodeLength(pc);
    }

    if (!map->putNew(this, scriptCounts)) {
        js_free(base);
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

static inline ScriptCountsMap::Ptr GetScriptCountsMapEntry(JSScript *script)
{
    JS_ASSERT(script->hasScriptCounts);
    ScriptCountsMap *map = script->compartment()->scriptCountsMap;
    ScriptCountsMap::Ptr p = map->lookup(script);
    JS_ASSERT(p);
    return p;
}

js::PCCounts
JSScript::getPCCounts(jsbytecode *pc) {
    JS_ASSERT(size_t(pc - code) < length);
    ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
    return p->value.pcCountsVector[pc - code];
}

void
JSScript::addIonCounts(ion::IonScriptCounts *ionCounts)
{
    ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
    if (p->value.ionCounts)
        ionCounts->setPrevious(p->value.ionCounts);
    p->value.ionCounts = ionCounts;
}

ion::IonScriptCounts *
JSScript::getIonCounts()
{
    ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
    return p->value.ionCounts;
}

ScriptCounts
JSScript::releaseScriptCounts()
{
    ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
    ScriptCounts counts = p->value;
    compartment()->scriptCountsMap->remove(p);
    hasScriptCounts = false;
    return counts;
}

void
JSScript::destroyScriptCounts(FreeOp *fop)
{
    if (hasScriptCounts) {
        ScriptCounts scriptCounts = releaseScriptCounts();
        scriptCounts.destroy(fop);
    }
}

#ifdef JS_THREADSAFE
void
SourceCompressorThread::compressorThread(void *arg)
{
    PR_SetCurrentThreadName("JS Source Compressing Thread");
    static_cast<SourceCompressorThread *>(arg)->threadLoop();
}

bool
SourceCompressorThread::init()
{
    JS_ASSERT(!thread);
    lock = PR_NewLock();
    if (!lock)
        return false;
    wakeup = PR_NewCondVar(lock);
    if (!wakeup)
        return false;
    done = PR_NewCondVar(lock);
    if (!done)
        return false;
    thread = PR_CreateThread(PR_USER_THREAD, compressorThread, this, PR_PRIORITY_NORMAL,
                             PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (!thread)
        return false;
    return true;
}

void
SourceCompressorThread::finish()
{
    if (thread) {
        PR_Lock(lock);
        // We should only be compressing things when in the compiler.
        JS_ASSERT(state == IDLE);
        PR_NotifyCondVar(wakeup);
        state = SHUTDOWN;
        PR_Unlock(lock);
        PR_JoinThread(thread);
    }
    if (wakeup)
        PR_DestroyCondVar(wakeup);
    if (done)
        PR_DestroyCondVar(done);
    if (lock)
        PR_DestroyLock(lock);
}

bool
SourceCompressorThread::internalCompress()
{
    JS_ASSERT(state == COMPRESSING);
    JS_ASSERT(tok);

    ScriptSource *ss = tok->ss;
    JS_ASSERT(!ss->ready());
    size_t compressedLength = 0;
    size_t nbytes = sizeof(jschar) * ss->length_;

    // Memory allocation functions on JSRuntime and JSContext are not
    // threadsafe. We have to use the js_* variants.

#ifdef USE_ZLIB
    const size_t COMPRESS_THRESHOLD = 512;
    if (nbytes >= COMPRESS_THRESHOLD) {
        // Try to keep the maximum memory usage down by only allocating half the
        // size of the string, first.
        size_t firstSize = nbytes / 2;
        if (!ss->adjustDataSize(firstSize))
            return false;
        Compressor comp(reinterpret_cast<const unsigned char *>(tok->chars), nbytes);
        if (!comp.init())
            return false;
        comp.setOutput(ss->data.compressed, firstSize);
        bool cont = !stop;
        while (cont) {
            switch (comp.compressMore()) {
              case Compressor::CONTINUE:
                break;
              case Compressor::MOREOUTPUT: {
                if (comp.outWritten() == nbytes) {
                    cont = false;
                    break;
                }

                // The compressed output is greater than half the size of the
                // original string. Reallocate to the full size.
                if (!ss->adjustDataSize(nbytes))
                    return false;
                comp.setOutput(ss->data.compressed, nbytes);
                break;
              }
              case Compressor::DONE:
                cont = false;
                break;
              case Compressor::OOM:
                return false;
            }
            cont = cont && !stop;
        }
        compressedLength = comp.outWritten();
        if (stop || compressedLength == nbytes)
            compressedLength = 0;
    }
#endif
    if (compressedLength == 0) {
        if (!ss->adjustDataSize(nbytes))
            return false;
        PodCopy(ss->data.source, tok->chars, ss->length());
    } else {
        // Shrink the buffer to the size of the compressed data. Shouldn't fail.
        JS_ALWAYS_TRUE(ss->adjustDataSize(compressedLength));
    }
    ss->compressedLength_ = compressedLength;
    return true;
}

void
SourceCompressorThread::threadLoop()
{
    PR_Lock(lock);
    while (true) {
        switch (state) {
          case SHUTDOWN:
            PR_Unlock(lock);
            return;
          case IDLE:
            PR_WaitCondVar(wakeup, PR_INTERVAL_NO_TIMEOUT);
            break;
          case COMPRESSING:
            if (!internalCompress())
                tok->oom = true;

            // We hold the lock, so no one should have changed this.
            JS_ASSERT(state == COMPRESSING);
            state = IDLE;
            PR_NotifyCondVar(done);
            break;
        }
    }
}

void
SourceCompressorThread::compress(SourceCompressionToken *sct)
{
    if (tok)
        // We have reentered the compiler. (This can happen through the
        // debugger.) Complete the current compression before starting the next
        // one.
        waitOnCompression(tok);
    JS_ASSERT(state == IDLE);
    JS_ASSERT(!tok);
    stop = false;
    PR_Lock(lock);
    tok = sct;
    state = COMPRESSING;
    PR_NotifyCondVar(wakeup);
    PR_Unlock(lock);
}

void
SourceCompressorThread::waitOnCompression(SourceCompressionToken *userTok)
{
    JS_ASSERT(userTok == tok);
    PR_Lock(lock);
    while (state == COMPRESSING)
        PR_WaitCondVar(done, PR_INTERVAL_NO_TIMEOUT);
    JS_ASSERT(state == IDLE);
    SourceCompressionToken *saveTok = tok;
    tok = NULL;
    PR_Unlock(lock);

    JS_ASSERT(!saveTok->ss->ready());
#ifdef DEBUG
    saveTok->ss->ready_ = true;
#endif

    // Update memory accounting.
    if (!saveTok->oom)
        saveTok->cx->runtime->updateMallocCounter(NULL, saveTok->ss->computedSizeOfData());

    saveTok->ss = NULL;
    saveTok->chars = NULL;
}

void
SourceCompressorThread::abort(SourceCompressionToken *userTok)
{
    JS_ASSERT(userTok == tok);
    stop = true;
}
#endif /* JS_THREADSAFE */

static const unsigned char emptySource[] = "";

/* Adjust the amount of memory this script source uses for source data,
   reallocating if needed. */
bool
ScriptSource::adjustDataSize(size_t nbytes)
{
    // Allocating 0 bytes has undefined behavior, so special-case it.
    if (nbytes == 0) {
        if (data.compressed != emptySource)
            js_free(data.compressed);
        data.compressed = const_cast<unsigned char *>(emptySource);
        return true;
    }

    // |data.compressed| can be NULL.
    void *buf = js_realloc(data.compressed, nbytes);
    if (!buf && data.compressed != emptySource)
        js_free(data.compressed);
    data.compressed = static_cast<unsigned char *>(buf);
    return !!data.compressed;
}

void
JSScript::setScriptSource(ScriptSource *ss)
{
    JS_ASSERT(ss);
    ss->incref();
    scriptSource_ = ss;
}

/* static */ bool
JSScript::loadSource(JSContext *cx, HandleScript script, bool *worked)
{
    JS_ASSERT(!script->scriptSource_->hasSourceData());
    *worked = false;
    if (!cx->runtime->sourceHook || !script->scriptSource_->sourceRetrievable())
        return true;
    jschar *src = NULL;
    uint32_t length;
    if (!cx->runtime->sourceHook(cx, script, &src, &length))
        return false;
    if (!src)
        return true;
    ScriptSource *ss = script->scriptSource();
    ss->setSource(src, length);
    *worked = true;
    return true;
}

JSFlatString *
JSScript::sourceData(JSContext *cx)
{
    JS_ASSERT(scriptSource_->hasSourceData());
    return scriptSource_->substring(cx, sourceStart, sourceEnd);
}

JSStableString *
SourceDataCache::lookup(ScriptSource *ss)
{
    if (!map_)
        return NULL;
    if (Map::Ptr p = map_->lookup(ss))
        return p->value;
    return NULL;
}

void
SourceDataCache::put(ScriptSource *ss, JSStableString *str)
{
    if (!map_) {
        map_ = js_new<Map>();
        if (!map_)
            return;
        if (!map_->init()) {
            purge();
            return;
        }
    }

    (void) map_->put(ss, str);
}

void
SourceDataCache::purge()
{
    js_delete(map_);
    map_ = NULL;
}

JSFlatString *
ScriptSource::substring(JSContext *cx, uint32_t start, uint32_t stop)
{
    JS_ASSERT(ready());
    const jschar *chars;
#if USE_ZLIB
    Rooted<JSStableString *> cached(cx, NULL);
    if (compressed()) {
        cached = cx->runtime->sourceDataCache.lookup(this);
        if (!cached) {
            const size_t nbytes = sizeof(jschar) * (length_ + 1);
            jschar *decompressed = static_cast<jschar *>(cx->malloc_(nbytes));
            if (!decompressed)
                return NULL;
            if (!DecompressString(data.compressed, compressedLength_,
                                  reinterpret_cast<unsigned char *>(decompressed), nbytes)) {
                JS_ReportOutOfMemory(cx);
                js_free(decompressed);
                return NULL;
            }
            decompressed[length_] = 0;
            cached = js_NewString(cx, decompressed, length_);
            if (!cached) {
                js_free(decompressed);
                return NULL;
            }
            cx->runtime->sourceDataCache.put(this, cached);
        }
        chars = cached->chars().get();
        JS_ASSERT(chars);
    } else {
        chars = data.source;
    }
#else
    chars = data.source;
#endif
    return js_NewStringCopyN(cx, chars + start, stop - start);
}

bool
ScriptSource::setSourceCopy(JSContext *cx, StableCharPtr src, uint32_t length,
                            bool argumentsNotIncluded, SourceCompressionToken *tok)
{
    JS_ASSERT(!hasSourceData());
    length_ = length;
    argumentsNotIncluded_ = argumentsNotIncluded;

#ifdef JS_THREADSAFE
    if (tok && cx->runtime->useHelperThreads()) {
#ifdef DEBUG
        ready_ = false;
#endif
        tok->ss = this;
        tok->chars = src.get();
        cx->runtime->sourceCompressorThread.compress(tok);
    } else
#endif
    {
        if (!adjustDataSize(sizeof(jschar) * length))
            return false;
        PodCopy(data.source, src.get(), length_);
    }

    return true;
}

void
ScriptSource::setSource(const jschar *src, uint32_t length)
{
    JS_ASSERT(!hasSourceData());
    length_ = length;
    JS_ASSERT(!argumentsNotIncluded_);
    data.source = const_cast<jschar *>(src);
}

bool
SourceCompressionToken::complete()
{
    JS_ASSERT_IF(!ss, !chars);
#ifdef JS_THREADSAFE
    if (active()) {
        cx->runtime->sourceCompressorThread.waitOnCompression(this);
        JS_ASSERT(!active());
    }
    if (oom) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
#endif
    return true;
}

void
SourceCompressionToken::abort()
{
    JS_ASSERT(active());
#ifdef JS_THREADSAFE
    cx->runtime->sourceCompressorThread.abort(this);
#endif
}

void
ScriptSource::destroy(JSRuntime *rt)
{
    JS_ASSERT(ready());
    adjustDataSize(0);
    js_free(sourceMap_);
#ifdef DEBUG
    ready_ = false;
#endif
    js_free(this);
}

size_t
ScriptSource::sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf)
{
    JS_ASSERT(ready());

    // |data| is a union, but both members are pointers to allocated memory,
    // |emptySource|, or NULL, so just using |data.compressed| will work.
    return mallocSizeOf(this) + ((data.compressed != emptySource) ? mallocSizeOf(data.compressed) : 0);
}

template<XDRMode mode>
bool
ScriptSource::performXDR(XDRState<mode> *xdr)
{
    uint8_t hasSource = hasSourceData();
    if (!xdr->codeUint8(&hasSource))
        return false;

    uint8_t retrievable = sourceRetrievable_;
    if (!xdr->codeUint8(&retrievable))
        return false;
    sourceRetrievable_ = retrievable;

    if (hasSource && !sourceRetrievable_) {
        // Only set members when we know decoding cannot fail. This prevents the
        // script source from being partially initialized.
        uint32_t length = length_;
        if (!xdr->codeUint32(&length))
            return false;

        uint32_t compressedLength = compressedLength_;
        if (!xdr->codeUint32(&compressedLength))
            return false;

        uint8_t argumentsNotIncluded = argumentsNotIncluded_;
        if (!xdr->codeUint8(&argumentsNotIncluded))
            return false;

        size_t byteLen = compressedLength ? compressedLength : (length * sizeof(jschar));
        if (mode == XDR_DECODE) {
            if (!adjustDataSize(byteLen))
                return false;
        }
        if (!xdr->codeBytes(data.compressed, byteLen)) {
            if (mode == XDR_DECODE) {
                js_free(data.compressed);
                data.compressed = NULL;
            }
            return false;
        }
        length_ = length;
        compressedLength_ = compressedLength;
        argumentsNotIncluded_ = argumentsNotIncluded;
    }

    uint8_t haveSourceMap = hasSourceMap();
    if (!xdr->codeUint8(&haveSourceMap))
        return false;

    if (haveSourceMap) {
        uint32_t sourceMapLen = (mode == XDR_DECODE) ? 0 : js_strlen(sourceMap_);
        if (!xdr->codeUint32(&sourceMapLen))
            return false;

        if (mode == XDR_DECODE) {
            size_t byteLen = (sourceMapLen + 1) * sizeof(jschar);
            sourceMap_ = static_cast<jschar *>(xdr->cx()->malloc_(byteLen));
            if (!sourceMap_)
                return false;
        }
        if (!xdr->codeChars(sourceMap_, sourceMapLen)) {
            if (mode == XDR_DECODE) {
                js_free(sourceMap_);
                sourceMap_ = NULL;
            }
            return false;
        }
        sourceMap_[sourceMapLen] = '\0';
    }

#ifdef DEBUG
    if (mode == XDR_DECODE)
        ready_ = true;
#endif

    return true;
}

bool
ScriptSource::setSourceMap(JSContext *cx, jschar *sourceMapURL, const char *filename)
{
    JS_ASSERT(sourceMapURL);
    if (hasSourceMap()) {
        if (!JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING, js_GetErrorMessage, NULL,
                                          JSMSG_ALREADY_HAS_SOURCEMAP, filename)) {
            js_free(sourceMapURL);
            return false;
        }
    }
    sourceMap_ = sourceMapURL;
    return true;
}

const jschar *
ScriptSource::sourceMap()
{
    JS_ASSERT(hasSourceMap());
    return sourceMap_;
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
            js_free(entry);
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
    if (IsIncrementalGCInProgress(rt) && rt->gcIsFull)
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
            js_free(entry);
            e.removeFront();
        }
    }
}

void
js::FreeScriptFilenames(JSRuntime *rt)
{
    ScriptFilenameTable &table = rt->scriptFilenameTable;
    for (ScriptFilenameTable::Enum e(table); !e.empty(); e.popFront())
        js_free(e.front());

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
ScriptDataSize(uint32_t length, uint32_t nsrcnotes, uint32_t nbindings, uint32_t natoms,
               uint32_t nobjects, uint32_t nregexps, uint32_t ntrynotes, uint32_t nconsts)
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

    size += nbindings * sizeof(Binding);
    size += length * sizeof(jsbytecode);
    size += nsrcnotes * sizeof(jssrcnote);
    return size;
}

JSScript *
JSScript::Create(JSContext *cx, HandleObject enclosingScope, bool savedCallerFun,
                 const CompileOptions &options, unsigned staticLevel,
                 ScriptSource *ss, uint32_t bufStart, uint32_t bufEnd)
{
    RootedScript script(cx, js_NewGCScript(cx));
    if (!script)
        return NULL;

    PodZero(script.get());
    new (&script->bindings) Bindings;

    script->enclosingScope_ = enclosingScope;
    script->savedCallerFun = savedCallerFun;

    /* Establish invariant: principals implies originPrincipals. */
    if (options.principals) {
        script->principals = options.principals;
        script->originPrincipals
            = options.originPrincipals ? options.originPrincipals : options.principals;
        JS_HoldPrincipals(script->principals);
        JS_HoldPrincipals(script->originPrincipals);
    } else if (options.originPrincipals) {
        script->originPrincipals = options.originPrincipals;
        JS_HoldPrincipals(script->originPrincipals);
    }

    script->compileAndGo = options.compileAndGo;
    script->noScriptRval = options.noScriptRval;

    script->version = options.version;
    JS_ASSERT(script->getVersion() == options.version);     // assert that no overflow occurred

    // This is an unsigned-to-uint16_t conversion, test for too-high values.
    // In practice, recursion in Parser and/or BytecodeEmitter will blow the
    // stack if we nest functions more than a few hundred deep, so this will
    // never trigger.  Oh well.
    if (staticLevel > UINT16_MAX) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TOO_DEEP, js_function_str);
        return NULL;
    }
    script->staticLevel = uint16_t(staticLevel);

    script->setScriptSource(ss);
    script->sourceStart = bufStart;
    script->sourceEnd = bufEnd;
    script->userBit = options.userBit;

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

/* static */ bool
JSScript::partiallyInit(JSContext *cx, Handle<JSScript*> script,
                        uint32_t length, uint32_t nsrcnotes, uint32_t natoms,
                        uint32_t nobjects, uint32_t nregexps, uint32_t ntrynotes, uint32_t nconsts,
                        uint32_t nTypeSets)
{
    size_t size = ScriptDataSize(length, nsrcnotes, script->bindings.count(), natoms, nobjects,
                                 nregexps, ntrynotes, nconsts);
    script->data = AllocScriptData(cx, size);
    if (!script->data)
        return false;

    script->length = length;

    JS_ASSERT(nTypeSets <= UINT16_MAX);
    script->nTypeSets = uint16_t(nTypeSets);

    uint8_t *cursor = script->data;
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

    cursor = script->bindings.switchToScriptStorage(reinterpret_cast<Binding *>(cursor));

    script->code = (jsbytecode *)cursor;
    JS_ASSERT(cursor + length * sizeof(jsbytecode) + nsrcnotes * sizeof(jssrcnote) == script->data + size);

    return true;
}

/* static */ bool
JSScript::fullyInitTrivial(JSContext *cx, Handle<JSScript*> script)
{
    if (!partiallyInit(cx, script, /* length = */ 1, /* nsrcnotes = */ 1, 0, 0, 0, 0, 0, 0))
        return false;

    script->code[0] = JSOP_STOP;
    script->notes()[0] = SRC_NULL;

    return true;
}

/* static */ bool
JSScript::fullyInitFromEmitter(JSContext *cx, Handle<JSScript*> script, BytecodeEmitter *bce)
{
    /* The counts of indexed things must be checked during code generation. */
    JS_ASSERT(bce->atomIndices->count() <= INDEX_LIMIT);
    JS_ASSERT(bce->objectList.length <= INDEX_LIMIT);
    JS_ASSERT(bce->regexpList.length <= INDEX_LIMIT);

    uint32_t mainLength = bce->offset();
    uint32_t prologLength = bce->prologOffset();
    uint32_t nsrcnotes = uint32_t(bce->countFinalSourceNotes());
    if (!partiallyInit(cx, script, prologLength + mainLength, nsrcnotes, bce->atomIndices->count(),
                       bce->objectList.length, bce->regexpList.length, bce->tryNoteList.length(),
                       bce->constList.length(), bce->typesetCount))
        return false;

    JS_ASSERT(script->mainOffset == 0);
    script->mainOffset = prologLength;
    PodCopy<jsbytecode>(script->code, bce->prologBase(), prologLength);
    PodCopy<jsbytecode>(script->main(), bce->base(), mainLength);
    uint32_t nfixed = bce->sc->isFunction ? script->bindings.numVars() : 0;
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
        bce->reportError(NULL, JSMSG_NEED_DIET, "script");
        return false;
    }
    script->nslots = script->nfixed + bce->maxStackDepth;

    FunctionBox *funbox = bce->sc->isFunction ? bce->sc->asFunbox() : NULL;

    if (!FinishTakingSrcNotes(cx, bce, script->notes()))
        return false;
    if (bce->tryNoteList.length() != 0)
        bce->tryNoteList.finish(script->trynotes());
    if (bce->objectList.length != 0)
        bce->objectList.finish(script->objects());
    if (bce->regexpList.length != 0)
        bce->regexpList.finish(script->regexps());
    if (bce->constList.length() != 0)
        bce->constList.finish(script->consts());
    script->strict = bce->sc->strict;
    script->explicitUseStrict = bce->sc->hasExplicitUseStrict();
    script->bindingsAccessedDynamically = bce->sc->bindingsAccessedDynamically();
    script->funHasExtensibleScope = funbox ? funbox->hasExtensibleScope() : false;
    script->hasSingletons = bce->hasSingletons;
#ifdef JS_METHODJIT
    if (cx->compartment->debugMode())
        script->debugMode = true;
#endif

    if (funbox) {
        if (funbox->argumentsHasLocalBinding()) {
            // This must precede the script->bindings.transfer() call below
            script->setArgumentsHasVarBinding();
            if (funbox->definitelyNeedsArgsObj())
                script->setNeedsArgsObj(true);
        } else {
            JS_ASSERT(!funbox->definitelyNeedsArgsObj());
        }

        script->ndefaults = funbox->ndefaults;
    }

    RootedFunction fun(cx, NULL);
    if (funbox) {
        JS_ASSERT(!bce->script->noScriptRval);
        script->isGenerator = funbox->isGenerator();
        script->isGeneratorExp = funbox->inGenexpLambda;
        script->setFunction(funbox->function());
    }

    /*
     * initScriptCounts updates scriptCountsMap if necessary. The other script
     * maps in JSCompartment are populated lazily.
     */
    if (cx->hasRunOption(JSOPTION_PCCOUNT))
        (void) script->initScriptCounts(cx);

    for (unsigned i = 0, n = script->bindings.numArgs(); i < n; ++i) {
        if (script->formalIsAliased(i)) {
            script->funHasAnyAliasedFormal = true;
            break;
        }
    }

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

bool
JSScript::isShortRunning()
{
    return length < 100 &&
           hasAnalysis() &&
           !analysis()->hasFunctionCalls()
#ifdef JS_METHODJIT
           && getMaxLoopCount() < 40
#endif
           ;
}

bool
JSScript::enclosingScriptsCompiledSuccessfully() const
{
    /*
     * When a nested script is succesfully compiled, it is eagerly given the
     * static JSFunction of its enclosing script. The enclosing function's
     * 'script' field will be NULL until the enclosing script successfully
     * compiles. Thus, we can detect failed compilation by looking for
     * JSFunctions in the enclosingScope chain without scripts.
     */
    RawObject enclosing = enclosingScope_;
    while (enclosing) {
        if (enclosing->isFunction()) {
            RawFunction fun = enclosing->toFunction();
            if (!fun->hasScript())
                return false;
            enclosing = fun->nonLazyScript()->enclosingScope_;
        } else {
            enclosing = enclosing->asStaticBlock().enclosingStaticScope();
        }
    }
    return true;
}

JS_FRIEND_API(void)
js_CallNewScriptHook(JSContext *cx, JSScript *script, JSFunction *fun)
{
    JS_ASSERT(!script->isActiveEval);
    if (JSNewScriptHook hook = cx->runtime->debugHooks.newScriptHook) {
        AutoKeepAtoms keep(cx->runtime);
        hook(cx, script->filename, script->lineno, script, fun,
             cx->runtime->debugHooks.newScriptHookData);
    }
}

void
js::CallDestroyScriptHook(FreeOp *fop, RawScript script)
{
    if (JSDestroyScriptHook hook = fop->runtime()->debugHooks.destroyScriptHook)
        hook(fop, script, fop->runtime()->debugHooks.destroyScriptHookData);
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
    fop->runtime()->spsProfiler.onScriptFinalized(this);

    JS_ASSERT_IF(principals, originPrincipals);
    if (principals)
        JS_DropPrincipals(fop->runtime(), principals);
    if (originPrincipals)
        JS_DropPrincipals(fop->runtime(), originPrincipals);

    if (types)
        types->destroy();

#ifdef JS_METHODJIT
    mjit::ReleaseScriptCode(fop, this);
# ifdef JS_ION
    ion::DestroyIonScripts(fop, this);
# endif
#endif

    destroyScriptCounts(fop);
    destroyDebugScript(fop);
    scriptSource_->decref(fop->runtime());

    if (data) {
        JS_POISON(data, 0xdb, computedSizeOfData());
        fop->free_(data);
    }
}

static const uint32_t GSN_CACHE_THRESHOLD = 100;
static const uint32_t GSN_CACHE_MAP_INIT_SIZE = 20;

void
GSNCache::purge()
{
    code = NULL;
    if (map.initialized())
        map.finish();
}

jssrcnote *
js_GetSrcNote(JSContext *cx, RawScript script, jsbytecode *pc)
{
    GSNCache *cache = &cx->runtime->gsnCache;
    cx = NULL;  // nulling |cx| ensures GC can't be triggered, so |RawScript script| is safe

    size_t target = pc - script->code;
    if (target >= size_t(script->length))
        return NULL;

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
js::PCToLineNumber(unsigned startLine, jssrcnote *notes, jsbytecode *code, jsbytecode *pc,
                   unsigned *columnp)
{
    unsigned lineno = startLine;
    unsigned column = 0;

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
            column = 0;
        } else if (type == SRC_NEWLINE) {
            if (offset <= target)
                lineno++;
            column = 0;
        }

        if (offset > target)
            break;

        if (type == SRC_COLSPAN) {
            ptrdiff_t colspan = js_GetSrcNoteOffset(sn, 0);

            if (colspan >= SN_COLSPAN_DOMAIN / 2)
                colspan -= SN_COLSPAN_DOMAIN;
            JS_ASSERT(ptrdiff_t(column) + colspan >= 0);
            column += colspan;
        }
    }

    if (columnp)
        *columnp = column;

    return lineno;
}

unsigned
js::PCToLineNumber(RawScript script, jsbytecode *pc, unsigned *columnp)
{
    /* Cope with StackFrame.pc value prior to entering js_Interpret. */
    if (!pc)
        return 0;

    return PCToLineNumber(script->lineno, script->notes(), script->code, pc, columnp);
}

/* The line number limit is the same as the jssrcnote offset limit. */
#define SN_LINE_LIMIT   (SN_3BYTE_OFFSET_FLAG << 16)

jsbytecode *
js_LineNumberToPC(RawScript script, unsigned target)
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
js_GetScriptLineExtent(RawScript script)
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

unsigned
js::CurrentLine(JSContext *cx)
{
    AutoAssertNoGC nogc;
    return PCToLineNumber(cx->fp()->script(), cx->regs().pc);
}

void
js::CurrentScriptFileLineOriginSlow(JSContext *cx, const char **file, unsigned *linenop,
                                    JSPrincipals **origin)
{
    AutoAssertNoGC nogc;
    NonBuiltinScriptFrameIter iter(cx);

    if (iter.done()) {
        *file = NULL;
        *linenop = 0;
        *origin = NULL;
        return;
    }

    UnrootedScript script = iter.script();
    *file = script->filename;
    *linenop = PCToLineNumber(iter.script(), iter.pc());
    *origin = script->originPrincipals;
}

template <class T>
static inline T *
Rebase(RawScript dst, RawScript src, T *srcp)
{
    size_t off = reinterpret_cast<uint8_t *>(srcp) - src->data;
    return reinterpret_cast<T *>(dst->data + off);
}

JSScript *
js::CloneScript(JSContext *cx, HandleObject enclosingScope, HandleFunction fun, HandleScript src)
{
    AssertCanGC();

    /* NB: Keep this in sync with XDRScript. */

    uint32_t nconsts   = src->hasConsts()   ? src->consts()->length   : 0;
    uint32_t nobjects  = src->hasObjects()  ? src->objects()->length  : 0;
    uint32_t nregexps  = src->hasRegexps()  ? src->regexps()->length  : 0;
    uint32_t ntrynotes = src->hasTrynotes() ? src->trynotes()->length : 0;

    /* Script data */

    size_t size = ScriptDataSize(src->length, src->numNotes(), src->bindings.count(), src->natoms,
                                 nobjects, nregexps, ntrynotes, nconsts);

    uint8_t *data = AllocScriptData(cx, size);
    if (!data)
        return NULL;

    /* Bindings */

    Rooted<Bindings> bindings(cx);
    InternalHandle<Bindings*> bindingsHandle =
        InternalHandle<Bindings*>::fromMarkedLocation(bindings.address());
    if (!Bindings::clone(cx, bindingsHandle, data, src))
        return NULL;

    /* Objects */

    AutoObjectVector objects(cx);
    if (nobjects != 0) {
        HeapPtrObject *vector = src->objects()->vector;
        for (unsigned i = 0; i < nobjects; i++) {
            RootedObject obj(cx, vector[i]);
            RootedObject clone(cx);
            if (obj->isStaticBlock()) {
                Rooted<StaticBlockObject*> innerBlock(cx, &obj->asStaticBlock());

                RootedObject enclosingScope(cx);
                if (StaticBlockObject *enclosingBlock = innerBlock->enclosingBlock())
                    enclosingScope = objects[FindBlockIndex(src, *enclosingBlock)];
                else
                    enclosingScope = fun;

                clone = CloneStaticBlockObject(cx, enclosingScope, innerBlock);
            } else if (obj->isFunction()) {
                RootedFunction innerFun(cx, obj->toFunction());

                StaticScopeIter ssi(innerFun->nonLazyScript()->enclosingStaticScope());
                RootedObject enclosingScope(cx);
                if (!ssi.done() && ssi.type() == StaticScopeIter::BLOCK)
                    enclosingScope = objects[FindBlockIndex(src, ssi.block())];
                else
                    enclosingScope = fun;

                clone = CloneInterpretedFunction(cx, enclosingScope, innerFun);
            } else {
                /*
                 * Clone object literals emitted for the JSOP_NEWOBJECT opcode. We only emit that
                 * instead of the less-optimized JSOP_NEWINIT for self-hosted code or code compiled
                 * with JSOPTION_COMPILE_N_GO set. As we don't clone the latter type of code, this
                 * case should only ever be hit when cloning objects from self-hosted code.
                 */
                clone = CloneObjectLiteral(cx, cx->global(), obj);
            }
            if (!clone || !objects.append(clone))
                return NULL;
        }
    }

    /* RegExps */

    AutoObjectVector regexps(cx);
    for (unsigned i = 0; i < nregexps; i++) {
        HeapPtrObject *vector = src->regexps()->vector;
        for (unsigned i = 0; i < nregexps; i++) {
            RawObject clone = CloneScriptRegExpObject(cx, vector[i]->asRegExp());
            if (!clone || !regexps.append(clone))
                return NULL;
        }
    }

    /* Now that all fallible allocation is complete, create the GC thing. */

    CompileOptions options(cx);
    options.setPrincipals(cx->compartment->principals)
           .setOriginPrincipals(src->originPrincipals)
           .setCompileAndGo(src->compileAndGo)
           .setNoScriptRval(src->noScriptRval)
           .setVersion(src->getVersion())
           .setUserBit(src->userBit);
    RootedScript dst(cx, JSScript::Create(cx, enclosingScope, src->savedCallerFun,
                                          options, src->staticLevel,
                                          src->scriptSource(), src->sourceStart, src->sourceEnd));
    if (!dst) {
        js_free(data);
        return NULL;
    }
    AutoAssertNoGC nogc;

    dst->bindings = bindings;

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
    if (src->argumentsHasVarBinding()) {
        dst->setArgumentsHasVarBinding();
        if (src->analyzedArgsUsage())
            dst->setNeedsArgsObj(src->needsArgsObj());
    }
    dst->cloneHasArray(src);
    dst->strict = src->strict;
    dst->explicitUseStrict = src->explicitUseStrict;
    dst->bindingsAccessedDynamically = src->bindingsAccessedDynamically;
    dst->funHasExtensibleScope = src->funHasExtensibleScope;
    dst->funHasAnyAliasedFormal = src->funHasAnyAliasedFormal;
    dst->hasSingletons = src->hasSingletons;
    dst->isGenerator = src->isGenerator;
    dst->isGeneratorExp = src->isGeneratorExp;

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
            js_free(debug);
            js_delete(map);
            return false;
        }
        compartment()->debugScriptMap = map;
    }

    if (!map->putNew(this, debug)) {
        js_free(debug);
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
    if (hasMJITInfo()) {
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
            js_free(releaseDebugScript());
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
JSScript::getOrCreateBreakpointSite(JSContext *cx, jsbytecode *pc)
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
JSScript::clearBreakpointsIn(FreeOp *fop, js::Debugger *dbg, RawObject handler)
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

    if (enclosingScope_)
        MarkObject(trc, &enclosingScope_, "enclosing");

    if (IS_GC_MARKING_TRACER(trc) && filename)
        MarkScriptFilename(trc->runtime, filename);

    bindings.trace(trc);

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

#ifdef JS_ION
    ion::TraceIonScripts(trc, this);
#endif
}

void
JSScript::setArgumentsHasVarBinding()
{
    argsHasVarBinding_ = true;
    needsArgsAnalysis_ = true;
}

void
JSScript::setNeedsArgsObj(bool needsArgsObj)
{
    JS_ASSERT(!analyzedArgsUsage());
    JS_ASSERT_IF(needsArgsObj, argumentsHasVarBinding());
    needsArgsAnalysis_ = false;
    needsArgsObj_ = needsArgsObj;
}

/* static */ bool
JSScript::argumentsOptimizationFailed(JSContext *cx, HandleScript script)
{
    AssertCanGC();
    JS_ASSERT(script->analyzedArgsUsage());
    JS_ASSERT(script->argumentsHasVarBinding());
    JS_ASSERT(!script->isGenerator);

    /*
     * It is possible that the apply speculation has already failed, everything
     * has been fixed up, but there was an outstanding magic value on the
     * stack that has just now flowed into an apply. In this case, there is
     * nothing to do; GuardFunApplySpeculation will patch in the real argsobj.
     */
    if (script->needsArgsObj())
        return true;

    script->needsArgsObj_ = true;

    InternalBindingsHandle bindings(script, &script->bindings);
    const unsigned var = Bindings::argumentsVarIndex(cx, bindings);

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
        /*
         * We cannot reliably create an arguments object for Ion activations of
         * this script.  To maintain the invariant that "script->needsArgsObj
         * implies fp->hasArgsObj", the Ion bail mechanism will create an
         * arguments object right after restoring the StackFrame and before
         * entering the interpreter (in ion::ThunkToInterpreter).  This delay is
         * safe since the engine avoids any observation of a StackFrame when it
         * beginsIonActivation (see StackIter::interpFrame comment).
         */
        if (i.isIon())
            continue;
        StackFrame *fp = i.interpFrame();
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
            if (fp->unaliasedLocal(var).isMagic(JS_OPTIMIZED_ARGUMENTS))
                fp->unaliasedLocal(var) = ObjectValue(*argsobj);
        }
    }

#ifdef JS_METHODJIT
    if (script->hasMJITInfo()) {
        mjit::ExpandInlineFrames(cx->compartment);
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
    return bindings.bindingIsAliased(bindings.numArgs() + varSlot);
}

bool
JSScript::formalIsAliased(unsigned argSlot)
{
    return bindings.bindingIsAliased(argSlot);
}

bool
JSScript::formalLivesInArgumentsObject(unsigned argSlot)
{
    return argsObjAliasesFormals() && !formalIsAliased(argSlot);
}

