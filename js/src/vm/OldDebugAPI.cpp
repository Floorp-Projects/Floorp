/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS debugging API.
 */

#include "js/OldDebugAPI.h"

#include <string.h>

#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jstypes.h"

#include "asmjs/AsmJSModule.h"
#include "frontend/SourceNotes.h"
#include "vm/Debugger.h"
#include "vm/Shape.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsscriptinlines.h"

#include "vm/Debugger-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::PodZero;

JS_PUBLIC_API(bool)
JS_GetDebugMode(JSContext *cx)
{
    return cx->compartment()->debugMode();
}

JS_PUBLIC_API(bool)
JS_SetDebugMode(JSContext *cx, bool debug)
{
    return JS_SetDebugModeForCompartment(cx, cx->compartment(), debug);
}

JS_PUBLIC_API(void)
JS_SetRuntimeDebugMode(JSRuntime *rt, bool debug)
{
    rt->debugMode = !!debug;
}

JSTrapStatus
js::ScriptDebugPrologue(JSContext *cx, AbstractFramePtr frame, jsbytecode *pc)
{
    JS_ASSERT_IF(frame.isInterpreterFrame(), frame.asInterpreterFrame() == cx->interpreterFrame());

    RootedValue rval(cx);
    JSTrapStatus status = Debugger::onEnterFrame(cx, frame, &rval);
    switch (status) {
      case JSTRAP_CONTINUE:
        break;
      case JSTRAP_THROW:
        cx->setPendingException(rval);
        break;
      case JSTRAP_ERROR:
        cx->clearPendingException();
        break;
      case JSTRAP_RETURN:
        frame.setReturnValue(rval);
        break;
      default:
        MOZ_CRASH("bad Debugger::onEnterFrame JSTrapStatus value");
    }
    return status;
}

bool
js::ScriptDebugEpilogue(JSContext *cx, AbstractFramePtr frame, jsbytecode *pc, bool okArg)
{
    JS_ASSERT_IF(frame.isInterpreterFrame(), frame.asInterpreterFrame() == cx->interpreterFrame());

    bool ok = okArg;

    return Debugger::onLeaveFrame(cx, frame, ok);
}

JSTrapStatus
js::DebugExceptionUnwind(JSContext *cx, AbstractFramePtr frame, jsbytecode *pc)
{
    JS_ASSERT(cx->compartment()->debugMode());

    if (cx->compartment()->getDebuggees().empty())
        return JSTRAP_CONTINUE;

    /* Call debugger throw hook if set. */
    RootedValue rval(cx);
    JSTrapStatus status = Debugger::onExceptionUnwind(cx, &rval);

    switch (status) {
      case JSTRAP_ERROR:
        cx->clearPendingException();
        break;

      case JSTRAP_RETURN:
        cx->clearPendingException();
        frame.setReturnValue(rval);
        break;

      case JSTRAP_THROW:
        cx->setPendingException(rval);
        break;

      case JSTRAP_CONTINUE:
        break;

      default:
        MOZ_CRASH("Invalid trap status");
    }

    return status;
}

JS_FRIEND_API(bool)
JS_SetDebugModeForAllCompartments(JSContext *cx, bool debug)
{
    for (ZonesIter zone(cx->runtime(), SkipAtoms); !zone.done(); zone.next()) {
        // Invalidate a zone at a time to avoid doing a ZoneCellIter
        // per compartment.
        AutoDebugModeInvalidation invalidate(zone);
        for (CompartmentsInZoneIter c(zone); !c.done(); c.next()) {
            // Ignore special compartments (atoms, JSD compartments)
            if (c->principals) {
                if (!c->setDebugModeFromC(cx, !!debug, invalidate))
                    return false;
            }
        }
    }
    return true;
}

JS_FRIEND_API(bool)
JS_SetDebugModeForCompartment(JSContext *cx, JSCompartment *comp, bool debug)
{
    AutoDebugModeInvalidation invalidate(comp);
    return comp->setDebugModeFromC(cx, !!debug, invalidate);
}

static bool
CheckDebugMode(JSContext *cx)
{
    bool debugMode = JS_GetDebugMode(cx);
    /*
     * :TODO:
     * This probably should be an assertion, since it's indicative of a severe
     * API misuse.
     */
    if (!debugMode) {
        JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage,
                                     nullptr, JSMSG_NEED_DEBUG_MODE);
    }
    return debugMode;
}

JS_PUBLIC_API(bool)
JS_SetSingleStepMode(JSContext *cx, HandleScript script, bool singleStep)
{
    assertSameCompartment(cx, script);

    if (!CheckDebugMode(cx))
        return false;

    return script->setStepModeFlag(cx, singleStep);
}


/************************************************************************/

JS_PUBLIC_API(unsigned)
JS_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    return js::PCToLineNumber(script, pc);
}

JS_PUBLIC_API(jsbytecode *)
JS_LineNumberToPC(JSContext *cx, JSScript *script, unsigned lineno)
{
    return js_LineNumberToPC(script, lineno);
}

JS_PUBLIC_API(JSScript *)
JS_GetFunctionScript(JSContext *cx, HandleFunction fun)
{
    if (fun->isNative())
        return nullptr;
    if (fun->isInterpretedLazy()) {
        AutoCompartment funCompartment(cx, fun);
        JSScript *script = fun->getOrCreateScript(cx);
        if (!script)
            MOZ_CRASH();
        return script;
    }
    return fun->nonLazyScript();
}

/************************************************************************/

JS_PUBLIC_API(const char *)
JS_GetScriptFilename(JSScript *script)
{
    return script->filename();
}

JS_PUBLIC_API(const jschar *)
JS_GetScriptSourceMap(JSContext *cx, JSScript *script)
{
    ScriptSource *source = script->scriptSource();
    JS_ASSERT(source);
    return source->hasSourceMapURL() ? source->sourceMapURL() : nullptr;
}

JS_PUBLIC_API(unsigned)
JS_GetScriptBaseLineNumber(JSContext *cx, JSScript *script)
{
    return script->lineno();
}

/***************************************************************************/

extern JS_PUBLIC_API(void)
JS_DumpPCCounts(JSContext *cx, HandleScript script)
{
    JS_ASSERT(script->hasScriptCounts());

    Sprinter sprinter(cx);
    if (!sprinter.init())
        return;

    fprintf(stdout, "--- SCRIPT %s:%d ---\n", script->filename(), (int) script->lineno());
    js_DumpPCCounts(cx, script, &sprinter);
    fputs(sprinter.string(), stdout);
    fprintf(stdout, "--- END SCRIPT %s:%d ---\n", script->filename(), (int) script->lineno());
}

JS_PUBLIC_API(void)
JS_DumpCompartmentPCCounts(JSContext *cx)
{
    for (ZoneCellIter i(cx->zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        RootedScript script(cx, i.get<JSScript>());
        if (script->compartment() != cx->compartment())
            continue;

        if (script->hasScriptCounts())
            JS_DumpPCCounts(cx, script);
    }

    for (unsigned thingKind = FINALIZE_OBJECT0; thingKind < FINALIZE_OBJECT_LIMIT; thingKind++) {
        for (ZoneCellIter i(cx->zone(), (AllocKind) thingKind); !i.done(); i.next()) {
            JSObject *obj = i.get<JSObject>();
            if (obj->compartment() != cx->compartment())
                continue;

            if (obj->is<AsmJSModuleObject>()) {
                AsmJSModule &module = obj->as<AsmJSModuleObject>().module();

                Sprinter sprinter(cx);
                if (!sprinter.init())
                    return;

                fprintf(stdout, "--- Asm.js Module ---\n");

                for (size_t i = 0; i < module.numFunctionCounts(); i++) {
                    jit::IonScriptCounts *counts = module.functionCounts(i);
                    DumpIonScriptCounts(&sprinter, counts);
                }

                fputs(sprinter.string(), stdout);
                fprintf(stdout, "--- END Asm.js Module ---\n");
            }
        }
    }
}

static const char *
FormatValue(JSContext *cx, const Value &vArg, JSAutoByteString &bytes)
{
    RootedValue v(cx, vArg);

    /*
     * We could use Maybe<AutoCompartment> here, but G++ can't quite follow
     * that, and warns about uninitialized members being used in the
     * destructor.
     */
    RootedString str(cx);
    if (v.isObject()) {
        AutoCompartment ac(cx, &v.toObject());
        str = ToString<CanGC>(cx, v);
    } else {
        str = ToString<CanGC>(cx, v);
    }

    if (!str)
        return nullptr;
    const char *buf = bytes.encodeLatin1(cx, str);
    if (!buf)
        return nullptr;
    const char *found = strstr(buf, "function ");
    if (found && (found - buf <= 2))
        return "[function]";
    return buf;
}

static char *
FormatFrame(JSContext *cx, const NonBuiltinScriptFrameIter &iter, char *buf, int num,
            bool showArgs, bool showLocals, bool showThisProps)
{
    JS_ASSERT(!cx->isExceptionPending());
    RootedScript script(cx, iter.script());
    jsbytecode* pc = iter.pc();

    RootedObject scopeChain(cx, iter.scopeChain());
    JSAutoCompartment ac(cx, scopeChain);

    const char *filename = script->filename();
    unsigned lineno = PCToLineNumber(script, pc);
    RootedFunction fun(cx, iter.maybeCallee());
    RootedString funname(cx);
    if (fun)
        funname = fun->atom();

    RootedValue thisVal(cx);
    if (iter.hasUsableAbstractFramePtr() && iter.computeThis(cx)) {
        thisVal = iter.computedThisValue();
    }

    // print the frame number and function name
    if (funname) {
        JSAutoByteString funbytes;
        buf = JS_sprintf_append(buf, "%d %s(", num, funbytes.encodeLatin1(cx, funname));
    } else if (fun) {
        buf = JS_sprintf_append(buf, "%d anonymous(", num);
    } else {
        buf = JS_sprintf_append(buf, "%d <TOP LEVEL>", num);
    }
    if (!buf)
        return buf;

    if (showArgs && iter.hasArgs()) {
        BindingVector bindings(cx);
        if (fun && fun->isInterpreted()) {
            if (!FillBindingVector(script, &bindings))
                return buf;
        }


        bool first = true;
        for (unsigned i = 0; i < iter.numActualArgs(); i++) {
            RootedValue arg(cx);
            if (i < iter.numFormalArgs() && script->formalIsAliased(i)) {
                for (AliasedFormalIter fi(script); ; fi++) {
                    if (fi.frameIndex() == i) {
                        arg = iter.callObj().aliasedVar(fi);
                        break;
                    }
                }
            } else if (script->argsObjAliasesFormals() && iter.hasArgsObj()) {
                arg = iter.argsObj().arg(i);
            } else {
                arg = iter.unaliasedActual(i, DONT_CHECK_ALIASING);
            }

            JSAutoByteString valueBytes;
            const char *value = FormatValue(cx, arg, valueBytes);

            JSAutoByteString nameBytes;
            const char *name = nullptr;

            if (i < bindings.length()) {
                name = nameBytes.encodeLatin1(cx, bindings[i].name());
                if (!buf)
                    return nullptr;
            }

            if (value) {
                buf = JS_sprintf_append(buf, "%s%s%s%s%s%s",
                                        !first ? ", " : "",
                                        name ? name :"",
                                        name ? " = " : "",
                                        arg.isString() ? "\"" : "",
                                        value ? value : "?unknown?",
                                        arg.isString() ? "\"" : "");
                if (!buf)
                    return buf;

                first = false;
            } else {
                buf = JS_sprintf_append(buf, "    <Failed to get argument while inspecting stack frame>\n");
                if (!buf)
                    return buf;
                cx->clearPendingException();

            }
        }
    }

    // print filename and line number
    buf = JS_sprintf_append(buf, "%s [\"%s\":%d]\n",
                            fun ? ")" : "",
                            filename ? filename : "<unknown>",
                            lineno);
    if (!buf)
        return buf;


    // Note: Right now we don't dump the local variables anymore, because
    // that is hard to support across all the JITs etc.

    // print the value of 'this'
    if (showLocals) {
        if (!thisVal.isUndefined()) {
            JSAutoByteString thisValBytes;
            RootedString thisValStr(cx, ToString<CanGC>(cx, thisVal));
            const char *str = nullptr;
            if (thisValStr &&
                (str = thisValBytes.encodeLatin1(cx, thisValStr)))
            {
                buf = JS_sprintf_append(buf, "    this = %s\n", str);
                if (!buf)
                    return buf;
            } else {
                buf = JS_sprintf_append(buf, "    <failed to get 'this' value>\n");
                cx->clearPendingException();
            }
        }
    }

    if (showThisProps && thisVal.isObject()) {
        RootedObject obj(cx, &thisVal.toObject());

        AutoIdVector keys(cx);
        if (!GetPropertyNames(cx, obj, JSITER_OWNONLY, &keys)) {
            cx->clearPendingException();
            return buf;
        }

        RootedId id(cx);
        for (size_t i = 0; i < keys.length(); i++) {
            RootedId id(cx, keys[i]);
            RootedValue key(cx, IdToValue(id));
            RootedValue v(cx);

            if (!JSObject::getGeneric(cx, obj, obj, id, &v)) {
                buf = JS_sprintf_append(buf, "    <Failed to fetch property while inspecting stack frame>\n");
                cx->clearPendingException();
                continue;
            }

            JSAutoByteString nameBytes;
            JSAutoByteString valueBytes;
            const char *name = FormatValue(cx, key, nameBytes);
            const char *value = FormatValue(cx, v, valueBytes);
            if (name && value) {
                buf = JS_sprintf_append(buf, "    this.%s = %s%s%s\n",
                                        name,
                                        v.isString() ? "\"" : "",
                                        value,
                                        v.isString() ? "\"" : "");
                if (!buf)
                    return buf;
            } else {
                buf = JS_sprintf_append(buf, "    <Failed to format values while inspecting stack frame>\n");
                cx->clearPendingException();
            }
        }
    }

    JS_ASSERT(!cx->isExceptionPending());
    return buf;
}

JS_PUBLIC_API(char *)
JS::FormatStackDump(JSContext *cx, char *buf, bool showArgs, bool showLocals, bool showThisProps)
{
    int num = 0;

    for (NonBuiltinScriptFrameIter i(cx); !i.done(); ++i) {
        buf = FormatFrame(cx, i, buf, num, showArgs, showLocals, showThisProps);
        num++;
    }

    if (!num)
        buf = JS_sprintf_append(buf, "JavaScript stack is empty\n");

    return buf;
}
