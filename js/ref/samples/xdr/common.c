#include <jsapi.h>
#include <stdio.h>

/*
 * Stolen from js.c, including questionable implicit-newline behaviour.
 */
static JSBool
Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i, n;
    JSString *str;

    for (i = n = 0; i < argc; i++) {
	str = JS_ValueToString(cx, argv[i]);
	if (!str)
	    return JS_FALSE;
	printf("%s%s", i ? " " : "", JS_GetStringBytes(str));
	n++;
    }
    if (n)
	putchar('\n');
    return JS_TRUE;
}

static JSClass global_class = {
    "global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

static JSFunctionSpec global_functions[] = {
  {"print",		Print,		0},
  {0}
};

/*
 * Also stolen from js.c.
 */
static void
my_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    int i, j, k, n;

    fputs("js: ", stderr);
    if (!report) {
	fprintf(stderr, "%s\n", message);
	return;
    }

    if (report->filename)
	fprintf(stderr, "%s, ", report->filename);
    if (report->lineno)
	fprintf(stderr, "line %u: ", report->lineno);
    fputs(message, stderr);
    if (!report->linebuf) {
	putc('\n', stderr);
	return;
    }

    fprintf(stderr, ":\n%s\n", report->linebuf);
    n = report->tokenptr - report->linebuf;
    for (i = j = 0; i < n; i++) {
	if (report->linebuf[i] == '\t') {
	    for (k = (j + 8) & ~7; j < k; j++)
		putc('.', stderr);
	    continue;
	}
	putc('.', stderr);
	j++;
    }
    fputs("^\n", stderr);
}

/*
 * Initialize the JS engine:
 * - JSRuntime: per-process state, including GC.
 * - JSContext: script execution and engine interaction context, 
 *              possibly one of many.
 * - JSObject:  global object with ECMA-and-other standard classes.
 */
JSBool
Init(JSRuntime **rtp, JSContext **cxp, JSObject **globalp) {
  *rtp = JS_NewRuntime(8L * 1024L * 1024L);
  if (!*rtp) {
    fprintf(stderr, "failed to create JSRuntime\n");
    return JS_FALSE;
  }

  *cxp = JS_NewContext(*rtp, 8192);
  if (!*cxp) {
    fprintf(stderr, "failed to create JSContext\n");
    return JS_FALSE;
  }
  
  *globalp = JS_NewObject(*cxp, &global_class, NULL, NULL);
  if (!*globalp) {
    fprintf(stderr, "failed to create global object\n");
    return JS_FALSE;
  }

  if (!JS_InitStandardClasses(*cxp, *globalp)) {
    fprintf(stderr, "failed to initialize standard classes\n");
    return JS_FALSE;
  }

  if (!JS_DefineFunctions(*cxp, *globalp, global_functions)) {
    fprintf(stderr, "failed to define global functions\n");
    return JS_FALSE;
  }

  JS_SetErrorReporter(*cxp, my_ErrorReporter);
  return JS_TRUE;
}

