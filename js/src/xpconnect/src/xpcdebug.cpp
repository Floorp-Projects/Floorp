/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifdef DEBUG
#include "xpcprivate.h"

#ifdef TAB
#undef TAB
#endif
#define TAB "    "

static const char* JSVAL2String(JSContext* cx, jsval val, JSBool* isString)
{
    const char* value = nsnull;
    JSString* value_str = JS_ValueToString(cx, val);
    if(value_str)
        value = JS_GetStringBytes(value_str);
    if(value)
    {
        const char* found = strstr(value, "function ");
        if(found && (value == found || value+1 == found || value+2 == found))
            value = "[function]";
    }

    if(isString)
        *isString = JSVAL_IS_STRING(val);
    return value;
}

static char* FormatJSFrame(JSContext* cx, JSStackFrame* fp,
                           char* buf, int num,
                           JSBool showArgs, JSBool showLocals, JSBool showThisProps)
{
    if(JS_IsNativeFrame(cx, fp))
        return JS_sprintf_append(buf, "%d [native frame]\n", num);

    JSPropertyDescArray callProps = {0, nsnull};
    JSPropertyDescArray thisProps = {0, nsnull};
    JSObject* thisObj = nsnull;
    JSObject* callObj = nsnull;
    const char* funname = nsnull;
    const char* filename = nsnull;
    PRInt32 lineno = 0;
    JSFunction* fun = nsnull;
    uint32 namedArgCount = 0;
    jsval val;
    const char* name;
    const char* value;
    JSBool isString;

    // get the info for this stack frame

    JSScript* script = JS_GetFrameScript(cx, fp);
    jsbytecode* pc = JS_GetFramePC(cx, fp);
    if(script && pc)
    {
        filename = JS_GetScriptFilename(cx, script);
        lineno =  (PRInt32) JS_PCToLineNumber(cx, script, pc);
        fun = JS_GetFrameFunction(cx, fp);
        if(fun)
            funname = JS_GetFunctionName(fun);

        if(showArgs || showLocals)
        {
            callObj = JS_GetFrameCallObject(cx, fp);
            if(callObj)
                if(!JS_GetPropertyDescArray(cx, callObj, &callProps))
                    callProps.array = nsnull;  // just to be sure
        }

        thisObj = JS_GetFrameThis(cx, fp);
        if(showThisProps)
        {
            if(thisObj)
                if(!JS_GetPropertyDescArray(cx, thisObj, &thisProps))
                    thisProps.array = nsnull;  // just to be sure
        }
    }

    // print the frame number and function name

    if(funname)
        buf = JS_sprintf_append(buf, "%d %s(", num, funname);
    else if(fun)
        buf = JS_sprintf_append(buf, "%d anonymous(", num);
    else
        buf = JS_sprintf_append(buf, "%d <TOP LEVEL>", num);
    if(!buf) goto out;

    // print the function arguments

    if(showArgs && callObj)
    {
        for(uint32 i = 0; i < callProps.length; i++)
        {
            JSPropertyDesc* desc = &callProps.array[i];
            if(desc->flags & JSPD_ARGUMENT)
            {
                name = JSVAL2String(cx, desc->id, &isString);
                if(!isString)
                    name = nsnull;
                value = JSVAL2String(cx, desc->value, &isString);

                buf = JS_sprintf_append(buf, "%s%s%s%s%s%s",
                                        namedArgCount ? ", " : "",
                                        name ? name :"",
                                        name ? " = " : "",
                                        isString ? "\"" : "",
                                        value ? value : "?unknown?",
                                        isString ? "\"" : "");
                if(!buf) goto out;
                namedArgCount++;
            }
        }

        // print any unnamed trailing args (found in 'arguments' object)

        if(JS_GetProperty(cx, callObj, "arguments", &val) &&
           JSVAL_IS_OBJECT(val))
        {
            uint32 argCount;
            JSObject* argsObj = JSVAL_TO_OBJECT(val);
            if(JS_GetProperty(cx, argsObj, "length", &val) &&
               JS_ValueToECMAUint32(cx, val, &argCount) &&
               argCount > namedArgCount)
            {
                for(uint32 k = namedArgCount; k < argCount; k++)
                {
                    char number[8];
                    JS_snprintf(number, 8, "%d", (int) k);

                    if(JS_GetProperty(cx, argsObj, number, &val))
                    {
                        value = JSVAL2String(cx, val, &isString);
                        buf = JS_sprintf_append(buf, "%s%s%s%s",
                                        k ? ", " : "",
                                        isString ? "\"" : "",
                                        value ? value : "?unknown?",
                                        isString ? "\"" : "");
                        if(!buf) goto out;
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
    if(!buf) goto out;

    // print local variables

    if(showLocals && callProps.array)
    {
        for(uint32 i = 0; i < callProps.length; i++)
        {
            JSPropertyDesc* desc = &callProps.array[i];
            if(desc->flags & JSPD_VARIABLE)
            {
                name = JSVAL2String(cx, desc->id, nsnull);
                value = JSVAL2String(cx, desc->value, &isString);

                if(name && value)
                {
                    buf = JS_sprintf_append(buf, TAB "%s = %s%s%s\n",
                                            name,
                                            isString ? "\"" : "",
                                            value,
                                            isString ? "\"" : "");
                    if(!buf) goto out;
                }
            }
        }
    }

    // print the value of 'this'

    if(showLocals && thisObj)
    {
        jsval thisJSVal = OBJECT_TO_JSVAL(thisObj);
        JSString* thisValStr;
        char* thisVal;

        if(nsnull != (thisValStr = JS_ValueToString(cx, thisJSVal)) &&
           nsnull != (thisVal = JS_GetStringBytes(thisValStr)))
        {
            buf = JS_sprintf_append(buf, TAB "this = %s\n", thisVal);
            if(!buf) goto out;
        }
    }

    // print the properties of 'this'

    if(showThisProps && thisProps.array)
    {

        for(uint32 i = 0; i < thisProps.length; i++)
        {
            JSPropertyDesc* desc = &thisProps.array[i];
            if(desc->flags & JSPD_ENUMERATE)
            {

                name = JSVAL2String(cx, desc->id, nsnull);
                value = JSVAL2String(cx, desc->value, &isString);
                if(name && value)
                {
                    buf = JS_sprintf_append(buf, TAB "this.%s = %s%s%s\n",
                                            name,
                                            isString ? "\"" : "",
                                            value,
                                            isString ? "\"" : "");
                    if(!buf) goto out;
                }
            }
        }
    }

out:
    if(callProps.array)
        JS_PutPropertyDescArray(cx, &callProps);
    if(thisProps.array)
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

    while(nsnull != (fp = JS_FrameIterator(cx, &iter)))
    {
        buf = FormatJSFrame(cx, fp, buf, num, showArgs, showLocals, showThisProps);
        num++;
    }

    if(!num)
        buf = JS_sprintf_append(buf, "JavaScript stack is empty\n");

    return buf;
}

JSBool
xpc_DumpJSStack(JSContext* cx, JSBool showArgs, JSBool showLocals, JSBool showThisProps)
{
    char* buf;

    buf = FormatJSStackDump(cx, nsnull, showArgs, showLocals, showThisProps);
    if(buf)
    {
        printf(buf);
        JS_smprintf_free(buf);
    }
    else
        printf("Failed to format JavaScript stack for dump\n");
    return JS_TRUE;
}

/***************************************************************************/

JS_STATIC_DLL_CALLBACK(void)
xpcDumpEvalErrorReporter(JSContext *cx, const char *message,
                         JSErrorReport *report)
{
    printf("Error: %s\n", message);
}

JSBool
xpc_DumpEvalInJSStackFrame(JSContext* cx, JSUint32 frameno, const char* text)
{
    JSStackFrame* fp;
    JSStackFrame* iter = nsnull;
    JSUint32 num = 0;

    if(!cx || !text)
    {
        printf("invalid params passed to xpc_DumpEvalInJSStackFrame!\n");
        return JS_FALSE;
    }

    printf("js[%d]> %s\n", frameno, text);

    while(nsnull != (fp = JS_FrameIterator(cx, &iter)))
    {
        if(num == frameno)
            break;
        num++;
    }

    if(!fp)
    {
        printf("invalid frame number!\n");
        return JS_FALSE;
    }

    JSExceptionState* exceptionState = JS_SaveExceptionState(cx);
    JSErrorReporter older = JS_SetErrorReporter(cx, xpcDumpEvalErrorReporter);

    jsval rval;
    JSString* str;
    const char* chars;
    if(JS_EvaluateInStackFrame(cx, fp, text, strlen(text), "eval", 1, &rval) &&
       nsnull != (str = JS_ValueToString(cx, rval)) &&
       nsnull != (chars = JS_GetStringBytes(str)))
    {
        printf("%s\n", chars);
    }
    else
        printf("eval failed!\n");
    JS_SetErrorReporter(cx, older);
    JS_RestoreExceptionState(cx, exceptionState);
    return JS_TRUE;
}

/***************************************************************************/

JSTrapStatus JS_DLL_CALLBACK
xpc_DebuggerKeywordHandler(JSContext *cx, JSScript *script, jsbytecode *pc,
                           jsval *rval, void *closure)
{
    static const char line[] =
    "------------------------------------------------------------------------\n";
    printf(line);
    printf("Hit JavaScript \"debugger\" keyword. JS call stack...\n");
    xpc_DumpJSStack(cx, JS_TRUE, JS_TRUE, JS_FALSE);
    printf(line);
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
        if(member_count == max_count)
            return overflow;
        for(int i = 0; i < member_count; i++)
            if(array[i] == obj)
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
    if(OBJ_IS_NATIVE(obj))
        printf("%#p 'native' <%s>",
               obj,
               ((JSClass*)(obj->slots[JSSLOT_CLASS]-1))->name);
    else
        printf("%#p 'host'", obj);

}

static void PrintObject(JSObject* obj, int depth, ObjectPile* pile)
{
    PrintObjectBasics(obj);

    switch(pile->Visit(obj))
    {
    case ObjectPile::primary:
        printf("\n");
        break;
    case ObjectPile::seen:
        printf(" (SEE ABOVE)\n");
        return;
    case ObjectPile::overflow:
        printf(" (TOO MANY OBJECTS)\n");
        return;
    }

    if(!OBJ_IS_NATIVE(obj))
        return;

    JSObject* parent = (JSObject*)(obj->slots[JSSLOT_PARENT]);
    JSObject* proto  = (JSObject*)(obj->slots[JSSLOT_PROTO]);

    printf("%*sparent: ", INDENT(depth+1));
    if(parent)
        PrintObject(parent, depth+1, pile);
    else
        printf("null\n");
    printf("%*sproto: ", INDENT(depth+1));
    if(proto)
        PrintObject(proto, depth+1, pile);
    else
        printf("null\n");
}

JSBool
xpc_DumpJSObject(JSObject* obj)
{
    ObjectPile pile;

    printf("Debugging reminders...\n");
    printf("  class:  (JSClass*)(obj->slots[2]-1)\n");
    printf("  parent: (JSObject*)(obj->slots[1])\n");
    printf("  proto:  (JSObject*)(obj->slots[0])\n");
    printf("\n");

    if(obj)
        PrintObject(obj, 0, &pile);
    else
        printf("xpc_DumpJSObject passed null!\n");

    return JS_TRUE;
}
#endif
