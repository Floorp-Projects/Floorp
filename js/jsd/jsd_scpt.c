/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JavaScript Debugging support - Script support
 */

#include "jsd.h"

/* Comment this out to disable (NT specific) dumping as we go */
/*
** #ifdef DEBUG      
** #define JSD_DUMP 1
** #endif            
*/

#define NOT_SET_YET -1

/***************************************************************************/

#ifdef DEBUG
void JSD_ASSERT_VALID_SCRIPT(JSDScript* jsdscript)
{
    JS_ASSERT(jsdscript);
    JS_ASSERT(jsdscript->script);
}
void JSD_ASSERT_VALID_EXEC_HOOK(JSDExecHook* jsdhook)
{
    JS_ASSERT(jsdhook);
    JS_ASSERT(jsdhook->hook);
}
#endif

#ifdef LIVEWIRE
static JSBool
HasFileExtention(const char* name, const char* ext)
{
    int i;
    int len = strlen(ext);
    const char* p = strrchr(name,'.');
    if( !p )
        return JS_FALSE;
    p++;
    for(i = 0; i < len; i++ )
    {
        JS_ASSERT(islower(ext[i]));
        if( 0 == p[i] || tolower(p[i]) != ext[i] )
            return JS_FALSE;
    }
    if( 0 != p[i] )
        return JS_FALSE;
    return JS_TRUE;
}    
#endif /* LIVEWIRE */

static JSDScript*
_newJSDScript(JSDContext*  jsdc,
              JSContext    *cx,
              JSScript     *script,
              JSFunction*  function)
{
    JSDScript*  jsdscript;
    uintN     lineno;
    const char* raw_filename;

    JS_ASSERT(JSD_SCRIPTS_LOCKED(jsdc));

    /* these are inlined javascript: urls and we can't handle them now */
    lineno = (uintN) JS_GetScriptBaseLineNumber(cx, script);
    if( lineno == 0 )
        return NULL;

    jsdscript = (JSDScript*) calloc(1, sizeof(JSDScript));
    if( ! jsdscript )
        return NULL;

    raw_filename = JS_GetScriptFilename(cx,script);
    
    JS_APPEND_LINK(&jsdscript->links, &jsdc->scripts);
    jsdscript->jsdc         = jsdc;
    jsdscript->script       = script;        
    jsdscript->function     = function;
    jsdscript->lineBase     = lineno;
    jsdscript->lineExtent   = (uintN)NOT_SET_YET;
    jsdscript->data         = NULL;
#ifndef LIVEWIRE
    jsdscript->url          = (char*) jsd_BuildNormalizedURL(raw_filename);
#else
    jsdscript->app = LWDBG_GetCurrentApp();    
    if( jsdscript->app && raw_filename )
    {
        jsdscript->url = jsdlw_BuildAppRelativeFilename(jsdscript->app, raw_filename);
        if( function )
        {
            jsdscript->lwscript = 
                LWDBG_GetScriptOfFunction(jsdscript->app,
                                          JS_GetFunctionName(function));
    
            /* also, make sure this file is added to filelist if is .js file */
            if( HasFileExtention(raw_filename,"js") || 
                HasFileExtention(raw_filename,"sjs") )
            {
                jsdlw_PreLoadSource(jsdc, jsdscript->app, raw_filename, JS_FALSE);
            }
        }
        else
        {
            jsdscript->lwscript = LWDBG_GetCurrentTopLevelScript();
        }
    }
#endif

    JS_INIT_CLIST(&jsdscript->hooks);
    
    return jsdscript;
}           

static void 
_destroyJSDScript(JSDContext*  jsdc,
                  JSDScript*   jsdscript)
{
    JS_ASSERT(JSD_SCRIPTS_LOCKED(jsdc));

    /* destroy all hooks */
    jsd_ClearAllExecutionHooksForScript(jsdc, jsdscript);

    JS_REMOVE_LINK(&jsdscript->links);
    if(jsdscript->url)
        free(jsdscript->url);

    if(jsdscript)
        free(jsdscript);
}

/***************************************************************************/

#ifdef JSD_DUMP
#ifndef XP_WIN
void
OutputDebugString (char *buf)
{
    fprintf (stderr, "%s", buf);
}
#endif

static void
_dumpJSDScript(JSDContext* jsdc, JSDScript* jsdscript, const char* leadingtext)
{
    const char* name;
    const char* fun;
    uintN base;
    uintN extent;
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
_dumpJSDScriptList( JSDContext* jsdc )
{
    JSDScript* iterp = NULL;
    JSDScript* jsdscript = NULL;
    
    OutputDebugString( "*** JSDScriptDump\n" );
    while( NULL != (jsdscript = jsd_IterateScripts(jsdc, &iterp)) )
        _dumpJSDScript( jsdc, jsdscript, "  script: " );
}
#endif /* JSD_DUMP */

/***************************************************************************/
void 
jsd_DestroyAllJSDScripts( JSDContext* jsdc )
{
    JSDScript *jsdscript;
    JSDScript *next;

    JS_ASSERT(JSD_SCRIPTS_LOCKED(jsdc));

    for( jsdscript = (JSDScript*)jsdc->scripts.next;
         jsdscript != (JSDScript*)&jsdc->scripts;
         jsdscript = next )
    {
        next = (JSDScript*)jsdscript->links.next;
        _destroyJSDScript( jsdc, jsdscript );
    }
}

JSDScript*
jsd_FindJSDScript( JSDContext*  jsdc,
                   JSScript     *script )
{
    JSDScript *jsdscript;

    JS_ASSERT(JSD_SCRIPTS_LOCKED(jsdc));

    for( jsdscript = (JSDScript *)jsdc->scripts.next;
         jsdscript != (JSDScript *)&jsdc->scripts;
         jsdscript = (JSDScript *)jsdscript->links.next )
    {
        if (jsdscript->script == script)
            return jsdscript;
    }
    return NULL;
}               

JSDScript*
jsd_IterateScripts(JSDContext* jsdc, JSDScript **iterp)
{
    JSDScript *jsdscript = *iterp;
    
    JS_ASSERT(JSD_SCRIPTS_LOCKED(jsdc));

    if( !jsdscript )
        jsdscript = (JSDScript *)jsdc->scripts.next;
    if( jsdscript == (JSDScript *)&jsdc->scripts )
        return NULL;
    *iterp = (JSDScript*) jsdscript->links.next;
    return jsdscript;
}

void *
jsd_SetScriptPrivate(JSDScript *jsdscript, void *data)
{
    void *rval = jsdscript->data;
    jsdscript->data = data;
    return rval;
}

void *
jsd_GetScriptPrivate(JSDScript *jsdscript)
{
    return jsdscript->data;
}

JSBool
jsd_IsActiveScript(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSDScript *current;

    JS_ASSERT(JSD_SCRIPTS_LOCKED(jsdc));

    for( current = (JSDScript *)jsdc->scripts.next;
         current != (JSDScript *)&jsdc->scripts;
         current = (JSDScript *)current->links.next )
    {
        if(jsdscript == current)
            return JS_TRUE;
    }
    return JS_FALSE;
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

uintN
jsd_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript)
{
    return jsdscript->lineBase;
}

uintN
jsd_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript)
{
    if( NOT_SET_YET == (int)jsdscript->lineExtent )
        jsdscript->lineExtent = JS_GetScriptLineExtent(jsdc->dumbContext, jsdscript->script);
    return jsdscript->lineExtent;
}

jsuword
jsd_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, uintN line)
{
#ifdef LIVEWIRE
    if( jsdscript && jsdscript->lwscript )
    {
        uintN newline;
        jsdlw_RawToProcessedLineNumber(jsdc, jsdscript, line, &newline);
        if( line != newline )
            line = newline;
    }
#endif

    return (jsuword) JS_LineNumberToPC(jsdc->dumbContext, 
                                        jsdscript->script, line );
}

uintN
jsd_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, jsuword pc)
{
    uintN first = jsdscript->lineBase;
    uintN last = first + jsd_GetScriptLineExtent(jsdc, jsdscript) - 1;
    uintN line = JS_PCToLineNumber(jsdc->dumbContext, 
                                     jsdscript->script, (jsbytecode*)pc);

    if( line < first )
        return first;
    if( line > last )
        return last;

#ifdef LIVEWIRE
    if( jsdscript && jsdscript->lwscript )
    {
        uintN newline;
        jsdlw_ProcessedToRawLineNumber(jsdc, jsdscript, line, &newline);
        line = newline;
    }
#endif

    return line;    
}

JSBool
jsd_SetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc hook, void* callerdata)
{
    JSD_LOCK();
    jsdc->scriptHook = hook;
    jsdc->scriptHookData = callerdata;
    JSD_UNLOCK();
    return JS_TRUE;
}

JSBool
jsd_GetScriptHook(JSDContext* jsdc, JSD_ScriptHookProc* hook, void** callerdata)
{
    JSD_LOCK();
    if( hook )
        *hook = jsdc->scriptHook;
    if( callerdata )
        *callerdata = jsdc->scriptHookData;
    JSD_UNLOCK();
    return JS_TRUE;
}    

/***************************************************************************/

void JS_DLL_CALLBACK
jsd_NewScriptHookProc( 
                JSContext   *cx,
                const char  *filename,      /* URL this script loads from */
                uintN       lineno,         /* line where this script starts */
                JSScript    *script,
                JSFunction  *fun,                
                void*       callerdata )
{
    JSDScript* jsdscript = NULL;
    JSDContext* jsdc = (JSDContext*) callerdata;
    JSD_ScriptHookProc      hook;
    void*                   hookData;
    
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    if( JSD_IS_DANGEROUS_THREAD(jsdc) )
        return;
    
#ifdef LIVEWIRE
    if( 1 == lineno )
        jsdlw_PreLoadSource(jsdc, LWDBG_GetCurrentApp(), filename, JS_TRUE );
#endif
    
    JSD_LOCK_SCRIPTS(jsdc);
    jsdscript = _newJSDScript(jsdc, cx, script, fun);
    JSD_UNLOCK_SCRIPTS(jsdc);
    if( ! jsdscript )
        return;

#ifdef JSD_DUMP
    JSD_LOCK_SCRIPTS(jsdc);
    _dumpJSDScript(jsdc, jsdscript, "***NEW Script: ");
    _dumpJSDScriptList( jsdc );
    JSD_UNLOCK_SCRIPTS(jsdc);
#endif /* JSD_DUMP */

    /* local in case jsdc->scriptHook gets cleared on another thread */
    JSD_LOCK();
    hook = jsdc->scriptHook;
    hookData = jsdc->scriptHookData;
    JSD_UNLOCK();

    if( hook )
        hook(jsdc, jsdscript, JS_TRUE, hookData);
}                

void JS_DLL_CALLBACK
jsd_DestroyScriptHookProc( 
                JSContext   *cx,
                JSScript    *script,
                void*       callerdata )
{
    JSDScript* jsdscript = NULL;
    JSDContext* jsdc = (JSDContext*) callerdata;
    JSD_ScriptHookProc      hook;
    void*                   hookData;
    
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    if( JSD_IS_DANGEROUS_THREAD(jsdc) )
        return;
    
    JSD_LOCK_SCRIPTS(jsdc);
    jsdscript = jsd_FindJSDScript(jsdc, script);
    JSD_UNLOCK_SCRIPTS(jsdc);

    if( ! jsdscript )
        return;

#ifdef JSD_DUMP
    JSD_LOCK_SCRIPTS(jsdc);
    _dumpJSDScript(jsdc, jsdscript, "***DESTROY Script: ");
    JSD_UNLOCK_SCRIPTS(jsdc);
#endif /* JSD_DUMP */

    /* local in case hook gets cleared on another thread */
    JSD_LOCK();
    hook = jsdc->scriptHook;
    hookData = jsdc->scriptHookData;
    JSD_UNLOCK();

    if( hook )
        hook(jsdc, jsdscript, JS_FALSE, hookData);

    JSD_LOCK_SCRIPTS(jsdc);
    _destroyJSDScript(jsdc, jsdscript);
    JSD_UNLOCK_SCRIPTS(jsdc);

#ifdef JSD_DUMP
    JSD_LOCK_SCRIPTS(jsdc);
    _dumpJSDScriptList(jsdc);
    JSD_UNLOCK_SCRIPTS(jsdc);
#endif /* JSD_DUMP */
}                


/***************************************************************************/

static JSDExecHook*
_findHook(JSDContext* jsdc, JSDScript* jsdscript, jsuword pc)
{
    JSDExecHook* jsdhook;
    JSCList* list = &jsdscript->hooks;

    for( jsdhook = (JSDExecHook*)list->next;
         jsdhook != (JSDExecHook*)list;
         jsdhook = (JSDExecHook*)jsdhook->links.next )
    {
        if (jsdhook->pc == pc)
            return jsdhook;
    }
    return NULL;
}

static JSBool
_isActiveHook(JSDContext* jsdc, JSScript *script, JSDExecHook* jsdhook)
{
    JSDExecHook* current;
    JSCList* list;
    JSDScript* jsdscript;

    JSD_LOCK_SCRIPTS(jsdc);
    jsdscript = jsd_FindJSDScript(jsdc, script);
    if( ! jsdscript)
    {
        JSD_UNLOCK_SCRIPTS(jsdc);
        return JS_FALSE;
    }

    list = &jsdscript->hooks;

    for( current = (JSDExecHook*)list->next;
         current != (JSDExecHook*)list;
         current = (JSDExecHook*)current->links.next )
    {
        if(current == jsdhook)
        {
            JSD_UNLOCK_SCRIPTS(jsdc);
            return JS_TRUE;
        }
    }
    JSD_UNLOCK_SCRIPTS(jsdc);
    return JS_FALSE;
}


JSTrapStatus JS_DLL_CALLBACK
jsd_TrapHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                void *closure)
{
    JSDExecHook* jsdhook = (JSDExecHook*) JSVAL_TO_PRIVATE(((jsval)closure));
    JSD_ExecutionHookProc hook;
    void* hookData;
    JSDContext*  jsdc;
    JSDScript* jsdscript;

    JSD_LOCK();

    if( NULL == (jsdc = jsd_JSDContextForJSContext(cx)) ||
        ! _isActiveHook(jsdc, script, jsdhook) )
    {
        JSD_UNLOCK();
        return JSTRAP_CONTINUE;
    }

    JSD_ASSERT_VALID_EXEC_HOOK(jsdhook);
    JS_ASSERT(jsdhook->pc == (jsuword)pc);
    JS_ASSERT(jsdhook->jsdscript->script == script);
    JS_ASSERT(jsdhook->jsdscript->jsdc == jsdc);

    hook = jsdhook->hook;
    hookData = jsdhook->callerdata;
    jsdscript = jsdhook->jsdscript;

    /* do not use jsdhook-> after this point */
    JSD_UNLOCK();

    if( ! jsdc || ! jsdc->inited )
        return JSTRAP_CONTINUE;

    if( JSD_IS_DANGEROUS_THREAD(jsdc) )
        return JSTRAP_CONTINUE;

#ifdef LIVEWIRE
    if( ! jsdlw_UserCodeAtPC(jsdc, jsdscript, (jsuword)pc) )
        return JSTRAP_CONTINUE;
#endif

    return jsd_CallExecutionHook(jsdc, cx, JSD_HOOK_BREAKPOINT,
                                 hook, hookData, rval);
}



JSBool
jsd_SetExecutionHook(JSDContext*           jsdc, 
                     JSDScript*            jsdscript,
                     jsuword               pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata)
{
    JSDExecHook* jsdhook;

    JSD_LOCK();
    if( ! hook )
    {
        jsd_ClearExecutionHook(jsdc, jsdscript, pc);
        JSD_UNLOCK();
        return JS_TRUE;
    }

    jsdhook = _findHook(jsdc, jsdscript, pc);
    if( jsdhook )
    {
        jsdhook->hook       = hook;
        jsdhook->callerdata = callerdata;
        return JS_TRUE;
    }
    /* else... */

    jsdhook = (JSDExecHook*)calloc(1, sizeof(JSDExecHook));
    if( ! jsdhook )
        return JS_FALSE;
    jsdhook->jsdscript  = jsdscript;
    jsdhook->pc         = pc;
    jsdhook->hook       = hook;
    jsdhook->callerdata = callerdata;

    if( ! JS_SetTrap(jsdc->dumbContext, jsdscript->script, 
                     (jsbytecode*)pc, jsd_TrapHandler, 
                     (void*) PRIVATE_TO_JSVAL(jsdhook)) )
    {
        free(jsdhook);
        return JS_FALSE;
    }

    JS_APPEND_LINK(&jsdhook->links, &jsdscript->hooks);
    JSD_UNLOCK();

    return JS_TRUE;
}

JSBool
jsd_ClearExecutionHook(JSDContext*           jsdc, 
                       JSDScript*            jsdscript,
                       jsuword               pc)
{
    JSDExecHook* jsdhook;

    JSD_LOCK();

    jsdhook = _findHook(jsdc, jsdscript, pc);
    if( ! jsdhook )
    {
        JS_ASSERT(0);
        JSD_UNLOCK();
        return JS_FALSE;
    }

    JS_ClearTrap(jsdc->dumbContext, jsdscript->script, 
                 (jsbytecode*)pc, NULL, NULL );

    JS_REMOVE_LINK(&jsdhook->links);
    free(jsdhook);

    JSD_UNLOCK();
    return JS_TRUE;
}

JSBool
jsd_ClearAllExecutionHooksForScript(JSDContext* jsdc, JSDScript* jsdscript)
{
    JSDExecHook* jsdhook;
    JSCList* list = &jsdscript->hooks;

    JSD_LOCK();

    while( (JSDExecHook*)list != (jsdhook = (JSDExecHook*)list->next) )
    {
        JS_REMOVE_LINK(&jsdhook->links);
        free(jsdhook);
    }

    JS_ClearScriptTraps(jsdc->dumbContext, jsdscript->script);
    JSD_UNLOCK();

    return JS_TRUE;
}

JSBool
jsd_ClearAllExecutionHooks(JSDContext* jsdc)
{
    JSDScript* jsdscript;
    JSDScript* iterp = NULL;

    JSD_LOCK();
    while( NULL != (jsdscript = jsd_IterateScripts(jsdc, &iterp)) )
        jsd_ClearAllExecutionHooksForScript(jsdc, jsdscript);
    JSD_UNLOCK();
    return JS_TRUE;
}

void
jsd_ScriptCreated(JSDContext* jsdc,
                  JSContext   *cx,
                  const char  *filename,    /* URL this script loads from */
                  uintN       lineno,       /* line where this script starts */
                  JSScript    *script,
                  JSFunction  *fun)
{
    jsd_NewScriptHookProc(cx, filename, lineno, script, fun, jsdc);
}

void
jsd_ScriptDestroyed(JSDContext* jsdc,
                    JSContext   *cx,
                    JSScript    *script)
{
    jsd_DestroyScriptHookProc(cx, script, jsdc);
}
