/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
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
 *   Brendan Eich <brendan@mozilla.org
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

#define jstracer_cpp___

#include "jsinterp.cpp"

JSObject*
js_GetRecorder(JSContext* cx)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    if (tm->recorder)
        return tm->recorder;
    JSScript* script = JS_CompileFile(cx, JS_GetGlobalObject(cx), "recorder.js");
    JS_ASSERT(script != NULL);
    jsval result;

#ifdef DEBUG
    JSBool ok =
#endif
    JS_ExecuteScript(cx, JS_GetGlobalObject(cx), script, &result);
    JS_ASSERT(ok && !JSVAL_IS_PRIMITIVE(result));
    return tm->recorder = JSVAL_TO_OBJECT(result);
}

void
js_CallRecorder(JSContext* cx, const char* name, uintN argc, jsval* argv)
{
    jsval rval;
    JSBool ok =
        JS_CallFunctionName(cx, js_GetRecorder(cx), name, argc, argv, &rval);
    if (!ok) {
#ifdef DEBUG
        printf("recorder: unsupported instruction '%s'\n", name);
#endif        
        JS_ClearPendingException(cx);
        js_TriggerRecorderError(cx);
    }        
    JS_ASSERT(ok);
}

void
js_CallRecorder(JSContext* cx, const char* name, jsval a)
{
    jsval args[] = { a };
    js_CallRecorder(cx, name, 1, args);
}

void
js_CallRecorder(JSContext* cx, const char* name, jsval a, jsval b)
{
    jsval args[] = { a, b };
    js_CallRecorder(cx, name, 2, args);
}

void
js_CallRecorder(JSContext* cx, const char* name, jsval a, jsval b, jsval c)
{
    jsval args[] = { a, b, c };
    js_CallRecorder(cx, name, 3, args);
}

void
js_CallRecorder(JSContext* cx, const char* name, jsval a, jsval b, jsval c, jsval d)
{
    jsval args[] = { a, b, c, d };
    js_CallRecorder(cx, name, 4, args);
}

void
js_TriggerRecorderError(JSContext* cx)
{
    jsval error = JSVAL_TRUE;
    JS_SetProperty(cx, js_GetRecorder(cx), "error", &error); 
}

bool 
js_GetRecorderError(JSContext* cx)
{
    jsval error;
    bool ok = JS_GetProperty(cx, js_GetRecorder(cx), "error", &error);
    return ok && (error != JSVAL_FALSE);
}
