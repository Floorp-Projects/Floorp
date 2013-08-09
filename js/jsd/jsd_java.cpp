/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* this is all going away... replaced by code in js/jsd/java */

#if 0

#include "native.h"
#include "jsdebug.h"

#include "_gen/netscape_jsdebug_DebugController.h"
#include "_gen/netscape_jsdebug_Script.h"
#include "_gen/netscape_jsdebug_JSThreadState.h"
#include "_gen/netscape_jsdebug_JSStackFrameInfo.h"
#include "_gen/netscape_jsdebug_JSPC.h"
#include "_gen/netscape_jsdebug_JSSourceTextProvider.h"
#include "_gen/netscape_jsdebug_JSErrorReporter.h"

static JSDContext* context = 0;
static struct Hnetscape_jsdebug_DebugController* controller = 0;

/***************************************************************************/
/* helpers */

static JHandle*
_constructInteger(ExecEnv *ee, long i)
{
    return (JHandle*) 
        execute_java_constructor(ee, "java/lang/Integer", 0, "(I)", i);
}

static JHandle*
_putHash(ExecEnv *ee, JHandle* tbl, JHandle* key, JHandle* ob)
{
    return (JHandle*) 
        execute_java_dynamic_method( 
            ee, tbl, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", 
            key, ob );
}

static JHandle*
_getHash(ExecEnv *ee, JHandle* tbl, JHandle* key)
{
    return (JHandle*) 
        execute_java_dynamic_method( 
            ee, tbl, "get",
            "(Ljava/lang/Object;)Ljava/lang/Object;", 
            key );
}

static JHandle*
_removeHash(ExecEnv *ee, JHandle* tbl, JHandle* key)
{
    return (JHandle*) 
        execute_java_dynamic_method( 
            ee, tbl, "remove",
            "(Ljava/lang/Object;)Ljava/lang/Object;", 
            key );
}

struct Hnetscape_jsdebug_JSStackFrameInfo*
_constructJSStackFrameInfo( ExecEnv* ee, JSDStackFrameInfo* jsdframe, 
                            struct Hnetscape_jsdebug_JSThreadState* threadState )
{
    struct Hnetscape_jsdebug_JSStackFrameInfo* frame;

    frame = (struct Hnetscape_jsdebug_JSStackFrameInfo*)
        execute_java_constructor( ee, "netscape/jsdebug/JSStackFrameInfo", 0, 
                                  "(Lnetscape/jsdebug/JSThreadState;)",
                                  threadState );
    if( ! frame )
        return NULL;

    /* XXX fill in additional fields */
    unhand(frame)->_nativePtr = (long) jsdframe;

    return frame;
}

struct Hnetscape_jsdebug_JSPC*
_constructJSPC( ExecEnv* ee, struct Hnetscape_jsdebug_Script* script, long pc )
{
    struct Hnetscape_jsdebug_JSPC * pcOb;

    pcOb = (struct Hnetscape_jsdebug_JSPC *)
        execute_java_constructor( ee, "netscape/jsdebug/JSPC", 0, 
                                  "(Lnetscape/jsdebug/Script;I)",
                                  script, pc );
    if( ! pcOb )
        return NULL;

    /* XXX fill in additional fields */

    return pcOb;
}

static Hnetscape_jsdebug_Script*
_scriptObFromJSDScriptPtr( ExecEnv* ee, JSDScript* jsdscript )
{
    JHandle* tbl = (JHandle*) unhand(controller)->scriptTable;
    JHandle* key = _constructInteger(ee,(long)jsdscript);
    return (Hnetscape_jsdebug_Script*) _getHash( ee, tbl, key );
}

/***************************************************************************/

void
_scriptHook( JSDContext* jsdc,
             JSDScript*  jsdscript,
             bool        creating,
             void*       callerdata )
{
    Hnetscape_jsdebug_Script* script;
    ExecEnv* ee = EE();

    if( ! context || ! controller || ! ee )
        return;

    if( creating )
    {
        char* url      = (char*)JSD_GetScriptFilename       (jsdc, jsdscript);
        JSString* function =    JSD_GetScriptFunctionId     (jsdc, jsdscript);
        int base       =        JSD_GetScriptBaseLineNumber (jsdc, jsdscript);
        int extent     =        JSD_GetScriptLineExtent     (jsdc, jsdscript);

        if( ! url )
        {
            return;
            /* url = ""; */
        }

        /* create Java Object for Script */
        script = (Hnetscape_jsdebug_Script*)
                execute_java_constructor(ee, "netscape/jsdebug/Script", 0, "()");

        if( ! script )
            return;

        /* set the members */
        unhand(script)->_url = makeJavaString(url,strlen(url));
        unhand(script)->_function = function ? makeJavaString(function,strlen(function)) : 0;
        unhand(script)->_baseLineNumber = base;
        unhand(script)->_lineExtent = extent;
        unhand(script)->_nativePtr = (long)jsdscript;

        /* add it to the hash table */
        _putHash( ee, (JHandle*) unhand(controller)->scriptTable,
                  _constructInteger(ee, (long)jsdscript), (JHandle*)script );

        /* call the hook */
        if( unhand(controller)->scriptHook )
        {
            execute_java_dynamic_method( ee,(JHandle*)unhand(controller)->scriptHook,
                                         "justLoadedScript",
                                         "(Lnetscape/jsdebug/Script;)V", 
                                         script );
        }
    }
    else
    {
        JHandle* tbl = (JHandle*) unhand(controller)->scriptTable;
        JHandle* key = _constructInteger(ee,(long)jsdscript);

        /* find Java Object for Script    */
        script = (Hnetscape_jsdebug_Script*) _getHash( ee, tbl, key );

        if( ! script )
            return;

        /* remove it from the hash table  */
        _removeHash( ee, tbl, key );

        /* call the hook */
        if( unhand(controller)->scriptHook )
        {
            execute_java_dynamic_method( ee,(JHandle*)unhand(controller)->scriptHook,
                                         "aboutToUnloadScript",
                                         "(Lnetscape/jsdebug/Script;)V", 
                                         script );
        }
        /* set the Script as invalid */
        execute_java_dynamic_method( ee,(JHandle*)script,
                                     "_setInvalid",
                                     "()V" );
    }
}

/***************************************************************************/
unsigned
_executionHook( JSDContext*     jsdc, 
                JSDThreadState* jsdstate,
                unsigned         type,
                void*           callerdata )
{
    Hnetscape_jsdebug_JSThreadState* threadState;
    Hnetscape_jsdebug_Script* script;
    JHandle* pcOb;
    JSDStackFrameInfo* jsdframe;
    JSDScript*  jsdscript;
    int pc;
    JHandle* tblScript;
    JHandle* keyScript;
    ExecEnv* ee = EE();

    if( ! context || ! controller || ! ee )
        return JSD_HOOK_RETURN_HOOK_ERROR;

    /* get the JSDStackFrameInfo */
    jsdframe = JSD_GetStackFrame(jsdc, jsdstate);
    if( ! jsdframe )
        return JSD_HOOK_RETURN_HOOK_ERROR;

    /* get the JSDScript */
    jsdscript = JSD_GetScriptForStackFrame(jsdc, jsdstate, jsdframe);
    if( ! jsdscript )
        return JSD_HOOK_RETURN_HOOK_ERROR;

    /* find Java Object for Script    */
    tblScript = (JHandle*) unhand(controller)->scriptTable;
    keyScript = _constructInteger(ee, (long)jsdscript);
    script = (Hnetscape_jsdebug_Script*) _getHash( ee, tblScript, keyScript );
    if( ! script )
        return JSD_HOOK_RETURN_HOOK_ERROR;

    /* generate a JSPC */
    pc = JSD_GetPCForStackFrame(jsdc, jsdstate, jsdframe);

    pcOb = (JHandle*)
        _constructJSPC(ee, script, pc);
    if( ! pcOb )
        return JSD_HOOK_RETURN_HOOK_ERROR;

    /* build a JSThreadState */
    threadState = (struct Hnetscape_jsdebug_JSThreadState*)
            execute_java_constructor( ee, "netscape/jsdebug/JSThreadState",0,"()");
    if( ! threadState )
        return JSD_HOOK_RETURN_HOOK_ERROR;

    /* populate the ThreadState */
    /* XXX FILL IN THE REST... */
    unhand(threadState)->valid           = 1; /* correct value for true? */
    unhand(threadState)->currentFramePtr = (long) jsdframe;
    unhand(threadState)->nativeThreadState = (long) jsdstate;
    unhand(threadState)->continueState = netscape_jsdebug_JSThreadState_DEBUG_STATE_RUN;

    /* XXX FILL IN THE REST... */


    /* find and call the appropriate Hook */
    if( JSD_HOOK_INTERRUPTED == type )
    {
        JHandle* hook;

        /* clear the JSD level hook (must reset on next sendInterrupt0()*/
        JSD_ClearInterruptHook(context); 

        hook = (JHandle*) unhand(controller)->interruptHook;
        if( ! hook )
            return JSD_HOOK_RETURN_HOOK_ERROR;

        /* call the hook */
        execute_java_dynamic_method( 
            ee, hook, "aboutToExecute",
            "(Lnetscape/jsdebug/ThreadStateBase;Lnetscape/jsdebug/PC;)V",
            threadState, pcOb );
    }
    else if( JSD_HOOK_DEBUG_REQUESTED == type )
    {
        JHandle* hook;

        hook = (JHandle*) unhand(controller)->debugBreakHook;
        if( ! hook )
            return JSD_HOOK_RETURN_HOOK_ERROR;

        /* call the hook */
        execute_java_dynamic_method( 
            ee, hook, "aboutToExecute",
            "(Lnetscape/jsdebug/ThreadStateBase;Lnetscape/jsdebug/PC;)V",
            threadState, pcOb );
    }
    else if( JSD_HOOK_BREAKPOINT == type )
    {
        JHandle* hook;

        hook = (JHandle*)
                execute_java_dynamic_method( 
                        ee,(JHandle*)controller,
                        "getInstructionHook0",
                        "(Lnetscape/jsdebug/PC;)Lnetscape/jsdebug/InstructionHook;",
                        pcOb );
        if( ! hook )
            return JSD_HOOK_RETURN_HOOK_ERROR;

        /* call the hook */
        execute_java_dynamic_method( 
            ee, hook, "aboutToExecute",
            "(Lnetscape/jsdebug/ThreadStateBase;)V",
            threadState );
    }

    if( netscape_jsdebug_JSThreadState_DEBUG_STATE_THROW == 
        unhand(threadState)->continueState )
        return JSD_HOOK_RETURN_ABORT;

    return JSD_HOOK_RETURN_CONTINUE;
}

unsigned
_errorReporter( JSDContext*     jsdc, 
                JSContext*      cx,
                const char*     message, 
                JSErrorReport*  report,
                void*           callerdata )
{
    JHandle* reporter;
    JHandle* msg         = NULL;
    JHandle* filename    = NULL;
    int      lineno      = 0;
    JHandle* linebuf     = NULL;
    int      tokenOffset = 0;
    ExecEnv* ee = EE();

    if( ! context || ! controller || ! ee )
        return JSD_ERROR_REPORTER_PASS_ALONG;

    reporter = (JHandle*) unhand(controller)->errorReporter;
    if( ! reporter )
        return JSD_ERROR_REPORTER_PASS_ALONG;

    if( message )
        msg = (JHandle*) makeJavaString((char*)message, strlen(message));
    if( report && report->filename )
        filename = (JHandle*) makeJavaString((char*)report->filename, strlen(report->filename));
    if( report && report->linebuf )
        linebuf = (JHandle*) makeJavaString((char*)report->linebuf, strlen(report->linebuf));
    if( report )
        lineno = report->lineno;
    if( report && report->linebuf && report->tokenptr )
        tokenOffset = report->tokenptr - report->linebuf;

    return (int) 
        execute_java_dynamic_method( 
                ee, reporter, "reportError",
                "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;I)I", 
                msg,
                filename,
                lineno,
                linebuf,
                tokenOffset );
}

/***************************************************************************/
/* from "_gen\netscape_jsdebug_DebugController.h" */


/* XXX HACK */
JSDContext* _simContext = 0;

void netscape_jsdebug_DebugController__setController(struct Hnetscape_jsdebug_DebugController * self,/*boolean*/ long on)
{
    if(on)
    {
        context = JSD_DebuggerOn();
        if( ! context )
            return;

        _simContext = context; /* XXX HACK */

        unhand(self)->_nativeContext = (long) context;
        controller = self;
        JSD_SetScriptHook(context, _scriptHook, (void*)1 );
        JSD_SetErrorReporter(context, _errorReporter, (void*)1 );
        JSD_SetDebugBreakHook(context, _executionHook, (void*)1 );
    }
    else
    {
        /* XXX stop somehow... */
        /* kill context        */
        JSD_SetDebugBreakHook(context, NULL, NULL );
        JSD_SetErrorReporter(context, NULL, NULL);
        JSD_SetScriptHook(context, NULL, NULL);
        context = 0;
        controller = 0;
    }
}

void netscape_jsdebug_DebugController_setInstructionHook0(struct Hnetscape_jsdebug_DebugController * self,struct Hnetscape_jsdebug_PC * pcOb)
{
    Hnetscape_jsdebug_Script* script;
    JSDScript* jsdscript;
    unsigned pc;
    ExecEnv* ee = EE();

    if( ! context || ! controller || ! ee )
        return;

    script = (Hnetscape_jsdebug_Script*)
        execute_java_dynamic_method(ee, (JHandle*)pcOb, "getScript","()Lnetscape/jsdebug/Script;");

    if( ! script )
        return;

    jsdscript = (JSDScript*) unhand(script)->_nativePtr;
    if( ! jsdscript )
        return;

    pc = (unsigned)
        execute_java_dynamic_method(ee, (JHandle*)pcOb, "getPC","()I");

    JSD_SetExecutionHook(context, jsdscript, pc, _executionHook, 0);
}

void netscape_jsdebug_DebugController_sendInterrupt0(struct Hnetscape_jsdebug_DebugController * self)
{
    if( ! context || ! controller )
        return;
    JSD_SetInterruptHook(context, _executionHook, 0);
}

struct Hjava_lang_String *netscape_jsdebug_DebugController_executeScriptInStackFrame0(struct Hnetscape_jsdebug_DebugController *self,struct Hnetscape_jsdebug_JSStackFrameInfo *frame,struct Hjava_lang_String *src,struct Hjava_lang_String *filename,long lineno)
{
    struct Hnetscape_jsdebug_JSThreadState* threadStateOb;
    JSDThreadState* jsdthreadstate;
    JSDStackFrameInfo* jsdframe;
    char* filenameC;
    char* srcC;
    JSString* jsstr;
    jsval rval;
    bool success;
    int srclen;

    threadStateOb = (struct Hnetscape_jsdebug_JSThreadState*)unhand(frame)->threadState;
    jsdthreadstate = (JSDThreadState*) unhand(threadStateOb)->nativeThreadState;

    jsdframe = (JSDStackFrameInfo*) unhand(frame)->_nativePtr;

    if( ! context || ! controller || ! jsdframe )
        return NULL;

    filenameC = allocCString(filename);
    if( ! filenameC )
        return NULL;
    srcC = allocCString(src);
    if( ! srcC )
    {
        free(filenameC);
        return NULL;
    }

    srclen = strlen(srcC);

    success = JSD_EvaluateScriptInStackFrame(context, jsdthreadstate, jsdframe, 
                                             srcC, srclen,
                                             filenameC, lineno, &rval);

    /* XXX crashing on Windows under Symantec (though I can't see why!)*/

    free(filenameC);
    free(srcC);


    if( ! success )
        return NULL;

    if( JSVAL_IS_NULL(rval) || JSVAL_IS_VOID(rval) )
        return NULL;

    jsstr = JSD_ValToStringInStackFrame(context,jsdthreadstate,jsdframe,rval);
    if( ! jsstr )
        return NULL;

    /* XXXbe should use JS_GetStringChars and preserve Unicode. */
    return makeJavaString((char*)JS_GetStringBytes(jsstr), JS_GetStringLength(jsstr));
}    

long netscape_jsdebug_DebugController_getNativeMajorVersion(struct Hnetscape_jsdebug_DebugController* self)
{
    return (long) JSD_GetMajorVersion();
}

long netscape_jsdebug_DebugController_getNativeMinorVersion(struct Hnetscape_jsdebug_DebugController* self)
{
    return (long) JSD_GetMinorVersion();
}

/***************************************************************************/
/* from "_gen\netscape_jsdebug_Script.h" */

struct Hnetscape_jsdebug_JSPC *netscape_jsdebug_Script_getClosestPC(struct Hnetscape_jsdebug_Script * self,long line)
{
    unsigned pc;
    JSDScript* jsdscript;

    if( ! context || ! controller )
        return 0;

    jsdscript = (JSDScript*) unhand(self)->_nativePtr;
    if( ! jsdscript )
        return 0;

    pc = JSD_GetClosestPC(context, jsdscript, line);

    if( -1 == pc )
        return 0;
    return _constructJSPC( 0, self, pc);
}

/***************************************************************************/
/* from "_gen\netscape_jsdebug_JSThreadState.h" */

long netscape_jsdebug_JSThreadState_countStackFrames(struct Hnetscape_jsdebug_JSThreadState * self)
{
    JSDThreadState* jsdstate;
        
    if( ! context || ! controller )
        return 0;

    jsdstate = (JSDThreadState*) unhand(self)->nativeThreadState;

    if( ! jsdstate )
        return 0;

    return (long) JSD_GetCountOfStackFrames(context, jsdstate);
}

struct Hnetscape_jsdebug_StackFrameInfo *netscape_jsdebug_JSThreadState_getCurrentFrame(struct Hnetscape_jsdebug_JSThreadState * self)
{
    JSDThreadState* jsdstate;
    JSDStackFrameInfo* jsdframe;
        
    if( ! context || ! controller )
        return NULL;

    jsdstate = (JSDThreadState*) unhand(self)->nativeThreadState;

    if( ! jsdstate )
        return NULL;

    jsdframe = JSD_GetStackFrame(context, jsdstate);
    if( ! jsdframe )
        return NULL;

    return (struct Hnetscape_jsdebug_StackFrameInfo*)
                _constructJSStackFrameInfo( 0, jsdframe, self );
}


/***************************************************************************/
/* from "_gen\netscape_jsdebug_JSStackFrameInfo.h" */

struct Hnetscape_jsdebug_StackFrameInfo *netscape_jsdebug_JSStackFrameInfo_getCaller0(struct Hnetscape_jsdebug_JSStackFrameInfo * self)
{
    JSDStackFrameInfo* jsdframeCur;
    JSDStackFrameInfo* jsdframeCaller;
    struct Hnetscape_jsdebug_JSThreadState* threadState;
    JSDThreadState* jsdthreadstate;

    if( ! context || ! controller )
        return NULL;

    jsdframeCur = (JSDStackFrameInfo*) unhand(self)->_nativePtr;
    if( ! jsdframeCur )
        return NULL;

    threadState = (struct Hnetscape_jsdebug_JSThreadState*) unhand(self)->threadState;
    if( ! threadState )
        return NULL;

    jsdthreadstate = (JSDThreadState*) unhand(threadState)->nativeThreadState;
    if( ! jsdthreadstate )
        return NULL;

    jsdframeCaller = JSD_GetCallingStackFrame(context, jsdthreadstate, jsdframeCur);
    if( ! jsdframeCaller )
        return NULL;

    return (struct Hnetscape_jsdebug_StackFrameInfo*)
                _constructJSStackFrameInfo( 0, jsdframeCaller, threadState );
}
struct Hnetscape_jsdebug_PC *netscape_jsdebug_JSStackFrameInfo_getPC(struct Hnetscape_jsdebug_JSStackFrameInfo * self)
{
    JSDScript* jsdscript;
    JSDStackFrameInfo* jsdframe;
    struct Hnetscape_jsdebug_Script* script;
    struct Hnetscape_jsdebug_JSThreadState* threadState;
    JSDThreadState* jsdthreadstate;
    int pc;
    ExecEnv* ee = EE();

    if( ! context || ! controller || ! ee )
        return NULL;

    jsdframe = (JSDStackFrameInfo*) unhand(self)->_nativePtr;
    if( ! jsdframe )
        return NULL;

    threadState = (struct Hnetscape_jsdebug_JSThreadState*) unhand(self)->threadState;
    if( ! threadState )
        return NULL;

    jsdthreadstate = (JSDThreadState*) unhand(threadState)->nativeThreadState;
    if( ! jsdthreadstate )
        return NULL;

    jsdscript = JSD_GetScriptForStackFrame(context, jsdthreadstate, jsdframe );
    if( ! jsdscript )
        return NULL;

    script = _scriptObFromJSDScriptPtr(ee, jsdscript);
    if( ! script )
        return NULL;

    pc = JSD_GetPCForStackFrame(context, jsdthreadstate, jsdframe);
    if( ! pc )
        return NULL;

    return (struct Hnetscape_jsdebug_PC*) _constructJSPC(ee, script, pc);
}

/***************************************************************************/
/* from "_gen\netscape_jsdebug_JSPC.h" */

struct Hnetscape_jsdebug_SourceLocation *netscape_jsdebug_JSPC_getSourceLocation(struct Hnetscape_jsdebug_JSPC * self)
{
    JSDScript* jsdscript;
    struct Hnetscape_jsdebug_Script* script;
    struct Hnetscape_jsdebug_JSPC* newPCOb;
    int line;
    int newpc;
    int pc;
    ExecEnv* ee = EE();

    if( ! context || ! controller || ! ee )
        return NULL;

    script = unhand(self)->script;

    if( ! script )
        return NULL;

    jsdscript = (JSDScript*) unhand(script)->_nativePtr;
    if( ! jsdscript )
        return NULL;
    pc = unhand(self)->pc;

    line = JSD_GetClosestLine(context, jsdscript, pc);
    newpc = JSD_GetClosestPC(context, jsdscript, line);

    newPCOb = _constructJSPC(ee, script, newpc );
    if( ! newPCOb )
        return NULL;

    return (struct Hnetscape_jsdebug_SourceLocation *)
        execute_java_constructor( ee, "netscape/jsdebug/JSSourceLocation", 0, 
                                  "(Lnetscape/jsdebug/JSPC;I)",
                                  newPCOb, line );
}

/***************************************************************************/
/* from "_gen\netscape_jsdebug_JSSourceTextProvider.h" */

struct Hnetscape_jsdebug_SourceTextItem *netscape_jsdebug_JSSourceTextProvider_loadSourceTextItem0(struct Hnetscape_jsdebug_JSSourceTextProvider * self,struct Hjava_lang_String * url)
{
    /* this should attempt to load the source for the indicated URL */
    return NULL;    
}

void netscape_jsdebug_JSSourceTextProvider_refreshSourceTextVector(struct Hnetscape_jsdebug_JSSourceTextProvider * self)
{

    JHandle* vec;
    JHandle* itemOb;
    JSDSourceText* iterp = 0;
    JSDSourceText* item;
    const char* url;
    struct Hjava_lang_String* urlOb;
    ExecEnv* ee = EE();

    if( ! context || ! controller || ! ee )
        return;

    /* create new vector */
    vec = (JHandle*) execute_java_constructor(ee, "netscape/util/Vector", 0, "()");
    if( ! vec )
        return;

    /* lock the native subsystem */
    JSD_LockSourceTextSubsystem(context);

    /* iterate through the native items */
    while( 0 != (item = JSD_IterateSources(context, &iterp)) )
    {
        int urlStrLen;
        int status = JSD_GetSourceStatus(context,item);

        /* try to find Java object */
        url = JSD_GetSourceURL(context, item);
        if( ! url || 0 == (urlStrLen = strlen(url)) ) /* ignoring those with no url */
            continue;

        urlOb = makeJavaString((char*)url,urlStrLen);
        if( ! urlOb )
            continue;

        itemOb = (JHandle*)
            execute_java_dynamic_method( ee, (JHandle*)self, "findSourceTextItem0",
                      "(Ljava/lang/String;)Lnetscape/jsdebug/SourceTextItem;",
                      urlOb );
                      
        if( ! itemOb )
        {
            /* if not found then generate new item */
            struct Hjava_lang_String* textOb;
            const char* str;
            int length;

            if( ! JSD_GetSourceText(context, item, &str, &length ) )
            {
                str = "";
                length = 0;
            }
            textOb = makeJavaString((char*)str, length);

            itemOb = (JHandle*)
                execute_java_constructor(ee, "netscape/jsdebug/SourceTextItem",0,  
                     "(Ljava/lang/String;Ljava/lang/String;I)",
                     urlOb, textOb, status );
        }
        else if( JSD_IsSourceDirty(context, item) && 
                 JSD_SOURCE_CLEARED != status )
        {
            /* if found and dirty then update */
            struct Hjava_lang_String* textOb;
            const char* str;
            int length;

            if( ! JSD_GetSourceText(context, item, &str, &length ) )
            {
                str = "";
                length = 0;
            }
            textOb = makeJavaString((char*)str, length);
            execute_java_dynamic_method(ee, itemOb, "setText", 
                                        "(Ljava/lang/String;)V", textOb);
            execute_java_dynamic_method(ee, itemOb, "setStatus", 
                                        "(I)V", status );
            execute_java_dynamic_method(ee, itemOb, "setDirty", "(Z)V", 1 );
        }

        /* we have our copy; clear the native cached text */
        if( JSD_SOURCE_INITED  != status && 
            JSD_SOURCE_PARTIAL != status &&
            JSD_SOURCE_CLEARED != status )
        {
            JSD_ClearSourceText(context, item);
        }

        /* set the item clean */
        JSD_SetSourceDirty(context, item, FALSE );

        /* add the item to the vector */
        if( itemOb )
            execute_java_dynamic_method(ee, vec, "addElement", 
                                        "(Ljava/lang/Object;)V", itemOb );
    }
    /* unlock the native subsystem */
    JSD_UnlockSourceTextSubsystem(context);

    /* set main vector to our new vector */

    unhand(self)->_sourceTextVector = (struct Hnetscape_util_Vector*) vec;
}    


#endif
