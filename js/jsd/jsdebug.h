/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Header for JavaScript Debugging support - All public functions
 */

#ifndef jsdebug_h___
#define jsdebug_h___

#include "jsapi.h"

extern "C" {

/*
 * The linkage of JSD API functions differs depending on whether the file is
 * used within the JSD library or not.  Any source file within the JSD
 * libraray should define EXPORT_JSD_API whereas any client of the library
 * should not.
 */
#ifdef EXPORT_JSD_API
#define JSD_PUBLIC_API(t)    JS_EXPORT_API(t)
#define JSD_PUBLIC_DATA(t)   JS_EXPORT_DATA(t)
#else
#define JSD_PUBLIC_API(t)    JS_IMPORT_API(t)
#define JSD_PUBLIC_DATA(t)   JS_IMPORT_DATA(t)
#endif

#define JSD_FRIEND_API(t)    JSD_PUBLIC_API(t)
#define JSD_FRIEND_DATA(t)   JSD_PUBLIC_DATA(t)

/***************************************************************************/
/* Opaque typedefs for handles */

typedef struct JSDContext        JSDContext;
typedef struct JSDScript         JSDScript;
typedef struct JSDSourceText     JSDSourceText;
typedef struct JSDThreadState    JSDThreadState;
typedef struct JSDStackFrameInfo JSDStackFrameInfo;
typedef struct JSDValue          JSDValue;
typedef struct JSDProperty       JSDProperty;
typedef struct JSDObject         JSDObject;

/***************************************************************************/
/* High Level calls */

/*
* callback stuff for calls in EXE to be accessed by this code
* when it lives in an explicitly loaded DLL
*/

/*
* This callback allows JSD to inform the embedding when JSD has been
* turned on or off. This is especially useful in the Java-based debugging
* system used in mozilla because the debugger applet controls starting
* up the JSD system.
*/
typedef void
(* JSD_SetContextProc)(JSDContext* jsdc, void* user);

/* This struct could have more fields in future versions */
typedef struct
{
    unsigned              size;       /* size of this struct (init before use)*/
    JSD_SetContextProc setContext;
} JSD_UserCallbacks;

/*
* Used by an embedding to tell JSD what JSRuntime to use and to set
* callbacks without starting up JSD. This assumes only one JSRuntime
* will be used. This exists to support the mozilla Java-based debugger
* system.
*/
extern JSD_PUBLIC_API(void)
JSD_SetUserCallbacks(JSRuntime*         jsrt,
                     JSD_UserCallbacks* callbacks,
                     void*              user);

/*
* Startup JSD.
* This version of the init function requires that JSD_SetUserCallbacks()
* has been previously called (with a non-NULL callback struct pointer)
*/
extern JSD_PUBLIC_API(JSDContext*)
JSD_DebuggerOn(void);

/*
* Startup JSD on a particular JSRuntime with (optional) callbacks
*/
extern JSD_PUBLIC_API(JSDContext*)
JSD_DebuggerOnForUser(JSRuntime*         jsrt,
                      JSD_UserCallbacks* callbacks,
                      void*              user);

/*
 * Startup JSD in an application that uses compartments. Debugger
 * objects will be allocated in the same compartment as scopeobj.
 */
extern JSD_PUBLIC_API(JSDContext*)
JSD_DebuggerOnForUserWithCompartment(JSRuntime*         jsrt,
                                     JSD_UserCallbacks* callbacks,
                                     void*              user,
                                     JSObject*          scopeobj);

/*
* Shutdown JSD for this JSDContext
*/
extern JSD_PUBLIC_API(void)
JSD_DebuggerOff(JSDContext* jsdc);

/*
 * Pause JSD for this JSDContext
 */
extern JSD_PUBLIC_API(void)
JSD_DebuggerPause(JSDContext* jsdc);

/*
 * Unpause JSD for this JSDContext
 */
extern JSD_PUBLIC_API(void)
JSD_DebuggerUnpause(JSDContext* jsdc);

/*
* Get the Major Version (initial JSD release used major version = 1)
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetMajorVersion(void);

/*
* Get the Minor Version (initial JSD release used minor version = 0)
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetMinorVersion(void);

/*
* Returns the default JSD global associated with a given JSDContext.
*/
extern JSD_PUBLIC_API(JSObject*)
JSD_GetDefaultGlobal(JSDContext* jsdc);

/*
* Returns a JSRuntime this context is associated with
*/
extern JSD_PUBLIC_API(JSRuntime*)
JSD_GetJSRuntime(JSDContext* jsdc);

/*
* Set the private data for this context, returns previous value
*/
extern JSD_PUBLIC_API(void *)
JSD_SetContextPrivate(JSDContext *jsdc, void *data);

/*
* Get the private data for this context
*/
extern JSD_PUBLIC_API(void *)
JSD_GetContextPrivate(JSDContext *jsdc);

/*
* Clear profile data for all scripts
*/
extern JSD_PUBLIC_API(void)
JSD_ClearAllProfileData(JSDContext* jsdc);

/*
* Context flags.
*/

/* Include native frames in JSDThreadStates. */
#define JSD_INCLUDE_NATIVE_FRAMES 0x01
/*
* Normally, if a script has a 0 in JSD_SCRIPT_PROFILE_BIT it is included in
* profile data, otherwise it is not profiled.  Setting the JSD_PROFILE_WHEN_SET
* flag reverses this convention.
*/
#define JSD_PROFILE_WHEN_SET      0x02
/*
* Normally, when the script in the top frame of a thread state has a 1 in
* JSD_SCRIPT_DEBUG_BIT, the execution hook is ignored.  Setting the
* JSD_DEBUG_WHEN_SET flag reverses this convention.
*/
#define JSD_DEBUG_WHEN_SET        0x04
/*
* When this flag is set the internal call hook will collect profile data.
*/
#define JSD_COLLECT_PROFILE_DATA  0x08
/*
* When this flag is set, stack frames that are disabled for debugging
* will not appear in the call stack chain.
*/
#define JSD_HIDE_DISABLED_FRAMES  0x10
/*
* When this flag is set, the debugger will only check the
* JSD_SCRIPT_DEBUG_BIT on the top (most recent) stack frame.  This
* makes it possible to stop in an enabled frame which was called from
* a stack that contains a disabled frame.
*
* When this flag is *not* set, any stack that contains a disabled frame
* will not be debugged (the execution hook will not be invoked.)
*
* This only applies when the reason for calling the hook would have
* been JSD_HOOK_INTERRUPTED or JSD_HOOK_THROW.  JSD_HOOK_BREAKPOINT,
* JSD_HOOK_DEBUG_REQUESTED, and JSD_HOOK_DEBUGGER_KEYWORD always stop,
* regardless of this setting, as long as the top frame is not disabled.
*
* If JSD_HIDE_DISABLED_FRAMES is set, this is effectively set as well.
*/
#define JSD_MASK_TOP_FRAME_ONLY   0x20

/*
* 0x40 was formerly used to hook into object creation.
*/
#define JSD_DISABLE_OBJECT_TRACE_RETIRED 0x40


extern JSD_PUBLIC_API(void)
JSD_SetContextFlags (JSDContext* jsdc, uint32_t flags);

extern JSD_PUBLIC_API(uint32_t)
JSD_GetContextFlags (JSDContext* jsdc);     

/*
* Notify JSD that this JSContext is 'in use'. This allows JSD to hook the
* ErrorReporter. For the most part this is done automatically whenever
* events like script loading happen. But, it is a good idea to call this
* from the embedding when new contexts come into use.
*/
extern JSD_PUBLIC_API(void)
JSD_JSContextInUse(JSDContext* jsdc, JSContext* context);

/*
* Find the JSDContext (if any) associated with the JSRuntime of a
* given JSContext.
*/
extern JSD_PUBLIC_API(JSDContext*)
JSD_JSDContextForJSContext(JSContext* context);

/***************************************************************************/
/* Script functions */

/*
* Lock the entire script subsystem. This grabs a highlevel lock that
* protects the JSD internal information about scripts. It is important
* to wrap script related calls in this lock in multithreaded situations
* -- i.e. where the debugger is running on a different thread than the
* interpreter -- or when multiple debugger threads may be accessing this
* subsystem. It is safe (and best) to use this locking even if the
* environment might not be multi-threaded. Safely nestable.
*/
extern JSD_PUBLIC_API(void)
JSD_LockScriptSubsystem(JSDContext* jsdc);

/*
* Unlock the entire script subsystem -- see JSD_LockScriptSubsystem
*/
extern JSD_PUBLIC_API(void)
JSD_UnlockScriptSubsystem(JSDContext* jsdc);

/*
* Iterate through all the active scripts for this JSDContext.
* NOTE: must initialize iterp to NULL to start iteration.
* NOTE: must lock and unlock the subsystem
* example:
*
*  JSDScript jsdscript;
*  JSDScript iter = NULL;
*
*  JSD_LockScriptSubsystem(jsdc);
*  while((jsdscript = JSD_IterateScripts(jsdc, &iter)) != NULL) {
*     *** use jsdscript here ***
*  }
*  JSD_UnlockScriptSubsystem(jsdc);
*/
extern JSD_PUBLIC_API(JSDScript*)
JSD_IterateScripts(JSDContext* jsdc, JSDScript **iterp);

/*
* Get the number of times this script has been called.
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetScriptCallCount(JSDContext* jsdc, JSDScript *script);

/*
* Get the max number of times this script called itself, directly or indirectly.
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetScriptMaxRecurseDepth(JSDContext* jsdc, JSDScript *script);

/*
* Get the shortest execution time recorded.
*/
extern JSD_PUBLIC_API(double)
JSD_GetScriptMinExecutionTime(JSDContext* jsdc, JSDScript *script);

/*
* Get the longest execution time recorded.
*/
extern JSD_PUBLIC_API(double)
JSD_GetScriptMaxExecutionTime(JSDContext* jsdc, JSDScript *script);

/*
* Get the total amount of time spent in this script.
*/
extern JSD_PUBLIC_API(double)
JSD_GetScriptTotalExecutionTime(JSDContext* jsdc, JSDScript *script);

/*
* Get the shortest execution time recorded, excluding time spent in called
* functions.
*/
extern JSD_PUBLIC_API(double)
JSD_GetScriptMinOwnExecutionTime(JSDContext* jsdc, JSDScript *script);

/*
* Get the longest execution time recorded, excluding time spent in called
* functions.
*/
extern JSD_PUBLIC_API(double)
JSD_GetScriptMaxOwnExecutionTime(JSDContext* jsdc, JSDScript *script);

/*
* Get the total amount of time spent in this script, excluding time spent
* in called functions.
*/
extern JSD_PUBLIC_API(double)
JSD_GetScriptTotalOwnExecutionTime(JSDContext* jsdc, JSDScript *script);

/*
* Clear profile data for this script.
*/
extern JSD_PUBLIC_API(void)
JSD_ClearScriptProfileData(JSDContext* jsdc, JSDScript *script);

/*
* Get the JSScript for a JSDScript
*/
extern JSD_PUBLIC_API(JSScript*)
JSD_GetJSScript(JSDContext* jsdc, JSDScript *script);

/*
* Get the JSFunction for a JSDScript
*/
extern JSD_PUBLIC_API(JSFunction*)
JSD_GetJSFunction(JSDContext* jsdc, JSDScript *script);

/*
* Determines whether or not to collect profile information for this
* script.  The context flag JSD_PROFILE_WHEN_SET decides the logic.
*/
#define JSD_SCRIPT_PROFILE_BIT 0x01
/*
* Determines whether or not to ignore breakpoints, etc. in this script.
* The context flag JSD_DEBUG_WHEN_SET decides the logic.
*/
#define JSD_SCRIPT_DEBUG_BIT   0x02
/*
 * Determines whether to invoke the onScriptDestroy callback for this
 * script. The default is for this to be true if the onScriptCreated
 * callback was invoked for this script.
 */
#define JSD_SCRIPT_CALL_DESTROY_HOOK_BIT 0x04

extern JSD_PUBLIC_API(uint32_t)
JSD_GetScriptFlags(JSDContext *jsdc, JSDScript* jsdscript);

extern JSD_PUBLIC_API(void)
JSD_SetScriptFlags(JSDContext *jsdc, JSDScript* jsdscript, uint32_t flags);

/*
* Set the private data for this script, returns previous value
*/
extern JSD_PUBLIC_API(void *)
JSD_SetScriptPrivate(JSDScript* jsdscript, void *data);

/*
* Get the private data for this script
*/
extern JSD_PUBLIC_API(void *)
JSD_GetScriptPrivate(JSDScript* jsdscript);

/*
* Determine if this script is still loaded in the interpreter
*/
extern JSD_PUBLIC_API(bool)
JSD_IsActiveScript(JSDContext* jsdc, JSDScript *jsdscript);

/*
* Get the filename associated with this script
*/
extern JSD_PUBLIC_API(const char*)
JSD_GetScriptFilename(JSDContext* jsdc, JSDScript *jsdscript);

/*
* Get the function name associated with this script (NULL if not a function).
* If the function does not have a name the result is the string "anonymous".
* *** new for gecko 2.0 ****
*/
extern JSD_PUBLIC_API(JSString *)
JSD_GetScriptFunctionId(JSDContext* jsdc, JSDScript *jsdscript);

/*
* Get the base linenumber of the sourcefile from which this script was loaded.
* This is one-based -- i.e. the first line of a file is line '1'. This may
* return 0 if this infomation is unknown.
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript);

/*
* Get the count of source lines associated with this script (1 or greater)
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript);

/*
* Declaration of callback for notification of script creation and destruction.
* 'creating' is true if creating new script, false if destroying existing
* script (callback called just before actual destruction).
* 'callerdata' is what was passed to JSD_SetScriptHook to set the hook.
*/
typedef void
(* JSD_ScriptHookProc)(JSDContext* jsdc,
                       JSDScript*  jsdscript,
                       bool        creating,
                       void*       callerdata);

/*
* Set a hook to be called when scripts are created or destroyed (loaded or
* unloaded).
* 'callerdata' can be whatever you want it to be.
*/
extern JSD_PUBLIC_API(bool)
JSD_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata);

/*
* Get the current script hook.
*/
extern JSD_PUBLIC_API(bool)
JSD_GetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc* hook, void** callerdata);

/*
* Get a 'Program Counter' value for a given line. This represents the location
* of the first bit of executable code for this line of source. This 'pc' should 
* be considered an opaque handle.
* 0 is returned for invalid scripts, or lines that lie outside the script.
* If no code is on the given line, then the returned pc represents the first
* code within the script (if any) after the given line.
* This function can be used to set breakpoints -- see JSD_SetExecutionHook
*/
extern JSD_PUBLIC_API(uintptr_t)
JSD_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, unsigned line);

/*
* Get the source line number for a given 'Program Counter' location.
* Returns 0 if no source line information is appropriate (or available) for
* the given pc.
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, uintptr_t pc);

/*
 * Get a list of lines and the corresponding earliest PC for each (see
 * JSD_GetClosestPC). Lines with no PCs associated will not be returned. NULL
 * may be passed for either lines or pcs to avoid filling anything in for that
 * argument.
 */
extern JSD_PUBLIC_API(bool)
JSD_GetLinePCs(JSDContext* jsdc, JSDScript* jsdscript,
               unsigned startLine, unsigned maxLines,
               unsigned* count, unsigned** lines, uintptr_t** pcs);

/* these are only used in cases where scripts are created outside of JS*/

/*
* Direct call to notify JSD that a script has been created.
* Embeddings that use the normal jsapi script functions need not call this.
* Any embedding that follows the (discouraged!) practice of contructing script
* structures manually should call this function to inform JSD. (older ssjs
* systems do this).
*/
extern JSD_PUBLIC_API(void)
JSD_ScriptCreated(JSDContext* jsdc,
                  JSContext   *cx,
                  const char  *filename,    /* URL this script loads from */
                  unsigned       lineno,       /* line where this script starts */
                  JSScript    *script,
                  JSFunction  *fun);

/*
* see JSD_ScriptCreated
*/
extern JSD_PUBLIC_API(void)
JSD_ScriptDestroyed(JSDContext* jsdc,
                    JSFreeOp    *fop,
                    JSScript    *script);

/***************************************************************************/
/* Source Text functions */

/*
* In some embeddings (e.g. mozilla) JavaScript source code from a 'file' may be
* execute before the entire 'file' has even been loaded. This system supports
* access to such incrmentally loaded source. It also allows for the possibility
* that source loading may fail or be aborted (though the source that did load
* may still be usable). Remember that this source is the entire 'file'
* contents and that the JavaScript code may only be part of that source.
*
* For any given URL there can only be one source text item (the most recently
* loaded).
*/

/* these coorespond to netscape.jsdebug.SourceTextItem.java values -
*  change in both places if anywhere
*/

typedef enum
{
    JSD_SOURCE_INITED       = 0, /* initialized, but contains no source yet */
    JSD_SOURCE_PARTIAL      = 1, /* some source loaded, more expected */
    JSD_SOURCE_COMPLETED    = 2, /* all source finished loading */
    JSD_SOURCE_ABORTED      = 3, /* user aborted loading, some may be loaded */
    JSD_SOURCE_FAILED       = 4, /* loading failed, some may be loaded */
    JSD_SOURCE_CLEARED      = 5  /* text has been cleared by debugger */
} JSDSourceStatus;

/*
* Lock the entire source text subsystem. This grabs a highlevel lock that
* protects the JSD internal information about sources. It is important to
* wrap source text related calls in this lock in multithreaded situations
* -- i.e. where the debugger is running on a different thread than the
* interpreter (or the loader of sources) -- or when multiple debugger
* threads may be accessing this subsystem. It is safe (and best) to use
* this locking even if the environment might not be multi-threaded.
* Safely Nestable.
*/
extern JSD_PUBLIC_API(void)
JSD_LockSourceTextSubsystem(JSDContext* jsdc);

/*
* Unlock the entire source text subsystem. see JSD_LockSourceTextSubsystem.
*/
extern JSD_PUBLIC_API(void)
JSD_UnlockSourceTextSubsystem(JSDContext* jsdc);

/*
* Iterate the source test items. Use the same pattern of calls shown in
* the example for JSD_IterateScripts - NOTE that the SourceTextSubsystem.
* must be locked before and unlocked after iterating.
*/
extern JSD_PUBLIC_API(JSDSourceText*)
JSD_IterateSources(JSDContext* jsdc, JSDSourceText **iterp);

/*
* Find the source text item for the given URL (or filename - or whatever
* string the given embedding uses to describe source locations).
* Returns NULL is not found.
*/
extern JSD_PUBLIC_API(JSDSourceText*)
JSD_FindSourceForURL(JSDContext* jsdc, const char* url);

/*
* Get the URL string associated with the given source text item
*/
extern JSD_PUBLIC_API(const char*)
JSD_GetSourceURL(JSDContext* jsdc, JSDSourceText* jsdsrc);

/*
* Get the actual source text. This gives access to the actual storage of
* the source - it sHould *not* be written to.
* The buffer is NOT zero terminated (nor is it guaranteed to have space to
* hold a zero terminating char).
* XXX this is 8-bit character data. Unicode source is not yet supported.
*/
extern JSD_PUBLIC_API(bool)
JSD_GetSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc,
                  const char** ppBuf, int* pLen);

/*
* Clear the text -- delete the text and set the status to JSD_SOURCE_CLEARED.
* This is useful if source is done loading and the debugger wishes to store
* the text data itself (e.g. in a Java String). This allows avoidance of
* storing the same text in multiple places.
*/
extern JSD_PUBLIC_API(void)
JSD_ClearSourceText(JSDContext* jsdc, JSDSourceText* jsdsrc);

/*
* Return the status of the source text item. see JSDSourceStatus enum.
*/
extern JSD_PUBLIC_API(JSDSourceStatus)
JSD_GetSourceStatus(JSDContext* jsdc, JSDSourceText* jsdsrc);

/*
* Has the source been altered since the last call to JSD_SetSourceDirty?
* Use of JSD_IsSourceDirty and JSD_SetSourceDirty is still supported, but
* discouraged in favor of the JSD_GetSourceAlterCount system. This dirty
* scheme ASSUMES that there is only one consumer of the data.
*/
extern JSD_PUBLIC_API(bool)
JSD_IsSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc);

/*
* Clear the dirty flag
*/
extern JSD_PUBLIC_API(void)
JSD_SetSourceDirty(JSDContext* jsdc, JSDSourceText* jsdsrc, bool dirty);

/*
* Each time a source text item is altered this value is incremented. Any
* consumer can store this value when they retieve other data about the
* source text item and then check later to see if the current value is
* different from their stored value. Thus they can know if they have stale
* data or not. NOTE: this value is not gauranteed to start at any given number.
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc);

/*
* Force an increment in the alter count for a source text item. This is
* normally automatic when the item changes, but a give consumer may want to
* force this to amke an item appear to have changed even if it has not.
*/
extern JSD_PUBLIC_API(unsigned)
JSD_IncrementSourceAlterCount(JSDContext* jsdc, JSDSourceText* jsdsrc);

/*
* Destroy *all* the source text items
* (new for server-side USE WITH CARE)
*/
extern JSD_PUBLIC_API(void)
JSD_DestroyAllSources( JSDContext* jsdc );

/* functions for adding source items */

/*
* Add a new item for a given URL. If an item already exists for the given URL
* then the old item is removed.
* 'url' may not be NULL.
*/
extern JSD_PUBLIC_API(JSDSourceText*)
JSD_NewSourceText(JSDContext* jsdc, const char* url);

/*
* Append text (or change status -- e.g. set completed) for a source text
* item. Text need not be zero terminated. Callers should consider the returned
* JSDSourceText to be the 'current' item for future use. This may return NULL
* if called after this item has been replaced by a call to JSD_NewSourceText.
*/
extern JSD_PUBLIC_API(JSDSourceText*)
JSD_AppendSourceText(JSDContext*     jsdc,
                     JSDSourceText*  jsdsrc,
                     const char*     text,       /* *not* zero terminated */
                     size_t          length,
                     JSDSourceStatus status);

/*
* Unicode varient of JSD_AppendSourceText.
*
* NOTE: At this point text is stored in 8bit ASCII so this function just
* extracts the bottom 8bits from each jschar. At some future point we may
* switch to storing and exposing 16bit Unicode.
*/
extern JSD_PUBLIC_API(JSDSourceText*)
JSD_AppendUCSourceText(JSDContext*     jsdc,
                       JSDSourceText*  jsdsrc,
                       const jschar*   text,       /* *not* zero terminated */
                       size_t          length,
                       JSDSourceStatus status);
/*
 * Convienence function for adding complete source of url in one call.
 * same as:
 *   JSDSourceText* jsdsrc;
 *   JSD_LOCK_SOURCE_TEXT(jsdc);
 *   jsdsrc = jsd_NewSourceText(jsdc, url);
 *   if(jsdsrc)
 *       jsdsrc = jsd_AppendSourceText(jsdc, jsdsrc,
 *                                     text, length, JSD_SOURCE_PARTIAL);
 *   if(jsdsrc)
 *       jsdsrc = jsd_AppendSourceText(jsdc, jsdsrc,
 *                                     NULL, 0, JSD_SOURCE_COMPLETED);
 *   JSD_UNLOCK_SOURCE_TEXT(jsdc);
 *   return jsdsrc ? true : false;
 */
extern JSD_PUBLIC_API(bool)
JSD_AddFullSourceText(JSDContext* jsdc,
                      const char* text,       /* *not* zero terminated */
                      size_t      length,
                      const char* url);

/***************************************************************************/
/* Execution/Interrupt Hook functions */

/* possible 'type' params for JSD_ExecutionHookProc */
#define JSD_HOOK_INTERRUPTED            0
#define JSD_HOOK_BREAKPOINT             1
#define JSD_HOOK_DEBUG_REQUESTED        2
#define JSD_HOOK_DEBUGGER_KEYWORD       3
#define JSD_HOOK_THROW                  4

/* legal return values for JSD_ExecutionHookProc */
#define JSD_HOOK_RETURN_HOOK_ERROR      0
#define JSD_HOOK_RETURN_CONTINUE        1
#define JSD_HOOK_RETURN_ABORT           2
#define JSD_HOOK_RETURN_RET_WITH_VAL    3
#define JSD_HOOK_RETURN_THROW_WITH_VAL  4
#define JSD_HOOK_RETURN_CONTINUE_THROW  5

/*
* Implement a callback of this form in order to hook execution.
*/
typedef unsigned
(* JSD_ExecutionHookProc)(JSDContext*     jsdc,
                          JSDThreadState* jsdthreadstate,
                          unsigned           type,
                          void*           callerdata,
                          jsval*          rval);

/* possible 'type' params for JSD_CallHookProc */
#define JSD_HOOK_TOPLEVEL_START  0   /* about to evaluate top level script */
#define JSD_HOOK_TOPLEVEL_END    1   /* done evaluting top level script    */
#define JSD_HOOK_FUNCTION_CALL   2   /* about to call a function           */
#define JSD_HOOK_FUNCTION_RETURN 3   /* done calling function              */

/*
* Implement a callback of this form in order to hook function call/returns.
* Return true from a TOPLEVEL_START or FUNCTION_CALL type call hook if you
* want to hear about the TOPLEVEL_END or FUNCTION_RETURN too.  Return value is
* ignored to TOPLEVEL_END and FUNCTION_RETURN type hooks.
*/
typedef bool
(* JSD_CallHookProc)(JSDContext*     jsdc,
                     JSDThreadState* jsdthreadstate,
                     unsigned           type,
                     void*           callerdata);

/*
* Set Hook to be called whenever the given pc is about to be executed --
* i.e. for 'trap' or 'breakpoint'
*/
extern JSD_PUBLIC_API(bool)
JSD_SetExecutionHook(JSDContext*           jsdc,
                     JSDScript*            jsdscript,
                     uintptr_t             pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

/*
* Clear the hook for this pc
*/
extern JSD_PUBLIC_API(bool)
JSD_ClearExecutionHook(JSDContext*          jsdc,
                       JSDScript*           jsdscript,
                       uintptr_t            pc);

/*
* Clear all the pc specific hooks for this script
*/
extern JSD_PUBLIC_API(bool)
JSD_ClearAllExecutionHooksForScript(JSDContext* jsdc, JSDScript* jsdscript);

/*
* Clear all the pc specific hooks for the entire JSRuntime associated with
* this JSDContext
*/
extern JSD_PUBLIC_API(bool)
JSD_ClearAllExecutionHooks(JSDContext* jsdc);

/*
* Set a hook to be called before the next instruction is executed. Depending
* on the threading situation and whether or not an JS code is currently
* executing the hook might be called before this call returns, or at some
* future time. The hook will continue to be called as each instruction
* executes until cleared.
*/
extern JSD_PUBLIC_API(bool)
JSD_SetInterruptHook(JSDContext*           jsdc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata);

/*
* Call the interrupt hook at least once per source line
*/
extern JSD_PUBLIC_API(bool)
JSD_EnableSingleStepInterrupts(JSDContext* jsdc, JSDScript *jsdscript, bool enable);

/*
* Clear the current interrupt hook.
*/
extern JSD_PUBLIC_API(bool)
JSD_ClearInterruptHook(JSDContext* jsdc);

/*
* Set the hook that should be called whenever a JSD_ErrorReporter hook
* returns JSD_ERROR_REPORTER_DEBUG.
*/
extern JSD_PUBLIC_API(bool)
JSD_SetDebugBreakHook(JSDContext*           jsdc,
                      JSD_ExecutionHookProc hook,
                      void*                 callerdata);

/*
* Clear the debug break hook
*/
extern JSD_PUBLIC_API(bool)
JSD_ClearDebugBreakHook(JSDContext* jsdc);

/*
* Set the hook that should be called when the 'debugger' keyword is
* encountered by the JavaScript interpreter during execution.
*/
extern JSD_PUBLIC_API(bool)
JSD_SetDebuggerHook(JSDContext*           jsdc,
                    JSD_ExecutionHookProc hook,
                    void*                 callerdata);

/*
* Clear the 'debugger' keyword hook
*/
extern JSD_PUBLIC_API(bool)
JSD_ClearDebuggerHook(JSDContext* jsdc);

/*
* Set the hook that should be called when a JS exception is thrown.
* NOTE: the 'do default' return value is: JSD_HOOK_RETURN_CONTINUE_THROW
*/
extern JSD_PUBLIC_API(bool)
JSD_SetThrowHook(JSDContext*           jsdc,
                 JSD_ExecutionHookProc hook,
                 void*                 callerdata);
/*
* Clear the throw hook
*/
extern JSD_PUBLIC_API(bool)
JSD_ClearThrowHook(JSDContext* jsdc);

/*
* Set the hook that should be called when a toplevel script begins or completes.
*/
extern JSD_PUBLIC_API(bool)
JSD_SetTopLevelHook(JSDContext*      jsdc,
                    JSD_CallHookProc hook,
                    void*            callerdata);
/*
* Clear the toplevel call hook
*/
extern JSD_PUBLIC_API(bool)
JSD_ClearTopLevelHook(JSDContext* jsdc);

/*
* Set the hook that should be called when a function call or return happens.
*/
extern JSD_PUBLIC_API(bool)
JSD_SetFunctionHook(JSDContext*      jsdc,
                    JSD_CallHookProc hook,
                    void*            callerdata);
/*
* Clear the function call hook
*/
extern JSD_PUBLIC_API(bool)
JSD_ClearFunctionHook(JSDContext* jsdc);

/***************************************************************************/
/* Stack Frame functions */

/*
* Get the count of call stack frames for the given JSDThreadState
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetCountOfStackFrames(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

/*
* Get the 'current' call stack frame for the given JSDThreadState
*/
extern JSD_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetStackFrame(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

/*
* Get the JSContext for the given JSDThreadState
*/
extern JSD_PUBLIC_API(JSContext*)
JSD_GetJSContext(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

/*
* Get the calling call stack frame for the given frame
*/
extern JSD_PUBLIC_API(JSDStackFrameInfo*)
JSD_GetCallingStackFrame(JSDContext* jsdc,
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe);

/*
* Get the script for the given call stack frame
*/
extern JSD_PUBLIC_API(JSDScript*)
JSD_GetScriptForStackFrame(JSDContext* jsdc,
                           JSDThreadState* jsdthreadstate,
                           JSDStackFrameInfo* jsdframe);

/*
* Get the 'Program Counter' for the given call stack frame
*/
extern JSD_PUBLIC_API(uintptr_t)
JSD_GetPCForStackFrame(JSDContext* jsdc,
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe);

/*
* Get the JavaScript Call Object for the given call stack frame.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_GetCallObjectForStackFrame(JSDContext* jsdc,
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe);

/*
* Get the head of the scope chain for the given call stack frame.
* the chain can be traversed using JSD_GetValueParent.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_GetScopeChainForStackFrame(JSDContext* jsdc,
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe);

/*
* Get the 'this' Object for the given call stack frame.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_GetThisForStackFrame(JSDContext* jsdc,
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe);

/*
* Get the name of the function executing in this stack frame.  Especially useful
* for native frames (without script objects.)
* *** new for gecko 2.0 ****
*/
extern JSD_PUBLIC_API(JSString *)
JSD_GetIdForStackFrame(JSDContext* jsdc,
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe);

/*
* True if stack frame represents a frame created as a result of a debugger
* evaluation.
*/
extern JSD_PUBLIC_API(bool)
JSD_IsStackFrameDebugger(JSDContext* jsdc,
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe);

/*
* True if stack frame is constructing a new object.
*/
extern JSD_PUBLIC_API(bool)
JSD_IsStackFrameConstructing(JSDContext* jsdc,
                             JSDThreadState* jsdthreadstate,
                             JSDStackFrameInfo* jsdframe);

/*
* Evaluate the given unicode source code in the context of the given stack frame.
* returns true and puts result in rval on success, false on failure.
* NOTE: The ErrorReporter hook might be called if this fails.
*/
extern JSD_PUBLIC_API(bool)
JSD_EvaluateUCScriptInStackFrame(JSDContext* jsdc,
                                 JSDThreadState* jsdthreadstate,
                                 JSDStackFrameInfo* jsdframe,
                                 const jschar *bytes, unsigned length,
                                 const char *filename, unsigned lineno,
                                 JS::MutableHandleValue rval);

/*
* Same as above, but does not eat exceptions.
*/
extern JSD_PUBLIC_API(bool)
JSD_AttemptUCScriptInStackFrame(JSDContext* jsdc,
                                JSDThreadState* jsdthreadstate,
                                JSDStackFrameInfo* jsdframe,
                                const jschar *bytes, unsigned length,
                                const char *filename, unsigned lineno,
                                JS::MutableHandleValue rval);

/* single byte character version of JSD_EvaluateUCScriptInStackFrame */
extern JSD_PUBLIC_API(bool)
JSD_EvaluateScriptInStackFrame(JSDContext* jsdc,
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, unsigned length,
                               const char *filename, unsigned lineno, JS::MutableHandleValue rval);

/*
* Same as above, but does not eat exceptions.
*/
extern JSD_PUBLIC_API(bool)
JSD_AttemptScriptInStackFrame(JSDContext* jsdc,
                              JSDThreadState* jsdthreadstate,
                              JSDStackFrameInfo* jsdframe,
                              const char *bytes, unsigned length,
                              const char *filename, unsigned lineno, JS::MutableHandleValue rval);

/*
* Convert the given jsval to a string
* NOTE: The ErrorReporter hook might be called if this fails.
*/
extern JSD_PUBLIC_API(JSString*)
JSD_ValToStringInStackFrame(JSDContext* jsdc,
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val);

/*
* Get the JSDValue currently being thrown as an exception (may be NULL).
* NOTE: must eventually release by calling JSD_DropValue (if not NULL)
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_GetException(JSDContext* jsdc, JSDThreadState* jsdthreadstate);

/*
* Set the JSDValue currently being thrown as an exception.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_SetException(JSDContext* jsdc, JSDThreadState* jsdthreadstate, 
                 JSDValue* jsdval);

/***************************************************************************/
/* Error Reporter functions */

/*
* XXX The ErrorReporter Hook scheme is going to change soon to more
*     Fully support exceptions.
*/

/* legal return values for JSD_ErrorReporter */
#define JSD_ERROR_REPORTER_PASS_ALONG   0 /* pass along to regular reporter */
#define JSD_ERROR_REPORTER_RETURN       1 /* don't pass to error reporter */
#define JSD_ERROR_REPORTER_DEBUG        2 /* force call to DebugBreakHook */
#define JSD_ERROR_REPORTER_CLEAR_RETURN 3 /* clear exception and don't pass */

/*
* Implement a callback of this form in order to hook the ErrorReporter
*/
typedef unsigned
(* JSD_ErrorReporter)(JSDContext*     jsdc,
                      JSContext*      cx,
                      const char*     message,
                      JSErrorReport*  report,
                      void*           callerdata);

/* Set ErrorReporter hook */
extern JSD_PUBLIC_API(bool)
JSD_SetErrorReporter(JSDContext*       jsdc,
                     JSD_ErrorReporter reporter,
                     void*             callerdata);

/* Get Current ErrorReporter hook */
extern JSD_PUBLIC_API(bool)
JSD_GetErrorReporter(JSDContext*        jsdc,
                     JSD_ErrorReporter* reporter,
                     void**             callerdata);

/***************************************************************************/
/* Generic locks that callers can use for their own purposes */

struct JSDStaticLock;

/*
* Is Locking and GetThread supported in this build?
*/
extern JSD_PUBLIC_API(bool)
JSD_IsLockingAndThreadIdSupported();

/*
* Create a reentrant/nestable lock
*/
extern JSD_PUBLIC_API(JSDStaticLock*)
JSD_CreateLock();

/*
* Aquire lock for this thread (or block until available). Increments a
* counter if this thread already owns the lock.
*/
extern JSD_PUBLIC_API(void)
JSD_Lock(JSDStaticLock* lock);

/*
* Release lock for this thread (or decrement the counter if JSD_Lock
* was previous called more than once).
*/
extern JSD_PUBLIC_API(void)
JSD_Unlock(JSDStaticLock* lock);

/*
* For debugging only if not (JS_THREADSAFE AND DEBUG) then returns true
*    So JSD_IsLocked(lock) may not equal !JSD_IsUnlocked(lock)
*/
extern JSD_PUBLIC_API(bool)
JSD_IsLocked(JSDStaticLock* lock);

/*
* See above...
*/
extern JSD_PUBLIC_API(bool)
JSD_IsUnlocked(JSDStaticLock* lock);

/*
* return an ID uniquely identifying this thread.
*/
extern JSD_PUBLIC_API(void*)
JSD_CurrentThread();

/***************************************************************************/
/* Value and Property Functions  --- All NEW for 1.1 --- */

/*
* NOTE: JSDValue and JSDProperty objects are reference counted. This allows
* for rooting these objects AND any underlying garbage collected jsvals.
* ALL JSDValue and JSDProperty objects returned by the functions below
* MUST eventually be released using the appropriate JSD_Dropxxx function.
*/

/*
* Create a new JSDValue to wrap the given jsval
* NOTE: must eventually release by calling JSD_DropValue (if not NULL)
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_NewValue(JSDContext* jsdc, jsval val);

/*
* Release the JSDValue. After this call the object MUST not be referenced again!
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(void)
JSD_DropValue(JSDContext* jsdc, JSDValue* jsdval);

/*
* Get the jsval wrapped by this JSDValue
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(jsval)
JSD_GetValueWrappedJSVal(JSDContext* jsdc, JSDValue* jsdval);

/*
* Clear all property and association information about the given JSDValue.
* Such information will be lazily regenerated when later accessed. This
* function must be called to make changes to the properties of an object
* visible to the accessor functions below (if the properties et.al. have
* changed since a previous call to those accessors).
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(void)
JSD_RefreshValue(JSDContext* jsdc, JSDValue* jsdval);

/**************************************************/

/*
* Does the JSDValue wrap a JSObject?
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_IsValueObject(JSDContext* jsdc, JSDValue* jsdval);

/*
* Does the JSDValue wrap a number (int or double)?
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_IsValueNumber(JSDContext* jsdc, JSDValue* jsdval);

/*
* Does the JSDValue wrap an int?
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_IsValueInt(JSDContext* jsdc, JSDValue* jsdval);

/*
* Does the JSDValue wrap a double?
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_IsValueDouble(JSDContext* jsdc, JSDValue* jsdval);

/*
* Does the JSDValue wrap a JSString?
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_IsValueString(JSDContext* jsdc, JSDValue* jsdval);

/*
* Does the JSDValue wrap a bool?
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_IsValueBoolean(JSDContext* jsdc, JSDValue* jsdval);

/*
* Does the JSDValue wrap a JSVAL_NULL?
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_IsValueNull(JSDContext* jsdc, JSDValue* jsdval);

/*
* Does the JSDValue wrap a JSVAL_VOID?
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_IsValueVoid(JSDContext* jsdc, JSDValue* jsdval);

/*
* Does the JSDValue wrap a primative (not a JSObject)?
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_IsValuePrimitive(JSDContext* jsdc, JSDValue* jsdval);

/*
* Does the JSDValue wrap a function?
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_IsValueFunction(JSDContext* jsdc, JSDValue* jsdval);

/*
* Does the JSDValue wrap a native function?
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_IsValueNative(JSDContext* jsdc, JSDValue* jsdval);

/**************************************************/

/*
* Return bool value (does NOT do conversion).
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(bool)
JSD_GetValueBoolean(JSDContext* jsdc, JSDValue* jsdval);

/*
* Return int32_t value (does NOT do conversion).
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(int32_t)
JSD_GetValueInt(JSDContext* jsdc, JSDValue* jsdval);

/*
* Return double value (does NOT do conversion).
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(double)
JSD_GetValueDouble(JSDContext* jsdc, JSDValue* jsdval);

/*
* Return JSString value (DOES do conversion if necessary).
* NOTE that the JSString returned is not protected from garbage
* collection. It should be immediately read or wrapped using
* JSD_NewValue(jsdc,STRING_TO_JSVAL(str)) if necessary.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSString*)
JSD_GetValueString(JSDContext* jsdc, JSDValue* jsdval);

/*
* Return name of function IFF JSDValue represents a function.
* *** new for gecko 2.0 ****
*/
extern JSD_PUBLIC_API(JSString *)
JSD_GetValueFunctionId(JSDContext* jsdc, JSDValue* jsdval);

/*
* Return function object IFF JSDValue represents a function or an object
* wrapping a function.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSFunction*)
JSD_GetValueFunction(JSDContext* jsdc, JSDValue* jsdval);

/**************************************************/

/*
* Return the number of properties for the JSDValue.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetCountOfProperties(JSDContext* jsdc, JSDValue* jsdval);

/*
* Iterate through the properties of the JSDValue.
* Use form similar to that shown for JSD_IterateScripts (no locking required).
* NOTE: each JSDProperty returned must eventually be released by calling
* JSD_DropProperty.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDProperty*)
JSD_IterateProperties(JSDContext* jsdc, JSDValue* jsdval, JSDProperty **iterp);

/* 
* Get the JSDProperty for the property of this JSDVal with this name.
* NOTE: must eventually release by calling JSD_DropProperty (if not NULL)
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDProperty*)
JSD_GetValueProperty(JSDContext* jsdc, JSDValue* jsdval, JSString* name);

/*
* Get the prototype object for this JSDValue.
* NOTE: must eventually release by calling JSD_DropValue (if not NULL)
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_GetValuePrototype(JSDContext* jsdc, JSDValue* jsdval);

/*
* Get the parent object for this JSDValue.
* NOTE: must eventually release by calling JSD_DropValue (if not NULL)
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_GetValueParent(JSDContext* jsdc, JSDValue* jsdval);

/*
* Get the ctor object for this JSDValue (or likely its prototype's ctor).
* NOTE: must eventually release by calling JSD_DropValue (if not NULL)
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_GetValueConstructor(JSDContext* jsdc, JSDValue* jsdval);

/*
* Get the name of the class for this object.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(const char*)
JSD_GetValueClassName(JSDContext* jsdc, JSDValue* jsdval);

/*
* Get the script for the given value if the given value represents a
* scripted function.  Otherwise, return null.
*/
extern JSD_PUBLIC_API(JSDScript*)
JSD_GetScriptForValue(JSDContext* jsdc, JSDValue* jsdval);

/**************************************************/

/* possible or'd together bitflags returned by JSD_GetPropertyFlags
 *
 * XXX these must stay the same as the JSPD_ flags in jsdbgapi.h
 */
#define JSDPD_ENUMERATE  JSPD_ENUMERATE    /* visible to for/in loop */
#define JSDPD_READONLY   JSPD_READONLY     /* assignment is error */
#define JSDPD_PERMANENT  JSPD_PERMANENT    /* property cannot be deleted */
#define JSDPD_ALIAS      JSPD_ALIAS        /* property has an alias id */
#define JSDPD_EXCEPTION  JSPD_EXCEPTION    /* exception occurred looking up */
                                           /* proprety, value is exception  */
#define JSDPD_ERROR      JSPD_ERROR        /* native getter returned false */
                                           /* without throwing an exception */
/* this is not one of the JSPD_ flags in jsdbgapi.h  - careful not to overlap*/
#define JSDPD_HINTED     0x800             /* found via explicit lookup */

/*
* Release this JSDProperty
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(void)
JSD_DropProperty(JSDContext* jsdc, JSDProperty* jsdprop);

/*
* Get the JSDValue represeting the name of this property (int or string)
* NOTE: must eventually release by calling JSD_DropValue
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_GetPropertyName(JSDContext* jsdc, JSDProperty* jsdprop);

/*
* Get the JSDValue represeting the current value of this property
* NOTE: must eventually release by calling JSD_DropValue (if not NULL)
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_GetPropertyValue(JSDContext* jsdc, JSDProperty* jsdprop);

/*
* Get the JSDValue represeting the alias of this property (if JSDPD_ALIAS set)
* NOTE: must eventually release by calling JSD_DropValue (if not NULL)
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_GetPropertyAlias(JSDContext* jsdc, JSDProperty* jsdprop);

/*
* Get the flags for this property
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetPropertyFlags(JSDContext* jsdc, JSDProperty* jsdprop);

/***************************************************************************/
/* Object Functions  --- All NEW for 1.1 --- */

/*
* JSDObjects exist to allow a means of iterating through all JSObjects in the
* engine. They are created and destroyed as the wrapped JSObjects are created
* and destroyed in the engine. JSDObjects additionally track the location in
* the JavaScript source where their wrapped JSObjects were created and the name
* and location of the (non-native) constructor used.
*
* NOTE: JSDObjects are NOT reference counted. The have only weak links to
* jsObjects - thus they do not inhibit garbage collection of JSObjects. If
* you need a JSDObject to safely persist then wrap it in a JSDValue (using
* jsd_GetValueForObject).
*/

/*
* Lock the entire Object subsystem -- see JSD_UnlockObjectSubsystem
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(void)
JSD_LockObjectSubsystem(JSDContext* jsdc);

/*
* Unlock the entire Object subsystem -- see JSD_LockObjectSubsystem
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(void)
JSD_UnlockObjectSubsystem(JSDContext* jsdc);

/*
* Iterate through the known objects
* Use form similar to that shown for JSD_IterateScripts.
* NOTE: the ObjectSubsystem must be locked before and unlocked after iterating.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDObject*)
JSD_IterateObjects(JSDContext* jsdc, JSDObject** iterp);

/*
* Get the JSObject represented by this JSDObject
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSObject*)
JSD_GetWrappedObject(JSDContext* jsdc, JSDObject* jsdobj);

/*
* Get the URL of the line of source that caused this object to be created.
* May be NULL.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(const char*)
JSD_GetObjectNewURL(JSDContext* jsdc, JSDObject* jsdobj);

/*
* Get the line number of the line of source that caused this object to be
* created. May be 0 indicating that the line number is unknown.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetObjectNewLineNumber(JSDContext* jsdc, JSDObject* jsdobj);

/*
* Get the URL of the line of source of the constructor for this object.
* May be NULL.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(const char*)
JSD_GetObjectConstructorURL(JSDContext* jsdc, JSDObject* jsdobj);

/*
* Get the line number of the line of source of the constructor for this object.
* created. May be 0 indicating that the line number is unknown.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(unsigned)
JSD_GetObjectConstructorLineNumber(JSDContext* jsdc, JSDObject* jsdobj);

/*
* Get the name of the constructor for this object.
* May be NULL.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(const char*)
JSD_GetObjectConstructorName(JSDContext* jsdc, JSDObject* jsdobj);

/*
* Get JSDObject representing this JSObject.
* May return NULL.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDObject*)
JSD_GetJSDObjectForJSObject(JSDContext* jsdc, JSObject* jsobj);

/*
* Get JSDObject representing this JSDValue.
* May return NULL.
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDObject*)
JSD_GetObjectForValue(JSDContext* jsdc, JSDValue* jsdval);

/*
* Create a JSDValue to wrap (and root) this JSDObject.
* NOTE: must eventually release by calling JSD_DropValue (if not NULL)
* *** new for version 1.1 ****
*/
extern JSD_PUBLIC_API(JSDValue*)
JSD_GetValueForObject(JSDContext* jsdc, JSDObject* jsdobj);

} // extern "C"

#endif /* jsdebug_h___ */
