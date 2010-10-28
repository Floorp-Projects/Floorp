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
#include "jsstaticcheck.h"
#include "jsutil.h"

JS_BEGIN_EXTERN_C

/*
 * Convenience constants.
 */
#define JS_BITS_PER_UINT32_LOG2 5
#define JS_BITS_PER_UINT32      32

/* The alignment required of objects stored in GC arenas. */
static const uintN JS_GCTHING_ALIGN = 8;
static const uintN JS_GCTHING_ZEROBITS = 3;

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
typedef struct JSFunctionBox        JSFunctionBox;
typedef struct JSObjectBox          JSObjectBox;
typedef struct JSParseNode          JSParseNode;
typedef struct JSProperty           JSProperty;
typedef struct JSSharpObjectMap     JSSharpObjectMap;
typedef struct JSThread             JSThread;
typedef struct JSThreadData         JSThreadData;
typedef struct JSTreeContext        JSTreeContext;
typedef struct JSTryNote            JSTryNote;

/* Friend "Advanced API" typedefs. */
typedef struct JSAtom               JSAtom;
typedef struct JSAtomList           JSAtomList;
typedef struct JSAtomListElement    JSAtomListElement;
typedef struct JSAtomMap            JSAtomMap;
typedef struct JSAtomState          JSAtomState;
typedef struct JSCodeSpec           JSCodeSpec;
typedef struct JSPrinter            JSPrinter;
typedef struct JSRegExpStatics      JSRegExpStatics;
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

struct ArgumentsData;

class RegExp;
class RegExpStatics;
class AutoStringRooter;
class ExecuteArgsGuard;
class InvokeFrameGuard;
class InvokeArgsGuard;
class InvokeSessionGuard;
class TraceRecorder;
struct TraceMonitor;
class StackSpace;
class StackSegment;
class FrameRegsIter;

struct Compiler;
struct Parser;
class TokenStream;
struct Token;
struct TokenPos;
struct TokenPtr;

class ContextAllocPolicy;
class SystemAllocPolicy;

template <class T,
          size_t MinInlineCapacity = 0,
          class AllocPolicy = ContextAllocPolicy>
class Vector;

template <class>
struct DefaultHasher;

template <class Key,
          class Value,
          class HashPolicy = DefaultHasher<Key>,
          class AllocPolicy = ContextAllocPolicy>
class HashMap;

template <class T,
          class HashPolicy = DefaultHasher<T>,
          class AllocPolicy = ContextAllocPolicy>
class HashSet;

class DeflatedStringCache;

class PropertyCache;
struct PropertyCacheEntry;

struct Shape;
struct EmptyShape;

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
                  jsval closure);

typedef JSTrapStatus
(* JSInterruptHook)(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                    void *closure);

typedef JSTrapStatus
(* JSDebuggerHandler)(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                      void *closure);

typedef JSTrapStatus
(* JSThrowHook)(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                void *closure);

typedef JSBool
(* JSWatchPointHandler)(JSContext *cx, JSObject *obj, jsid id, jsval old,
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

typedef JSBool
(* JSDebugErrorHook)(JSContext *cx, const char *message, JSErrorReport *report,
                     void *closure);

typedef struct JSDebugHooks {
    JSInterruptHook     interruptHook;
    void                *interruptHookData;
    JSNewScriptHook     newScriptHook;
    void                *newScriptHookData;
    JSDestroyScriptHook destroyScriptHook;
    void                *destroyScriptHookData;
    JSDebuggerHandler   debuggerHandler;
    void                *debuggerHandlerData;
    JSSourceHandler     sourceHandler;
    void                *sourceHandlerData;
    JSInterpreterHook   executeHook;
    void                *executeHookData;
    JSInterpreterHook   callHook;
    void                *callHookData;
    JSThrowHook         throwHook;
    void                *throwHookData;
    JSDebugErrorHook    debugErrorHook;
    void                *debugErrorHookData;
} JSDebugHooks;

/* js::ObjectOps function pointer typedefs. */

/*
 * Look for id in obj and its prototype chain, returning false on error or
 * exception, true on success.  On success, return null in *propp if id was
 * not found.  If id was found, return the first object searching from obj
 * along its prototype chain in which id names a direct property in *objp, and
 * return a non-null, opaque property pointer in *propp.
 *
 * If JSLookupPropOp succeeds and returns with *propp non-null, that pointer
 * may be passed as the prop parameter to a JSAttributesOp, as a short-cut
 * that bypasses id re-lookup.
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
 * Get or set attributes of the property obj[id]. Return false on error or
 * exception, true with current attributes in *attrsp.
 */
typedef JSBool
(* JSAttributesOp)(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp);

/*
 * A generic type for functions mapping an object to another object, or null
 * if an error or exception was thrown on cx.
 */
typedef JSObject *
(* JSObjectOp)(JSContext *cx, JSObject *obj);

/*
 * Hook that creates an iterator object for a given object. Returns the
 * iterator object or null if an error or exception was thrown on cx.
 */
typedef JSObject *
(* JSIteratorOp)(JSContext *cx, JSObject *obj, JSBool keysonly);

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
 * Hack to expose obj->getOps()->outer to the C implementation of the debugger
 * interface.
 */
extern JS_FRIEND_API(JSObject *)
js_ObjectToOuterObject(JSContext *cx, JSObject *obj);

JS_END_EXTERN_C

#endif /* jsprvtd_h___ */
