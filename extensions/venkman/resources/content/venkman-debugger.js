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

console._scriptHook = {
    onScriptCreated: function scripthook (script) {
        addScript (script);

        /* if this script's line extent is 0, the file containing this script is
         * fully loaded, check to see if we need to set any breakpoints */
        if (script.lineExtent == 0)
        {
            if (script.fileName)
                console._scriptsOutlinerView.setScripts(console._scripts);
            for (var i = 0; i < console._futureBreaks.length; ++i)
                if (script.fileName.search(console._futureBreaks[i].filePattern)
                    != -1)
                {
                    setBreakpoint (script.fileName,
                                   console._futureBreaks[i].line);
                }
        }
                
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

function initDebugger()
{   
    console._continueCodeStack = new Array(); /* top of stack is the default */
                                              /* return code for the most    */
                                              /* recent debugTrap().         */
    console._breakpoints = new Array();  /* breakpoints in loaded scripts.   */
    console._futureBreaks = new Array(); /* breakpoints in scripts that we   */
                                         /* don't know about yet.            */
    console._scripts = new Object();     /* scripts, keyed by filename.      */
    console._sources = new Object();     /* source code, keyed by filename.  */
    console._stopLevel = 0;              /* nest level.                      */
    console._scriptCount = 0;            /* active script count.             */
    
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
    
    /* build an array of frames */
    console.frames = new Array(frame);
    while ((frame = frame.callingFrame))
        console.frames.push(frame);

    frame = console.frames[0];
    
    if (showFrame)
        display (formatFrame(frame));
    
    if (sourceContext > -1)
        displaySource (frame.script.fileName, frame.line, sourceContext);
    
    setCurrentFrameByIndex(0);
    console.onDebugTrap()
    
    console.jsds.enterNestedEventLoop(); 

    /* execution pauses here until someone calls 
     * console.dbg.exitNestedEventLoop() 
     */

    if (tn)
        display (getMsg(MSN_CONT, tn), MT_CONT);

    $ = new Array();
    clearCurrentFrame();
    delete console.frames;
    
    return console._continueCodeStack.pop();
}

function getCurrentFrame()
{
    return console.frames[console._currentFrameIndex];
}

function getCurrentFrameIndex()
{
    if (typeof console._currentFrameIndex == "undefined")
        return -1;
    
    return console._currentFrameIndex;
}

function setCurrentFrameByIndex (index)
{
    if (!console.frames)
        throw BadMojo (ERR_NO_STACK);
    
    ASSERT (index >= 0 && index < console.frames.length, "index out of range");

    console._currentFrameIndex = index;
    console.onFrameChanged (console.frames[console._currentFrameIndex],
                            console._currentFrameIndex);
}

function clearCurrentFrame ()
{
    if (!console.frames)
        throw BadMojo (ERR_NO_STACK);

    delete console._currentFrameIndex;
}

function findNextExecutableLine (script, line)
{
    var pc = script.lineToPc (line + 1);
    
}

function formatArguments (v)
{
    var ary = new Array();
    var p = new Object();
    v.getProperties (p, {});
    p = p.value;
    for (var i = 0; i < p.length; ++i)
    {
        if (p[i].flags & jsdIProperty.FLAG_ARGUMENT)
            ary.push (getMsg(MSN_FMT_ARGUMENT,
                             [p[i].name.stringValue,
                              formatValue(p[i].value, true)]));
    }
    
    return ary.join (MSG_COMMASP); 
}

function formatProperty (p)
{
    if (!p)
        throw BadMojo (ERR_REQUIRED_PARAM, "p");

    var flags = p.flags;
    var s = "";
    if (flags & jsdIProperty.FLAG_ENUMERATE)
        s += MSG_VF_ENUMERABLE;
    if (flags & jsdIProperty.FLAG_READONLY)
        s += MSG_VF_READONLY;        
    if (flags & jsdIProperty.FLAG_PERMANENT)
        s += MSG_VF_PERMANENT;
    if (flags & jsdIProperty.FLAG_ALIAS)
        s += MSG_VF_ALIAS;
    if (flags & jsdIProperty.FLAG_ARGUMENT)
        s += MSG_VF_ARGUMENT;
    if (flags & jsdIProperty.FLAG_VARIABLE)
        s += MSG_VF_VARIABLE;
    if (flags & jsdIProperty.FLAG_HINTED)
        s += MSG_VF_HINTED;

    return getMsg(MSN_FMT_PROPERTY, [s, p.name.stringValue,
                                     formatValue(p.value)]);
}

function formatScript (script)
{
    if (!script)
        throw BadMojo (ERR_REQUIRED_PARAM, "script");

    return getMsg (MSN_FMT_SCRIPT, [script.functionName, script.fileName]);
}

function formatFrame (f)
{
    if (!f)
        throw BadMojo (ERR_REQUIRED_PARAM, "f");
 
    return getMsg (MSN_FMT_FRAME,
                   [f.script.functionName, formatArguments(f.scope),
                    f.script.fileName, f.line]);

    return s;
}

function formatValue (v, summary)
{
    if (!v)
        throw BadMojo (ERR_REQUIRED_PARAM, "v");
    
    var type;
    var value;
        
    switch (v.jsType)
    {
        case jsdIValue.TYPE_BOOLEAN:
            type = MSG_TYPE_BOOLEAN;
            value = String(v.booleanValue);
            break;
        case jsdIValue.TYPE_DOUBLE:
            type = MSG_TYPE_DOUBLE;
            value = v.doubleValue;
            break;
        case jsdIValue.TYPE_INT:
            type = MSG_TYPE_INT;
            value = v.intValue;
            break;
        case jsdIValue.TYPE_FUNCTION:
            type = MSG_TYPE_FUNCTION;
            value = v.jsFunctionName;
            break;
        case jsdIValue.TYPE_NULL:
            type = MSG_TYPE_NULL;
            value = MSG_TYPE_NULL;
            break;
        case jsdIValue.TYPE_OBJECT:
            if (!summary)
            {
                type = MSG_TYPE_OBJECT;
                value = getMsg(MSN_FMT_OBJECT, String(v.propertyCount));
            }
            else
            {
                type = (v.jsClassName) ? v.jsClassName : MSG_TYPE_OBJECT;
                value = "{" + String(v.propertyCount) + "}";
            }
            break;
        case jsdIValue.TYPE_STRING:
            type = MSG_TYPE_STRING;
            value = v.stringValue;
            break;
        case jsdIValue.TYPE_VOID:
            type = MSG_TYPE_VOID;
            value = MSG_TYPE_VOID;            
            break;
        default:
            type = MSG_TYPE_UNKNOWN;
            value = MSG_TYPE_UNKNOWN;
            break;
    }

    if (summary)
        return getMsg (MSN_FMT_VALUE_SHORT, [type, value]);

    if (v.jsClassName)
        return getMsg (MSN_FMT_VALUE_LONG, [type, v.jsClassName, value]);

    return getMsg (MSN_FMT_VALUE_MED, [type, value]);

}

function loadSource (url, cb)
{
    var observer = {
        onComplete: function oncomplete (data, url, status) {
            var ary = data.split("\n");
            for (var i = 0; i < ary.length; ++i)
                /* need to use new String here so we can decorate it later */
                ary[i] = new String(ary[i].replace(/\r$/, ""));
            console._sources[url] = ary;
            cb(data, url, status);
        }
    };

    if (url.search (/^(chrome:|file:)/) != -1)
        observer.onComplete (loadURLNow(url), url, Components.results.NS_OK);
    else
        loadURLAsync (url, observer);
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

    display (getMsg(MSN_ERR_BP_NODICE, [fileName, line]), MT_ERROR);
    return 0;
}

function clearBreakpointByNumber (number)
{
    var bp = console._breakpoints[number];
    if (!bp)
    {
        display (getMsg(MSN_ERR_BP_NOINDEX, idx, MT_ERROR));
        return 0;
    }

    var matches = bp.length;
    var fileName = bp.fileName;
    var line = bp.line;
    
    for (var i = 0; i < bp.length; ++i)
        bp[i].script.clearBreakpoint(bp[i].pc);
    arrayRemoveAt (console._breakpoints, number);

    var fileName = bp.fileName;
    var line = bp.line;

    display (getMsg(MSN_BP_DISABLED, [fileName, line, matches]));
    
    if (console._sources[fileName])
        delete console._sources[fileName][line - 1].isBreakpoint;
    else
    {
        function cb (data, url) {
            delete console._sources[fileName][line - 1].isBreakpoint;
        }
        loadSource(fileName, cb);
    }

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

    if (bpList.length)
        display (getMsg(MSN_BP_CREATED,
                        [fileName, line, bpList.length]));
    else
        display (getMsg(MSN_ERR_BP_NOLINE, [fileName, line]),
                 MT_ERROR);
    
    if (bpList.length > 0)
    {
        var found = false;
        
        for (var b in console._breakpoints)
            if (console._breakpoints[b].fileName == fileName &&
                console._breakpoints[b].line == line)
            {
                console._breakpoints[b] = bpList;
                found = true;
                break;
            }

        if (!found)
            console._breakpoints.push (bpList);
    }
    
    if (console._sources[fileName])
        console._sources[fileName][line - 1].isBreakpoint = true;
    else
    {
        function cb (data, url) {
            console._sources[fileName][line - 1].isBreakpoint = true;
        }
        loadSource(fileName, cb);
    }
                
    return bpList;
}

function isFutureBreakpoint (filePattern, line)
{
    for (var b in console._futureBreaks)
        if (console._futureBreaks[b].filePattern == filePattern &&
            console._futureBreaks[b].line == line)
        {
            return true;
        }

    return false;
}
                
function setFutureBreakpoint (filePattern, line)
{
    if (!isFutureBreakpoint(filePattern, line))
    {
        console._futureBreaks.push({filePattern: filePattern, line: line});
        display (getMsg(MSN_FBP_CREATED, [filePattern, line]));
    }

    return true;
}

function clearFutureBreakpoint (filePattern, line)
{
    for (var i = 0; i < console._futureBreaks.length; ++i)
    {
        if (console._futureBreaks[i].filePattern == filePattern &&
            console._futureBreaks[i].line == line)
        {
            return clearFutureBreakpointByNumber(i);
        }
    }
    
    display (getMsg(MSN_ERR_BP_NODICE, [filePattern, line]), MT_ERROR);

    return false;
}

function clearFutureBreakpointByNumber (idx)
{
    var filePattern = console._futureBreaks[idx].filePattern;
    var line = console._futureBreaks[idx].line;
    
    arrayRemoveAt (console._futureBreaks, idx);
    
    display (getMsg(MSN_FBP_DISABLED, [filePattern, line]));

    return true;
}
