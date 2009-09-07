/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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

#ifndef jsprvtd_h___
#define jsprvtd_h___
/*
 * JS private typename definitions.
 *
 * This header is included only in other .h files, for convenience and for
 * simplicity of type naming.  The alternative for structures is to use tags,
 * which are named the same as their typedef names (legal in C/C++, and less
 * noisy than suffixing the typedef name with "Struct" or "Str").  Instead,
 * all .h files that include this file may use the same typedef name, whether
 * declaring a pointer to struct type, or defining a member of struct type.
 *
 * A few fundamental scalar types are defined here too.  Neither the scalar
 * nor the struct typedefs should change much, therefore the nearly-global
 * make dependency induced by this file should not prove painful.
 */

#include "jspubtd.h"
#include "jsutil.h"

/* Internal identifier (jsid) macros. */

#define JSID_IS_ATOM(id)            JSVAL_IS_STRING((jsval)(id))
#define JSID_TO_ATOM(id)            ((JSAtom *)(id))
#define ATOM_TO_JSID(atom)          (JS_ASSERT(ATOM_IS_STRING(atom)),         \
                                     (jsid)(atom))

#define JSID_IS_INT(id)             JSVAL_IS_INT((jsval)(id))
#define JSID_TO_INT(id)             JSVAL_TO_INT((jsval)(id))
#define INT_TO_JSID(i)              ((jsid)INT_TO_JSVAL(i))
#define INT_JSVAL_TO_JSID(v)        ((jsid)(v))
#define INT_JSID_TO_JSVAL(id)       ((jsval)(id))

#define JSID_IS_OBJECT(id)          JSVAL_IS_OBJECT((jsval)(id))
#define JSID_TO_OBJECT(id)          JSVAL_TO_OBJECT((jsval)(id))
#define OBJECT_TO_JSID(obj)         ((jsid)OBJECT_TO_JSVAL(obj))
#define OBJECT_JSVAL_TO_JSID(v)     ((jsid)v)

#define ID_TO_VALUE(id)             ((jsval)(id))

/*
 * Convenience constants.
 */
#define JS_BITS_PER_UINT32_LOG2 5
#define JS_BITS_PER_UINT32      32

/* Scalar typedefs. */
typedef uint8  jsbytecode;
typedef uint8  jssrcnote;
typedef uint32 jsatomid;

/* Struct typedefs. */
typedef struct JSArgumentFormatMap  JSArgumentFormatMap;
typedef struct JSCodeGenerator      JSCodeGenerator;
typedef struct JSGCThing            JSGCThing;
typedef struct JSGenerator          JSGenerator;
typedef struct JSNativeEnumerator   JSNativeEnumerator;
typedef struct JSCompiler           JSCompiler;
typedef struct JSFunctionBox        JSFunctionBox;
typedef struct JSObjectBox          JSObjectBox;
typedef struct JSParseNode          JSParseNode;
typedef struct JSPropCacheEntry     JSPropCacheEntry;
typedef struct JSProperty           JSProperty;
typedef struct JSSharpObjectMap     JSSharpObjectMap;
typedef struct JSTempValueRooter    JSTempValueRooter;
typedef struct JSThread             JSThread;
typedef struct JSThreadData         JSThreadData;
typedef struct JSToken              JSToken;
typedef struct JSTokenPos           JSTokenPos;
typedef struct JSTokenPtr           JSTokenPtr;
typedef struct JSTokenStream        JSTokenStream;
typedef struct JSTraceMonitor       JSTraceMonitor;
typedef struct JSTreeContext        JSTreeContext;
typedef struct JSTryNote            JSTryNote;
typedef struct JSWeakRoots          JSWeakRoots;

/* Friend "Advanced API" typedefs. */
typedef struct JSAtom               JSAtom;
typedef struct JSAtomList           JSAtomList;
typedef struct JSAtomListElement    JSAtomListElement;
typedef struct JSAtomMap            JSAtomMap;
typedef struct JSAtomState          JSAtomState;
typedef struct JSCodeSpec           JSCodeSpec;
typedef struct JSPrinter            JSPrinter;
typedef struct JSRegExp             JSRegExp;
typedef struct JSRegExpStatics      JSRegExpStatics;
typedef struct JSScope              JSScope;
typedef struct JSScopeOps           JSScopeOps;
typedef struct JSScopeProperty      JSScopeProperty;
typedef struct JSStackHeader        JSStackHeader;
typedef struct JSSubString          JSSubString;
typedef struct JSNativeTraceInfo    JSNativeTraceInfo;
typedef struct JSSpecializedNative  JSSpecializedNative;
typedef struct JSXML                JSXML;
typedef struct JSXMLArray           JSXMLArray;
typedef struct JSXMLArrayCursor     JSXMLArrayCursor;

/*
 * Template declarations.
 *
 * jsprvtd.h can be included in both C and C++ translation units. For C++, it
 * may possibly be wrapped in an extern "C" block which does not agree with
 * templates.
 */
#ifdef __cplusplus
extern "C++" {

namespace js {

class ContextAllocPolicy;
class SystemAllocPolicy;

template <class T,
          size_t MinInlineCapacity = 0,
          class AllocPolicy = ContextAllocPolicy>
class Vector;

} /* namespace js */

/* Common instantiations. */
typedef js::Vector<jschar, 32> JSCharBuffer;

} /* export "C++" */
#endif  /* __cplusplus */

/* "Friend" types used by jscntxt.h and jsdbgapi.h. */
typedef enum JSTrapStatus {
    JSTRAP_ERROR,
    JSTRAP_CONTINUE,
    JSTRAP_RETURN,
    JSTRAP_THROW,
    JSTRAP_LIMIT
} JSTrapStatus;

typedef JSTrapStatus
(* JSTrapHandler)(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                  void *closure);

typedef JSBool
(* JSWatchPointHandler)(JSContext *cx, JSObject *obj, jsval id, jsval old,
                        jsval *newp, void *closure);

/* called just after script creation */
typedef void
(* JSNewScriptHook)(JSContext  *cx,
                    const char *filename,  /* URL of script */
                    uintN      lineno,     /* first line */
                    JSScript   *script,
                    JSFunction *fun,
                    void       *callerdata);

/* called just before script destruction */
typedef void
(* JSDestroyScriptHook)(JSContext *cx,
                        JSScript  *script,
                        void      *callerdata);

typedef void
(* JSSourceHandler)(const char *filename, uintN lineno, jschar *str,
                    size_t length, void **listenerTSData, void *closure);

/*
 * This hook captures high level script execution and function calls (JS or
 * native).  It is used by JS_SetExecuteHook to hook top level scripts and by
 * JS_SetCallHook to hook function calls.  It will get called twice per script
 * or function call: just before execution begins and just after it finishes.
 * In both cases the 'current' frame is that of the executing code.
 *
 * The 'before' param is JS_TRUE for the hook invocation before the execution
 * and JS_FALSE for the invocation after the code has run.
 *
 * The 'ok' param is significant only on the post execution invocation to
 * signify whether or not the code completed 'normally'.
 *
 * The 'closure' param is as passed to JS_SetExecuteHook or JS_SetCallHook
 * for the 'before'invocation, but is whatever value is returned from that
 * invocation for the 'after' invocation. Thus, the hook implementor *could*
 * allocate a structure in the 'before' invocation and return a pointer to that
 * structure. The pointer would then be handed to the hook for the 'after'
 * invocation. Alternately, the 'before' could just return the same value as
 * in 'closure' to cause the 'after' invocation to be called with the same
 * 'closure' value as the 'before'.
 *
 * Returning NULL in the 'before' hook will cause the 'after' hook *not* to
 * be called.
 */
typedef void *
(* JSInterpreterHook)(JSContext *cx, JSStackFrame *fp, JSBool before,
                      JSBool *ok, void *closure);

typedef void
(* JSObjectHook)(JSContext *cx, JSObject *obj, JSBool isNew, void *closure);

typedef JSBool
(* JSDebugErrorHook)(JSContext *cx, const char *message, JSErrorReport *report,
                     void *closure);

typedef struct JSDebugHooks {
    JSTrapHandler       interruptHandler;
    void                *interruptHandlerData;
    JSNewScriptHook     newScriptHook;
    void                *newScriptHookData;
    JSDestroyScriptHook destroyScriptHook;
    void                *destroyScriptHookData;
    JSTrapHandler       debuggerHandler;
    void                *debuggerHandlerData;
    JSSourceHandler     sourceHandler;
    void                *sourceHandlerData;
    JSInterpreterHook   executeHook;
    void                *executeHookData;
    JSInterpreterHook   callHook;
    void                *callHookData;
    JSObjectHook        objectHook;
    void                *objectHookData;
    JSTrapHandler       throwHook;
    void                *throwHookData;
    JSDebugErrorHook    debugErrorHook;
    void                *debugErrorHookData;
} JSDebugHooks;

/*
 * Type definitions for temporary GC roots that register with GC local C
 * variables. See jscntxt.h for details.
 */
typedef void
(* JSTempValueTrace)(JSTracer *trc, JSTempValueRooter *tvr);

typedef union JSTempValueUnion {
    jsval               value;
    JSObject            *object;
    JSXML               *xml;
    JSTempValueTrace    trace;
    JSScopeProperty     *sprop;
    JSWeakRoots         *weakRoots;
    JSCompiler          *compiler;
    JSScript            *script;
    jsval               *array;
} JSTempValueUnion;

struct JSTempValueRooter {
    JSTempValueRooter   *down;
    ptrdiff_t           count;
    JSTempValueUnion    u;
};

/* JSObjectOps function pointer typedefs. */

/*
 * Look for id in obj and its prototype chain, returning false on error or
 * exception, true on success.  On success, return null in *propp if id was
 * not found.  If id was found, return the first object searching from obj
 * along its prototype chain in which id names a direct property in *objp, and
 * return a non-null, opaque property pointer in *propp.
 *
 * If JSLookupPropOp succeeds and returns with *propp non-null, that pointer
 * may be passed as the prop parameter to a JSAttributesOp, as a short-cut
 * that bypasses id re-lookup.  In any case, a non-null *propp result after a
 * successful lookup must be dropped via JSObjectOps.dropProperty.
 *
 * NB: successful return with non-null *propp means the implementation may
 * have locked *objp and added a reference count associated with *propp, so
 * callers should not risk deadlock by nesting or interleaving other lookups
 * or any obj-bearing ops before dropping *propp.
 */
typedef JSBool
(* JSLookupPropOp)(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                   JSProperty **propp);

/*
 * Define obj[id], a direct property of obj named id, having the given initial
 * value, with the specified getter, setter, and attributes.
 */
typedef JSBool
(* JSDefinePropOp)(JSContext *cx, JSObject *obj, jsid id, jsval value,
                   JSPropertyOp getter, JSPropertyOp setter, uintN attrs);

/*
 * Get, set, or delete obj[id], returning false on error or exception, true
 * on success.  If getting or setting, the new value is returned in *vp on
 * success.  If deleting without error, *vp will be JSVAL_FALSE if obj[id] is
 * permanent, and JSVAL_TRUE if id named a direct property of obj that was in
 * fact deleted, or if id names no direct property of obj (id could name a
 * prototype property, or no property in obj or its prototype chain).
 */
typedef JSBool
(* JSPropertyIdOp)(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

/*
 * Get or set attributes of the property obj[id].  Return false on error or
 * exception, true with current attributes in *attrsp.  If prop is non-null,
 * it must come from the *propp out parameter of a prior JSDefinePropOp or
 * JSLookupPropOp call.
 */
typedef JSBool
(* JSAttributesOp)(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                   uintN *attrsp);

/*
 * JSObjectOps.checkAccess type: check whether obj[id] may be accessed per
 * mode, returning false on error/exception, true on success with obj[id]'s
 * last-got value in *vp, and its attributes in *attrsp.
 */
typedef JSBool
(* JSCheckAccessIdOp)(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
                      jsval *vp, uintN *attrsp);

/*
 * A generic type for functions taking a context, object, and property, with
 * no return value.  Used by JSObjectOps.dropProperty currently (see above,
 * JSDefinePropOp and JSLookupPropOp, for the object-locking protocol in which
 * dropProperty participates).
 */
typedef void
(* JSPropertyRefOp)(JSContext *cx, JSObject *obj, JSProperty *prop);

/*
 * The following determines whether JS_EncodeCharacters and JS_DecodeBytes
 * treat char[] as utf-8 or simply as bytes that need to be inflated/deflated.
 */
#ifdef JS_C_STRINGS_ARE_UTF8
# define js_CStringsAreUTF8 JS_TRUE
#else
extern JSBool js_CStringsAreUTF8;
#endif

/*
 * Maximum supported value of Arguments.length. It bounds the maximum number
 * of arguments that can be supplied to the function call using
 * Function.prototype.apply. This value also gives the maximum number of
 * elements in the array initializer.
 */
#define JS_ARGS_LENGTH_MAX      (JS_BIT(24) - 1)

#endif /* jsprvtd_h___ */
