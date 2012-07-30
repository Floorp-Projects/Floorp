/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"

#ifdef XP_WIN
#include <windows.h>
#endif

#ifdef TAB
#undef TAB
#endif
#define TAB "    "

static void DebugDump(const char* fmt, ...)
{
  char buffer[2048];
  va_list ap;
  va_start(ap, fmt);
#ifdef XPWIN
  _vsnprintf(buffer, sizeof(buffer), fmt, ap);
#else
  vsnprintf(buffer, sizeof(buffer), fmt, ap);
#endif
  buffer[sizeof(buffer)-1] = '\0';
  va_end(ap);
#ifdef XP_WIN
  if (IsDebuggerPresent()) {
    OutputDebugStringA(buffer);
  }
#endif
  printf("%s", buffer);
}

static const char* JSVAL2String(JSContext* cx, jsval val, JSBool* isString,
                                JSAutoByteString *bytes)
{
    JSAutoRequest ar(cx);
    const char* value = nullptr;
    JSString* value_str = JS_ValueToString(cx, val);
    if (value_str)
        value = bytes->encode(cx, value_str);
    if (value) {
        const char* found = strstr(value, "function ");
        if (found && (value == found || value+1 == found || value+2 == found))
            value = "[function]";
    }

    if (isString)
        *isString = JSVAL_IS_STRING(val);
    return value;
}

static char* FormatJSFrame(JSContext* cx, JSStackFrame* fp,
                           char* buf, int num,
                           JSBool showArgs, JSBool showLocals, JSBool showThisProps)
{
    JSPropertyDescArray callProps = {0, nullptr};
    JSPropertyDescArray thisProps = {0, nullptr};
    JSBool gotThisVal = false;
    jsval thisVal;
    JSObject* callObj = nullptr;
    JSString* funname = nullptr;
    JSAutoByteString funbytes;
    const char* filename = nullptr;
    PRInt32 lineno = 0;
    JSFunction* fun = nullptr;
    uint32_t namedArgCount = 0;
    JSBool isString;

    // get the info for this stack frame

    JSScript* script = JS_GetFrameScript(cx, fp);
    jsbytecode* pc = JS_GetFramePC(cx, fp);

    JSAutoRequest ar(cx);
    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, JS_GetGlobalForFrame(fp)))
        return buf;

    if (script && pc) {
        filename = JS_GetScriptFilename(cx, script);
        lineno =  (PRInt32) JS_PCToLineNumber(cx, script, pc);
        fun = JS_GetFrameFunction(cx, fp);
        if (fun)
            funname = JS_GetFunctionId(fun);

        if (showArgs || showLocals) {
            callObj = JS_GetFrameCallObject(cx, fp);
            if (callObj)
                if (!JS_GetPropertyDescArray(cx, callObj, &callProps))
                    callProps.array = nullptr;  // just to be sure
        }

        gotThisVal = JS_GetFrameThis(cx, fp, &thisVal);
        if (!gotThisVal ||
            !showThisProps ||
            JSVAL_IS_PRIMITIVE(thisVal) ||
            !JS_GetPropertyDescArray(cx, JSVAL_TO_OBJECT(thisVal),
                                     &thisProps)) {
            thisProps.array = nullptr;  // just to be sure
        }
    }

    // print the frame number and function name

    if (funname)
        buf = JS_sprintf_append(buf, "%d %s(", num, funbytes.encode(cx, funname));
    else if (fun)
        buf = JS_sprintf_append(buf, "%d anonymous(", num);
    else
        buf = JS_sprintf_append(buf, "%d <TOP LEVEL>", num);
    if (!buf) goto out;

    // print the function arguments

    if (showArgs && callObj) {
        for (uint32_t i = 0; i < callProps.length; i++) {
            JSPropertyDesc* desc = &callProps.array[i];
            JSAutoByteString nameBytes;
            const char* name = JSVAL2String(cx, desc->id, &isString, &nameBytes);
            if (!isString)
                name = nullptr;
            JSAutoByteString valueBytes;
            const char* value = JSVAL2String(cx, desc->value, &isString, &valueBytes);

            buf = JS_sprintf_append(buf, "%s%s%s%s%s%s",
                                    namedArgCount ? ", " : "",
                                    name ? name :"",
                                    name ? " = " : "",
                                    isString ? "\"" : "",
                                    value ? value : "?unknown?",
                                    isString ? "\"" : "");
            if (!buf) goto out;
            namedArgCount++;
        }

        // print any unnamed trailing args (found in 'arguments' object)
        JS::Value val;
        if (JS_GetProperty(cx, callObj, "arguments", &val) &&
            val.isObject()) {
            uint32_t argCount;
            JSObject* argsObj = &val.toObject();
            if (JS_GetProperty(cx, argsObj, "length", &val) &&
                JS_ValueToECMAUint32(cx, val, &argCount) &&
                argCount > namedArgCount) {
                for (uint32_t k = namedArgCount; k < argCount; k++) {
                    char number[8];
                    JS_snprintf(number, 8, "%d", (int) k);

                    if (JS_GetProperty(cx, argsObj, number, &val)) {
                        JSAutoByteString valueBytes;
                        const char *value = JSVAL2String(cx, val, &isString, &valueBytes);
                        buf = JS_sprintf_append(buf, "%s%s%s%s",
                                                k ? ", " : "",
                                                isString ? "\"" : "",
                                                value ? value : "?unknown?",
                                                isString ? "\"" : "");
                        if (!buf) goto out;
                    }
                }
            }
        }
    }

    // print filename and line number

    buf = JS_sprintf_append(buf, "%s [\"%s\":%d]\n",
                            fun ? ")" : "",
                            filename ? filename : "<unknown>",
                            lineno);
    if (!buf) goto out;

    // print local variables

    if (showLocals && callProps.array) {
        for (uint32_t i = 0; i < callProps.length; i++) {
            JSPropertyDesc* desc = &callProps.array[i];
            JSAutoByteString nameBytes;
            JSAutoByteString valueBytes;
            const char *name = JSVAL2String(cx, desc->id, nullptr, &nameBytes);
            const char *value = JSVAL2String(cx, desc->value, &isString, &valueBytes);

            if (name && value) {
                buf = JS_sprintf_append(buf, TAB "%s = %s%s%s\n",
                                        name,
                                        isString ? "\"" : "",
                                        value,
                                        isString ? "\"" : "");
                if (!buf) goto out;
            }
        }
    }

    // print the value of 'this'

    if (showLocals) {
        if (gotThisVal) {
            JSString* thisValStr;
            JSAutoByteString thisValBytes;

            if (nullptr != (thisValStr = JS_ValueToString(cx, thisVal)) &&
                thisValBytes.encode(cx, thisValStr)) {
                buf = JS_sprintf_append(buf, TAB "this = %s\n", thisValBytes.ptr());
                if (!buf) goto out;
            }
        } else
            buf = JS_sprintf_append(buf, TAB "<failed to get 'this' value>\n");
    }

    // print the properties of 'this', if it is an object

    if (showThisProps && thisProps.array) {

        for (uint32_t i = 0; i < thisProps.length; i++) {
            JSPropertyDesc* desc = &thisProps.array[i];
            if (desc->flags & JSPD_ENUMERATE) {
                JSAutoByteString nameBytes;
                JSAutoByteString valueBytes;
                const char *name = JSVAL2String(cx, desc->id, nullptr, &nameBytes);
                const char *value = JSVAL2String(cx, desc->value, &isString, &valueBytes);
                if (name && value) {
                    buf = JS_sprintf_append(buf, TAB "this.%s = %s%s%s\n",
                                            name,
                                            isString ? "\"" : "",
                                            value,
                                            isString ? "\"" : "");
                    if (!buf) goto out;
                }
            }
        }
    }

out:
    if (callProps.array)
        JS_PutPropertyDescArray(cx, &callProps);
    if (thisProps.array)
        JS_PutPropertyDescArray(cx, &thisProps);
    return buf;
}

static char* FormatJSStackDump(JSContext* cx, char* buf,
                               JSBool showArgs, JSBool showLocals,
                               JSBool showThisProps)
{
    JSStackFrame* fp;
    JSStackFrame* iter = nullptr;
    int num = 0;

    while (nullptr != (fp = JS_FrameIterator(cx, &iter))) {
        buf = FormatJSFrame(cx, fp, buf, num, showArgs, showLocals, showThisProps);
        num++;
    }

    if (!num)
        buf = JS_sprintf_append(buf, "JavaScript stack is empty\n");

    return buf;
}

JSBool
xpc_DumpJSStack(JSContext* cx, JSBool showArgs, JSBool showLocals, JSBool showThisProps)
{
    if (char* buf = xpc_PrintJSStack(cx, showArgs, showLocals, showThisProps)) {
        DebugDump("%s\n", buf);
        JS_smprintf_free(buf);
    }
    return true;
}

char*
xpc_PrintJSStack(JSContext* cx, JSBool showArgs, JSBool showLocals,
                 JSBool showThisProps)
{
    char* buf;
    JSExceptionState *state = JS_SaveExceptionState(cx);
    if (!state)
        DebugDump("%s", "Call to a debug function modifying state!\n");

    JS_ClearPendingException(cx);

    buf = FormatJSStackDump(cx, nullptr, showArgs, showLocals, showThisProps);
    if (!buf)
        DebugDump("%s", "Failed to format JavaScript stack for dump\n");

    JS_RestoreExceptionState(cx, state);
    return buf;
}

/***************************************************************************/

static void
xpcDumpEvalErrorReporter(JSContext *cx, const char *message,
                         JSErrorReport *report)
{
    DebugDump("Error: %s\n", message);
}

JSBool
xpc_DumpEvalInJSStackFrame(JSContext* cx, uint32_t frameno, const char* text)
{
    JSStackFrame* fp;
    JSStackFrame* iter = nullptr;
    uint32_t num = 0;

    if (!cx || !text) {
        DebugDump("%s", "invalid params passed to xpc_DumpEvalInJSStackFrame!\n");
        return false;
    }

    DebugDump("js[%d]> %s\n", frameno, text);

    while (nullptr != (fp = JS_FrameIterator(cx, &iter))) {
        if (num == frameno)
            break;
        num++;
    }

    if (!fp) {
        DebugDump("%s", "invalid frame number!\n");
        return false;
    }

    JSAutoRequest ar(cx);

    JSExceptionState* exceptionState = JS_SaveExceptionState(cx);
    JSErrorReporter older = JS_SetErrorReporter(cx, xpcDumpEvalErrorReporter);

    jsval rval;
    JSString* str;
    JSAutoByteString bytes;
    if (JS_EvaluateInStackFrame(cx, fp, text, strlen(text), "eval", 1, &rval) &&
        nullptr != (str = JS_ValueToString(cx, rval)) &&
        bytes.encode(cx, str)) {
        DebugDump("%s\n", bytes.ptr());
    } else
        DebugDump("%s", "eval failed!\n");
    JS_SetErrorReporter(cx, older);
    JS_RestoreExceptionState(cx, exceptionState);
    return true;
}

/***************************************************************************/

JSTrapStatus
xpc_DebuggerKeywordHandler(JSContext *cx, JSScript *script, jsbytecode *pc,
                           jsval *rval, void *closure)
{
    static const char line[] =
    "------------------------------------------------------------------------\n";
    DebugDump("%s", line);
    DebugDump("%s", "Hit JavaScript \"debugger\" keyword. JS call stack...\n");
    xpc_DumpJSStack(cx, true, true, false);
    DebugDump("%s", line);
    return JSTRAP_CONTINUE;
}

JSBool xpc_InstallJSDebuggerKeywordHandler(JSRuntime* rt)
{
    return JS_SetDebuggerHandler(rt, xpc_DebuggerKeywordHandler, nullptr);
}

/***************************************************************************/

// The following will dump info about an object to stdout...


// Quick and dirty (debug only damnit!) class to track which JSObjects have
// been visited as we traverse.

class ObjectPile
{
public:
    enum result {primary, seen, overflow};

    result Visit(JSObject* obj)
    {
        if (member_count == max_count)
            return overflow;
        for (int i = 0; i < member_count; i++)
            if (array[i] == obj)
                return seen;
        array[member_count++] = obj;
        return primary;
    }

    ObjectPile() : member_count(0){}

private:
    enum {max_count = 50};
    JSObject* array[max_count];
    int member_count;
};


static const int tab_width = 2;
#define INDENT(_d) (_d)*tab_width, " "

static void PrintObjectBasics(JSObject* obj)
{
    if (JS_IsNative(obj))
        DebugDump("%p 'native' <%s>",
                  (void *)obj, js::GetObjectClass(obj)->name);
    else
        DebugDump("%p 'host'", (void *)obj);
}

static void PrintObject(JSObject* obj, int depth, ObjectPile* pile)
{
    PrintObjectBasics(obj);

    switch (pile->Visit(obj)) {
    case ObjectPile::primary:
        DebugDump("%s", "\n");
        break;
    case ObjectPile::seen:
        DebugDump("%s", " (SEE ABOVE)\n");
        return;
    case ObjectPile::overflow:
        DebugDump("%s", " (TOO MANY OBJECTS)\n");
        return;
    }

    if (!JS_IsNative(obj))
        return;

    JSObject* parent = js::GetObjectParent(obj);
    JSObject* proto  = js::GetObjectProto(obj);

    DebugDump("%*sparent: ", INDENT(depth+1));
    if (parent)
        PrintObject(parent, depth+1, pile);
    else
        DebugDump("%s", "null\n");
    DebugDump("%*sproto: ", INDENT(depth+1));
    if (proto)
        PrintObject(proto, depth+1, pile);
    else
        DebugDump("%s", "null\n");
}

JSBool
xpc_DumpJSObject(JSObject* obj)
{
    ObjectPile pile;

    DebugDump("%s", "Debugging reminders...\n");
    DebugDump("%s", "  class:  (JSClass*)(obj->fslots[2]-1)\n");
    DebugDump("%s", "  parent: (JSObject*)(obj->fslots[1])\n");
    DebugDump("%s", "  proto:  (JSObject*)(obj->fslots[0])\n");
    DebugDump("%s", "\n");

    if (obj)
        PrintObject(obj, 0, &pile);
    else
        DebugDump("%s", "xpc_DumpJSObject passed null!\n");

    return true;
}

#ifdef DEBUG
void
xpc_PrintAllReferencesTo(void *p)
{
    /* p must be a JS object */
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    JS_DumpHeap(rt->GetJSRuntime(), stdout, nullptr, JSTRACE_OBJECT, p, 0x7fffffff, nullptr);
}
#endif
