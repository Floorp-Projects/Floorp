#ifndef JS_CONTEXT_H
#define JS_CONTEXT_H

#include "jsIContext.h"
#include "jsRuntime.h"

static void ErrorReporterHandler(JSContext *cx, const char *message,
				 JSErrorReport *report);
class jsContext: public jsIContext {
    JSContext *cx;
    jsIErrorReporter *reporter;
    
public:
    jsContext() { };
    jsContext(JSRuntime *rt, uintN stacksize);
    ~jsContext();

    /**
     * We shouldn't need this, but for now...
     */
    JSContext *GetJS(void);
    jsIFunction *compileFunction(jsIScriptable *scope,
				 JSString *source,
				 JSString *sourceName,
				 int lineno);
    JSString *decompileScript(jsIScript *script,
			      jsIScriptable *scope,
			      int indent);
    JSString *decompileFunction(jsIFunction *fun,
				int indent);
    JSString *decompileFunctionBody(jsIFunction *fun,
				    int indent);
    jsIScriptable *newObject(jsIScriptable *scope);
    jsIScriptable *newObject(jsIScriptable *scope,
			     JSString *constructorName);
    jsIScriptable *newObject(jsIScriptable *scope,
			     JSString *constructorName,
			     jsval *argv,
			     uintN argc);
    jsIScriptable *newArray(jsIScriptable *scope);
    jsIScriptable *newArray(jsIScriptable *scope,
			    uintN length);
    jsIScriptable *newArray(jsIScriptable *scope,
			    jsval *elements,
			    uintN length);
    nsresult toBoolean(jsval v, JSBool *bp);
    jsdouble *toNumber(jsval v);
    JSString *toString(jsval v);
    jsIScriptable *toScriptable(jsval v, jsIScriptable *scope);
    nsresult evaluateString(jsIScriptable *scope,
			    JSString *source,
			    JSString *sourceName,
			    uintN lineno,
			    jsval *rval);
    nsresult initStandardObjects(jsIScriptable *scope);
    void reportError(JSString *message);
    void reportWarning(JSString *message);
    jsIErrorReporter *setErrorReporter(jsIErrorReporter *reporter);
    uintN setLanguageVersion(uintN version);
    uintN getLanguageVersion(void);
    void enter(void);
    void exit(void);

    JSString *newStringCopyZ(const char *string);
    JSString *newUCStringCopyZ(const jschar *string);
    JSString *newStringCopyN(const char *string, size_t len);
    JSString *newUCStringCopyN(const jschar *string, size_t len);
    JSString *newString(char *string, size_t len);
    JSString *newUCString(jschar *string, size_t len);

    JSBool addRoot(void *root);
    JSBool addNamedRoot(void *root, const char *name);
    JSBool removeRoot(void *root);

    friend class jsRuntime;
    friend void ErrorReporterHandler(JSContext *cx, const char *message,
				     JSErrorReport *report);
    NS_DECL_ISUPPORTS
};

#endif /* JS_CONTEXT_H */
