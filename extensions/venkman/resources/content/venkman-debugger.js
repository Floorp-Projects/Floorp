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
const jsdIValue = Components.interfaces.jsdIValue;
const jsdIProperty = Components.interfaces.jsdIProperty;

var $ = new Array(); /* array to store results from evals in debug frames */

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
function dh_exehook (cx, state, type, rv)
{
    dd ("onDebuggerHook (" + cx + ", " + state + ", " + type + ")");

    return debugTrap(cx, state, type);
}

function debugTrap (cx, state, type)
{
    ++console.stopLevel;

    /* set our default return value */
    console.continueCodeStack.push (jsdIExecutionHook.RETURN_CONTINUE);

    $ = new Array();
    console.currentContext = cx;
    console.currentThreadState = state;
    console.currentFrameIndex = 0;
    var frame = state.topFrame;
    
    /* build an array of frames */
    console.frames = new Array(frame);
    while ((frame = frame.callingFrame))
        console.frames.push(frame);

    var tn = "";
    switch (type)
    {
        case jsdIExecutionHook.TYPE_BREAKPOINT:
            tn = MSG_WORD_BREAKPOINT;
            break;
        case jsdIExecutionHook.TYPE_DEBUG_REQUESTED:
            tn = MSG_WORD_DEBUG;
            break;
        case jsdIExecutionHook.TYPE_DEBUGGER_KEYWORD:
            tn = MSG_WORD_DEBUGGER;
            break;
        case jsdIExecutionHook.TYPE_THROW:
            tn = MSG_WORD_THROW;
            break;
        default:
            /* don't print stop/cont messages for other types */
    }

    if (tn)
        display (getMsg(MSN_STOP, tn), MT_STOP);

    display (formatStackFrame(console.frames[console.currentFrameIndex]));
    
    console.jsds.enterNestedEventLoop(); 

    /* execution pauses here until someone calls 
     * console.dbg.exitNestedEventLoop() 
     */

    if (tn)
        display (getMsg(MSN_CONT, tn), MT_CONT);

    $ = new Array();
    delete console.currentContext;
    delete console.currentThreadState;
    delete console.currentFrameIndex;
    delete console.frames;
    
    return console.continueCodeStack.pop();
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

function formatProperty (p)
{
    if (!p)
        throw BadMojo (ERR_REQUIRED_PARAM, "p");

    var flags = p.flags;
    var s = "[";
    s += (flags & jsdIProperty.FLAG_ENUMERATE) ? "e" : "-";
    s += (flags & jsdIProperty.FLAG_READONLY)  ? "r" : "-";
    s += (flags & jsdIProperty.FLAG_PERMANENT) ? "p" : "-";
    s += (flags & jsdIProperty.FLAG_ALIAS)     ? "A" : "-";
    s += (flags & jsdIProperty.FLAG_ARGUMENT)  ? "a" : "-";
    s += (flags & jsdIProperty.FLAG_VARIABLE)  ? "v" : "-";
    s += (flags & jsdIProperty.FLAG_HINTED)    ? "h]" : "-]";

    if (p.name)
        s += " " + p.name.stringValue;
    
    if (p.value)
        s += " " + formatValue(p.value);

    return s;
}

function formatScript (scr)
{
    if (!scr)
        throw BadMojo (ERR_REQUIRED_PARAM, "scr");

    return MSG_TYPE_FUNCTION + " " + scr.functionName + " in " + scr.fileName;
}

function formatStackFrame (f)
{
    if (!f)
        throw BadMojo (ERR_REQUIRED_PARAM, "f");

    var s = formatScript (f.script);
    s += " " + MSG_TYPE_LINE + " " + f.line;

    return s;
}

function formatValue (v)
{
    if (!v)
        throw BadMojo (ERR_REQUIRED_PARAM, "v");
    
    var s = "[";
    var value = "";
        
    if (v.isNative)
        s += MSG_TYPE_NATIVE + " ";
    
    switch (v.jsType)
    {
        case jsdIValue.TYPE_DOUBLE:
            s += MSG_TYPE_DOUBLE;
            value = v.doubleValue;
            break;
        case jsdIValue.TYPE_INT:
            s += MSG_TYPE_INT;
            value = v.intValue;
            break;
        case jsdIValue.TYPE_FUNCTION:
            s += MSG_TYPE_FUNCTION;
            value = v.jsFunctionName;
            break;
        case jsdIValue.TYPE_NULL:
            s += MSG_TYPE_NULL;
            break;
        case jsdIValue.TYPE_OBJECT:
            s += MSG_TYPE_OBJECT;
            var pcount = new Object();
            value = String(v.propertyCount) + " " + MSG_TYPE_PROPERTIES;
            break;
        case jsdIValue.TYPE_STRING:
            s += MSG_TYPE_STRING;
            value = v.stringValue;
            break;
        case jsdIValue.TYPE_VOID:
            s += MSG_TYPE_VOID;
            break;
        default:
            s += MSG_TYPE_UNKNOWN;
            break;
    }

    /*
    if (v.isPrimitive)
        s += " (" + MSG_TYPE_PRIMITIVE + ")";
    */
    s += "]";

    if (v.jsClassName)
        s += " [" + MSG_TYPE_CLASS + ": " + v.jsClassName + "]";

    if (value)
        s += " " + value;

    
    return s;
}

function initDebugger()
{   
    console.continueCodeStack = new Array();
    
    /* create the debugger instance */
    console.jsds = Components.classes[JSD_CTRID].getService(jsdIDebuggerService);
    console.jsds.init();
    //console.jsds.scriptHook = console.scriptHooker;
    console.jsds.debuggerHook = console.debuggerHooker;
    //dbg.interruptHook = interruptHooker;
}

function displayCallStack ()
{
    for (i = 0; i < console.frames.length; ++i)
        displayFrame (console.frames[i], i);
}

function displayProperties (v)
{
    if (!v)
        throw BadMojo (ERR_REQUIRED_PARAM, "v");
    
    var p = new Object();
    v.getProperties (p, {});
    for (var i in p.value) display(formatProperty (p.value[i]));
}

function displayFrame (f, idx)
{
    if (!f)
        throw BadMojo (ERR_REQUIRED_PARAM, "f");

    if (typeof idx == "undefined")
    {
        for (idx = 0; idx < console.frames.length; ++idx)
            if (f == console.frames[idx])
                break;
    
        if (idx >= console.frames.length)
            idx = MSG_VAL_UNKNOWN;
    }
    
    if (typeof idx == "number")
        idx = "#" + idx;
    
    display(idx + ": " + formatStackFrame (f));
}
