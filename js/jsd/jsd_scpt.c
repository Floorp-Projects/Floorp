/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JavaScript Debugging support - Script support
 */

#include "jsd.h"
#include "jsfriendapi.h"

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
              JSScript     *script)
{
    JSDScript*  jsdscript;
    unsigned     lineno;
    const char* raw_filename;

    JS_ASSERT(JSD_SCRIPTS_LOCKED(jsdc));

    /* these are inlined javascript: urls and we can't handle them now */
    lineno = (unsigned) JS_GetScriptBaseLineNumber(cx, script);
    if( lineno == 0 )
        return NULL;

    jsdscript = (JSDScript*) calloc(1, sizeof(JSDScript));
    if( ! jsdscript )
        return NULL;

    raw_filename = JS_GetScriptFilename(cx,script);

    JS_HashTableAdd(jsdc->scriptsTable, (void *)script, (void *)jsdscript);
    JS_APPEND_LINK(&jsdscript->links, &jsdc->scripts);
    jsdscript->jsdc         = jsdc;
    jsdscript->script       = script;  
    jsdscript->lineBase     = lineno;
    jsdscript->lineExtent   = (unsigned)NOT_SET_YET;
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
            JSString* funid = JS_GetFunctionId(function);
            char* funbytes;
            const char* funnanme;
            if( fuinid )
            {
                funbytes = JS_EncodeString(cx, funid);
                funname = funbytes ? funbytes : "";
            }
            else
            {
                funbytes = NULL;
                funname = "anonymous";
            }
            jsdscript->lwscript = 
                LWDBG_GetScriptOfFunction(jsdscript->app,funname);
            JS_Free(cx, funbytes);
    
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

    if (jsdscript->profileData)
        free(jsdscript->profileData);
    
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
    JSString* fun;
    unsigned base;
    unsigned extent;
    char Buf[256];
    size_t n;

    name   = jsd_GetScriptFilename(jsdc, jsdscript);
    fun    = jsd_GetScriptFunctionId(jsdc, jsdscript);
    base   = jsd_GetScriptBaseLineNumber(jsdc, jsdscript);
    extent = jsd_GetScriptLineExtent(jsdc, jsdscript);
    n = size_t(snprintf(Buf, sizeof(Buf), "%sscript=%08X, %s, ",
                        leadingtext, (unsigned) jsdscript->script,
                        name ? name : "no URL"));
    if (n + 1 < sizeof(Buf)) {
        if (fun) {
            n += size_t(snprintf(Buf + n, sizeof(Buf) - n, "%s", "no fun"));
        } else {
            n += JS_PutEscapedFlatString(Buf + n, sizeof(Buf) - n,
                                         JS_ASSERT_STRING_IS_FLAT(fun), 0);
            Buf[sizeof(Buf) - 1] = '\0';
        }
        if (n + 1 < sizeof(Buf))
            snprintf(Buf + n, sizeof(Buf) - n, ", %d-%d\n", base, base + extent - 1);
    }
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
static JSHashNumber
jsd_hash_script(const void *key)
{
    return ((JSHashNumber)(ptrdiff_t) key) >> 2; /* help lame MSVC1.5 on Win16 */
}

static void *
jsd_alloc_script_table(void *priv, size_t size)
{
    return malloc(size);
}

static void
jsd_free_script_table(void *priv, void *item, size_t size)
{
    free(item);
}

static JSHashEntry *
jsd_alloc_script_entry(void *priv, const void *item)
{
    return (JSHashEntry*) malloc(sizeof(JSHashEntry));
}

static void
jsd_free_script_entry(void *priv, JSHashEntry *he, unsigned flag)
{
    if (flag == HT_FREE_ENTRY)
    {
        _destroyJSDScript((JSDContext*) priv, (JSDScript*) he->value);
        free(he);
    }
}

static JSHashAllocOps script_alloc_ops = {
    jsd_alloc_script_table, jsd_free_script_table,
    jsd_alloc_script_entry, jsd_free_script_entry
};

#ifndef JSD_SCRIPT_HASH_SIZE
#define JSD_SCRIPT_HASH_SIZE 1024
#endif

JSBool
jsd_InitScriptManager(JSDContext* jsdc)
{
    JS_INIT_CLIST(&jsdc->scripts);
    jsdc->scriptsTable = JS_NewHashTable(JSD_SCRIPT_HASH_SIZE, jsd_hash_script,
                                         JS_CompareValues, JS_CompareValues,
                                         &script_alloc_ops, (void*) jsdc);
    return !!jsdc->scriptsTable;
}

void
jsd_DestroyScriptManager(JSDContext* jsdc)
{
    JSD_LOCK_SCRIPTS(jsdc);
    if (jsdc->scriptsTable)
        JS_HashTableDestroy(jsdc->scriptsTable);
    JSD_UNLOCK_SCRIPTS(jsdc);
}

JSDScript*
jsd_FindJSDScript( JSDContext*  jsdc,
                   JSScript     *script )
{
    JS_ASSERT(JSD_SCRIPTS_LOCKED(jsdc));
    return (JSDScript*) JS_HashTableLookup(jsdc->scriptsTable, (void *)script);
}

JSDScript *
jsd_FindOrCreateJSDScript(JSDContext    *jsdc,
                          JSContext     *cx,
                          JSScript      *script,
                          JSStackFrame  *fp)
{
    JSDScript *jsdscript;
    JS_ASSERT(JSD_SCRIPTS_LOCKED(jsdc));

    jsdscript = jsd_FindJSDScript(jsdc, script);
    if (jsdscript)
        return jsdscript;

    /* Fallback for unknown scripts: create a new script. */
    if (!fp)
        JS_FrameIterator(cx, &fp);
    if (fp)
        jsdscript = _newJSDScript(jsdc, cx, script);

    return jsdscript;
}

JSDProfileData*
jsd_GetScriptProfileData(JSDContext* jsdc, JSDScript *script)
{
    if (!script->profileData)
        script->profileData = (JSDProfileData*)calloc(1, sizeof(JSDProfileData));

    return script->profileData;
}

uint32_t
jsd_GetScriptFlags(JSDContext *jsdc, JSDScript *script)
{
    return script->flags;
}

void
jsd_SetScriptFlags(JSDContext *jsdc, JSDScript *script, uint32_t flags)
{
    script->flags = flags;
}

unsigned
jsd_GetScriptCallCount(JSDContext* jsdc, JSDScript *script)
{
    if (script->profileData)
        return script->profileData->callCount;

    return 0;
}

unsigned
jsd_GetScriptMaxRecurseDepth(JSDContext* jsdc, JSDScript *script)
{
    if (script->profileData)
        return script->profileData->maxRecurseDepth;

    return 0;
}

double
jsd_GetScriptMinExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    if (script->profileData)
        return script->profileData->minExecutionTime;

    return 0.0;
}

double
jsd_GetScriptMaxExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    if (script->profileData)
        return script->profileData->maxExecutionTime;

    return 0.0;
}

double
jsd_GetScriptTotalExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    if (script->profileData)
        return script->profileData->totalExecutionTime;

    return 0.0;
}

double
jsd_GetScriptMinOwnExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    if (script->profileData)
        return script->profileData->minOwnExecutionTime;

    return 0.0;
}

double
jsd_GetScriptMaxOwnExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    if (script->profileData)
        return script->profileData->maxOwnExecutionTime;

    return 0.0;
}

double
jsd_GetScriptTotalOwnExecutionTime(JSDContext* jsdc, JSDScript *script)
{
    if (script->profileData)
        return script->profileData->totalOwnExecutionTime;

    return 0.0;
}

void
jsd_ClearScriptProfileData(JSDContext* jsdc, JSDScript *script)
{
    if (script->profileData)
    {
        free(script->profileData);
        script->profileData = NULL;
    }
}    

JSScript *
jsd_GetJSScript (JSDContext *jsdc, JSDScript *script)
{
    return script->script;
}

JSFunction *
jsd_GetJSFunction (JSDContext *jsdc, JSDScript *script)
{
    return JS_GetScriptFunction(jsdc->dumbContext, script->script);
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

JSString*
jsd_GetScriptFunctionId(JSDContext* jsdc, JSDScript *jsdscript)
{
    JSString* str;
    JSFunction *fun = jsd_GetJSFunction(jsdc, jsdscript);

    if( ! fun )
        return NULL;
    str = JS_GetFunctionId(fun);

    /* For compatibility we return "anonymous", not an empty string here. */
    return str ? str : JS_GetAnonymousString(jsdc->jsrt);
}

unsigned
jsd_GetScriptBaseLineNumber(JSDContext* jsdc, JSDScript *jsdscript)
{
    return jsdscript->lineBase;
}

unsigned
jsd_GetScriptLineExtent(JSDContext* jsdc, JSDScript *jsdscript)
{
    if( NOT_SET_YET == (int)jsdscript->lineExtent )
        jsdscript->lineExtent = JS_GetScriptLineExtent(jsdc->dumbContext, jsdscript->script);
    return jsdscript->lineExtent;
}

uintptr_t
jsd_GetClosestPC(JSDContext* jsdc, JSDScript* jsdscript, unsigned line)
{
    uintptr_t pc;
    JSCrossCompartmentCall *call;

    if( !jsdscript )
        return 0;
#ifdef LIVEWIRE
    if( jsdscript->lwscript )
    {
        unsigned newline;
        jsdlw_RawToProcessedLineNumber(jsdc, jsdscript, line, &newline);
        if( line != newline )
            line = newline;
    }
#endif

    call = JS_EnterCrossCompartmentCallScript(jsdc->dumbContext, jsdscript->script);
    if(!call)
        return 0;
    pc = (uintptr_t) JS_LineNumberToPC(jsdc->dumbContext, jsdscript->script, line );
    JS_LeaveCrossCompartmentCall(call);
    return pc;
}

unsigned
jsd_GetClosestLine(JSDContext* jsdc, JSDScript* jsdscript, uintptr_t pc)
{
    JSCrossCompartmentCall *call;
    unsigned first = jsdscript->lineBase;
    unsigned last = first + jsd_GetScriptLineExtent(jsdc, jsdscript) - 1;
    unsigned line = 0;

    call = JS_EnterCrossCompartmentCallScript(jsdc->dumbContext, jsdscript->script);
    if(!call)
        return 0;
    if (pc)
        line = JS_PCToLineNumber(jsdc->dumbContext, jsdscript->script, (jsbytecode*)pc);
    JS_LeaveCrossCompartmentCall(call);

    if( line < first )
        return first;
    if( line > last )
        return last;

#ifdef LIVEWIRE
    if( jsdscript && jsdscript->lwscript )
    {
        unsigned newline;
        jsdlw_ProcessedToRawLineNumber(jsdc, jsdscript, line, &newline);
        line = newline;
    }
#endif

    return line;    
}

JSBool
jsd_GetLinePCs(JSDContext* jsdc, JSDScript* jsdscript,
               unsigned startLine, unsigned maxLines,
               unsigned* count, unsigned** retLines, uintptr_t** retPCs)
{
    JSCrossCompartmentCall *call;
    unsigned first = jsdscript->lineBase;
    unsigned last = first + jsd_GetScriptLineExtent(jsdc, jsdscript) - 1;
    JSBool ok;
    unsigned *lines;
    jsbytecode **pcs;
    unsigned i;

    if (last < startLine)
        return JS_TRUE;

    call = JS_EnterCrossCompartmentCallScript(jsdc->dumbContext, jsdscript->script);
    if (!call)
        return JS_FALSE;

    ok = JS_GetLinePCs(jsdc->dumbContext, jsdscript->script,
                       startLine, maxLines,
                       count, retLines, &pcs);

    if (ok) {
        if (retPCs) {
            for (i = 0; i < *count; ++i) {
                (*retPCs)[i] = (*pcs)[i];
            }
        }

        JS_free(jsdc->dumbContext, pcs);
    }

    JS_LeaveCrossCompartmentCall(call);
    return ok;
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

JSBool
jsd_EnableSingleStepInterrupts(JSDContext* jsdc, JSDScript* jsdscript, JSBool enable)
{
    JSCrossCompartmentCall *call;
    JSBool rv;
    call = JS_EnterCrossCompartmentCallScript(jsdc->dumbContext, jsdscript->script);
    if(!call)
        return JS_FALSE;
    JSD_LOCK();
    rv = JS_SetSingleStepMode(jsdc->dumbContext, jsdscript->script, enable);
    JSD_UNLOCK();
    JS_LeaveCrossCompartmentCall(call);
    return rv;
}


/***************************************************************************/

void
jsd_NewScriptHookProc( 
                JSContext   *cx,
                const char  *filename,      /* URL this script loads from */
                unsigned       lineno,         /* line where this script starts */
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
    
    JSD_LOCK_SCRIPTS(jsdc);
    jsdscript = _newJSDScript(jsdc, cx, script);
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

void
jsd_DestroyScriptHookProc( 
                JSFreeOp    *fop,
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
    JS_HashTableRemove(jsdc->scriptsTable, (void *)script);
    JSD_UNLOCK_SCRIPTS(jsdc);

#ifdef JSD_DUMP
    JSD_LOCK_SCRIPTS(jsdc);
    _dumpJSDScriptList(jsdc);
    JSD_UNLOCK_SCRIPTS(jsdc);
#endif /* JSD_DUMP */
}                


/***************************************************************************/

static JSDExecHook*
_findHook(JSDContext* jsdc, JSDScript* jsdscript, uintptr_t pc)
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


JSTrapStatus
jsd_TrapHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                jsval closure)
{
    JSDExecHook* jsdhook = (JSDExecHook*) JSVAL_TO_PRIVATE(closure);
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
    JS_ASSERT(!jsdhook->pc || jsdhook->pc == (uintptr_t)pc);
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
    if( ! jsdlw_UserCodeAtPC(jsdc, jsdscript, (uintptr_t)pc) )
        return JSTRAP_CONTINUE;
#endif

    return jsd_CallExecutionHook(jsdc, cx, JSD_HOOK_BREAKPOINT,
                                 hook, hookData, rval);
}



JSBool
jsd_SetExecutionHook(JSDContext*           jsdc, 
                     JSDScript*            jsdscript,
                     uintptr_t             pc,
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata)
{
    JSDExecHook* jsdhook;
    JSBool rv;
    JSCrossCompartmentCall *call;

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
        JSD_UNLOCK();
        return JS_TRUE;
    }
    /* else... */

    jsdhook = (JSDExecHook*)calloc(1, sizeof(JSDExecHook));
    if( ! jsdhook ) {
        JSD_UNLOCK();
        return JS_FALSE;
    }
    jsdhook->jsdscript  = jsdscript;
    jsdhook->pc         = pc;
    jsdhook->hook       = hook;
    jsdhook->callerdata = callerdata;

    call = JS_EnterCrossCompartmentCallScript(jsdc->dumbContext, jsdscript->script);
    if(!call) {
        free(jsdhook);
        JSD_UNLOCK();
        return JS_FALSE;
    }

    rv = JS_SetTrap(jsdc->dumbContext, jsdscript->script, 
                    (jsbytecode*)pc, jsd_TrapHandler,
                    PRIVATE_TO_JSVAL(jsdhook));

    JS_LeaveCrossCompartmentCall(call);

    if ( ! rv ) {
        free(jsdhook);
        JSD_UNLOCK();
        return JS_FALSE;
    }

    JS_APPEND_LINK(&jsdhook->links, &jsdscript->hooks);
    JSD_UNLOCK();

    return JS_TRUE;
}

JSBool
jsd_ClearExecutionHook(JSDContext*           jsdc, 
                       JSDScript*            jsdscript,
                       uintptr_t             pc)
{
    JSCrossCompartmentCall *call;
    JSDExecHook* jsdhook;

    JSD_LOCK();

    jsdhook = _findHook(jsdc, jsdscript, pc);
    if( ! jsdhook )
    {
        JSD_UNLOCK();
        return JS_FALSE;
    }

    call = JS_EnterCrossCompartmentCallScript(jsdc->dumbContext, jsdscript->script);
    if(!call) {
        JSD_UNLOCK();
        return JS_FALSE;
    }

    JS_ClearTrap(jsdc->dumbContext, jsdscript->script, 
                 (jsbytecode*)pc, NULL, NULL );

    JS_LeaveCrossCompartmentCall(call);

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

    /* No cross-compartment call here because we may be in the middle of GC */
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
                  unsigned       lineno,       /* line where this script starts */
                  JSScript    *script,
                  JSFunction  *fun)
{
    jsd_NewScriptHookProc(cx, filename, lineno, script, fun, jsdc);
}

void
jsd_ScriptDestroyed(JSDContext* jsdc,
                    JSFreeOp    *fop,
                    JSScript    *script)
{
    jsd_DestroyScriptHookProc(fop, script, jsdc);
}
