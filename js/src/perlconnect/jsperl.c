/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * PerlConnect module.
 */

/*
    The first two headers are from the Perl distribution.
    Play with "perl -MExtUtils::Embed -e ccopts -e ldopts"
    to find out which directories should be included. Refer
    to perlembed man page for more info.
*/
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "../jsapi.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* PerlConnect. Provides means for OO JS <==> Perl communications            */
/* See README.html for more info on PerlConnect. Look for TODO in this file  */
/* for things that are bogus or not completely implemented. Has been tested  */
/* with 5.004 only                                                           */
/*---------------------------------------------------------------------------*/

/* Forward declarations */
static JSBool PerlConstruct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *v);
static JSBool PerlFinilize(JSContext *cx, JSObject *obj);
static JSBool perl_eval(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval);
static JSBool perl_call(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval);
static JSBool perl_use(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval);
static JSBool use(JSContext *cx, JSObject *obj, int argc, jsval *argv, const char* t);
static JSBool PMGetProperty(JSContext *cx, JSObject *obj, jsval name, jsval* rval);
static JSBool PMSetProperty(JSContext *cx, JSObject *obj, jsval name, jsval *rval);
static JSBool PerlToString(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval);
static JSBool processReturn(JSContext *cx, JSObject *obj, jsval* rval);
static JSBool checkError(JSContext *cx);
static JSBool PMToString(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval);
static JSBool PVToString(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval);
static SV*    PVGetRef(JSContext *cx, JSObject *obj);
static JSBool PVGetProperty(JSContext *cx, JSObject *obj, jsval name, jsval *rval);
static JSBool PVSetProperty(JSContext *cx, JSObject *obj, jsval name, jsval *rval);
static JSBool PVGetElement(JSContext *cx, JSObject *obj, jsint index, jsval *rval);
static JSBool PVSetElement(JSContext *cx, JSObject *obj, jsint index, jsval v);
static JSBool PVGetKey(JSContext *cx, JSObject *obj, char* name, jsval *rval);
static JSBool PVSetKey(JSContext *cx, JSObject *obj, char* name, jsval v);
static JSBool PVConvert(JSContext *cx, JSObject *obj, JSType type, jsval *v);
static JSBool PVFinalize(JSContext *cx, JSObject *obj);
/* Exported functions */
JS_EXPORT_API(JSObject*)	JS_InitPerlClass(JSContext *cx, JSObject *obj);
JS_EXPORT_API(JSBool)		JSVALToSV(JSContext *cx, JSObject *obj, jsval v, SV** sv);
JS_EXPORT_API(JSBool)		SVToJSVAL(JSContext *cx, JSObject *obj, SV *ref, jsval *rval);

/*
    The following is required by the Perl dynamic loading mechanism to
    link with modules that use C properly. See perlembed man page for details.
    This allows things like sockets to be called via PerlConnect.
*/
#ifdef __cplusplus
#  define EXTERN_C extern "C"
#else
#  define EXTERN_C extern
#endif

EXTERN_C void boot_DynaLoader _((CV* cv));
EXTERN_C void
xs_init()
{
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, __FILE__);
}

/* These properties are not processed by the getter for PerlValue */
static char* predefined_methods[] = {"toString", "valueOf", "type", "length"};

/* Represents a perl interpreter */
JSClass perlClass = {
    "Perl", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, PMGetProperty, /*PMSetProperty*/JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,  PerlFinilize
};

JSFunctionSpec perlMethods[] = {
    {"toString",  PerlToString, 0},
    {"eval",  perl_eval, 0},
    {"call",  perl_call, 0},
    {"use",   perl_use,  0},
    { NULL, NULL,0 }
};


/* Represents a Perl module */
JSClass perlModuleClass = {
    "PerlModule", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, PMGetProperty, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

JSFunctionSpec perlModuleMethods[] = {
    {"toString",  PMToString, 0},
    { NULL, NULL,0 }
};


/* Represents a value returned from Perl */
JSClass perlValueClass = {
    "PerlValue", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, PVGetProperty, PVSetProperty,
    JS_EnumerateStub, JS_ResolveStub, PVConvert, PVFinalize
};

JSFunctionSpec perlValueMethods[] = {
    {"toString",  PVToString, 0},
    { NULL, NULL,             0}
};

/*
    Initializes Perl class. Should be called by applications that
    want to enable PerlConnect. This will probably preload the Perl
    DLL even though Perl might not actually be used. We may postpone
    this and load the DLL at runtime after the constructor is called.
*/
static JSObject*
js_InitPerlClass(JSContext *cx, JSObject *obj)
{
    jsval v = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "main"));
    JSObject *module = JS_NewObject(cx, &perlModuleClass, NULL, obj);

    JS_DefineFunctions(cx, module, perlModuleMethods);
    JS_SetProperty(cx, module, "path", &v);

    return JS_InitClass(cx, obj, module, &perlClass, PerlConstruct, 0,
        NULL, NULL, NULL, NULL);
}

/* Public wrapper for the function above */
JSObject*
JS_InitPerlClass(JSContext *cx, JSObject *obj)
{
    return js_InitPerlClass(cx, obj);
}

/*
    Perl constructor. Allocates a new interpreter and defines methods on it.
    The constuctor is sort of bogus in that it doesn't create a new namespace
    and all the variables defined in one instance of the Perl object will be
    visible in others. In the future, I think it may be a good idea to use
    Safe.pm to provide independent contexts for different Perl objects and
    prohibit certain operations (like exit(), alarm(), die(), etc.). Or we
    may simple disallow calling the constuctor more than once.
*/
static JSBool
PerlConstruct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *v)
{
    PerlInterpreter *perl;
    JSObject *perlObject;
    JSBool ok;
    char *embedding[] = {"", "-e", "0"};
    char *t = "use PerlConnect qw(perl_eval perl_resolve perl_call $js $ver);";

    /* create a new interpreter */
    perl = perl_alloc();

    if(perl==NULL){
        fputs("Can't allocate a new interpreter", stderr);
        return 1;
    }

    perl_construct(perl);
    perl_parse(perl, xs_init, 3, embedding, NULL);
    perl_run(perl);

    ok = use(cx, obj, argc, argv, t);

    /* make it into an object */
    perlObject = JS_NewObject(cx, &perlClass, NULL, NULL);
    JS_DefineFunctions(cx, perlObject, perlMethods);
    JS_SetPrivate(cx, perlObject, perl);
    *v = OBJECT_TO_JSVAL(perlObject);
    return ok;
}

/* Destructor. Deallocates the interpreter */
static JSBool
PerlFinilize(JSContext *cx, JSObject *obj)
{
    PerlInterpreter *perl = JS_GetPrivate(cx, obj);

    perl_destruct(perl);
    perl_free(perl);
    return JS_TRUE;
}

/*
    Returns a string representation of the Perl interpreter.
    Can add printing of the Perl version, @ISA, etc., like the
    output produced by perl -V. Can also make certain variables
    available off the Perl object, like Perl.version, etc.
*/
static JSBool
PerlToString(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    SV* sv = perl_get_sv("JS::ver", FALSE);
    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, SvPV(sv, na)));
    return JS_TRUE;
}

/*
    Evaluates the first parameter in Perl and put the eval's
    return value into *rval. The return value is of type PerlValue.
    This procedure uses JS::perl_eval. Example of use of perl.eval():
        p = new Perl();
        str = p.eval("'-' x 80");   // str contains 80 dashes
*/
static JSBool
perl_eval(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval)
{
    char *statement;
    char *args[] = {NULL, NULL};    /* two elements */

    if(argc!=1){
        JS_ReportError(cx, "Perl.eval expects one parameter");
        return JS_FALSE;
    }
    statement = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

    args[0] = statement;
    perl_call_argv("JS::perl_eval", G_SCALAR|G_KEEPERR|G_EVAL, args);
    return processReturn(cx, obj, rval);
}

/*
    Call the perl procedure specified as the first argument and
    pass all the other arguments as parameters. The return value
    is returned in *rval. Example of use of perl.call():
        p = new Perl('Time::gmtime');
        time = p.call("Time::gmtime::gmtime");   // time is now the following array:
                                                 // [40,42,1,22,6,98,3,202,0]
    NB: The full function name has to be supplied, i.e. Time::gmtime::gmtime
    instead of gmtime unless gmtime is exported into the current package.

    This method is also used when one uses the full package name syntax like
    this:
        p = new Perl("Sys::Hostname", "JS")
        result = p.JS.c(1, 2, 4)
        p.hostname()

    This gets called from PMGetProperty, which creates a
    function whose native method is perl_call. Also see
    JS::perl_call.
*/
static JSBool
perl_call(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval)
{
    JSBool ok;
    int count, i;
    char* fun_name;
    SV *sv;
    dSP;

    /* Differetiate between direct and method-like call */
    if((JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION) &&
            strcmp("call", JS_GetFunctionName(JS_ValueToFunction(cx, argv[-2])))){
        fun_name = JS_GetFunctionName(JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[-2])));
        i=0;
    }else{
        fun_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
        i=1;
    }

    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSVpv(fun_name,0)));

    for(;i<argc;i++){
        JSVALToSV(cx, obj, argv[i], &sv);
        XPUSHs(sv);
    }
    PUTBACK;

    count = perl_call_sv(newSVpv("JS::perl_call", 0), G_KEEPERR|G_SCALAR|G_EVAL|G_DISCARD);
    if(count!=0){
        fprintf(stderr, "Implementation error: count=%d, must be 0!\n", count);
        return JS_FALSE;
    }
    ok = processReturn(cx, obj, rval);

    return ok;
}

/*
    Loads Perl libraries specified as arguments.
*/
static JSBool
perl_use(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval)
{
    return use(cx, obj, argc, argv, NULL);
}

/*
    Utility function used by perl_use and Perl's constructor.
    Executes use lib1; use lib2, etc. in the current interpreter.
*/
static JSBool
use(JSContext *cx, JSObject *obj, int argc, jsval *argv, const char* t){
    char *evalStr = JS_malloc(cx, t?strlen(t)+1:1);
    int i;

    strcpy(evalStr, t?t:"");

    for(i=0;i<argc;i++){
        char *arg = JS_GetStringBytes(JS_ValueToString(cx, argv[i])), *tmp, old[256];

        /* call use() on every parameter */
        strcpy(old, evalStr);
	    free(evalStr);
        tmp = JS_malloc(cx, strlen(old)+strlen(arg)+6);
        sprintf(tmp, "%suse %s;", old, arg);
        evalStr = tmp;
    }

    perl_eval_sv(newSVpv(evalStr, 0), G_KEEPERR);

    checkError(cx);
    JS_free(cx, evalStr);
    return JS_TRUE;
}

/*
    Looks at $@ to see if there was an error. Used by
    perl_eval, perl_call, etc.
*/
static JSBool
checkError(JSContext *cx)
{
    if(SvTRUE(GvSV(errgv))){
        JS_ReportError(cx, "perl eval failed: %s",
            SvPV(GvSV(errgv), na));
        /* clear error status. there should be a way to do this faster */
        perl_eval_sv(newSVpv("undef $@;", 0), G_KEEPERR);
        return JS_FALSE;
    }
    return JS_TRUE;
}

/*
    Take the value of $JS::js and convert in to a jsval. It's stotred
    in *rval. perl_eval and perl_call use $JS::js to store return results.
*/
static JSBool
processReturn(JSContext *cx, JSObject *obj, jsval* rval)
{
    SV  *js;

    js = perl_get_sv("JS::js", FALSE);

    if(!js || !SvOK(js)){
        *rval = JSVAL_VOID;
        return JS_FALSE;
    }else if(!SvROK(js)){
        JS_ReportError(cx, "$js (%s) must be of reference type", SvPV(js,na));
        return JS_FALSE;
    }

    checkError(cx);

    return SVToJSVAL(cx, obj, js, rval);
}

/*
    Implements namespace-like syntax that maps Perl packages to
    JS objects. One can say
        p = new Perl('Foo::Bar')
    and then call
        a = p.Foo.Bar.f()
    or access variables exported from those packages like this:
        a = p.Foo.Bar["$var"]
    this syntax will also work:
        a = p.Foo.Bar.$var
    but if you want to access non-scalar values, you must use the subscript syntax:
        p.Foo.Bar["@arr"]
    and
        p.Foo.Bar["%hash"]
*/
static JSBool
PMGetProperty(JSContext *cx, JSObject *obj, jsval name, jsval *rval)
{
    char *last = JS_GetStringBytes(JS_ValueToString(cx, name)), *path, package[256];
    char *args[] = {NULL, NULL};
    char *predefined_methods[] = {"toString", "eval", "call", "use", "path"};
    int count;
    SV  *js;
    jsval v;
    int i;

    for(i=0;i<sizeof(predefined_methods)/sizeof(char*);i++){
        if(!strcmp(predefined_methods[i], last)){
            return JS_TRUE;
        }
    }

    JS_GetProperty(cx, obj, "path", &v);
    path = JS_GetStringBytes(JS_ValueToString(cx, v));
    sprintf(package, "%s::%s", path, last);
    args[0] = package;

    count = perl_call_argv("JS::perl_resolve", G_KEEPERR|G_SCALAR|G_EVAL|G_DISCARD, args);
    if(count!=0){
        fprintf(stderr, "Implementation error: count=%d, must be 0!\n", count);
        return JS_FALSE;
    }

    checkError(cx);

    js = perl_get_sv("JS::js", FALSE);

    if(js && SvOK(js)){
        if(SvROK(js)){
            SVToJSVAL(cx, obj, js, rval);
        }else{
            /* defined function */
            if(SvIV(js) == 1){
                JSFunction *f = JS_NewFunction(cx, (JSNative)perl_call, 0, 0, NULL, package);
                *rval = OBJECT_TO_JSVAL(JS_GetFunctionObject(f));
            }else
            if(SvIV(js) == 2){
                JSObject *module;
                module = JS_NewObject(cx, &perlModuleClass, NULL, obj);
                v = (js && SvTRUE(js))?STRING_TO_JSVAL(JS_NewStringCopyZ(cx,package)):JSVAL_VOID;
                JS_SetProperty(cx, module, "path", &v);
                *rval = OBJECT_TO_JSVAL(module);
            }else{
                JS_ReportError(cx, "Symbol %s is not defined", package);
                *rval = JSVAL_VOID;
            }
        }
        return JS_TRUE;
    }else{
        puts("failure");
        return JS_FALSE;
    }
}

/*
    Gets called when a Perl value gets assigned to like this:
        p.Foo.Bar["$var"] = 100
*/
static JSBool
PMSetProperty(JSContext *cx, JSObject *obj, jsval name, jsval *rval)
{
    /* TODO: just call SVToJSVAL() and make the assignment. */
    return JS_TRUE;
}

/*
    toString() for PerlModule. Prints the path the module represents.
    Note that the path doesn't necessarily have to be valid. We don't
    have a way to check that until we call a function from that package.
    TODO: In 5.005 exists Foo::{Bar::} checks is Foo::{Bar::} exists.
    We can use this to validate package names.
*/
static JSBool
PMToString(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval){
    char str[256];
    JSString *s;
    jsval v;

    JS_GetProperty(cx, obj,  "path", &v);
    s = JSVAL_TO_STRING(v);
    sprintf(str, "[PerlModule %s]", JS_GetStringBytes(s));
    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, str));
    return JS_TRUE;
}

/*
    Helped method. Retrieves the Perl reference stored
    in PerlValue object as private data.
*/
static SV*
PVGetRef(JSContext *cx, JSObject *obj)
{
    SV* ref;

    ref = (SV*)JS_GetInstancePrivate(cx, obj, &perlValueClass, NULL);

    if(!ref || !SvOK(ref) || !SvROK(ref)){
        JS_ReportError(cx, "Can't extract ref");
        return NULL;
    }
    return ref;
}

static JSBool
PVCallStub (JSContext *cx, JSObject *obj, uintN argc, 
            jsval *argv, jsval *rval) {
    JSFunction *fun;
    int i, cnt;
    I32 ax;
    SV *sv, *perl_object;
    GV *gv;
    HV *stash;
    char *name;

    dSP;

    fun = JS_ValueToFunction(cx, argv[-2]);
    perl_object = PVGetRef(cx, obj);

    fun = JS_ValueToFunction(cx, argv[-2]);
    name = JS_GetFunctionName(fun);
    stash = SvSTASH(SvRV(perl_object));
    gv = gv_fetchmeth(stash, name, strlen(name), 0);
    /* cnt = perl_call_pv(method_name, 0); */
    /* start of perl call stuff */
    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    XPUSHs(perl_object); /* self for perl object method */
    for (i = 0; i < argc; i++) {
        sv = sv_newmortal();
        JSVALToSV(cx, obj, argv[i], &sv);
        XPUSHs(sv);
    }
    PUTBACK;

    cnt = perl_call_sv((SV*)GvCV(gv), 0);
    
    SPAGAIN;
    /* adjust stack for use of ST macro (see perlcall) */
    SP -= cnt;
    ax = (SP - PL_stack_base) + 1;

    /* read value(s) */
    if (cnt == 1) {
        SVToJSVAL(cx, obj, ST(0), rval);
    } else {
        warn("sorry, but array results are not supported yet...");
    }
    PUTBACK;

    FREETMPS;
    LEAVE;

    return(JS_TRUE);
}

/*
    Retrieve property from PerlValue object by its name. Tries
    to look at the PerlValue object both as a hash and array.
    If the index is numerical, then it looks at the array part
    first. *rval contains the result.
*/
/* __PH__
    ...but. PVGetproperty now firstly look for method in given 
    object package. If such method if found, then is returned 
    universal method stub. Sideeffect of this behavior is, that
    method are looked first before properties of the same name.

    Second problem is security. In this way any perl method could 
    be called. We pay security leak for this. May be we could 
    support some Perl exporting process (via some package global 
    array).
*/
static JSBool
PVGetProperty(JSContext *cx, JSObject *obj, jsval name, jsval *rval)
{
    char* str;

    /* __PH__ OK, array properties should be served first */
    if(JSVAL_IS_INT(name)){
        int32 ip;

        JS_ValueToInt32(cx, name, &ip);
        PVGetElement(cx, obj, ip, rval);
        if(*rval!=JSVAL_VOID){
            return JS_TRUE;
        }
    }

    str = JS_GetStringBytes(JS_ValueToString(cx, name));

    /* __PH__ may, be */
    if(!strcmp(str, "length")){
        SV* sv = SvRV(PVGetRef(cx, obj));

        if(SvTYPE(sv)==SVt_PVAV){
            *rval = INT_TO_JSVAL(av_len((AV*)sv)+1);
            return JS_TRUE;
        }else
        if(SvTYPE(sv)==SVt_PVHV){
            *rval = INT_TO_JSVAL(av_len((AV*)sv)+1);
            return JS_TRUE;
        }else{
            *rval = INT_TO_JSVAL(0);
            return JS_TRUE;
        }
    }else{
        int i;
        /* __PH__ predefined methods have to win */
        for(i=0;i<sizeof(predefined_methods)/sizeof(char*);i++){
            if(!strcmp(predefined_methods[i], str)){
                return JS_TRUE;
            }
        }

        /* __PH__ properties in hash should be served at last (possibly) */
        PVGetKey(cx, obj, str, rval);
        if(*rval!=JSVAL_VOID){
            return JS_TRUE;
        }else{
#if 0
            char* str = JS_GetStringBytes(JS_ValueToString(cx, name));
            JS_ReportError(cx, "Perl: can't get property '%s'", str);
            return JS_FALSE;
#else
            /* when Volodya does another job, we can experiment :-) */
            char* str = JS_GetStringBytes(JS_ValueToString(cx, name));
            /* great, but who will dispose it? (GC of JS??) */
            JSFunction *fun = JS_NewFunction(cx, PVCallStub, 0, 0, NULL, str);
            *rval = OBJECT_TO_JSVAL(JS_GetFunctionObject(fun));
            return(JS_TRUE);
#endif
        }
    }
    return JS_TRUE;
}

/*
    Set property of PerlValue object. Like GetProperty is looks at
    both array and hash components.
*/
static JSBool
PVSetProperty(JSContext *cx, JSObject *obj, jsval name, jsval *rval)
{
    char* str = JS_GetStringBytes(JS_ValueToString(cx, name));

    if(JSVAL_IS_INT(name)){
        int32 ip;

        JS_ValueToInt32(cx, name, &ip);
        if(PVSetElement(cx, obj, ip, *rval)) return JS_TRUE;
    }
    return PVSetKey(cx, obj, str, *rval);
}

/*
    Retrieve numerical property of a PerlValue object.
    If the object doesn't contain an array, or the
    property doesn't exist, NULL is returned.
*/
static JSBool
PVGetElement(JSContext *cx, JSObject *obj, jsint index, jsval *rval)
{
    SV *ref, **sv;
    AV *list;

    *rval = JSVAL_VOID;

    ref = PVGetRef(cx, obj);

    if(SvTYPE(SvRV(ref)) != SVt_PVAV){
        return JS_FALSE;
    }

    list = (AV*)SvRV(ref);

    if(!list){
        return JS_FALSE;
    }
    sv = av_fetch(list, (I32)index, 0);
    if(!sv){
        return JS_FALSE;
    }
    return SVToJSVAL(cx, obj, newRV_inc(*sv), rval);
}

/*
    Set a numeric property of a PerlValue object.
    If the object doesn't contain an array or the
    index doesn't exist, JS_FALSE is returned.
*/
static JSBool
PVSetElement(JSContext *cx, JSObject *obj, jsint index, jsval v)
{
    SV *ref, **sv, *s;
    AV *list;

    ref = PVGetRef(cx, obj);

    if(SvTYPE(SvRV(ref)) != SVt_PVAV){
        return JS_FALSE;
    }

    list = (AV*)SvRV(ref);

    if(!list) return JS_FALSE;
    JSVALToSV(cx, obj, v, &s);
    sv = av_store(list, (I32)index, s);
    if(!sv) return JS_FALSE;
    return JS_TRUE;
}

/*
    Retrieve property. If the object doesn't contain an hash, or the
    property doesn't exist, NULL is returned.
*/
static JSBool
PVGetKey(JSContext *cx, JSObject *obj, char* name, jsval *rval)
{
    SV *ref, **sv;
    HV *hash;

    *rval = JSVAL_VOID;
    ref = PVGetRef(cx, obj);

    if(SvTYPE(SvRV(ref)) != SVt_PVHV){
        return JS_FALSE;
    }

    hash = (HV*)SvRV(ref);

    if(!hash){
        return JS_FALSE;
    }
    sv = hv_fetch(hash, name, strlen(name), 0);
    if(!sv){
        return JS_FALSE;
    }
    return SVToJSVAL(cx, obj, newRV_inc(*sv), rval);
}

/*
    Get property of a PerlValue object.
    If the object doesn't contain a hash or the
    property doesn't exist, JS_FALSE is returned.
*/
static JSBool
PVSetKey(JSContext *cx, JSObject *obj, char* name, jsval v)
{
    SV *ref, **sv, *s;
    HV *hash;

    ref = PVGetRef(cx, obj);

    if(SvTYPE(SvRV(ref)) != SVt_PVHV){
        return JS_FALSE;
    }

    hash = (HV*)SvRV(ref);

    if(!hash) return JS_FALSE;
    JSVALToSV(cx, obj, v, &s);
    sv = hv_store(hash, name, strlen(name), s, 0);
    if(!sv) return JS_FALSE;
    else return JS_TRUE;
}

/*
    toString() method for PerlValue. For arrays uses array's methods.
    If this fails, the type of the value gets returned. TODO: It's actually
    better to use a Perl module like Data::Dumpvar.pm to print complex
    data structures recursively.
*/
static JSBool
PVToString(JSContext *cx, JSObject *obj, int argc, jsval *argv, jsval* rval)
{
    SV* ref     = PVGetRef(cx, obj);
    SV* sv      = SvRV(ref);
    svtype type = SvTYPE(sv);
    /*jsval args[]= {STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "JS::Object::toString")),
                            OBJECT_TO_JSVAL(obj)};*/
    jsval v;

    /*return perl_call(cx, obj, 2, args, rval);*/

    if(type==SVt_PVAV){
        JSObject   *arrayObject = JS_NewArrayObject(cx,0,NULL);
        JSFunction *fun;

        JS_GetProperty(cx, arrayObject, "toString", &v);
        fun = JS_ValueToFunction(cx, v);
        JS_CallFunction(cx, obj, fun, 0, NULL, rval);
    }else{
        char out[256];
        JS_GetProperty(cx, obj, "type", &v);
        if(!JSVAL_IS_VOID(v))
            sprintf(out, "[%s]", JS_GetStringBytes(JSVAL_TO_STRING(v)));
        else
            strcpy(out, "[PerlValue]");

        *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, out));
    }
    return JS_TRUE;
}

static JSBool
PVConvert(JSContext *cx, JSObject *obj, JSType type, jsval *rval)
{
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

/*
    Takes care of GC in Perl: we need to decrement Perl's
    reference count when PV goes out of scope.
*/
static JSBool
PVFinalize(JSContext *cx, JSObject *obj)
{
    /* SV* sv = SvRV(PVGetRef(cx, obj)); */
    SV* sv = PVGetRef(cx, obj);
    /* SV *sv = PVGetRef(cx, obj);
       if ( SvROK(sv) ) sv = SvRV( sv ); _PH_ test*/

    /* TODO: GC */
    if(SvREFCNT(sv)>0){
        /*fprintf(stderr, "Finilization: %d references left", SvREFCNT(sv));*/
        SvREFCNT_dec(sv);
        /*fprintf(stderr, "Finilization: %d references left", SvREFCNT(sv));*/
    }

    return JS_TRUE;
}

/*
    Convert a jsval to a SV* (scalar value pointer).
    Used for parameter passing. This function is also
    used by the Perl part of PerlConnect.
*/

JSBool
JSVALToSV(JSContext *cx, JSObject *obj, jsval v, SV** sv)
{
    //*sv = &sv_undef; //__PH__??
    if(JSVAL_IS_PRIMITIVE(v)){
        if(JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v)){
            **sv = sv_undef;
        }else
        if(JSVAL_IS_INT(v)){
            sv_setiv(*sv, JSVAL_TO_INT(v));
        }else
        if(JSVAL_IS_DOUBLE(v)){
            sv_setnv(*sv, *JSVAL_TO_DOUBLE(v));
        }else
        if(JSVAL_IS_STRING(v)){
            sv_setpv(*sv, JS_GetStringBytes(JSVAL_TO_STRING(v)));
        }else{
            warn("Unknown primitive type");
        }
    }else{
        if(JSVAL_IS_OBJECT(v)){
            JSObject *object = JSVAL_TO_OBJECT(v);
            if(JS_InstanceOf(cx, object, &perlValueClass, NULL)){
                /* newSVsv(SvRV(PVGetRef(cx, object))); //__PH__?? */
                *sv = PVGetRef(cx, object);
            }else{
                if(JS_IsArrayObject(cx, object)){
                    sv_setref_pv(*sv, "JS::Object", (void*)object);
                    sv_magic(SvRV(*sv), newSViv(cx), '~', NULL, 0);
                }else{
                    sv_setref_pv(*sv, "JS::Object", (void*)object);
                    sv_magic(SvRV(*sv), newSViv(cx), '~', NULL, 0);
                }
            }
        }else{
            warn("Type conversion is not supported");
            **sv = sv_undef;  //__PH__??
            return JS_FALSE;
        }
    }

    return JS_TRUE;
}

/*
    Converts a reference Perl value to a jsval. If ref points
    to an immediate value, the value itself is returned in rval.
    O.w. a PerlValue object is returned. This function is also
    used by the Perl part of PerlConnect.
*/


#define _IS_UNDEF(a) (SvANY(a) == SvANY(&sv_undef))

JSBool
SVToJSVAL(JSContext *cx, JSObject *obj, SV *ref, jsval *rval) {
    SV *sv;
    char* name=NULL;

    if(_IS_UNDEF(ref) || !SvRV(ref) || !SvROK(ref)) {
        sv = ref;
    }else{
        sv = SvRV(ref);
    }
    /* printf("In SVToJSVAL value %s, type=%d\n", SvPV(sv, na), SvTYPE(sv)); */
 
    /* object references are processed as objecs */
    if ( _IS_UNDEF(sv) ){
        *rval = JSVAL_VOID;
    } else
    if(SvIOK(sv)){
      *rval = INT_TO_JSVAL(SvIV(sv));
    } else
    if(SvNOK(sv)){
        JS_NewDoubleValue(cx, SvNV(sv), rval);
    } else
    if(SvPOK(sv)){
        *rval = STRING_TO_JSVAL((JS_NewStringCopyZ(cx, SvPV(sv, na))));
    } else 
    if (1) /*(sv_isobject(ref))*/ {
        JSObject *perlValue;
        
        /*svtype type = SvTYPE(sv);
          switch(type){
          case SVt_RV:   name = "Perl Reference"; break;
          case SVt_PVAV: name = "Perl Array"; break;
          case SVt_PVHV: name = "Perl Hash"; break;
          case SVt_PVCV: name = "Perl Code Reference"; break;
          case SVt_PVMG: name = "Perl Magic"; break;
          default:
          warn("Unsupported type in SVToJSVAL: %d", type);
          *rval = JSVAL_VOID;
          return JS_FALSE;
          }*/
        
        /*printf("default\n");*/
        name = "Perl Value";
        /* __PH__ */
        SvREFCNT_inc(ref);
        perlValue = JS_DefineObject(cx, obj, "PerlValue",
                                    &perlValueClass, NULL, JSPROP_ENUMERATE);
        JS_SetPrivate(cx, perlValue, ref);
        JS_DefineFunctions(cx, perlValue, perlValueMethods);
        JS_DefineProperty(cx, perlValue, "type",
                          name?STRING_TO_JSVAL(JS_NewStringCopyZ(cx, name)):JSVAL_VOID,
                          NULL, NULL, JSPROP_PERMANENT|JSPROP_READONLY);
        *rval = OBJECT_TO_JSVAL(perlValue); 
    }

    
    return JS_TRUE;
}
