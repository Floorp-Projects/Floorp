/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
** JavaScript Debugger Navigator API - Script support
*/

#include "jsd.h"
#ifdef NSPR20
#ifdef XP_MAC
#include "prpriv.h"
#else
#include "private/prpriv.h"
#endif
#endif

/* Comment this out to disable (NT specific) dumping as we go */
/*
** #ifdef DEBUG      
** #define JSD_DUMP 1
** #endif            
*/

#define NOT_SET_YET -1

/***************************************************************************/
PRCList jsd_script_list = PR_INIT_STATIC_CLIST(&jsd_script_list);

#ifdef DEBUG
void JSD_ASSERT_VALID_SCRIPT( JSDScript* jsdscript )
{
    PR_ASSERT( jsdscript );
    PR_ASSERT( jsdscript->script );
}
void JSD_ASSERT_VALID_EXEC_HOOK( JSDExecHook* jsdhook )
{
    PR_ASSERT( jsdhook );
    PR_ASSERT( jsdhook->hook );
}
#endif

static JSDScript*
NewJSDScript( JSDContext*  jsdc,
              JSContext    *cx,
              JSScript     *script,
              JSFunction*  function )
{
    JSDScript*  jsdscript;
    PRUintn     lineno;

    /* these are inlined javascript: urls and we can't handle them now */
    lineno = (PRUintn) JS_GetScriptBaseLineNumber(cx, script);
    if( lineno == 0 )
        return NULL;

    jsdscript = PR_NEWZAP(JSDScript);
    if( ! jsdscript )
        return NULL;
    
    PR_APPEND_LINK(&jsdscript->links, &jsd_script_list);
    jsdscript->jsdc         = jsdc;
    jsdscript->script       = script;        
    jsdscript->function     = function;
    jsdscript->lineBase     = lineno;
    jsdscript->lineExtent   = (PRUintn)NOT_SET_YET;
    jsdscript->url = (char*)jsd_BuildNormalizedURL(JS_GetScriptFilename(cx,script));

    PR_INIT_CLIST(&jsdscript->hooks);
    
    return jsdscript;
}           

static void 
DestroyJSDScript( JSDContext*  jsdc,
                  JSDScript*   jsdscript )
{
    /* destroy all hooks */
    jsd_ClearAllExecutionHooksForScript(jsdc, jsdscript);

    PR_REMOVE_LINK(&jsdscript->links);
    PR_FREEIF(jsdscript->url);

    PR_FREEIF(jsdscript);
}

/***************************************************************************/

#ifdef JSD_DUMP
static void
DumpJSDScript( JSDContext* jsdc, JSDScript* jsdscript, const char* leadingtext)
{
    const char* name;
    const char* fun;
    PRUintn base;
    PRUintn extent;
    char Buf[256];
    
    name   = jsd_GetScriptFilename(jsdc, jsdscript);
    fun    = jsd_GetScriptFunctionName(jsdc, jsdscript);
    base   = jsd_GetScriptBaseLineNumber(jsdc, jsdscript);
    extent = jsd_GetScriptLineExtent(jsdc, jsdscript);
    
    sprintf( Buf, "%sscript=%08X, %s, %s, %d-%d\n", 
             leadingtext,
             (unsigned) jsdscript->script,
             name ? name : "no URL", 
             fun  ? fun  : "no fun", 
             base, base + extent - 1 );
    OutputDebugString( Buf );
}

static void
DumpJSDScriptList( JSDContext* jsdc )
{
    JSDScript* iterp = NULL;
    JSDScript* jsdscript = NULL;
    
    OutputDebugString( "*** JSDScriptDump\n" );
    while( NULL != (jsdscript = jsd_IterateScripts(jsdc, &iterp)) )
        DumpJSDScript( jsdc, jsdscript, "  script: " );
}
#endif /* JSD_DUMP */

/***************************************************************************/

#ifndef JSD_SIMULATION
static PRMonitor *jsd_script_mon = NULL; 
#endif /* JSD_SIMULATION */

void
jsd_LockScriptSubsystem(JSDContext* jsdc)
{
#ifndef JSD_SIMULATION
    if (jsd_script_mon == NULL)
        jsd_script_mon = PR_NewNamedMonitor("jsd-script-monitor");

    PR_EnterMonitor(jsd_script_mon);
#endif /* JSD_SIMULATION */
}

void
jsd_UnlockScriptSubsystem(JSDContext* jsdc)
{
#ifndef JSD_SIMULATION
    PR_ExitMonitor(jsd_script_mon);
#endif /* JSD_SIMULATION */
}

void 
jsd_DestroyAllJSDScripts( JSDContext* jsdc )
{
    JSDScript *jsdscript;
    JSDScript *next;

    for (jsdscript = (JSDScript*)jsd_script_list.next;
         jsdscript != (JSDScript*)&jsd_script_list;
         jsdscript = next) 
    {
        next = (JSDScript*)jsdscript->links.next;
        DestroyJSDScript( jsdc, jsdscript );
    }
}

JSDScript*
jsd_FindJSDScript( JSDContext*  jsdc,
                   JSScript     *script )
{
    JSDScript *jsdscript;

    for (jsdscript = (JSDScript *)jsd_script_list.next;
         jsdscript != (JSDScript *)&jsd_script_list;
         jsdscript = (JSDScript *)jsdscript->links.next) {
        if (jsdscript->script == script)
            return jsdscript;
    }
    return NULL;
}               

JSDScript*
jsd_IterateScripts(JSDContext* jsdc, JSDScript **iterp)
{
    JSDScript *jsdscript = *iterp;
    
    if (!jsdscript)
        jsdscript = (JSDScript *)jsd_script_list.next;
    if (jsdscript == (JSDScript *)&jsd_script_list)
        return NULL;
    *iterp = (JSDScript *)jsdscript->links.next;
    return jsdscript;
}

const char*
jsd_GetScriptFilename(JSDContext* jsdc, JSDScript *jsdscript)
{
    return jsdscript->url;
}

const char*
jsd_GetScriptFunctionName(JSDContext* jsdc, JSDScript *jsdscript)
{
    if( ! jsdscript->function )
        return NULL;
    return JS_GetFunctionName(jsdscript->function);
}

PRUintn
jsd_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript)
{
    return jsdscript->lineBase;
}

PRUintn
jsd_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript)
{
    if( NOT_SET_YET == jsdscript->lineExtent )
        jsdscript->lineExtent = JS_GetScriptLineExtent(jsdc->dumbContext, jsdscript->script);
    return jsdscript->lineExtent;
}

prword_t
jsd_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, PRUintn line)
{
    return (prword_t) JS_LineNumberToPC(jsdc->dumbContext, 
                                        jsdscript->script, line );
}

PRUintn
jsd_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, prword_t pc)
{
    PRUintn first = jsdscript->lineBase;
    PRUintn last = first + jsd_GetScriptLineExtent(jsdc, jsdscript) - 1;
    PRUintn line = JS_PCToLineNumber(jsdc->dumbContext, 
                                     jsdscript->script, (jsbytecode*)pc);

    if( line < first )
        return first;
    if( line > last )
        return last;
    return line;    
}

JSD_ScriptHookProc
jsd_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata)
{
    JSD_ScriptHookProc oldHook = jsdc->scriptHook;
    jsdc->scriptHook = hook;
    jsdc->scriptHookData = callerdata;
    return oldHook;
}

JSD_ScriptHookProc
jsd_GetScriptHook(JSDContext* jsdc)
{
    return jsdc->scriptHook;
}

/***************************************************************************/

void PR_CALLBACK
jsd_NewScriptHookProc( 
                JSContext   *cx,
                const char  *filename,      /* URL this script loads from */
                PRUintn     lineno,         /* line where this script starts */
                JSScript    *script,
                JSFunction  *fun,                
                void*       callerdata )
{
    JSDScript* jsdscript = NULL;
    JSDContext* jsdc = (JSDContext*) callerdata;
    
    if( jsd_IsCurrentThreadDangerous() )
        return;
    
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    jsd_LockScriptSubsystem(jsdc);

    jsd_JSContextUsed(jsdc, cx);
    
    jsdscript = NewJSDScript( jsdc, cx, script, fun );
    if( ! jsdscript )
    {
        jsd_UnlockScriptSubsystem(jsdc);
        return;
    }
    
#ifdef JSD_DUMP
    if( jsdscript )
    {
        DumpJSDScript( jsdc, jsdscript, "***NEW Script: " );
        DumpJSDScriptList( jsdc );
    }
#endif /* JSD_DUMP */

    if( jsdc->scriptHook )
        jsdc->scriptHook( jsdc, jsdscript, TRUE, jsdc->scriptHookData );
    
    jsd_UnlockScriptSubsystem(jsdc);
}                

void PR_CALLBACK
jsd_DestroyScriptHookProc( 
                JSContext   *cx,
                JSScript    *script,
                void*       callerdata )
{
    JSDScript* jsdscript = NULL;
    JSDContext* jsdc = (JSDContext*) callerdata;
    
    if( jsd_IsCurrentThreadDangerous() )
        return;

    JSD_ASSERT_VALID_CONTEXT(jsdc);
    
    jsd_LockScriptSubsystem(jsdc);
    
    jsd_JSContextUsed(jsdc, cx);

    jsdscript = jsd_FindJSDScript( jsdc, script );
    if( ! jsdscript )
    {
        jsd_UnlockScriptSubsystem(jsdc);
        return;
    }
    
#ifdef JSD_DUMP
    DumpJSDScript( jsdc, jsdscript, "***DESTROY Script: " );
#endif /* JSD_DUMP */
    
    if( jsdc->scriptHook )
        jsdc->scriptHook( jsdc, jsdscript, FALSE, jsdc->scriptHookData );

    DestroyJSDScript( jsdc, jsdscript );
    
#ifdef JSD_DUMP
    DumpJSDScriptList( jsdc );
#endif /* JSD_DUMP */

    jsd_UnlockScriptSubsystem(jsdc);
}                


/***************************************************************************/

static JSDExecHook*
FindHook( JSDContext* jsdc, JSDScript* jsdscript, prword_t pc)
{
    JSDExecHook* jsdhook;
    PRCList* list = &jsdscript->hooks;

    for (jsdhook = (JSDExecHook*)list->next;
         jsdhook != (JSDExecHook*)list;
         jsdhook = (JSDExecHook*)jsdhook->links.next)
    {
        if (jsdhook->pc == pc)
            return jsdhook;
    }
    return NULL;
}

JSTrapStatus PR_CALLBACK
jsd_TrapHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                void *closure)
{
    PRUintn hookanswer = JSD_HOOK_RETURN_CONTINUE;
    JSDThreadState* jsdthreadstate;
    JSDExecHook* jsdhook = (JSDExecHook*) closure;
    JSDContext*  jsdc;

    if( jsd_IsCurrentThreadDangerous() )
        return JSTRAP_CONTINUE;

    JSD_ASSERT_VALID_EXEC_HOOK(jsdhook);
    PR_ASSERT(jsdhook->pc == (prword_t)pc);
    PR_ASSERT(jsdhook->jsdscript->script == script);

    jsdc = jsdhook->jsdscript->jsdc;
    if( ! jsdc || ! jsdc->inited )
        return JSTRAP_CONTINUE;

    jsd_JSContextUsed(jsdc, cx);

    jsdthreadstate = jsd_NewThreadState(jsdc,cx);
    if( jsdthreadstate )
    {
        hookanswer =
            (*jsdhook->hook)(jsdc, jsdthreadstate, 
                             JSD_HOOK_BREAKPOINT, 
                             jsdhook->callerdata );

        jsd_DestroyThreadState(jsdc, jsdthreadstate);
    }

    *rval = NULL;           /* XXX fix this!!! */

    if( JSD_HOOK_RETURN_ABORT == hookanswer )
        return JSTRAP_ERROR;

    return JSTRAP_CONTINUE; /* XXX fix this!!! */
}



JSBool
jsd_SetExecutionHook(JSDContext*           jsdc, 
                     JSDScript*            jsdscript,
                     prword_t              pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata)
{
    JSDExecHook* jsdhook;

    if( ! hook )
        return jsd_ClearExecutionHook(jsdc, jsdscript, pc);

    jsdhook = FindHook(jsdc, jsdscript, pc);
    if( jsdhook )
    {
        jsdhook->hook       = hook;
        jsdhook->callerdata = callerdata;
        return JS_TRUE;
    }
    /* else... */

    jsdhook = PR_NEWZAP(JSDExecHook);
    if( ! jsdhook )
        return JS_FALSE;
    jsdhook->jsdscript  = jsdscript;
    jsdhook->pc         = pc;
    jsdhook->hook       = hook;
    jsdhook->callerdata = callerdata;

    if( ! JS_SetTrap(jsdc->dumbContext, jsdscript->script, 
                     (jsbytecode*)pc, jsd_TrapHandler, (void*) jsdhook) )
    {
        PR_FREEIF(jsdhook);
        return JS_FALSE;
    }

    PR_APPEND_LINK(&jsdhook->links, &jsdscript->hooks);
    return JS_TRUE;
}

JSBool
jsd_ClearExecutionHook(JSDContext*           jsdc, 
                       JSDScript*            jsdscript,
                       prword_t              pc)
{
    JSDExecHook* jsdhook;

    jsdhook = FindHook(jsdc, jsdscript, pc);
    if( ! jsdhook )
    {
        PR_ASSERT(0);
        return JS_FALSE;
    }

    JS_ClearTrap(jsdc->dumbContext, jsdscript->script, 
                 (jsbytecode*)pc, NULL, NULL );

    PR_REMOVE_LINK(&jsdhook->links);
    PR_FREEIF(jsdhook);

    return JS_TRUE;
}

JSBool
jsd_ClearAllExecutionHooksForScript(JSDContext* jsdc, JSDScript* jsdscript)
{
    JSDExecHook* jsdhook;
    PRCList* list = &jsdscript->hooks;

    while( (JSDExecHook*)list != (jsdhook = (JSDExecHook*)list->next) )
    {
        PR_REMOVE_LINK(&jsdhook->links);
        PR_FREEIF(jsdhook);
    }

    JS_ClearScriptTraps(jsdc->dumbContext, jsdscript->script);
    return JS_TRUE;
}

JSBool
jsd_ClearAllExecutionHooks(JSDContext* jsdc)
{
    JSDScript* jsdscript;
    JSDScript* iterp = NULL;

    while( NULL != (jsdscript = jsd_IterateScripts(jsdc, &iterp)) )
        jsd_ClearAllExecutionHooksForScript(jsdc, jsdscript);
    return JS_TRUE;
}

