/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
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

#include "xpcprivate.h"

#ifdef TAB
#undef TAB
#endif
#define TAB "    "

static const char* JSVAL2String(JSContext* cx, jsval val, JSBool* isString,
                                JSAutoByteString *bytes)
{
    JSAutoRequest ar(cx);
    const char* value = nsnull;
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
    JSPropertyDescArray callProps = {0, nsnull};
    JSPropertyDescArray thisProps = {0, nsnull};
    JSBool gotThisVal = false;
    jsval thisVal;
    JSObject* callObj = nsnull;
    JSString* funname = nsnull;
    JSAutoByteString funbytes;
    const char* filename = nsnull;
    PRInt32 lineno = 0;
    JSFunction* fun = nsnull;
    uint32_t namedArgCount = 0;
    jsval val;
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
                    callProps.array = nsnull;  // just to be sure
        }

        gotThisVal = JS_GetFrameThis(cx, fp, &thisVal);
        if (!gotThisVal ||
            !showThisProps ||
            JSVAL_IS_PRIMITIVE(thisVal) ||
            !JS_GetPropertyDescArray(cx, JSVAL_TO_OBJECT(thisVal),
                                     &thisProps)) {
            thisProps.array = nsnull;  // just to be sure
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
            if (desc->flags & JSPD_ARGUMENT) {
                JSAutoByteString nameBytes;
                const char* name = JSVAL2String(cx, desc->id, &isString, &nameBytes);
                if (!isString)
                    name = nsnull;
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
        }

        // print any unnamed trailing args (found in 'arguments' object)

        if (JS_GetProperty(cx, callObj, "arguments", &val) &&
            JSVAL_IS_OBJECT(val)) {
            uint32_t argCount;
            JSObject* argsObj = JSVAL_TO_OBJECT(val);
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
            if (desc->flags & JSPD_VARIABLE) {
                JSAutoByteString nameBytes;
                JSAutoByteString valueBytes;
                const char *name = JSVAL2String(cx, desc->id, nsnull, &nameBytes);
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
    }

    // print the value of 'this'

    if (showLocals) {
        if (gotThisVal) {
            JSString* thisValStr;
            JSAutoByteString thisValBytes;

            if (nsnull != (thisValStr = JS_ValueToString(cx, thisVal)) &&
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
                const char *name = JSVAL2String(cx, desc->id, nsnull, &nameBytes);
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
    JSStackFrame* iter = nsnull;
    int num = 0;

    while (nsnull != (fp = JS_FrameIterator(cx, &iter))) {
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
        fputs(buf, stdout);
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
        puts("Call to a debug function modifying state!");

    JS_ClearPendingException(cx);

    buf = FormatJSStackDump(cx, nsnull, showArgs, showLocals, showThisProps);
    if (!buf)
        puts("Failed to format JavaScript stack for dump");

    JS_RestoreExceptionState(cx, state);
    return buf;
}

/***************************************************************************/

static void
xpcDumpEvalErrorReporter(JSContext *cx, const char *message,
                         JSErrorReport *report)
{
    printf("Error: %s\n", message);
}

JSBool
xpc_DumpEvalInJSStackFrame(JSContext* cx, uint32_t frameno, const char* text)
{
    JSStackFrame* fp;
    JSStackFrame* iter = nsnull;
    uint32_t num = 0;

    if (!cx || !text) {
        puts("invalid params passed to xpc_DumpEvalInJSStackFrame!");
        return false;
    }

    printf("js[%d]> %s\n", frameno, text);

    while (nsnull != (fp = JS_FrameIterator(cx, &iter))) {
        if (num == frameno)
            break;
        num++;
    }

    if (!fp) {
        puts("invalid frame number!");
        return false;
    }

    JSAutoRequest ar(cx);

    JSExceptionState* exceptionState = JS_SaveExceptionState(cx);
    JSErrorReporter older = JS_SetErrorReporter(cx, xpcDumpEvalErrorReporter);

    jsval rval;
    JSString* str;
    JSAutoByteString bytes;
    if (JS_EvaluateInStackFrame(cx, fp, text, strlen(text), "eval", 1, &rval) &&
        nsnull != (str = JS_ValueToString(cx, rval)) &&
        bytes.encode(cx, str)) {
        printf("%s\n", bytes.ptr());
    } else
        puts("eval failed!");
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
    "------------------------------------------------------------------------";
    puts(line);
    puts("Hit JavaScript \"debugger\" keyword. JS call stack...");
    xpc_DumpJSStack(cx, true, true, false);
    puts(line);
    return JSTRAP_CONTINUE;
}

JSBool xpc_InstallJSDebuggerKeywordHandler(JSRuntime* rt)
{
    return JS_SetDebuggerHandler(rt, xpc_DebuggerKeywordHandler, nsnull);
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
        printf("%p 'native' <%s>",
               (void *)obj, js::GetObjectClass(obj)->name);
    else
        printf("%p 'host'", (void *)obj);
}

static void PrintObject(JSObject* obj, int depth, ObjectPile* pile)
{
    PrintObjectBasics(obj);

    switch (pile->Visit(obj)) {
    case ObjectPile::primary:
        puts("");
        break;
    case ObjectPile::seen:
        puts(" (SEE ABOVE)");
        return;
    case ObjectPile::overflow:
        puts(" (TOO MANY OBJECTS)");
        return;
    }

    if (!JS_IsNative(obj))
        return;

    JSObject* parent = js::GetObjectParent(obj);
    JSObject* proto  = js::GetObjectProto(obj);

    printf("%*sparent: ", INDENT(depth+1));
    if (parent)
        PrintObject(parent, depth+1, pile);
    else
        puts("null");
    printf("%*sproto: ", INDENT(depth+1));
    if (proto)
        PrintObject(proto, depth+1, pile);
    else
        puts("null");
}

JSBool
xpc_DumpJSObject(JSObject* obj)
{
    ObjectPile pile;

    puts("Debugging reminders...");
    puts("  class:  (JSClass*)(obj->fslots[2]-1)");
    puts("  parent: (JSObject*)(obj->fslots[1])");
    puts("  proto:  (JSObject*)(obj->fslots[0])");
    puts("");

    if (obj)
        PrintObject(obj, 0, &pile);
    else
        puts("xpc_DumpJSObject passed null!");

    return true;
}

#ifdef DEBUG
void
xpc_PrintAllReferencesTo(void *p)
{
    /* p must be a JS object */
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    JS_DumpHeap(rt->GetJSRuntime(), stdout, nsnull, JSTRACE_OBJECT, p, 0x7fffffff, nsnull);
}
#endif
