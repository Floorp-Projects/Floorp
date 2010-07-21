/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
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
 * The Original Code is JSAPI tests.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Jason Orendorff
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

#include "jsapi.h"
#include "jsprvtd.h"
#include "jsvector.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

class jsvalRoot
{
public:
    explicit jsvalRoot(JSContext *context, jsval value = JSVAL_NULL)
        : cx(context), v(value)
    {
        if (!JS_AddValueRoot(cx, &v)) {
            fprintf(stderr, "Out of memory in jsvalRoot constructor, aborting\n");
            abort();
        }
    }

    ~jsvalRoot() { JS_RemoveValueRoot(cx, &v); }

    operator jsval() const { return value(); }

    jsvalRoot & operator=(jsval value) {
        v = value;
        return *this;
    }

    jsval * addr() { return &v; }
    jsval value() const { return v; }

private:
    JSContext *cx;
    jsval v;
};

/* Note: Aborts on OOM. */
class JSAPITestString {
    js::Vector<char, 0, js::SystemAllocPolicy> chars;
public:
    JSAPITestString() {}
    JSAPITestString(const char *s) { *this += s; }
    JSAPITestString(const JSAPITestString &s) { *this += s; }

    const char *begin() const { return chars.begin(); }
    const char *end() const { return chars.end(); }
    size_t length() const { return chars.length(); }

    JSAPITestString & operator +=(const char *s) {
        if (!chars.append(s, strlen(s)))
            abort();
        return *this;
    }

    JSAPITestString & operator +=(const JSAPITestString &s) {
        if (!chars.append(s.begin(), s.length()))
            abort();
        return *this;
    }
};

inline JSAPITestString operator+(JSAPITestString a, const char *b) { return a += b; }
inline JSAPITestString operator+(JSAPITestString a, const JSAPITestString &b) { return a += b; }

class JSAPITest
{
public:
    static JSAPITest *list;
    JSAPITest *next;

    JSRuntime *rt;
    JSContext *cx;
    JSObject *global;
    bool knownFail;
    JSAPITestString msgs;

    JSAPITest() : rt(NULL), cx(NULL), global(NULL), knownFail(false) {
        next = list;
        list = this;
    }

    virtual ~JSAPITest() { uninit(); }

    virtual bool init() {
        rt = createRuntime();
        if (!rt)
            return false;
        cx = createContext();
        if (!cx)
            return false;
        JS_BeginRequest(cx);
        global = createGlobal();
        return global != NULL;
    }

    virtual void uninit() {
        if (cx) {
            JS_EndRequest(cx);
            JS_DestroyContext(cx);
            cx = NULL;
        }
        if (rt) {
            destroyRuntime();
            rt = NULL;
        }
    }

    virtual const char * name() = 0;
    virtual bool run() = 0;

#define EXEC(s) do { if (!exec(s, __FILE__, __LINE__)) return false; } while (false)

    bool exec(const char *bytes, const char *filename, int lineno) {
        jsvalRoot v(cx);
        return JS_EvaluateScript(cx, global, bytes, strlen(bytes), filename, lineno, v.addr()) ||
               fail(bytes, filename, lineno);
    }

#define EVAL(s, vp) do { if (!evaluate(s, __FILE__, __LINE__, vp)) return false; } while (false)

    bool evaluate(const char *bytes, const char *filename, int lineno, jsval *vp) {
        return JS_EvaluateScript(cx, global, bytes, strlen(bytes), filename, lineno, vp) ||
               fail(bytes, filename, lineno);
    }

    JSAPITestString toSource(jsval v) {
        JSString *str = JS_ValueToSource(cx, v);
        if (str)
            return JSAPITestString(JS_GetStringBytes(str));
        JS_ClearPendingException(cx);
        return JSAPITestString("<<error converting value to string>>");
    }

#define CHECK_SAME(actual, expected) \
    do { \
        if (!checkSame(actual, expected, #actual, #expected, __FILE__, __LINE__)) \
            return false; \
    } while (false)

    bool checkSame(jsval actual, jsval expected,
                   const char *actualExpr, const char *expectedExpr,
                   const char *filename, int lineno) {
        return JS_SameValue(cx, actual, expected) ||
               fail(JSAPITestString("CHECK_SAME failed: expected JS_SameValue(cx, ") +
                    actualExpr + ", " + expectedExpr + "), got !JS_SameValue(cx, " +
                    toSource(actual) + ", " + toSource(expected) + ")", filename, lineno);
    }

#define CHECK(expr) \
    do { \
        if (!(expr)) \
            return fail("CHECK failed: " #expr, __FILE__, __LINE__); \
    } while (false)

    bool fail(JSAPITestString msg = JSAPITestString(), const char *filename = "-", int lineno = 0) {
        if (JS_IsExceptionPending(cx)) {
            jsvalRoot v(cx);
            JS_GetPendingException(cx, v.addr());
            JS_ClearPendingException(cx);
            JSString *s = JS_ValueToString(cx, v);
            if (s)
                msg += JS_GetStringBytes(s);
        }
        fprintf(stderr, "%s:%d:%.*s\n", filename, lineno, (int) msg.length(), msg.begin());
        msgs += msg;
        return false;
    }

    JSAPITestString messages() const { return msgs; }

protected:
    static JSBool
    print(JSContext *cx, uintN argc, jsval *vp)
    {
        jsval *argv = JS_ARGV(cx, vp);
        for (uintN i = 0; i < argc; i++) {
            JSString *str = JS_ValueToString(cx, argv[i]);
            if (!str)
                return JS_FALSE;
            char *bytes = JS_EncodeString(cx, str);
            if (!bytes)
                return JS_FALSE;
            printf("%s%s", i ? " " : "", bytes);
            JS_free(cx, bytes);
        }

        putchar('\n');
        fflush(stdout);
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return JS_TRUE;
    }

    bool definePrint() {
        return JS_DefineFunction(cx, global, "print", (JSNative) print, 0, 0);
    }

    virtual JSRuntime * createRuntime() {
        return JS_NewRuntime(8L * 1024 * 1024);
    }

    virtual void destroyRuntime() {
        JS_ASSERT(!cx);
        JS_ASSERT(rt);
        JS_DestroyRuntime(rt);
    }

    static void reportError(JSContext *cx, const char *message, JSErrorReport *report) {
        fprintf(stderr, "%s:%u:%s\n",
                report->filename ? report->filename : "<no filename>",
                (unsigned int) report->lineno,
                message);
    }

    virtual JSContext * createContext() {
        JSContext *cx = JS_NewContext(rt, 8192);
        if (!cx)
            return NULL;
        JS_SetOptions(cx, JSOPTION_VAROBJFIX | JSOPTION_JIT);
        JS_SetVersion(cx, JSVERSION_LATEST);
        JS_SetErrorReporter(cx, &reportError);
        return cx;
    }

    virtual JSClass * getGlobalClass() {
        static JSClass basicGlobalClass = {
            "global", JSCLASS_GLOBAL_FLAGS,
            JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
            JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
            JSCLASS_NO_OPTIONAL_MEMBERS
        };
        return &basicGlobalClass;
    }

    virtual JSObject * createGlobal() {
        /* Create the global object. */
        JSObject *global = JS_NewGlobalObject(cx, getGlobalClass());
        if (!global)
            return NULL;

        /* Populate the global object with the standard globals,
           like Object and Array. */
        if (!JS_InitStandardClasses(cx, global))
            return NULL;
        return global;
    }
};

#define BEGIN_TEST(testname)                                            \
    class cls_##testname : public JSAPITest {                           \
    public:                                                             \
        virtual const char * name() { return #testname; }               \
        virtual bool run()

#define END_TEST(testname)                                              \
    };                                                                  \
    static cls_##testname cls_##testname##_instance;


