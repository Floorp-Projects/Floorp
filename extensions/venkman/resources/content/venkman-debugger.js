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
console._scripts = new Object();
console._sources = new Object();

console._scriptHook = {
    onScriptCreated: function scripthook (script) {       
        addScript (script);
    },

    onScriptDestroyed: function scripthook (script) {
        removeScript (script);
    }
};

console._executionHook = {
    onExecute: function exehook (frame, type, rv) {
        return debugTrap(frame, type, rv);
    }
};

function insertScriptEntry (script, scriptList)
{
    var newBLine = script.baseLineNumber;
    var newLineE = script.lineExtent;
    var newELine = newBLine + newLineE;
    
    for (i = scriptList.length - 1; i >= 0 ; --i)
    {
        /* scripts for this filename are stored in the scriptList array
         * in lowest-to-highest line number order.  We search backwards to
         * find the correct place to insert the new script, because that's
         * the most likely where it belongs.
         */
        var thisBLine = scriptList[i].baseLineNumber;
        var thisLineE = scriptList[i].lineExtent;
        var thisELine = thisBLine + thisLineE;
        
        if (script == scriptList[i])
        {
            dd ("ignoring duplicate script '" + formatScript(script) + "'.");
            return;
        }
        else if (newBLine >= thisBLine)
        {   /* new script starts after this starts. */
            /*
            if (newBLine == thisBLine && newELine == thisELine)
                dd ("script '" + formatScript(script) + "' loaded again.");
            else if (newBLine <= thisELine && newBLine >= thisBLine)
                dd ("script '" + formatScript(script) + "' is a subScript.");
            */

            arrayInsertAt(scriptList, i + 1, script);
            ++console._scriptCount;
            return;
        }
    }
    
    /* new script is the earliest one yet, insert it at the top of the array */
    arrayInsertAt(scriptList, 0, script);
}

function removeScriptEntry (script, scriptList)
{
    for (var i = 0; i < scriptList.length; ++i)
    {
        if (scriptList[i] == script)
        {
            arrayRemoveAt(scriptList, i);
            --console._scriptCount;
            return true;
        }
    }

    return false;
}
    
function removeScript (script)
{
    /* man this sucks, we can't ask the script any questions that would help us
     * find it (fileName, lineNumber), because the underlying JSDScript has
     * already been destroyed.  Instead, we have to walk all active scripts to
     * find it.
     */

    for (var s in console._scripts)
        if (removeScriptEntry(script, console._scripts[s]))
            return true;

    for (var b in console._breakpoints)
        for (s in console._breakpoints[b])
            if (console._breakpoints[b][s].script == script)
                if (console._breakpoints[b].length > 1)
                    arrayRemoveAt (console._breakpoints[b], s);
                else
                    delete console._breakpoints[b];
    
    return false;    
}

function addScript(script)
{
    var scriptList = console._scripts[script.fileName];
    if (!scriptList)
    {
        /* no scripts under this filename yet, create a new record for it */
        console._scripts[script.fileName] = [script];
    }
    else
        insertScriptEntry (script, scriptList);
}

function debugTrap (frame, type, rv)
{
    var tn = "";
    var showFrame = true;
    var sourceContext = 2;
    
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
        case jsdIExecutionHook.TYPE_INTERRUPTED:
            if (console._stepPast == frame.script.fileName + frame.line)
                return jsdIExecutionHook.RETURN_CONTINUE;
            delete console._stepPast;
            showFrame = false;
            sourceContext = 0;
            console.jsds.interruptHook = null;
            break;
        default:
            /* don't print stop/cont messages for other types */
    }

    if (tn)
        display (getMsg(MSN_STOP, tn), MT_STOP);

    ++console._stopLevel;

    /* set our default return value */
    console._continueCodeStack.push (jsdIExecutionHook.RETURN_CONTINUE);

    $ = new Array();
    console.currentFrameIndex = 0;
    
    /* build an array of frames */
    console.frames = new Array(frame);
    while ((frame = frame.callingFrame))
        console.frames.push(frame);

    frame = console.frames[0];
    
    if (showFrame)
        display (formatFrame(frame));
    
    if (sourceContext > -1)
        displaySource (frame.script.fileName, frame.line, sourceContext);
    
    console.jsds.enterNestedEventLoop(); 

    /* execution pauses here until someone calls 
     * console.dbg.exitNestedEventLoop() 
     */

    if (tn)
        display (getMsg(MSN_CONT, tn), MT_CONT);

    $ = new Array();
    delete console.currentFrameIndex;
    delete console.frames;
    
    return console._continueCodeStack.pop();
}

function detachDebugger()
{
    var count = console._stopLevel;
    var i;

    for (i = 0; i < count; ++i)
        console.jsds.exitNestedEventLoop();

    ASSERT (console._stopLevel == 0,
            "console.stopLevel != 0 after detachDebugger");

    console.jsds.clearAllBreakpoints();
    console.jsds.scriptHook = null;
    console.jsds.debuggerHook = null;
    console.jsds.off();
}

function findNextExecutableLine (script, line)
{
    var pc = script.lineToPc (line + 1);
    
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

function formatFrame (f)
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
        case jsdIValue.TYPE_BOOLEAN:
            s += MSG_TYPE_BOOLEAN;
            value = String(v.booleanValue);
            break;
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

function loadSource (url, cb)
{
    var observer = {
        onComplete: function oncomplete (data, url, status) {
            console._sources[url] = data.split("\n");
            cb(data, url, status);
        }
    };

    if (url.search (/^(chrome:|file:)/) != -1)
    {
        console._sources[url] = loadURLNow(url).split("\n");
        cb (console._sources[url], url, Components.results.NS_OK);
    }
    else
        loadURLAsync (url, observer);
}
        
function initDebugger()
{   
    console._continueCodeStack = new Array();
    console._breakpoints = new Array();
    console._stopLevel = 0;
    console._scriptCount = 0;
    
    /* create the debugger instance */
    if (!Components.classes[JSD_CTRID])
        throw BadMojo (ERR_NO_DEBUGGER);
    
    console.jsds = Components.classes[JSD_CTRID].getService(jsdIDebuggerService);
    console.jsds.on();
    console.jsds.breakpointHook = console._executionHook;
    console.jsds.debuggerHook = console._executionHook;
    console.jsds.scriptHook = console._scriptHook;
    //dbg.interruptHook = interruptHooker;

    var enumer = new Object();
    enumer.enumerateScript = function (script)
    {
        addScript(script);
        return true;
    }
    console.jsds.enumerateScripts(enumer);
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

    if (!(v instanceof jsdIValue))
        throw BadMojo (ERR_INVALID_PARAM, "v", String(v));

    var p = new Object();
    v.getProperties (p, {});
    for (var i in p.value) display(formatProperty (p.value[i]));
}

function displaySource (url, line, contextLines)
{
    function onSourceLoaded (data, url, status)
    {
        if (status == Components.results.NS_OK)
            displaySource (url, line, contextLines);
        else
            display (getMsg(MSN_ERR_SOURCE_LOAD_FAILED, url), MT_ERROR);
    }
    
    if (!url)
    {
        display (MSG_ERR_NO_SOURCE, MT_ERROR);
        return;
    }
    
    var source = console._sources[url];
    if (source)
        for (var i = line - contextLines; i <= line + contextLines; ++i)
            display (getMsg(MSN_SOURCE_LINE, [zeroPad (i, 3), source[i - 1]]),
                     i == line ? MT_STEP : MT_SOURCE);
    else
        loadSource (url, onSourceLoaded);
}
    
function displayFrame (f, idx, showSource)
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
    
    display(idx + ": " + formatFrame (f));
    if (showSource)
            displaySource (f.script.fileName, f.line, 2);
}

function getBreakpoint (fileName, line)
{
    for (var b in console._breakpoints)
        if (console._breakpoints[b].fileName == fileName &&
            console._breakpoints[b].line == line)
            return console._breakpoints[b];
    return null;
}

function clearBreakpoint (fileName, line)
{
    for (var b in console._breakpoints)
        if (console._breakpoints[b].fileName == fileName &&
            console._breakpoints[b].line == line)
            return clearBreakpointByNumber (Number(b));

    return 0;
}

function clearBreakpointByNumber (number)
{
    var bp = console._breakpoints[number];
    if (!bp)
        return 0;

    var matches = bp.length;
    for (var i = 0; i < bp.length; ++i)
        bp[i].script.clearBreakpoint(bp[i].pc);
    arrayRemoveAt (console._breakpoints, number);
    return matches;
}
    
function setBreakpoint (fileName, line)
{
    var ary = console._scripts[fileName];
    
    if (!ary)
    {
        display (getMsg(MSN_ERR_NOSCRIPT, fileName), MT_ERROR);
        return false;
    }
    
    var bpList = new Array();
    
    for (var i = 0; i < ary.length; ++i)
    {
        if (ary[i].baseLineNumber <= line && 
            ary[i].baseLineNumber + ary[i].lineExtent >= line)
        {
            var pc = ary[i].lineToPc(line);
            ary[i].setBreakpoint(pc);
            bpList.push ({fileName: fileName, script: ary[i], line: line,
                          pc: pc});
        }
    }

    bpList.fileName = fileName;
    bpList.line = line;
    
    if (bpList.length > 0)
        console._breakpoints.push (bpList);
    
    return bpList;
}

    
            
