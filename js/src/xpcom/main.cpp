extern "C" {
#include <stdio.h>
#include "jsapi.h"
#include "../prtypes.h"
#include "../prassert.h"
#include "../prlink.h"
extern PR_IMPORT_DATA(JSObjectOps) js_ObjectOps;
}
#include "jsIScriptable.h"

typedef nsISupports* (*FactoryFun)(JSContext*);

static JSClass global_class = {
    "global", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

struct FactoryStruct {
	PRLibrary* lib;
	FactoryFun create;
	FactoryStruct(PRLibrary* _lib, FactoryFun _f) : lib(_lib), create(_f) {}
};

// this is the constructor for jsIScriptable objects
static JSBool
ExtensionsConstructor(JSContext *cx,JSObject *obj,uintN argc,jsval *argv,jsval *vp)
{
	obj = JSVAL_TO_OBJECT(argv[-2]); // obj is now the factory
	FactoryStruct* factory = (FactoryStruct*)JS_GetPrivate(cx,obj);
	jsIScriptable* jsi = (jsIScriptable*)factory->create(cx);
	return (jsIScriptable::ToJS(cx,jsi,vp) == NS_OK ? JS_TRUE : JS_FALSE);
}

static JSBool
Extensions_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static void
Extensions_Finalize(JSContext *cx, JSObject *obj)
{
	FactoryStruct* factory;

	factory = (FactoryStruct*)JS_GetPrivate(cx,obj);
	if (factory) {
		PR_UnloadLibrary(factory->lib);
		delete factory;
	}
}

static JSBool
Extensions_lookupProperty(JSContext *cx, JSObject *obj, jsid id,
                         JSObject **objp, JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
                            , const char *file, uintN line
#endif
			    )
{
    return JS_TRUE;
}

static JSBool
Extensions_defineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                         JSPropertyOp getter, JSPropertyOp setter,
                         uintN attrs, JSProperty **propp)
{
    return JS_FALSE;
}

static JSBool
Extensions_getAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    return JS_FALSE;
}

static JSBool
Extensions_setAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    return JS_TRUE;
}

static JSBool
Extensions_deleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return JS_FALSE;
}

static JSBool
Extensions_defaultValue(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Extensions_getProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Extensions_setProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	return JS_TRUE;
}

static JSBool
Extensions_newEnumerate(JSContext *cx,JSObject *obj,JSIterateOp enum_op,jsval *statep,jsid *idp)
{
	return JS_TRUE;
}

static JSBool
Extensions_checkAccess(JSContext *cx, JSObject *obj, jsid id,
		       JSAccessMode mode, jsval *vp, uintN *attrsp)
{
	return JS_TRUE;
}

JSBool ExtensionsConstructor(JSContext*,JSObject*,uintN,jsval*,jsval*);

JSObjectOps Extensions_ops = {
    /* Mandatory non-null function pointer members. */
    NULL,                       /* newObjectMap */
    NULL,                       /* destroyObjectMap */
    Extensions_lookupProperty,
    Extensions_defineProperty,
    Extensions_getProperty,  
    Extensions_setProperty,
    Extensions_getAttributes,
    Extensions_setAttributes,
    Extensions_deleteProperty,
    Extensions_defaultValue,
    Extensions_newEnumerate,
    Extensions_checkAccess,
    /* Optionally non-null members start here. */
    NULL,                       /* thisObject */
    NULL,                       /* dropProperty */
    ExtensionsConstructor,      /* call */
    ExtensionsConstructor,      /* construct */
    NULL,                       /* xdrObject */
    NULL,                       /* hasInstance */
};

JSObjectOps *
Extensions_getObjectOps(JSContext *cx, JSClass *clazz)
{
    return &Extensions_ops;
}

static JSClass Extensions_class = {
    "Extensions", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   Extensions_Convert,   Extensions_Finalize,
    Extensions_getObjectOps,
};

// resolving Extensions.M by assigning it to an object which each time it
// is called, constructs a new instance from M's factory.
static JSBool
Factory_Resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	       JSObject **objp)
{
	char *str;
	JSObject *fact_obj;

	if (!JSVAL_IS_STRING(id))
		return JS_TRUE;
	str = JS_GetStringBytes(JSVAL_TO_STRING(id));
	// load factory.
	PRLibrary* factory = PR_LoadLibrary(str);
	if (factory == NULL)
		return JS_FALSE;
	FactoryFun fun = (FactoryFun)PR_FindSymbol(factory, "CreateInstance");
	if (fun == NULL)
		return JS_FALSE;
	fact_obj = JS_DefineObject(cx, obj, str, &Extensions_class, NULL, 0);
	JS_SetPrivate(cx,fact_obj,new FactoryStruct(factory,fun));
	*objp = obj;
	return JS_TRUE;
}

static JSClass Factory_class = {
    "ExtensionsFactory",
    JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,JS_PropertyStub,JS_PropertyStub,JS_PropertyStub,
    JS_EnumerateStub,(JSResolveOp)Factory_Resolve,JS_ConvertStub,JS_FinalizeStub
};

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

static void
Process(JSContext *cx, JSObject *obj, const char *filename)
{
	JSScript *jsrc = JS_CompileFile(cx,obj,filename);
	jsval rval;

	if (jsrc == NULL) {
#ifdef DEBUG
		printf("Failed compilation of %s\n",filename);
#endif
		return;
	}
	if (!JS_ExecuteScript(cx,obj,jsrc,&rval)) {
#ifdef DEBUG
		printf("Failed execution of %s\n",filename);
#endif
		return;
	}
}

static JSFunctionSpec helper_functions[] = {
    /*    name          native          nargs    */
    {"print",           Print,          0},
    {0}
};

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

int main(int argc, char* const* argv)
{
	JSRuntime *rt;
	JSContext *cx;
	JSObject *glob, *ext;
	const char *filename;

	if (argc >= 2)
		filename = (const char *) argv[1];
	else
		filename = (const char *) "test.js";
	// JS init
	rt = JS_Init(8L * 1024L * 1024L);
	if (!rt)
		return 0;
	cx = JS_NewContext(rt, 8192);
	PR_ASSERT(cx);
	JS_SetErrorReporter(cx, my_ErrorReporter);
	glob = JS_NewObject(cx, &global_class, NULL, NULL);
	PR_ASSERT(glob);
	if (!JS_InitStandardClasses(cx, glob))
		return -1;
	if (!JS_DefineFunctions(cx, glob, helper_functions))
		return -1;
	ext = JS_DefineObject(cx, glob, "Extensions", &Factory_class, NULL, 0);
	if (!ext)
		return -1;
	Extensions_ops.newObjectMap = js_ObjectOps.newObjectMap;
	Extensions_ops.destroyObjectMap = js_ObjectOps.destroyObjectMap;
	Process(cx, glob, filename);
	JS_DestroyContext(cx);
	JS_DestroyRuntime(rt);
	JS_ShutDown();
	return 1;
}
