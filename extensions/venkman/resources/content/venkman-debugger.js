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
const jsdIExecutionHook   = Components.interfaces.jsdIExecutionHook;
const jsdICallHook        = Components.interfaces.jsdICallHook;
const jsdIValue           = Components.interfaces.jsdIValue;
const jsdIProperty        = Components.interfaces.jsdIProperty;
const jsdIScript          = Components.interfaces.jsdIScript;
const jsdIStackFrame      = Components.interfaces.jsdIStackFrame;

const FTYPE_STD     = 0;
const FTYPE_SUMMARY = 1;
const FTYPE_ARRAY   = 2;

var $ = new Array(); /* array to store results from evals in debug frames */

console._scriptHook = {
    onScriptCreated: function scripthook (script) {
                         if (script.fileName && 
                             script.fileName != MSG_VAL_CONSOLE)
                         {
                             realizeScript (script);
                         }
                     },

    onScriptDestroyed: function scripthook (script) {
                         if (script.fileName && 
                             script.fileName != MSG_VAL_CONSOLE)
                         {
                             unrealizeScript (script);
                         }
                       }
};

console._executionHook = {
    onExecute: function exehook (frame, type, rv) {
                   if (frame.script && frame.script.fileName != MSG_VAL_CONSOLE)
                       return debugTrap(frame, type, rv);
                   ASSERT (frame.script, "Execution hook called with no script");
                   return jsdIExecutionHook.RETURN_CONTINUE;
               }
};

console._callHook = {
    onCall: function callhook (frame, type) {
                if (type == jsdICallHook.TYPE_FUNCTION_CALL)
                {
                    setStopState(false);
                    ++console._stepOverLevel;
                }
                else if (type == jsdICallHook.TYPE_FUNCTION_RETURN)
                {
                    if (--console._stepOverLevel <= 0)
                    {
                        setStopState(true);
                        console.jsds.functionHook = null;
                    }
                }
                dd ("Call Hook: " + frame.script.functionName + ", type " +
                    type + " callCount: " + console._stepOverLevel);
            }
};

console._errorHook = {
    onExecute: function exehook (frame, type, rv) {
        display ("error hook");
    }
};

function initDebugger()
{   
    console._continueCodeStack = new Array(); /* top of stack is the default  */
                                              /* return code for the most     */
                                              /* recent debugTrap().          */
    console.scripts = new Object();
    console._stopLevel = 0;                   /* nest level.                  */
    
    /* create the debugger instance */
    if (!Components.classes[JSD_CTRID])
        throw new BadMojo (ERR_NO_DEBUGGER);
    
    console.jsds = Components.classes[JSD_CTRID].getService(jsdIDebuggerService);
    console.jsds.on();
    console.jsds.breakpointHook = console._executionHook;
    console.jsds.debuggerHook = console._executionHook;
    console.jsds.errorHook = console._errorHook;
    console.jsds.scriptHook = console._scriptHook;
    
    setThrowMode(TMODE_IGNORE);

    var enumer = {
        enumerateScript: function _es (script) {
            realizeScript(script);
            return true;
        }
    };
    
    console.jsds.enumerateScripts(enumer);

}

function detachDebugger()
{
    console.jsds.topLevelHook = null;
    console.jsds.functionHook = null;
    console.jsds.off();

    if (console._stopLevel > 0)
    {
        --console._stopLevel;
        console.jsds.exitNestedEventLoop();
    }

}

function realizeScript(script)
{
    var container = console.scripts[script.fileName];
    if (!container)
    {
        container = console.scripts[script.fileName] =
            new SourceRecord (script.fileName);
        container.reserveChildren();
        console.scriptsView.childData.appendChild(container);
    }
    else if (!container.parentRecord)
    {
        /* source record exists but is not inserted in the source tree, either
         * we are reloading it, or the user added it manually */
        console.scriptsView.childData.appendChild(container);
    }
    
    var scriptRec = new ScriptRecord (script);
    container.appendChild (scriptRec);

    /* check to see if this script contains a breakpoint */
    for (var i = 0; i < console.breakpoints.childData.length; ++i)
    {
        var bpr = console.breakpoints.childData[i];
        if (bpr.matchesScriptRecord(scriptRec))
        {
            if (bpr.fileName == scriptRec.script.fileName)
            {
                /* if the names match exactly, piggyback the new script record
                 * on the existing breakpoint record. */
                bpr.addScriptRecord(scriptRec);
            }
            else
            {
                /* otherwise, we only matched a pattern, create a new breakpoint
                 * record. */
                setBreakpoint (scriptRec.script.fileName, bpr.line);
            }
        }
    }    
}

function unrealizeScript(script)
{
    var container = console.scripts[script.fileName];
    if (!container)
    {
        dd ("unrealizeScript: unknown file ``" + 
            script.fileName + "''");
        return;
    }

    var scriptRec = container.locateChildByScript (script);
    if (!scriptRec)
    {
        dd ("unrealizeScript: unable to locate script record");
        return;
    }
    
    var bplist = console.breakpoints.childData;
    for (var i = 0; i < bplist.length; ++i)
    {
        if (bplist[i].fileName == script.fileName)
        {
            var bpr = bplist[i];
            for (var j = 0; j < bpr.scriptRecords.length; ++j)
            {
                if (bpr.scriptRecords[j] == scriptRec)
                {
                    arrayRemoveAt(bpr.scriptRecords, j);
                    --scriptRec.bpcount;
                    bpr.invalidate();
                }
            }
        }
    }
    
    container.removeChildAtIndex (scriptRec.childIndex);
    if (container.childData.length == 0)
    {
        console.scriptsView.childData.removeChildAtIndex(container.childIndex);
    }
}

const TMODE_IGNORE = 0;
const TMODE_TRACE = 1;
const TMODE_BREAK = 2;

function getThrowMode (tmode)
{
    return console._throwMode;
}

function cycleThrowMode ()
{
    switch (console._throwMode)
    {
        case TMODE_IGNORE:
            setThrowMode(TMODE_TRACE);
            break;            

        case TMODE_TRACE:
            setThrowMode(TMODE_BREAK);
            break;

        case TMODE_BREAK:
            setThrowMode(TMODE_IGNORE);
            break;
    }
}

function setThrowMode (tmode)
{    
    switch (tmode) {
        case TMODE_IGNORE:
            console.jsds.throwHook = null;
            display (MSG_TMODE_IGNORE);
            break;            

        case TMODE_TRACE:
            console.jsds.throwHook = console._executionHook;
            display (MSG_TMODE_TRACE);
            break;

        case TMODE_BREAK:
            console.jsds.throwHook = console._executionHook;
            display (MSG_TMODE_BREAK);
            break;
            
        default:
            throw new BadMojo (ERR_INVALID_PARAM, "tmode");
    }
    
    console._throwMode = tmode;
}

function debugTrap (frame, type, rv)
{
    var tn = "";
    var retcode = jsdIExecutionHook.RETURN_CONTINUE;

    $ = new Array();
    
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
            display (getMsg(MSN_EXCP_TRACE, [formatValue(rv.value),
                                             formatFrame(frame)]), MT_ETRACE);
            if (rv.value.jsClassName == "Error")
                display (formatProperty(rv.value.getProperty("message")));

            if (console._throwMode != TMODE_BREAK)
                return jsdIExecutionHook.RETURN_CONTINUE_THROW;

            $[0] = rv.value;
            retcode = jsdIExecutionHook.RETURN_CONTINUE_THROW;
            tn = MSG_WORD_THROW;
            break;
        case jsdIExecutionHook.TYPE_INTERRUPTED:
            if (console._stepPast == frame.script.fileName + frame.line)
                return jsdIExecutionHook.RETURN_CONTINUE;
            delete console._stepPast;
            setStopState(false);
            break;
        default:
            /* don't print stop/cont messages for other types */
    }

    console.jsds.functionHook = null;
    ++console._stopLevel;

    /* set our default return value */
    console._continueCodeStack.push (retcode);

    if (tn)
        display (getMsg(MSN_STOP, tn), MT_STOP);
    if ($[0])
        display (getMsg(MSN_FMT_TMP_ASSIGN, [0, formatValue ($[0])]),
                 MT_FEVAL_OUT);
    
    /* build an array of frames */
    console.frames = new Array(frame);

    console.stackView.stack.unHide();
    console.stackView.stack.open();
    var frameRec = new FrameRecord(frame);
    console.stackView.stack.appendChild (frameRec);
    
    while ((frame = frame.callingFrame))
    {
        console.stackView.stack.appendChild (new FrameRecord(frame));
        console.frames.push(frame);
    }

    console.trapType = type;    
    console.jsds.enterNestedEventLoop({onNest: console.onDebugTrap}); 

    /* execution pauses here until someone calls 
     * console.dbg.exitNestedEventLoop() 
     */

    if (console._stopLevel > 0)
    {
        --console._stopLevel;
        console.jsds.exitNestedEventLoop();
    }

    if (tn)
        display (getMsg(MSN_CONT, tn), MT_CONT);

    rv.value = $[0];

    $ = new Array();
    clearCurrentFrame();
    delete console.frames;
    delete console.trapType;
    
    console.onDebugContinue();
    
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
        throw new BadMojo (ERR_NO_STACK);
    
    ASSERT (index >= 0 && index < console.frames.length, "index out of range");

    console._currentFrameIndex = index;
    var cf = console.frames[console._currentFrameIndex];
    console.stopLine = cf.line;
    console.stopFile = cf.script.fileName;
    console.onFrameChanged (cf, console._currentFrameIndex);

    return cf;
}

function clearCurrentFrame ()
{
    if (!console.frames)
        throw new BadMojo (ERR_NO_STACK);

    delete console.stopLine;
    delete console.stopFile;
    delete console._currentFrameIndex;
    console.onFrameChanged (null, 0);
}

function findNextExecutableLine (script, line)
{
    var pc = script.lineToPc (line + 1);
    
}

function formatArguments (v)
{
    if (!v)
        return "";

    var ary = new Array();
    var p = new Object();
    v.getProperties (p, {});
    p = p.value;
    for (var i = 0; i < p.length; ++i)
    {
        if (p[i].flags & jsdIProperty.FLAG_ARGUMENT)
            ary.push (getMsg(MSN_FMT_ARGUMENT,
                             [p[i].name.stringValue,
                              formatValue(p[i].value, FTYPE_SUMMARY)]));
    }
    
    return ary.join (MSG_COMMASP); 
}

function formatFlags (flags)
{
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

    return s;
}

function formatProperty (p, formatType)
{
    if (!p)
        throw new BadMojo (ERR_REQUIRED_PARAM, "p");

    var s = formatFlags (p.flags);

    if (formatType == FTYPE_ARRAY)
    {
        var rv = formatValue (p.value, FTYPE_ARRAY);
        return [p.name.stringValue, rv[1] ? rv[1] : rv[0], rv[2], s];
    }
    
    return getMsg(MSN_FMT_PROPERTY, [s, p.name.stringValue,
                                     formatValue(p.value)]);
}

function formatScript (script)
{
    if (!script)
        throw new BadMojo (ERR_REQUIRED_PARAM, "script");

    return getMsg (MSN_FMT_SCRIPT, [script.functionName, script.fileName]);
}

function formatFrame (f)
{
    if (!f)
        throw new BadMojo (ERR_REQUIRED_PARAM, "f");
 
    return getMsg (MSN_FMT_FRAME,
                   [f.script.functionName, formatArguments(f.scope),
                    f.script.fileName, f.line]);
}

function formatValue (v, formatType)
{
    if (!v)
        throw new BadMojo (ERR_REQUIRED_PARAM, "v");

    if (!(v instanceof jsdIValue))
        throw new BadMojo (ERR_INVALID_PARAM, "v", String(v));

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
            if (formatType == FTYPE_STD)
            {
                type = MSG_TYPE_OBJECT;
                value = getMsg(MSN_FMT_OBJECT, String(v.propertyCount));
            }
            else
            {
                if (v.jsClassName)
                    if (v.jsClassName == "XPCWrappedNative_NoHelper")
                        type = MSG_CLASS_XPCOBJ;
                    else
                        type = v.jsClassName;
                else
                    type = MSG_TYPE_OBJECT;
                value = "{" + String(v.propertyCount) + "}";
            }
            break;
        case jsdIValue.TYPE_STRING:
            type = MSG_TYPE_STRING;
            value = v.stringValue.quote();
            if (value.length > MAX_STR_LEN)
                value = getMsg(MSN_FMT_LONGSTR, value.length);
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

    if (formatType == FTYPE_SUMMARY)
        return getMsg (MSN_FMT_VALUE_SHORT, [type, value]);

    var className;
    if (v.jsClassName)
        if (v.jsClassName == "XPCWrappedNative_NoHelper")
            /* translate this long, unintuitive, and common class name into
             * something more palatable. */
            className = MSG_CLASS_XPCOBJ;
        else
            className = v.jsClassName;

    if (formatType == FTYPE_ARRAY)
        return [type, className, value];

    if (className)
        return getMsg (MSN_FMT_VALUE_LONG, [type, v.jsClassName, value]);

    return getMsg (MSN_FMT_VALUE_MED, [type, value]);

}

function XXXsetSourceFunctionMarks (url)
{
    source = console._sources[url];
    scriptArray = console._scripts[url];
    
    if (!source || !scriptArray)
    {
        dd ("Can't set function marks for " + url);
        return;
    }
    
    for (var i = 0; i < scriptArray.length; ++i)
    {
        /* the only scripts with empty filenames *should* be top level scripts,
         * and we don't show function marks for them, because the whole file
         * would end up being marked. */
        if (scriptArray[i].functionName && scriptArray[i].baseLineNumber > 1)
        {
            var j = scriptArray[i].baseLineNumber - 1;
            var stop = j + scriptArray[i].lineExtent - 1;
            if (!source[j] || !source[stop])
            {
                dd ("Script " + scriptArray[i].functionName + "(" +
                    scriptArray[i].baseLineNumber + ", " + 
                    scriptArray[i].lineExtent + ") does not actually exist in " +
                    url + " (" + source.length + " lines.)");
            }
            else
            {
                source[j].functionStart = true;
                source[stop].functionEnd = true;
                for (; j <= stop ; ++j)
                    source[j].functionLine = true;
            }
        }
        
    }
}

function displayCallStack ()
{
    for (i = 0; i < console.frames.length; ++i)
        displayFrame (console.frames[i], i);
}

function displayProperties (v)
{
    if (!v)
        throw new BadMojo (ERR_REQUIRED_PARAM, "v");

    if (!(v instanceof jsdIValue))
        throw new BadMojo (ERR_INVALID_PARAM, "v", String(v));

    var p = new Object();
    v.getProperties (p, {});
    for (var i in p.value) display(formatProperty (p.value[i]));
}

function displaySource (url, line, contextLines)
{
    function onSourceLoaded (status)
    {
        if (status == Components.results.NS_OK)
            displaySource (url, line, contextLines);
        else
            display (getMsg(MSN_ERR_SOURCE_LOAD_FAILED, url), MT_ERROR);
    }
    
    var rec = console.scripts[url];
 
    if (!rec)
    {
        display (MSG_ERR_NO_SOURCE, MT_ERROR);
        return;
    }
    
    if (rec.isLoaded)
        for (var i = line - contextLines; i <= line + contextLines; ++i)
        {
            if (i > 0 && i < rec.sourceText.length)
            {
                display (getMsg(MSN_SOURCE_LINE, [zeroPad (i, 3),
                                                  rec.sourceText[i - 1]]),
                         i == line ? MT_STEP : MT_SOURCE);
            }
        }
    else
    {
        rec.loadSource (onSourceLoaded);        
    }
}
    
function displayFrame (f, idx, showSource)
{
    if (!f)
        throw new BadMojo (ERR_REQUIRED_PARAM, "f");

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
    return console.breakpoints.locateChildByFileLine (fileName, line);
}

function disableBreakpoint (fileName, line)
{
    var bpr = console.breakpoints.locateChildByFileLine (fileName, line);
    if (!bpr)
    {
        display (getMsg(MSN_ERR_BP_NODICE, [fileName, line]), MT_ERROR);
        return 0;
    }
    
    return disableBreakpointByNumber (bpr.childIndex);
}

function disableBreakpointByNumber (number)
{
    var bpr = console.breakpoints.childData[number];
    if (!bpr)
    {
        display (getMsg(MSN_ERR_BP_NOINDEX, number, MT_ERROR));
        return;
    }

    bpr.enabled = false;
    display (getMsg(MSN_BP_DISABLED, [bpr.fileName, bpr.line,
                                      bpr.scriptMatches]));
}

function clearBreakpoint (fileName, line)
{
    var bpr = console.breakpoints.locateChildByFileLine (fileName, line);
    if (!bpr)
    {
        display (getMsg(MSN_ERR_BP_NODICE, [fileName, line]), MT_ERROR);
        return 0;
    }
    
    return clearBreakpointByNumber (bpr.childIndex);
}

function clearBreakpointByNumber (number)
{
    var bpr = console.breakpoints.childData[number];
    if (!bpr)
    {
        display (getMsg(MSN_ERR_BP_NOINDEX, number, MT_ERROR));
        return 0;
    }

    bpr.enabled = false;

    display (getMsg(MSN_BP_CLEARED, [bpr.fileName, bpr.line,
                                     bpr.scriptMatches]));
    
    var fileName = bpr.fileName;
    var line = bpr.line;
    var sourceRecord = console.scripts[fileName];

    if (sourceRecord.isLoaded && sourceRecord.sourceText[line - 1])
    {
        delete sourceRecord.sourceText[line - 1].bpRecord;
        if (console.sourceView.childData.fileName == fileName)
            console.sourceView.outliner.invalidateRow (line - 1);
    }

    console.breakpoints.removeChildAtIndex(number);
    return bpr.scriptMatches;
}
    
function setBreakpoint (fileName, line)
{
    var sourceRecord = console.scripts[fileName];
    
    if (!sourceRecord)
    {
        display (getMsg(MSN_ERR_NOSCRIPT, fileName), MT_ERROR);
        return null;
    }

    var bpr = console.breakpoints.locateChildByFileLine (fileName, line);
    if (bpr)
    {
        display (getMsg(MSN_BP_EXISTS, [fileName, line]), MT_INFO);
        return null;
    }
    
    bpr = new BPRecord (fileName, line);
    
    var ary = sourceRecord.childData;
    var found = false;
    
    for (var i = 0; i < ary.length; ++i)
    {
        if (ary[i].containsLine(line) && ary[i].isLineExecutable(line))
        {
            found = true;
            bpr.addScriptRecord(ary[i]);
        }
    }

    var matches = bpr.scriptMatches
    if (!matches)
    {
        display (getMsg(MSN_ERR_BP_NOLINE, [fileName, line]), MT_ERROR);
        return null;
    }
    
    if (sourceRecord.isLoaded && sourceRecord.sourceText[line - 1])
    {
        sourceRecord.sourceText[line - 1].bpRecord = bpr;
        if (console.sourceView.childData.fileName == fileName)
            console.sourceView.outliner.invalidateRow (line - 1);
    }
    
    console.breakpoints.appendChild (bpr);
    display (getMsg(MSN_BP_CREATED, [fileName, line, matches]));
    
    return bpr;
}

function setFutureBreakpoint (filePattern, line)
{
    var bpr = console.breakpoints.locateChildByFileLine (filePattern, line);
    if (bpr)
    {
        display (getMsg(MSN_BP_EXISTS, [filePattern, line]), MT_INFO);
        return null;
    }
    
    bpr = new BPRecord (filePattern, line);
    
    console.breakpoints.appendChild (bpr);
    display (getMsg(MSN_FBP_CREATED, [filePattern, line]));
    
    return bpr;
}
