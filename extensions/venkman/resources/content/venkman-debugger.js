/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is The JavaScript Debugger
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  Robert Ginda, <rginda@netscape.com>, original author
 *
 */

const JSD_CTRID = "@mozilla.org/js/jsd/debugger-service;1";
const jsdIDebuggerService = Components.interfaces.jsdIDebuggerService;
const jsdIExecutionHook = Components.interfaces.jsdIExecutionHook;

console.scriptHooker = new Object();
console.scriptHooker.onScriptLoaded =
function sh_scripthook (cx, script, creating)
{
    dd ("script created (" + creating + "): " + script.fileName + " (" +
        script.functionName + ") " + script.baseLineNumber + "..." + 
        script.lineExtent);
}

console.interruptHooker = new Object();
console.interruptHooker.onExecute =
function ih_exehook (cx, state, type, rv)
{
    dd ("onInterruptHook (" + cx + ", " + state + ", " + type + ")");
    return jsdIExecutionHook.RETURN_CONTINUE;
}

console.debuggerHooker = new Object();
console.debuggerHooker.onExecute =
function dh_exehook (aCx, state, type, rv)
{
    dd ("onDebuggerHook (" + aCx + ", " + state + ", " + type + ")");

    ++console.stopLevel;

    /* set our default return value */
    console.continueCodeStack.push (jsdIExecutionHook.RETURN_CONTINUE);

    console.currentContext = aCx;
    console.currentThreadState = state;

    display (getMsg(MSG_STOP_DEBUGGER), MT_STOP);

    console.jsds.enterNestedEventLoop(); 

    /* execution pauses here until someone calls 
     * console.dbg.exitNestedEventLoop() 
     */

    delete console.currentContext;
    delete console.currentThreadState;

    display (getMsg(MSG_CONT_DEBUGGER), MT_CONT);

    return console.continueCodeStack.pop();
}

function initDebugger()
{   
    console.continueCodeStack = new Array();
    
    /* create the debugger instance */
    console.jsds = Components.classes[JSD_CTRID].getService(jsdIDebuggerService);
    console.jsds.init();
    console.jsds.scriptHook = console.scriptHooker;
    console.jsds.debuggerHook = console.debuggerHooker;
    //dbg.interruptHook = interruptHooker;
}

function detachDebugger()
{
    var count = console.stopLevel;
    var i;

    for (i = 0; i < count; ++i)
        console.jsds.exitNestedEventLoop();

    ASSERT (console.stopLevel == 0,
            "console.stopLevel != 0 after detachDebugger");
    
    console.jsds.scriptHook = null;
    console.jsds.debuggerHook = null;
}

console.printCallStack =
function jsd_pcallstack ()
{
    display ();
}
