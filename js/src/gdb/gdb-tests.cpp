/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gdb-tests.h"
#include "jsfriendapi.h"

using namespace JS;

/* The class of the global object. */
JSClass global_class = {
    "global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_DeletePropertyStub, JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

template<typename T>
inline T *
checkPtr(T *ptr)
{
  if (! ptr)
    abort();
  return ptr;
}

void
checkBool(JSBool success)
{
  if (! success)
    abort();
}

/* The error reporter callback. */
void reportError(JSContext *cx, const char *message, JSErrorReport *report)
{
    fprintf(stderr, "%s:%u: %s\n",
            report->filename ? report->filename : "<no filename>",
            (unsigned int) report->lineno,
            message);
}

// prolog.py sets a breakpoint on this function; test functions can call it
// to easily return control to GDB where desired.
void breakpoint() {
    // If we leave this function empty, the linker will unify it with other
    // empty functions throughout SpiderMonkey. If we then set a GDB
    // breakpoint on it, that breakpoint will hit at all sorts of random
    // times. So make it perform a distinctive side effect.
    fprintf(stderr, "Called " __FILE__ ":breakpoint\n");
}

GDBFragment *GDBFragment::allFragments = NULL;

int
main (int argc, const char **argv)
{
    JSRuntime *runtime = checkPtr(JS_NewRuntime(1024 * 1024, JS_USE_HELPER_THREADS));
    JS_SetGCParameter(runtime, JSGC_MAX_BYTES, 0xffffffff);
    JS_SetNativeStackQuota(runtime, 5000000);

    JSContext *cx = checkPtr(JS_NewContext(runtime, 8192));
    JS_SetErrorReporter(cx, reportError);

    JSAutoRequest ar(cx);

    /* Create the global object. */
    JS::CompartmentOptions options;
    options.setVersion(JSVERSION_LATEST);
    RootedObject global(cx, checkPtr(JS_NewGlobalObject(cx, &global_class, NULL, options)));
    js::SetDefaultObjectForContext(cx, global);

    JSAutoCompartment ac(cx, global);

    /* Populate the global object with the standard globals,
       like Object and Array. */
    checkBool(JS_InitStandardClasses(cx, global));

    argv++;
    while (*argv) {
        const char *name = *argv++;
        GDBFragment *fragment;
        for (fragment = GDBFragment::allFragments; fragment; fragment = fragment->next) {
            if (strcmp(fragment->name(), name) == 0) {
                fragment->run(cx, argv);
                break;
            }
        }
        if (!fragment) {
            fprintf(stderr, "Unrecognized fragment name: %s\n", name);
            exit(1);
        }
    }

    return 0;
}
