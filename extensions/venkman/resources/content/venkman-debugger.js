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

var dbg;

const JSD_CTRID = "@mozilla.org/js/jsd/debugger-service;1";
const jsdIDebuggerService = Components.interfaces.jsdIDebuggerService;
const jsdIExecutionHook = Components.interfaces.jsdIExecutionHook;

var scriptHooker = new Object();
scriptHooker.onScriptHook =
function sh_scripthook (cx, script, creating)
{
    dd ("onScriptHook (" + cx + ", " + script + ", " + creating + ")");
}

var interruptHooker = new Object();
interruptHooker.onExecute =
function ih_exehook (cx, state, type, rv)
{
    dd ("onInterruptHook (" + cx + ", " + state + ", " + type + ")");
    return jsdIExecutionHook.HOOK_RETURN_CONTINUE;
}

var debuggerHooker = new Object();
debuggerHooker.onExecute =
function dh_exehook (cx, state, type, rv)
{
    dd ("onDebuggerHook (" + cx + ", " + state + ", " + type + ")");
    return jsdIExecutionHook.HOOK_RETURN_CONTINUE;
}

function initDebugger()
{   
    dbg = Components.classes[JSD_CTRID].getService(jsdIDebuggerService);
    dbg.init();
    dbg.scriptHook = scriptHooker;
    dbg.debuggerHook = debuggerHooker;
    //dbg.interruptHook = interruptHooker;
}
