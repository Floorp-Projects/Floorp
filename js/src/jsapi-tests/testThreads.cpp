/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#ifdef JS_THREADSAFE

#include "tests.h"
#include "prthread.h"

struct ThreadData {
    JSRuntime *rt;
    JSObject *obj;
    const char *code;
    bool ok;
};

BEGIN_TEST(testThreads_bug561444)
    {
        const char *code = "<a><b/></a>.b.@c = '';";
        EXEC(code);

        jsrefcount rc = JS_SuspendRequest(cx);
        {
            ThreadData data = {rt, global, code, false};
            PRThread *thread = 
                PR_CreateThread(PR_USER_THREAD, threadMain, &data,
                                PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
            CHECK(thread);
            PR_JoinThread(thread);
            CHECK(data.ok);
        }
        JS_ResumeRequest(cx, rc);
        return true;
    }

    static void threadMain(void *arg) {
        ThreadData *d = (ThreadData *) arg;

        JSContext *cx = JS_NewContext(d->rt, 8192);
        if (!cx)
            return;
        JS_BeginRequest(cx);
        {
            JSAutoEnterCompartment ac;
            jsval v;
            d->ok = ac.enter(cx, d->obj) &&
                    JS_EvaluateScript(cx, d->obj, d->code, strlen(d->code), __FILE__, __LINE__,
                                      &v);
        }
        JS_DestroyContext(cx);
    }
END_TEST(testThreads_bug561444)

const PRUint32 NATIVE_STACK_SIZE = 64 * 1024;
const PRUint32 NATIVE_STACK_HEADROOM = 8 * 1024;

template <class T>
class Repeat {
    size_t n;
    const T &t;

  public:
    Repeat(size_t n, const T &t) : n(n), t(t) {}

    bool operator()() const {
	for (size_t i = 0; i < n; i++)
	    if (!t())
		return false;
	return true;
    }
};

template <class T> Repeat<T> repeat(size_t n, const T &t) { return Repeat<T>(n, t); }

/* Class of callable that does something in n parallel threads. */
template <class T>
class Parallel {
    size_t n;
    const T &t;

    struct pair { const Parallel *self; bool ok; };

    static void threadMain(void *arg) {
	pair *p = (pair *) arg;
	if (!p->self->t())
	    p->ok = false;
    }

  public:
    Parallel(size_t n, const T &t) : n(n), t(t) {}

    bool operator()() const {
	pair p = {this, true};

        PRThread **thread = new PRThread *[n];
	if (!thread)
	    return false;

        size_t i;
        for (i = 0; i < n; i++) {
            thread[i] = PR_CreateThread(PR_USER_THREAD, threadMain, &p, PR_PRIORITY_NORMAL,
                                        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, NATIVE_STACK_SIZE);
            if (thread[i] == NULL) {
                p.ok = false;
                break;
            }
        }
        while (i--)
            PR_JoinThread(thread[i]);

	delete[] thread;
        return p.ok;
    }
};

template <class T> Parallel<T> parallel(size_t n, const T &t) { return Parallel<T>(n, t); }

/* Class of callable that creates a compartment and runs some code in it. */
class eval {
    JSRuntime *rt;
    const char *code;

  public:
    eval(JSRuntime *rt, const char *code) : rt(rt), code(code) {}

    bool operator()() const {
        JSContext *cx = JS_NewContext(rt, 8192);
	if (!cx)
	    return false;

        JS_SetNativeStackQuota(cx, NATIVE_STACK_SIZE - NATIVE_STACK_HEADROOM);
        bool ok = false;
	{
	    JSAutoRequest ar(cx);
	    JSObject *global =
		JS_NewCompartmentAndGlobalObject(cx, JSAPITest::basicGlobalClass(), NULL);
	    if (global) {
		JS_SetGlobalObject(cx, global);
		jsval rval;
		ok = JS_InitStandardClasses(cx, global) &&
		    JS_EvaluateScript(cx, global, code, strlen(code), "", 0, &rval);
	    }
	}
	JS_DestroyContextMaybeGC(cx);
        return ok;
    }
};

BEGIN_TEST(testThreads_bug604782)
{
    jsrefcount rc = JS_SuspendRequest(cx);
    bool ok = repeat(20, parallel(3, eval(rt, "for(i=0;i<1000;i++);")))();
    JS_ResumeRequest(cx, rc);
    CHECK(ok);
    return true;
}
END_TEST(testThreads_bug604782)

BEGIN_TEST(testThreads_bug609103)
{
    const char *code = 
        "var x = {};\n"
        "for (var i = 0; i < 10000; i++)\n"
        "    x = {next: x};\n";

    jsrefcount rc = JS_SuspendRequest(cx);
    bool ok = parallel(2, eval(rt, code))();
    JS_ResumeRequest(cx, rc);
    CHECK(ok);
    return true;
}
END_TEST(testThreads_bug609103)

#endif
