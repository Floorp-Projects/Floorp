/* -*- Mode: cc; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * jsIContext.h --- the XPCOM interface to the core JS engine functionality.
 */

class jsIContext;

#ifndef JS_ICONTEXT_H
#define JS_ICONTEXT_H

#include "jsIScriptable.h"
#include "jsIScript.h"
#include "jsIFunction.h"
#include "jsIErrorReporter.h"

#define JS_ICONTEXT_IID \
    { 0, 0, 0, \
	{0, 0, 0, 0, 0, 0, 0, 0}}

/**
 * jsIContext interface declaration
 */

class jsIContext: public nsISupports {
 public:
     virtual JSContext* GetJS() =0;
    /**
     * Compile a JavaScript function.
     * @return NULL on error, compiled jsIFunction *otherwise.
     */
    virtual jsIFunction *compileFunction(jsIScriptable *scope,
					 JSString *source,
					 JSString *sourceName,
					 int lineno) = 0;
    /**
     * Decompile a JavaScript script to equivalent source.
     */
    virtual JSString *decompileScript(jsIScript *script,
				      jsIScriptable *scope,
				      int indent) = 0;
    /**
     * Decompile a JavaScript function to equivalent source, including
     * the function declaration and parameter list.
     *
     * Provides function body of '[native code]' if the provided
     * jsIFunction * isn't of JS-source heritage.
     */
    virtual JSString *decompileFunction(jsIFunction *fun,
					int indent) = 0;

    /**
     * Decompile the body of a JavaScript function.
     *
     * Provides function body of '[native code]' if the provided
     * jsIFunction * isn't of JS-source heritage.
     */
    virtual JSString *decompileFunctionBody(jsIFunction *fun,
					    int indent) = 0;

    /**
     * Create a new JavaScript object. This is equivalent to evaluating 
     * "new Object()", which requires that the value "Object" in the
     * provided scope is a function.
     */
    virtual jsIScriptable *newObject(jsIScriptable *scope) = 0;
    
    /**
     * Create a new JavaScript object by executing the named constructor.
     * This requires that the value named by the constructorName argument.
     */
    virtual jsIScriptable *newObject(jsIScriptable *scope,
				     JSString *constructorName) = 0;
    
    /**
     * Create a new JavaScript object by executing the named constructor
     * with the provided arguments.
     */
    virtual jsIScriptable *newObject(jsIScriptable *scope,
				     JSString *constructorName,
				     jsval *argv,
				     uintN argc) = 0;
    
    /**
     * Create a new JavaScript Array with the specified inital length.
     */
    virtual jsIScriptable *newArray(jsIScriptable *scope,
				    uintN length) = 0;

    /**
     * Create a new JavaScript array with the provided initial contents.
     */
    virtual jsIScriptable *newArray(jsIScriptable *scope,
				    jsval *elements,
				    uintN length) = 0;

    /**
     * Convert a jsval to a JavaScript boolean value.
     * Note: the return value indicates the success of the operation,
     * not the resulting boolean value.  *bp stores the new boolean value.
     */
    virtual nsresult toBoolean(jsval v, JSBool *bp) = 0;

    /**
     * Convert a jsval to a (newly-allocated) JavaScript number value.
     */
    virtual jsdouble *toNumber(jsval v) = 0;

    /**
     * Convert a jsval to a JavaScript string value.
     */
    virtual JSString *toString(jsval v) = 0;

    /**
     * Convert a jsval to a JavaScript object.  The provided scope is
     * used to look up constructors Number, String and Boolean as required.
     */
    virtual jsIScriptable *toScriptable(jsval v, jsIScriptable *scope) = 0;
    
    /**
     * Evaluate a JavaScript source string.
     */
    virtual nsresult evaluateString(jsIScriptable *scope,
			      JSString *source,
			      JSString *sourceName,
			      uintN lineno,
			      jsval *rval) = 0;

    /**
     * Initialize the standard (ECMA-plus) objects in the given scope.
     * Makes scope an ECMA `global object'.
     */
    virtual nsresult initStandardObjects(jsIScriptable *scope) = 0;

    /**
     * Report a (usually fatal) runtime error.
     */
    virtual void reportError(JSString *message) = 0;

    /**
     * Report a warning.
     */
    virtual void reportWarning(JSString *message) = 0;
    
    /**
     * Change the error reporter for this context.
     */
    virtual jsIErrorReporter *setErrorReporter(jsIErrorReporter *reporter) = 0;
    
    /**
     * Set the current language version.
     */
    virtual uintN setLanguageVersion(uintN version) = 0;

    /**
     * Get the current language version.
     */
    virtual uintN getLanguageVersion(void) = 0;

    /**
     * Associate the Context with the current thread.
     * This should be called whenever a Context is operated upon by a
     * new thread.
     */
    virtual void enter(void) = 0;
    
    /**
     * Break the Context-thread association.
     */
    virtual void exit(void) = 0;

    virtual JSString *newStringCopyZ(const char *string) = 0;
    virtual JSString *newUCStringCopyZ(const jschar *string) = 0;
    virtual JSString *newStringCopyN(const char *string, size_t len) = 0;
    virtual JSString *newUCStringCopyN(const jschar *string, size_t len) = 0;
    virtual JSString *newString(char *string, size_t len) = 0;
    virtual JSString *newUCString(jschar *string, size_t len) = 0;

    virtual JSBool addRoot(void *root) = 0;
    virtual JSBool addNamedRoot(void *root, const char *name) = 0;
    virtual JSBool removeRoot(void *root) = 0;
};
			    
#endif /* JS_ICONTEXT_H */
