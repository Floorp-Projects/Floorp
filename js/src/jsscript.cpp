/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS script operations.
 */

#include "jsscriptinlines.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"

#include <string.h>

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsopcode.h"
#include "jstypes.h"
#include "jsutil.h"

#include "frontend/BytecodeEmitter.h"
#include "gc/Marking.h"
#include "ion/BaselineJIT.h"
#include "ion/IonCode.h"
#include "vm/Debugger.h"
#include "vm/Interpreter.h"
#include "vm/Shape.h"
#include "vm/Xdr.h"

#include "jsfuninlines.h"
#include "jsinferinlines.h"

#include "vm/Runtime-inl.h"
#include "vm/ScopeObject-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::frontend;

using mozilla::PodCopy;
using mozilla::PodZero;

typedef Rooted<GlobalObject *> RootedGlobalObject;

/* static */ unsigned
Bindings::argumentsVarIndex(ExclusiveContext *cx, InternalBindingsHandle bindings)
{
    HandlePropertyName arguments = cx->names().arguments;
    BindingIter bi(bindings);
    while (bi->name() != arguments)
        bi++;
    return bi.frameIndex();
}

bool
Bindings::initWithTemporaryStorage(ExclusiveContext *cx, InternalBindingsHandle self,
                                   unsigned numArgs, unsigned numVars,
                                   Binding *bindingArray)
{
    JS_ASSERT(!self->callObjShape_);
    JS_ASSERT(self->bindingArrayAndFlag_ == TEMPORARY_STORAGE_BIT);

    if (numArgs > UINT16_MAX || numVars > UINT16_MAX) {
        if (cx->isJSContext()) {
            JS_ReportErrorNumber(cx->asJSContext(), js_GetErrorMessage, NULL,
                                 self->numArgs_ > self->numVars_ ?
                                 JSMSG_TOO_MANY_FUN_ARGS :
                                 JSMSG_TOO_MANY_LOCALS);
        }
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
        EmptyShape::getInitialShape(cx, &CallObject::class_, NULL, cx->global(), NULL,
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

        StackBaseShape base(cx, &CallObject::class_, cx->global(), NULL,
                            BaseShape::VAROBJ | BaseShape::DELEGATE);

        UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return false;

        RootedId id(cx, NameToId(bi->name()));
        unsigned attrs = JSPROP_PERMANENT | JSPROP_ENUMERATE |
                         (bi->kind() == CONSTANT ? JSPROP_READONLY : 0);
        unsigned frameIndex = bi.frameIndex();
        StackShape child(nbase, id, slot++, 0, attrs, Shape::HAS_SHORTID, frameIndex);

        Shape *shape = self->callObjShape_->getChildBinding(cx, child);
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
                uint8_t *dstScriptData, HandleScript srcScript)
{
    /* The clone has the same bindingArray_ offset as 'src'. */
    Bindings &src = srcScript->bindings;
    ptrdiff_t off = (uint8_t *)src.bindingArray() - srcScript->data;
    JS_ASSERT(off >= 0);
    JS_ASSERT(off <= srcScript->dataSize);
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
GCMethods<Bindings>::initial()
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
FindBlockIndex(JSScript *script, StaticBlockObject &block)
{
    ObjectArray *objects = script->objects();
    HeapPtrObject *vector = objects->vector;
    unsigned length = objects->length;
    for (unsigned i = 0; i < length; ++i) {
        if (vector[i] == &block)
            return i;
    }

    MOZ_ASSUME_UNREACHABLE("Block not found");
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
        FunNeedsDeclEnvObject,
        FunHasAnyAliasedFormal,
        ArgumentsHasVarBinding,
        NeedsArgsObj,
        IsGenerator,
        IsGeneratorExp,
        OwnSource,
        ExplicitUseStrict,
        SelfHosted
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
        if (script->selfHosted)
            scriptBits |= (1 << SelfHosted);
        if (script->bindingsAccessedDynamically)
            scriptBits |= (1 << ContainsDynamicNameAccess);
        if (script->funHasExtensibleScope)
            scriptBits |= (1 << FunHasExtensibleScope);
        if (script->funNeedsDeclEnvObject)
            scriptBits |= (1 << FunNeedsDeclEnvObject);
        if (script->funHasAnyAliasedFormal)
            scriptBits |= (1 << FunHasAnyAliasedFormal);
        if (script->argumentsHasVarBinding())
            scriptBits |= (1 << ArgumentsHasVarBinding);
        if (script->analyzedArgsUsage() && script->needsArgsObj())
            scriptBits |= (1 << NeedsArgsObj);
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
        JS_ASSERT((version_ & VersionFlags::MASK) == unsigned(version_));

        // principals and originPrincipals are set with xdr->initScriptPrincipals(script) below.
        // staticLevel is set below.
        CompileOptions options(cx);
        options.setVersion(version_)
               .setNoScriptRval(!!(scriptBits & (1 << NoScriptRval)))
               .setSelfHostingMode(!!(scriptBits & (1 << SelfHosted)));
        JS::RootedScriptSource sourceObject(cx);
        if (scriptBits & (1 << OwnSource)) {
            ScriptSource *ss = cx->new_<ScriptSource>();
            if (!ss)
                return false;
            sourceObject = ScriptSourceObject::create(cx, ss);
            if (!sourceObject)
                return false;
        } else {
            JS_ASSERT(enclosingScript);
            sourceObject = enclosingScript->sourceObject();
        }
        script = JSScript::Create(cx, enclosingScope, !!(scriptBits & (1 << SavedCallerFun)),
                                  options, /* staticLevel = */ 0, sourceObject, 0, 0);
        if (!script)
            return false;
    }

    /* JSScript::partiallyInit assumes script->bindings is fully initialized. */
    LifoAllocScope las(&cx->tempLifoAlloc());
    if (!XDRScriptBindings(xdr, las, nargs, nvars, script))
        return false;

    if (mode == XDR_DECODE) {
        if (!JSScript::partiallyInit(cx, script, nobjects, nregexps, ntrynotes, nconsts, nTypeSets))
            return false;

        JS_ASSERT(!script->mainOffset);
        script->mainOffset = prologLength;
        script->length = length;
        script->nfixed = uint16_t(version >> 16);
        script->ndefaults = ndefaults;

        scriptp.set(script);

        if (scriptBits & (1 << Strict))
            script->strict = true;
        if (scriptBits & (1 << ExplicitUseStrict))
            script->explicitUseStrict = true;
        if (scriptBits & (1 << ContainsDynamicNameAccess))
            script->bindingsAccessedDynamically = true;
        if (scriptBits & (1 << FunHasExtensibleScope))
            script->funHasExtensibleScope = true;
        if (scriptBits & (1 << FunNeedsDeclEnvObject))
            script->funNeedsDeclEnvObject = true;
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

    if (scriptBits & (1 << OwnSource)) {
        if (!script->scriptSource()->performXDR<mode>(xdr))
            return false;
    }
    if (!xdr->codeUint32(&script->sourceStart))
        return false;
    if (!xdr->codeUint32(&script->sourceEnd))
        return false;

    if (!xdr->codeUint32(&lineno) || !xdr->codeUint32(&nslots))
        return false;

    if (mode == XDR_DECODE) {
        script->lineno = lineno;
        script->nslots = uint16_t(nslots);
        script->staticLevel = uint16_t(nslots >> 16);
        xdr->initScriptPrincipals(script);
    }

    jsbytecode *code = script->code;
    SharedScriptData *ssd;
    if (mode == XDR_DECODE) {
        ssd = SharedScriptData::new_(cx, length, nsrcnotes, natoms);
        if (!ssd)
            return false;
        code = ssd->data;
        if (natoms != 0) {
            script->natoms = natoms;
            script->atoms = ssd->atoms(length, nsrcnotes);
        }
    }

    if (!xdr->codeBytes(code, length) || !xdr->codeBytes(code + length, nsrcnotes)) {
        if (mode == XDR_DECODE)
            js_free(ssd);
        return false;
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

    if (mode == XDR_DECODE) {
        if (!SaveSharedScriptData(cx, script, ssd))
            return false;
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
            JSObject *obj = *objp;
            JS_ASSERT(obj->is<JSFunction>() || obj->is<StaticBlockObject>());
            isBlock = obj->is<BlockObject>() ? 1 : 0;
        }
        if (!xdr->codeUint32(&isBlock))
            return false;
        if (isBlock == 0) {
            /* Code the nested function's enclosing scope. */
            uint32_t funEnclosingScopeIndex = 0;
            if (mode == XDR_ENCODE) {
                JSScript *innerScript = (*objp)->as<JSFunction>().getOrCreateScript(cx);
                if (!innerScript)
                    return false;
                RootedObject staticScope(cx, innerScript->enclosingStaticScope());
                StaticScopeIter ssi(cx, staticScope);
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
                if (StaticBlockObject *block = (*objp)->as<StaticBlockObject>().enclosingBlock())
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

    if (mode == XDR_DECODE)
        scriptp.set(script);

    return true;
}

template bool
js::XDRScript(XDRState<XDR_ENCODE> *, HandleObject, HandleScript, HandleFunction,
              MutableHandleScript);

template bool
js::XDRScript(XDRState<XDR_DECODE> *, HandleObject, HandleScript, HandleFunction,
              MutableHandleScript);

void
JSScript::setSourceObject(js::ScriptSourceObject *object)
{
    sourceObject_ = object;
}

js::ScriptSourceObject *
JSScript::sourceObject() const
{
    return &sourceObject_->as<ScriptSourceObject>();
}

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
    for (frames = cx->runtime()->interpreterFrames; frames; frames = frames->older)
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

void
ScriptSourceObject::setSource(ScriptSource *source)
{
    if (source)
        source->incref();
    if (this->source())
        this->source()->decref();
    setReservedSlot(SOURCE_SLOT, PrivateValue(source));
}

void
ScriptSourceObject::finalize(FreeOp *fop, JSObject *obj)
{
    // ScriptSource::setSource automatically takes care of the refcount
    obj->as<ScriptSourceObject>().setSource(NULL);
}

Class ScriptSourceObject::class_ = {
    "ScriptSource",
    JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,        /* addProperty */
    JS_DeletePropertyStub,  /* delProperty */
    JS_PropertyStub,        /* getProperty */
    JS_StrictPropertyStub,  /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    ScriptSourceObject::finalize
};

ScriptSourceObject *
ScriptSourceObject::create(ExclusiveContext *cx, ScriptSource *source)
{
    RootedObject object(cx, NewObjectWithGivenProto(cx, &class_, NULL, cx->global()));
    if (!object)
        return NULL;
    JS::RootedScriptSource sourceObject(cx, &object->as<ScriptSourceObject>());
    sourceObject->setSlot(SOURCE_SLOT, PrivateValue(source));
    source->incref();
    return sourceObject;
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

const jschar *
SourceCompressorThread::currentChars() const
{
    JS_ASSERT(tok);
    return tok->chars;
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
        // We have reentered the compiler. Complete the current compression
        // before starting the next one.
        waitOnCompression(tok);
    JS_ASSERT(state == IDLE);
    JS_ASSERT(!tok);
    stop = false;
    PR_Lock(lock);
    sct->ss->ready_ = false;
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
    saveTok->ss->ready_ = true;

    // Update memory accounting.
    if (!saveTok->oom)
        saveTok->cx->runtime()->updateMallocCounter(NULL, saveTok->ss->computedSizeOfData());

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

/* static */ bool
JSScript::loadSource(JSContext *cx, HandleScript script, bool *worked)
{
    JS_ASSERT(!script->scriptSource()->hasSourceData());
    *worked = false;
    if (!cx->runtime()->sourceHook || !script->scriptSource()->sourceRetrievable())
        return true;
    jschar *src = NULL;
    uint32_t length;
    if (!cx->runtime()->sourceHook(cx, script, &src, &length))
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
    JS_ASSERT(scriptSource()->hasSourceData());
    return scriptSource()->substring(cx, sourceStart, sourceEnd);
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

const jschar *
ScriptSource::chars(JSContext *cx)
{
#ifdef JS_THREADSAFE
    if (!ready())
        return cx->runtime()->sourceCompressorThread.currentChars();
#endif
#ifdef USE_ZLIB
    if (compressed()) {
        JSStableString *cached = cx->runtime()->sourceDataCache.lookup(this);
        if (!cached) {
            const size_t nbytes = sizeof(jschar) * (length_ + 1);
            jschar *decompressed = static_cast<jschar *>(js_malloc(nbytes));
            if (!decompressed)
                return NULL;
            if (!DecompressString(data.compressed, compressedLength_,
                                  reinterpret_cast<unsigned char *>(decompressed), nbytes)) {
                JS_ReportOutOfMemory(cx);
                js_free(decompressed);
                return NULL;
            }
            decompressed[length_] = 0;
            cached = js_NewString<CanGC>(cx, decompressed, length_);
            if (!cached) {
                js_free(decompressed);
                return NULL;
            }
            cx->runtime()->sourceDataCache.put(this, cached);
        }
        return cached->chars().get();
    }
#endif
    return data.source;
}

JSStableString *
ScriptSource::substring(JSContext *cx, uint32_t start, uint32_t stop)
{
    JS_ASSERT(start <= stop);
    const jschar *chars = this->chars(cx);
    if (!chars)
        return NULL;
    JSFlatString *flatStr = js_NewStringCopyN<CanGC>(cx, chars + start, stop - start);
    if (!flatStr)
        return NULL;
    return flatStr->ensureStable(cx);
}

bool
ScriptSource::setSourceCopy(JSContext *cx, const jschar *src, uint32_t length,
                            bool argumentsNotIncluded, SourceCompressionToken *tok)
{
    JS_ASSERT(!hasSourceData());
    length_ = length;
    argumentsNotIncluded_ = argumentsNotIncluded;

#ifdef JS_THREADSAFE
    if (tok && cx->runtime()->useHelperThreads()) {
        tok->ss = this;
        tok->chars = src;
        cx->runtime()->sourceCompressorThread.compress(tok);
    } else
#endif
    {
        if (!adjustDataSize(sizeof(jschar) * length))
            return false;
        PodCopy(data.source, src, length_);
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
        cx->runtime()->sourceCompressorThread.waitOnCompression(this);
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
    cx->runtime()->sourceCompressorThread.abort(this);
#endif
}

void
ScriptSource::destroy()
{
    JS_ASSERT(ready());
    adjustDataSize(0);
    js_free(filename_);
    js_free(sourceMap_);
    ready_ = false;
    js_free(this);
}

size_t
ScriptSource::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    // |data| is a union, but both members are pointers to allocated memory,
    // |emptySource|, or NULL, so just using |data.compressed| will work.
    size_t n = mallocSizeOf(this);
    n += (ready() && data.compressed != emptySource)
       ? mallocSizeOf(data.compressed)
       : 0;
    n += mallocSizeOf(filename_);
    return n;
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

    uint8_t haveFilename = !!filename_;
    if (!xdr->codeUint8(&haveFilename))
        return false;

    if (haveFilename) {
        const char *fn = filename();
        if (!xdr->codeCString(&fn))
            return false;
        if (mode == XDR_DECODE && !setFilename(xdr->cx(), fn))
            return false;
    }

    if (mode == XDR_DECODE)
        ready_ = true;

    return true;
}

bool
ScriptSource::setFilename(ExclusiveContext *cx, const char *filename)
{
    JS_ASSERT(!filename_);
    size_t len = strlen(filename) + 1;
    if (len == 1)
        return true;
    filename_ = cx->pod_malloc<char>(len);
    if (!filename_)
        return false;
    js_memcpy(filename_, filename, len);
    return true;
}

bool
ScriptSource::setSourceMap(ExclusiveContext *cx, jschar *sourceMapURL)
{
    JS_ASSERT(sourceMapURL);
    if (hasSourceMap()) {
        if (cx->isJSContext() &&
            !JS_ReportErrorFlagsAndNumber(cx->asJSContext(), JSREPORT_WARNING, js_GetErrorMessage,
                                          NULL, JSMSG_ALREADY_HAS_SOURCEMAP, filename_))
        {
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
 * Shared script data management.
 */

SharedScriptData *
js::SharedScriptData::new_(ExclusiveContext *cx, uint32_t codeLength,
                           uint32_t srcnotesLength, uint32_t natoms)
{
    uint32_t baseLength = codeLength + srcnotesLength;
    uint32_t padding = sizeof(JSAtom *) - baseLength % sizeof(JSAtom *);
    uint32_t length = baseLength + padding + sizeof(JSAtom *) * natoms;

    SharedScriptData *entry = (SharedScriptData *)cx->malloc_(length +
                                                              offsetof(SharedScriptData, data));

    if (!entry)
        return NULL;
    entry->marked = false;
    entry->length = length;
    memset(entry->data + baseLength, 0, padding);
    return entry;
}

bool
js::SaveSharedScriptData(ExclusiveContext *cx, Handle<JSScript *> script, SharedScriptData *ssd)
{
    ASSERT(script != NULL);
    ASSERT(ssd != NULL);

    AutoLockForExclusiveAccess lock(cx);

    ScriptBytecodeHasher::Lookup l(ssd);

    ScriptDataTable::AddPtr p = cx->scriptDataTable().lookupForAdd(l);
    if (p) {
        js_free(ssd);
        ssd = *p;
    } else {
        if (!cx->scriptDataTable().add(p, ssd)) {
            js_free(ssd);
            js_ReportOutOfMemory(cx);
            return false;
        }
    }

#ifdef JSGC_INCREMENTAL
    /*
     * During the IGC we need to ensure that bytecode is marked whenever it is
     * accessed even if the bytecode was already in the table: at this point
     * old scripts or exceptions pointing to the bytecode may no longer be
     * reachable. This is effectively a read barrier.
     */
    if (cx->isJSContext()) {
        JSRuntime *rt = cx->asJSContext()->runtime();
        if (JS::IsIncrementalGCInProgress(rt) && rt->gcIsFull)
            ssd->marked = true;
    }
#endif

    script->code = ssd->data;
    script->atoms = ssd->atoms(script->length, script->numNotes());
    return true;
}

void
js::SweepScriptData(JSRuntime *rt)
{
    JS_ASSERT(rt->gcIsFull);
    ScriptDataTable &table = rt->scriptDataTable;

    bool keepAtoms = false;
    for (ThreadDataIter iter(rt); !iter.done(); iter.next())
        keepAtoms |= iter->gcKeepAtoms;

    for (ScriptDataTable::Enum e(table); !e.empty(); e.popFront()) {
        SharedScriptData *entry = e.front();
        if (entry->marked) {
            entry->marked = false;
        } else if (!keepAtoms) {
            js_free(entry);
            e.removeFront();
        }
    }
}

void
js::FreeScriptData(JSRuntime *rt)
{
    ScriptDataTable &table = rt->scriptDataTable;
    if (!table.initialized())
        return;

    for (ScriptDataTable::Enum e(table); !e.empty(); e.popFront())
        js_free(e.front());

    table.clear();
}

/*
 * JSScript::data and SharedScriptData::data have complex,
 * manually-controlled, memory layouts.
 *
 * JSScript::data begins with some optional array headers. They are optional
 * because they often aren't needed, i.e. the corresponding arrays often have
 * zero elements. Each header has a bit in JSScript::hasArrayBits that
 * indicates if it's present within |data|; from this the offset of each
 * present array header can be computed. Each header has an accessor function
 * in JSScript that encapsulates this offset computation.
 *
 * Array type       Array elements  Accessor
 * ----------       --------------  --------
 * ConstArray       Consts          consts()
 * ObjectArray      Objects         objects()
 * ObjectArray      Regexps         regexps()
 * TryNoteArray     Try notes       trynotes()
 *
 * Then are the elements of several arrays.
 * - Most of these arrays have headers listed above (if present). For each of
 *   these, the array pointer and the array length is stored in the header.
 * - The remaining arrays have pointers and lengths that are stored directly in
 *   JSScript. This is because, unlike the others, they are nearly always
 *   non-zero length and so the optional-header space optimization isn't
 *   worthwhile.
 *
 * Array elements   Pointed to by         Length
 * --------------   -------------         ------
 * Consts           consts()->vector      consts()->length
 * Objects          objects()->vector     objects()->length
 * Regexps          regexps()->vector     regexps()->length
 * Try notes        trynotes()->vector    trynotes()->length
 *
 * IMPORTANT: This layout has two key properties.
 * - It ensures that everything has sufficient alignment; in particular, the
 *   consts() elements need jsval alignment.
 * - It ensures there are no gaps between elements, which saves space and makes
 *   manual layout easy. In particular, in the second part, arrays with larger
 *   elements precede arrays with smaller elements.
 *
 * SharedScriptData::data contains data that can be shared within a
 * runtime. These items' layout is manually controlled to make it easier to
 * manage both during (temporary) allocation and during matching against
 * existing entries in the runtime. As the jsbytecode has to come first to
 * enable lookup by bytecode identity, SharedScriptData::data, the atoms part
 * has to manually be aligned sufficiently by adding padding after the notes
 * part.
 *
 * Array elements   Pointed to by         Length
 * --------------   -------------         ------
 * jsbytecode       code                  length
 * jsscrnote        notes()               numNotes()
 * Atoms            atoms                 natoms
 *
 * The following static assertions check JSScript::data's alignment properties.
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
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(HeapValue, HeapPtrObject));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(HeapPtrObject, HeapPtrObject));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(HeapPtrObject, JSTryNote));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(JSTryNote, uint32_t));
JS_STATIC_ASSERT(NO_PADDING_BETWEEN_ENTRIES(uint32_t, uint32_t));

static inline size_t
ScriptDataSize(uint32_t nbindings, uint32_t nobjects, uint32_t nregexps,
               uint32_t ntrynotes, uint32_t nconsts)
{
    size_t size = 0;

    if (nconsts != 0)
        size += sizeof(ConstArray) + nconsts * sizeof(Value);
    if (nobjects != 0)
        size += sizeof(ObjectArray) + nobjects * sizeof(JSObject *);
    if (nregexps != 0)
        size += sizeof(ObjectArray) + nregexps * sizeof(JSObject *);
    if (ntrynotes != 0)
        size += sizeof(TryNoteArray) + ntrynotes * sizeof(JSTryNote);

    if (nbindings != 0) {
	// Make sure bindings are sufficiently aligned.
        size = JS_ROUNDUP(size, JS_ALIGNMENT_OF(Binding)) + nbindings * sizeof(Binding);
    }

    return size;
}

void
JSScript::initCompartmentAndPrincipals(ExclusiveContext *cx, const CompileOptions &options)
{
    compartment_ = cx->compartment_;

    /* Establish invariant: principals implies originPrincipals. */
    if (options.principals) {
        JS_ASSERT(options.principals == cx->compartment_->principals);
        originPrincipals
            = options.originPrincipals ? options.originPrincipals : options.principals;
        JS_HoldPrincipals(originPrincipals);
    } else if (options.originPrincipals) {
        originPrincipals = options.originPrincipals;
        JS_HoldPrincipals(originPrincipals);
    }
}

JSScript *
JSScript::Create(ExclusiveContext *cx, HandleObject enclosingScope, bool savedCallerFun,
                 const CompileOptions &options, unsigned staticLevel,
                 JS::HandleScriptSource sourceObject, uint32_t bufStart, uint32_t bufEnd)
{
    JS_ASSERT(bufStart <= bufEnd);

    RootedScript script(cx, js_NewGCScript(cx));
    if (!script)
        return NULL;

    PodZero(script.get());
    new (&script->bindings) Bindings;

    script->enclosingScopeOrOriginalFunction_ = enclosingScope;
    script->savedCallerFun = savedCallerFun;
    script->initCompartmentAndPrincipals(cx, options);

    script->compileAndGo = options.compileAndGo;
    script->selfHosted = options.selfHostingMode;
    script->noScriptRval = options.noScriptRval;

    script->version = options.version;
    JS_ASSERT(script->getVersion() == options.version);     // assert that no overflow occurred

    // This is an unsigned-to-uint16_t conversion, test for too-high values.
    // In practice, recursion in Parser and/or BytecodeEmitter will blow the
    // stack if we nest functions more than a few hundred deep, so this will
    // never trigger.  Oh well.
    if (staticLevel > UINT16_MAX) {
        if (cx->isJSContext()) {
            JS_ReportErrorNumber(cx->asJSContext(),
                                 js_GetErrorMessage, NULL, JSMSG_TOO_DEEP, js_function_str);
        }
        return NULL;
    }
    script->staticLevel = uint16_t(staticLevel);

    script->sourceObject_ = sourceObject;
    script->sourceStart = bufStart;
    script->sourceEnd = bufEnd;

    return script;
}

static inline uint8_t *
AllocScriptData(ExclusiveContext *cx, size_t size)
{
    uint8_t *data = static_cast<uint8_t *>(cx->calloc_(JS_ROUNDUP(size, sizeof(Value))));
    if (!data)
        return NULL;

    // All script data is optional, so size might be 0. In that case, we don't care about alignment.
    JS_ASSERT(size == 0 || size_t(data) % sizeof(Value) == 0);
    return data;
}

/* static */ bool
JSScript::partiallyInit(ExclusiveContext *cx, HandleScript script, uint32_t nobjects,
                        uint32_t nregexps, uint32_t ntrynotes, uint32_t nconsts, uint32_t nTypeSets)
{
    size_t size = ScriptDataSize(script->bindings.count(), nobjects, nregexps, ntrynotes, nconsts);
    script->data = AllocScriptData(cx, size);
    if (!script->data)
        return false;
    script->dataSize = size;

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

    if (script->bindings.count() != 0) {
	// Make sure bindings are sufficiently aligned.
	cursor = reinterpret_cast<uint8_t*>
	    (JS_ROUNDUP(reinterpret_cast<uintptr_t>(cursor), JS_ALIGNMENT_OF(Binding)));
    }
    cursor = script->bindings.switchToScriptStorage(reinterpret_cast<Binding *>(cursor));

    JS_ASSERT(cursor == script->data + size);
    return true;
}

/* static */ bool
JSScript::fullyInitTrivial(ExclusiveContext *cx, Handle<JSScript*> script)
{
    if (!partiallyInit(cx, script, 0, 0, 0, 0, 0))
        return false;

    SharedScriptData *ssd = SharedScriptData::new_(cx, 1, 1, 0);
    if (!ssd)
        return false;

    ssd->data[0] = JSOP_STOP;
    ssd->data[1] = SRC_NULL;
    script->length = 1;
    return SaveSharedScriptData(cx, script, ssd);
}

/* static */ bool
JSScript::fullyInitFromEmitter(ExclusiveContext *cx, HandleScript script, BytecodeEmitter *bce)
{
    /* The counts of indexed things must be checked during code generation. */
    JS_ASSERT(bce->atomIndices->count() <= INDEX_LIMIT);
    JS_ASSERT(bce->objectList.length <= INDEX_LIMIT);
    JS_ASSERT(bce->regexpList.length <= INDEX_LIMIT);

    uint32_t mainLength = bce->offset();
    uint32_t prologLength = bce->prologOffset();
    uint32_t nsrcnotes = uint32_t(bce->countFinalSourceNotes());
    uint32_t natoms = bce->atomIndices->count();
    if (!partiallyInit(cx, script,
                       bce->objectList.length, bce->regexpList.length, bce->tryNoteList.length(),
                       bce->constList.length(), bce->typesetCount))
    {
        return false;
    }

    JS_ASSERT(script->mainOffset == 0);
    script->mainOffset = prologLength;

    script->lineno = bce->firstLine;

    script->length = prologLength + mainLength;
    script->natoms = natoms;
    SharedScriptData *ssd = SharedScriptData::new_(cx, script->length, nsrcnotes, natoms);
    if (!ssd)
        return false;

    jsbytecode *code = ssd->data;
    PodCopy<jsbytecode>(code, bce->prolog.code.begin(), prologLength);
    PodCopy<jsbytecode>(code + prologLength, bce->code().begin(), mainLength);
    if (!FinishTakingSrcNotes(cx, bce, (jssrcnote *)(code + script->length)))
        return false;
    InitAtomMap(bce->atomIndices.getMap(), ssd->atoms(script->length, nsrcnotes));

    if (!SaveSharedScriptData(cx, script, ssd))
        return false;

    uint32_t nfixed = bce->sc->isFunctionBox() ? script->bindings.numVars() : 0;
    JS_ASSERT(nfixed < SLOTNO_LIMIT);
    script->nfixed = uint16_t(nfixed);
    if (script->nfixed + bce->maxStackDepth >= JS_BIT(16)) {
        bce->reportError(NULL, JSMSG_NEED_DIET, "script");
        return false;
    }
    script->nslots = script->nfixed + bce->maxStackDepth;

    FunctionBox *funbox = bce->sc->isFunctionBox() ? bce->sc->asFunctionBox() : NULL;

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
    script->funNeedsDeclEnvObject = funbox ? funbox->needsDeclEnvObject() : false;
    script->hasSingletons = bce->hasSingletons;

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
    return dataSize;
}

size_t
JSScript::sizeOfData(mozilla::MallocSizeOf mallocSizeOf)
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
           !analysis()->hasFunctionCalls();
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
    JSObject *enclosing = enclosingStaticScope();
    while (enclosing) {
        if (enclosing->is<JSFunction>()) {
            JSFunction *fun = &enclosing->as<JSFunction>();
            if (!fun->hasScript() || !fun->nonLazyScript())
                return false;
            enclosing = fun->nonLazyScript()->enclosingStaticScope();
        } else {
            enclosing = enclosing->as<StaticBlockObject>().enclosingStaticScope();
        }
    }
    return true;
}

void
js::CallNewScriptHook(JSContext *cx, HandleScript script, HandleFunction fun)
{
    if (script->selfHosted)
        return;

    JS_ASSERT(!script->isActiveEval);
    if (JSNewScriptHook hook = cx->runtime()->debugHooks.newScriptHook) {
        AutoKeepAtoms keepAtoms(cx->perThreadData);
        hook(cx, script->filename(), script->lineno, script, fun,
             cx->runtime()->debugHooks.newScriptHookData);
    }
}

void
js::CallDestroyScriptHook(FreeOp *fop, JSScript *script)
{
    if (script->selfHosted)
        return;

    // The hook will only call into JS if a GC is not running.
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

    if (originPrincipals)
        JS_DropPrincipals(fop->runtime(), originPrincipals);

    if (types)
        types->destroy();

#ifdef JS_ION
    ion::DestroyIonScripts(fop, this);
#endif

    destroyScriptCounts(fop);
    destroyDebugScript(fop);

    if (data) {
        JS_POISON(data, 0xdb, computedSizeOfData());
        fop->free_(data);
    }

    fop->runtime()->lazyScriptCache.remove(this);
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
js_GetSrcNote(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    GSNCache *cache = &cx->runtime()->gsnCache;
    cx = NULL;  // nulling |cx| ensures GC can't be triggered, so |JSScript *script| is safe

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
js::PCToLineNumber(JSScript *script, jsbytecode *pc, unsigned *columnp)
{
    /* Cope with StackFrame.pc value prior to entering js_Interpret. */
    if (!pc)
        return 0;

    return PCToLineNumber(script->lineno, script->notes(), script->code, pc, columnp);
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

void
js::CurrentScriptFileLineOrigin(JSContext *cx, const char **file, unsigned *linenop,
                                JSPrincipals **origin, LineOption opt)
{
    if (opt == CALLED_FROM_JSOP_EVAL) {
        JSScript *script = NULL;
        jsbytecode *pc = NULL;
        types::TypeScript::GetPcScript(cx, &script, &pc);
        JS_ASSERT(JSOp(*pc) == JSOP_EVAL);
        JS_ASSERT(*(pc + JSOP_EVAL_LENGTH) == JSOP_LINENO);
        *file = script->filename();
        *linenop = GET_UINT16(pc + JSOP_EVAL_LENGTH);
        *origin = script->originPrincipals;
        return;
    }

    NonBuiltinScriptFrameIter iter(cx);

    if (iter.done()) {
        *file = NULL;
        *linenop = 0;
        *origin = NULL;
        return;
    }

    JSScript *script = iter.script();
    *file = script->filename();
    *linenop = PCToLineNumber(iter.script(), iter.pc());
    *origin = script->originPrincipals;
}

template <class T>
static inline T *
Rebase(JSScript *dst, JSScript *src, T *srcp)
{
    size_t off = reinterpret_cast<uint8_t *>(srcp) - src->data;
    return reinterpret_cast<T *>(dst->data + off);
}

JSScript *
js::CloneScript(JSContext *cx, HandleObject enclosingScope, HandleFunction fun, HandleScript src,
                NewObjectKind newKind /* = GenericObject */)
{
    /* NB: Keep this in sync with XDRScript. */

    uint32_t nconsts   = src->hasConsts()   ? src->consts()->length   : 0;
    uint32_t nobjects  = src->hasObjects()  ? src->objects()->length  : 0;
    uint32_t nregexps  = src->hasRegexps()  ? src->regexps()->length  : 0;
    uint32_t ntrynotes = src->hasTrynotes() ? src->trynotes()->length : 0;

    /* Script data */

    size_t size = src->dataSize;
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
            if (obj->is<StaticBlockObject>()) {
                Rooted<StaticBlockObject*> innerBlock(cx, &obj->as<StaticBlockObject>());

                RootedObject enclosingScope(cx);
                if (StaticBlockObject *enclosingBlock = innerBlock->enclosingBlock())
                    enclosingScope = objects[FindBlockIndex(src, *enclosingBlock)];
                else
                    enclosingScope = fun;

                clone = CloneStaticBlockObject(cx, enclosingScope, innerBlock);
            } else if (obj->is<JSFunction>()) {
                RootedFunction innerFun(cx, &obj->as<JSFunction>());
                if (innerFun->isNative()) {
                    assertSameCompartment(cx, innerFun);
                    clone = innerFun;
                } else {
                    if (innerFun->isInterpretedLazy()) {
                        AutoCompartment ac(cx, innerFun);
                        if (!innerFun->getOrCreateScript(cx))
                            return NULL;
                    }
                    RootedObject staticScope(cx, innerFun->nonLazyScript()->enclosingStaticScope());
                    StaticScopeIter ssi(cx, staticScope);
                    RootedObject enclosingScope(cx);
                    if (!ssi.done() && ssi.type() == StaticScopeIter::BLOCK)
                        enclosingScope = objects[FindBlockIndex(src, ssi.block())];
                    else
                        enclosingScope = fun;

                    clone = CloneFunctionAndScript(cx, enclosingScope, innerFun);
                }
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
            JSObject *clone = CloneScriptRegExpObject(cx, vector[i]->as<RegExpObject>());
            if (!clone || !regexps.append(clone))
                return NULL;
        }
    }

    /* Now that all fallible allocation is complete, create the GC thing. */

    CompileOptions options(cx);
    options.setPrincipals(cx->compartment()->principals)
           .setOriginPrincipals(src->originPrincipals)
           .setCompileAndGo(src->compileAndGo)
           .setSelfHostingMode(src->selfHosted)
           .setNoScriptRval(src->noScriptRval)
           .setVersion(src->getVersion());

    /* Make sure we clone the script source object with the script */
    JS::RootedScriptSource sourceObject(cx, ScriptSourceObject::create(cx, src->scriptSource()));
    if (!sourceObject)
        return NULL;

    RootedScript dst(cx, JSScript::Create(cx, enclosingScope, src->savedCallerFun,
                                          options, src->staticLevel,
                                          sourceObject, src->sourceStart, src->sourceEnd));
    if (!dst) {
        js_free(data);
        return NULL;
    }

    dst->bindings = bindings;

    /* This assignment must occur before all the Rebase calls. */
    dst->data = data;
    dst->dataSize = size;
    memcpy(data, src->data, size);

    /* Script filenames, bytecodes and atoms are runtime-wide. */
    dst->code = src->code;
    dst->atoms = src->atoms;

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
    dst->funNeedsDeclEnvObject = src->funNeedsDeclEnvObject;
    dst->funHasAnyAliasedFormal = src->funHasAnyAliasedFormal;
    dst->hasSingletons = src->hasSingletons;
    dst->treatAsRunOnce = src->treatAsRunOnce;
    dst->isGenerator = src->isGenerator;
    dst->isGeneratorExp = src->isGeneratorExp;

    /* Copy over hints. */
    dst->shouldInline = src->shouldInline;
    dst->shouldCloneAtCallsite = src->shouldCloneAtCallsite;
    dst->isCallsiteClone = src->isCallsiteClone;

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

bool
js::CloneFunctionScript(JSContext *cx, HandleFunction original, HandleFunction clone,
                        NewObjectKind newKind /* = GenericObject */)
{
    JS_ASSERT(clone->isInterpreted());

    RootedScript script(cx, clone->nonLazyScript());
    JS_ASSERT(script);
    JS_ASSERT(script->compartment() == original->compartment());
    JS_ASSERT_IF(script->compartment() != cx->compartment(),
                 !script->enclosingStaticScope());

    RootedObject scope(cx, script->enclosingStaticScope());

    clone->mutableScript().init(NULL);

    JSScript *cscript = CloneScript(cx, scope, clone, script, newKind);
    if (!cscript)
        return false;

    clone->setScript(cscript);
    cscript->setFunction(clone);

    RootedGlobalObject global(cx, script->compileAndGo ? &script->global() : NULL);

    script = clone->nonLazyScript();
    CallNewScriptHook(cx, script, clone);
    Debugger::onNewScript(cx, script, global);

    return true;
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
    for (frames = cx->runtime()->interpreterFrames; frames; frames = frames->older)
        frames->enableInterruptsIfRunning(this);

    return true;
}

void
JSScript::recompileForStepMode(FreeOp *fop)
{
#ifdef JS_ION
    if (hasBaselineScript())
        baseline->toggleDebugTraps(this, NULL);
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
        recompileForStepMode(cx->runtime()->defaultFreeOp());

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
    JS_ASSERT_IF(delta > 0, cx->compartment()->debugMode());

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
        site = cx->runtime()->new_<BreakpointSite>(this, pc);
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

bool
JSScript::hasBreakpointsAt(jsbytecode *pc)
{
    BreakpointSite *site = getBreakpointSite(pc);
    if (!site)
        return false;

    return site->enabledCount > 0 || site->trapHandler;
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

    JS_ASSERT_IF(trc->runtime->gcStrictCompartmentChecking, zone()->isCollecting());

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

    if (sourceObject())
        MarkObject(trc, &sourceObject_, "sourceObject");

    if (function())
        MarkObject(trc, &function_, "function");

    if (enclosingScopeOrOriginalFunction_)
        MarkObject(trc, &enclosingScopeOrOriginalFunction_, "enclosing");

    if (IS_GC_MARKING_TRACER(trc)) {
        compartment()->mark();

        if (code)
            MarkScriptBytecode(trc->runtime, code);
    }

    bindings.trace(trc);

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
LazyScript::markChildren(JSTracer *trc)
{
    if (function_)
        MarkObject(trc, &function_, "function");

    if (sourceObject_)
        MarkObject(trc, &sourceObject_, "sourceObject");

    if (enclosingScope_)
        MarkObject(trc, &enclosingScope_, "enclosingScope");

    if (script_)
        MarkScript(trc, &script_, "realScript");

    HeapPtrAtom *freeVariables = this->freeVariables();
    for (size_t i = 0; i < numFreeVariables(); i++)
        MarkString(trc, &freeVariables[i], "lazyScriptFreeVariable");

    HeapPtrFunction *innerFunctions = this->innerFunctions();
    for (size_t i = 0; i < numInnerFunctions(); i++)
        MarkObject(trc, &innerFunctions[i], "lazyScriptInnerFunction");
}

void
LazyScript::finalize(FreeOp *fop)
{
    if (table_)
        fop->free_(table_);

    if (originPrincipals_)
        JS_DropPrincipals(fop->runtime(), originPrincipals_);
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

void
js::SetFrameArgumentsObject(JSContext *cx, AbstractFramePtr frame,
                            HandleScript script, JSObject *argsobj)
{
    /*
     * Replace any optimized arguments in the frame with an explicit arguments
     * object. Note that 'arguments' may have already been overwritten.
     */

    InternalBindingsHandle bindings(script, &script->bindings);
    const unsigned var = Bindings::argumentsVarIndex(cx, bindings);

    if (script->varIsAliased(var)) {
        /*
         * Scan the script to find the slot in the call object that 'arguments'
         * is assigned to.
         */
        jsbytecode *pc = script->code;
        while (*pc != JSOP_ARGUMENTS)
            pc += GetBytecodeLength(pc);
        pc += JSOP_ARGUMENTS_LENGTH;
        JS_ASSERT(*pc == JSOP_SETALIASEDVAR);

        if (frame.callObj().as<ScopeObject>().aliasedVar(pc).isMagic(JS_OPTIMIZED_ARGUMENTS))
            frame.callObj().as<ScopeObject>().setAliasedVar(cx, pc, cx->names().arguments, ObjectValue(*argsobj));
    } else {
        if (frame.unaliasedLocal(var).isMagic(JS_OPTIMIZED_ARGUMENTS))
            frame.unaliasedLocal(var) = ObjectValue(*argsobj);
    }
}

/* static */ bool
JSScript::argumentsOptimizationFailed(JSContext *cx, HandleScript script)
{
    JS_ASSERT(script->function());
    JS_ASSERT(script->analyzedArgsUsage());
    JS_ASSERT(script->argumentsHasVarBinding());

    /*
     * It is possible that the arguments optimization has already failed,
     * everything has been fixed up, but there was an outstanding magic value
     * on the stack that has just now flowed into an apply. In this case, there
     * is nothing to do; GuardFunApplySpeculation will patch in the real
     * argsobj.
     */
    if (script->needsArgsObj())
        return true;

    JS_ASSERT(!script->isGenerator);

    script->needsArgsObj_ = true;

#ifdef JS_ION
    /*
     * Since we can't invalidate baseline scripts, set a flag that's checked from
     * JIT code to indicate the arguments optimization failed and JSOP_ARGUMENTS
     * should create an arguments object next time.
     */
    if (script->hasBaselineScript())
        script->baselineScript()->setNeedsArgsObj();
#endif

    /*
     * By design, the arguments optimization is only made when there are no
     * outstanding cases of MagicValue(JS_OPTIMIZED_ARGUMENTS) at any points
     * where the optimization could fail, other than an active invocation of
     * 'f.apply(x, arguments)'. Thus, there are no outstanding values of
     * MagicValue(JS_OPTIMIZED_ARGUMENTS) on the stack. However, there are
     * three things that need fixup:
     *  - there may be any number of activations of this script that don't have
     *    an argsObj that now need one.
     *  - jit code compiled (and possible active on the stack) with the static
     *    assumption of !script->needsArgsObj();
     *  - type inference data for the script assuming script->needsArgsObj
     */
    for (AllFramesIter i(cx); !i.done(); ++i) {
        /*
         * We cannot reliably create an arguments object for Ion activations of
         * this script.  To maintain the invariant that "script->needsArgsObj
         * implies fp->hasArgsObj", the Ion bail mechanism will create an
         * arguments object right after restoring the StackFrame and before
         * entering the interpreter (in ion::ThunkToInterpreter).  This delay is
         * safe since the engine avoids any observation of a StackFrame when it's
         * runningInJit (see ScriptFrameIter::interpFrame comment).
         */
        if (i.isIon())
            continue;
        AbstractFramePtr frame = i.abstractFramePtr();
        if (frame.isFunctionFrame() && frame.script() == script) {
            ArgumentsObject *argsobj = ArgumentsObject::createExpected(cx, frame);
            if (!argsobj) {
                /*
                 * We can't leave stack frames with script->needsArgsObj but no
                 * arguments object. It is, however, safe to leave frames with
                 * an arguments object but !script->needsArgsObj.
                 */
                script->needsArgsObj_ = false;
                return false;
            }

            SetFrameArgumentsObject(cx, frame, script, argsobj);
        }
    }

    if (script->hasAnalysis() && script->analysis()->ranInference()) {
        types::AutoEnterAnalysis enter(cx);
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

LazyScript::LazyScript(JSFunction *fun, void *table, uint32_t numFreeVariables, uint32_t numInnerFunctions,
                       JSVersion version, uint32_t begin, uint32_t end, uint32_t lineno, uint32_t column)
  : script_(NULL),
    function_(fun),
    enclosingScope_(NULL),
    sourceObject_(NULL),
    table_(table),
    originPrincipals_(NULL),
    version_(version),
    numFreeVariables_(numFreeVariables),
    numInnerFunctions_(numInnerFunctions),
    strict_(false),
    bindingsAccessedDynamically_(false),
    hasDebuggerStatement_(false),
    directlyInsideEval_(false),
    usesArgumentsAndApply_(false),
    hasBeenCloned_(false),
    begin_(begin),
    end_(end),
    lineno_(lineno),
    column_(column)
{
    JS_ASSERT(this->version() == version);
    JS_ASSERT(begin <= end);
}

void
LazyScript::initScript(JSScript *script)
{
    JS_ASSERT(script && !script_);
    script_ = script;
}

void
LazyScript::setParent(JSObject *enclosingScope, ScriptSourceObject *sourceObject,
                      JSPrincipals *originPrincipals)
{
    JS_ASSERT(sourceObject && !sourceObject_ && !enclosingScope_ && !originPrincipals_);
    enclosingScope_ = enclosingScope;
    sourceObject_ = sourceObject;
    originPrincipals_ = originPrincipals;
    if (originPrincipals)
        JS_HoldPrincipals(originPrincipals);
}

ScriptSourceObject *
LazyScript::sourceObject() const
{
    return sourceObject_ ? &sourceObject_->as<ScriptSourceObject>() : NULL;
}

/* static */ LazyScript *
LazyScript::Create(ExclusiveContext *cx, HandleFunction fun,
                   uint32_t numFreeVariables, uint32_t numInnerFunctions, JSVersion version,
                   uint32_t begin, uint32_t end, uint32_t lineno, uint32_t column)
{
    JS_ASSERT(begin <= end);

    size_t bytes = (numFreeVariables * sizeof(HeapPtrAtom))
                 + (numInnerFunctions * sizeof(HeapPtrFunction));

    void *table = NULL;
    if (bytes) {
        table = cx->malloc_(bytes);
        if (!table)
            return NULL;
    }

    LazyScript *res = js_NewGCLazyScript(cx);
    if (!res)
        return NULL;

    return new (res) LazyScript(fun, table, numFreeVariables, numInnerFunctions, version,
                                begin, end, lineno, column);
}

uint32_t
LazyScript::staticLevel(JSContext *cx) const
{
    for (StaticScopeIter ssi(cx, enclosingScope()); !ssi.done(); ssi++) {
        if (ssi.type() == StaticScopeIter::FUNCTION)
            return ssi.funScript()->staticLevel + 1;
    }
    return 1;
}

void
JSScript::updateBaselineOrIonRaw()
{
#ifdef JS_ION
    if (hasIonScript()) {
        baselineOrIonRaw = ion->method()->raw();
        baselineOrIonSkipArgCheck = ion->method()->raw() + ion->getSkipArgCheckEntryOffset();
    } else if (hasBaselineScript()) {
        baselineOrIonRaw = baseline->method()->raw();
        baselineOrIonSkipArgCheck = baseline->method()->raw();
    } else {
        baselineOrIonRaw = NULL;
        baselineOrIonSkipArgCheck = NULL;
    }
#endif
}

static inline void
LazyScriptHash(uint32_t lineno, uint32_t column, uint32_t begin, uint32_t end,
               HashNumber hashes[3])
{
    HashNumber hash = lineno;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ column;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ begin;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ end;

    hashes[0] = hash;
    hashes[1] = JS_ROTATE_LEFT32(hashes[0], 4) ^ begin;
    hashes[2] = JS_ROTATE_LEFT32(hashes[1], 4) ^ end;
}

void
LazyScriptHashPolicy::hash(const Lookup &lookup, HashNumber hashes[3])
{
    LazyScript *lazy = lookup.lazy;
    LazyScriptHash(lazy->lineno(), lazy->column(), lazy->begin(), lazy->end(), hashes);
}

void
LazyScriptHashPolicy::hash(JSScript *script, HashNumber hashes[3])
{
    LazyScriptHash(script->lineno, script->column, script->sourceStart, script->sourceEnd, hashes);
}

bool
LazyScriptHashPolicy::match(JSScript *script, const Lookup &lookup)
{
    JSContext *cx = lookup.cx;
    LazyScript *lazy = lookup.lazy;

    // To be a match, the script and lazy script need to have the same line
    // and column and to be at the same position within their respective
    // source blobs, and to have the same source contents and version.
    //
    // While the surrounding code in the source may differ, this is
    // sufficient to ensure that compiling the lazy script will yield an
    // identical result to compiling the original script.
    //
    // Note that the filenames and origin principals of the lazy script and
    // original script can differ. If there is a match, these will be fixed
    // up in the resulting clone by the caller.

    if (script->lineno != lazy->lineno() ||
        script->column != lazy->column() ||
        script->getVersion() != lazy->version() ||
        script->sourceStart != lazy->begin() ||
        script->sourceEnd != lazy->end())
    {
        return false;
    }

    // GC activity may destroy the character pointers being compared below.
    AutoSuppressGC suppress(cx);

    const jschar *scriptChars = script->scriptSource()->chars(cx);
    if (!scriptChars)
        return false;

    const jschar *lazyChars = lazy->source()->chars(cx);
    if (!lazyChars)
        return false;

    size_t begin = script->sourceStart;
    size_t length = script->sourceEnd - begin;
    return !memcmp(scriptChars + begin, lazyChars + begin, length);
}
