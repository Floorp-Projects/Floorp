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
const jsdIErrorHook       = Components.interfaces.jsdIErrorHook;
const jsdICallHook        = Components.interfaces.jsdICallHook;
const jsdIValue           = Components.interfaces.jsdIValue;
const jsdIProperty        = Components.interfaces.jsdIProperty;
const jsdIScript          = Components.interfaces.jsdIScript;
const jsdIStackFrame      = Components.interfaces.jsdIStackFrame;
const jsdIFilter          = Components.interfaces.jsdIFilter;

const COLLECT_PROFILE_DATA = jsdIDebuggerService.COLLECT_PROFILE_DATA;

const PCMAP_SOURCETEXT    = jsdIScript.PCMAP_SOURCETEXT;
const PCMAP_PRETTYPRINT   = jsdIScript.PCMAP_PRETTYPRINT;

const FTYPE_STD     = 0;
const FTYPE_SUMMARY = 1;
const FTYPE_ARRAY   = 2;

const FILTER_ENABLED  = jsdIFilter.FLAG_ENABLED;
const FILTER_DISABLED = ~jsdIFilter.FLAG_ENABLED;
const FILTER_PASS     = jsdIFilter.FLAG_PASS;
const FILTER_SYSTEM   = 0x100; /* system filter, do not show in UI */

var $ = new Array(); /* array to store results from evals in debug frames */

console._scriptHook = {
    onScriptCreated: realizeScript,
    onScriptDestroyed: unrealizeScript
};

console._executionHook = {onExecute: vnk_exehook};
function vnk_exehook (frame, type, rv)
{
    var hookReturn = jsdIExecutionHook.RETURN_CONTINUE;

    if (!ASSERT(!("frames" in console), "Execution hook called while stopped") ||
        frame.isNative ||
        !ASSERT(frame.script, "Execution hook called with no script") ||
        frame.script.fileName == MSG_VAL_CONSOLE)
    {
        return hookReturn;
    }

    var targetWindow = null;
    var wasModal = false;
    var ex;
    var cx;

    try
    {
        cx = frame.executionContext;
    }
    catch (ex)
    {
        dd ("no context");
        cx = null;
    }
    
    var targetWasEnabled = true;
    var debuggerWasEnabled = console.debuggerWindow.enabled;
    console.debuggerWindow.enabled = true;
    
    if (cx)
    {
        cx.scriptsEnabled = false;
        var glob = cx.globalObject;
        if (glob)
        {
            console.targetWindow =
                getBaseWindowFromWindow(glob.getWrappedValue());
            targetWasEnabled = console.targetWindow.enabled;
            console.targetWindow.enabled = false;
        }
    }

    try
    {
        hookReturn = debugTrap(frame, type, rv);
    }
    catch (ex)
    {
        display (MSG_ERR_INTERNAL_BPT, MT_ERROR);
        display (formatException(ex), MT_ERROR);
    }
    
    if (cx)
    {
        cx.scriptsEnabled = true;
        if (0 && targetWindow)
        {
            targetWindow.enabled = true;
            if (wasModal)
                targetWindow.modalMien = true;
        }
    }
    
    if (console.targetWindow)
        console.targetWindow.enabled = targetWasEnabled;
    console.debuggerWindow.enabled = debuggerWasEnabled;
    delete console.frames;
    delete console.targetWindow;
    if ("__exitAfterContinue__" in console)
        window.close();

    return hookReturn;
}

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
                dd ("Call Hook: " + frame.functionName + ", type " +
                    type + " callCount: " + console._stepOverLevel);
            }
};

console._errorHook = {
    onError: function errorhook (message, fileName, line, pos, flags,
                                 exception) {
                 try {
                     var flagstr;
                     flagstr =
                         (flags && jsdIErrorHook.REPORT_EXCEPTION) ? "x" : "-";
                     flagstr +=
                         (flags && jsdIErrorHook.REPORT_STRICT) ? "s" : "-";
                     
                     //dd ("===\n" + message + "\n" + fileName + "@" + 
                     //    line + ":" + pos + "; " + flagstr);
                     var msn = (flags & jsdIErrorHook.REPORT_WARNING) ?
                         MSN_ERPT_WARN : MSN_ERPT_ERROR;

                     if (console.errorMode != EMODE_IGNORE)
                         display (getMsg(msn, [message, flagstr, fileName,
                                               line, pos]), MT_ETRACE);
                     
                     if (console.errorMode == EMODE_BREAK)
                         return false;

                     return true;
                 }
                 catch (ex)
                 {
                     dd ("error in error hook: " + ex);
                 }
                 return true;
             }
};

function initDebugger()
{   
    dd ("initDebugger {");
    
    console._continueCodeStack = new Array(); /* top of stack is the default  */
                                              /* return code for the most     */
                                              /* recent debugTrap().          */
    console.scripts = new Object();
    
    /* create the debugger instance */
    if (!Components.classes[JSD_CTRID])
        throw new BadMojo (ERR_NO_DEBUGGER);
    
    console.jsds = Components.classes[JSD_CTRID].getService(jsdIDebuggerService);
    console.jsds.on();
    console.jsds.breakpointHook = console._executionHook;
    console.jsds.debuggerHook = console._executionHook;
    console.jsds.debugHook = console._executionHook;
    console.jsds.errorHook = console._errorHook;
    console.jsds.scriptHook = console._scriptHook;
    console.jsds.flags = jsdIDebuggerService.ENABLE_NATIVE_FRAMES;

    console.chromeFilter = {
        globalObject: null,
        flags: FILTER_SYSTEM | FILTER_ENABLED,
        urlPattern: "chrome:*",
        startLine: 0,
        endLine: 0
    };
    
    if (console.prefs["enableChromeFilter"])
    {
        console.enableChromeFilter = true;
        console.jsds.appendFilter(console.chromeFilter);
    }
    else
        console.enableChromeFilter = false;

    var venkmanFilter1 = {  /* glob based filter goes first, because it's the */
        globalObject: this, /* easiest to match.                              */
        flags: FILTER_SYSTEM | FILTER_ENABLED,
        urlPattern: null,
        startLine: 0,
        endLine: 0
    };
    var venkmanFilter2 = {  /* url based filter for XPCOM callbacks that may  */
        globalObject: null, /* not happen under our glob.                     */
        flags: FILTER_SYSTEM | FILTER_ENABLED,
        urlPattern: "chrome://venkman/*",
        startLine: 0,
        endLine: 0
    };
    console.jsds.appendFilter (venkmanFilter1);
    console.jsds.appendFilter (venkmanFilter2);

    console.throwMode = TMODE_IGNORE;
    console.errorMode = EMODE_IGNORE;

    var enumer = {
        enumerateScript: function _es (script) {
            realizeScript(script);
        }
    };
    
    console.scriptsView.freeze();
    console.jsds.enumerateScripts(enumer);
    console.scriptsView.thaw();

    dd ("} initDebugger");
}

function detachDebugger()
{
    if ("frames" in console)
        console.jsds.exitNestedEventLoop();
    
    console.jsds.topLevelHook = null;
    console.jsds.functionHook = null;
    console.jsds.breakpointHook = null;
    console.jsds.debuggerHook = null;
    console.jsds.errorHook = null;
    console.jsds.scriptHook = null;
    console.jsds.interruptHook = null;
    console.jsds.clearAllBreakpoints();
    console.jsds.clearFilters();
    if (!console.jsds.initAtStartup)
        console.jsds.off();
}

function isURLFiltered (url)
{
    return (!url || url == MSG_VAL_CONSOLE ||
            (console.enableChromeFilter && url.indexOf ("chrome:") == 0) ||
            url.indexOf ("x-jsd:internal") == 0);
}    

function realizeScript(script)
{
    var container;
    if (script.fileName in console.scripts)
    {
        container = console.scripts[script.fileName];
        if (!("parentRecord" in container))
        {
            /* container record exists but is not inserted in the scripts view,
             * either we are reloading it, or the user added it manually */
            if (!isURLFiltered(script.fileName))
                console.scriptsView.childData.appendChild(container);
        }
    }
    else
    {
        container = console.scripts[script.fileName] =
            new ScriptContainerRecord (script.fileName);
        container.reserveChildren();
        if (!isURLFiltered(script.fileName))
            console.scriptsView.childData.appendChild(container);
    }    

    var scriptRec = new ScriptRecord (script);
    if ("dbgRealize" in console && console.dbgRealize)
    {
        dd ("new scriptrecord: " + formatScript(script) + "\n" + 
            script.functionSource);
    }
    
    container.appendScriptRecord (scriptRec);
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

    if ("dbgRealize" in console && console.dbgRealize)
        dd ("remove scriptrecord: " + formatScript(script));

    var scriptRec = container.locateChildByScript (script);
    if (!scriptRec)
    {
        dd ("unrealizeScript: unable to locate script record");
        return;
    }
    
    if (console.sourceView.provider == scriptRec)
        console.sourceView.setCurrentSourceProvider(scriptRec.parentRecord);

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
    if (container.childData.length == 0 && "parentRecord" in container)
    {
        console.scriptsView.childData.removeChildAtIndex(container.childIndex);
    }
}

const EMODE_IGNORE = 0;
const EMODE_TRACE  = 1;
const EMODE_BREAK  = 2;

const TMODE_IGNORE = 0;
const TMODE_TRACE  = 1;
const TMODE_BREAK  = 2;

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

            if (console.throwMode != TMODE_BREAK)
                return jsdIExecutionHook.RETURN_CONTINUE_THROW;

            $[0] = rv.value;
            retcode = jsdIExecutionHook.RETURN_CONTINUE_THROW;
            tn = MSG_WORD_THROW;
            break;
        case jsdIExecutionHook.TYPE_INTERRUPTED:
            var line;
            if (console.sourceView.prettyPrint)
                line = frame.script.pcToLine (frame.pc, PCMAP_PRETTYPRINT);
            else
                line = frame.line;
            if (console._stepPast == frame.script.fileName + line)
                return jsdIExecutionHook.RETURN_CONTINUE;
            delete console._stepPast;
            setStopState(false);
            break;
        default:
            /* don't print stop/cont messages for other types */
    }

    console.jsds.functionHook = null;

    /* set our default return value */
    console._continueCodeStack.push (retcode);

    if (tn)
        display (getMsg(MSN_STOP, tn), MT_STOP);
    if (0 in $)
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
    window.focus();
    window.getAttention();

    console.jsds.enterNestedEventLoop({onNest: console.onDebugTrap}); 

    /* execution pauses here until someone calls 
     * console.dbg.exitNestedEventLoop() 
     */

    clearCurrentFrame();
    delete console.frames;
    delete console.trapType;
    rv.value = (0 in $) ? $[0] : null;
    $ = new Array();
    
    console.onDebugContinue();

    if (tn)
        display (getMsg(MSN_CONT, tn), MT_CONT);

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
    console.stopFile = (cf.isNative) ? MSG_URL_NATIVE : cf.script.fileName;
    console.stopLine = cf.line;
    delete console._pp_stopLine;
    console.onFrameChanged (cf, console._currentFrameIndex);

    return cf;
}

function clearCurrentFrame ()
{
    if (!console.frames)
        throw new BadMojo (ERR_NO_STACK);

    delete console.stopLine;
    delete console._pp_stopLine;
    delete console.stopFile;
    delete console._currentFrameIndex;
    console.onFrameChanged (null, 0);
}

function findNextExecutableLine (script, line)
{
    var pc = script.lineToPc (line + 1, PCMAP_SOURCETEXT);
    
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
    var url = (f.isNative) ? MSG_URL_NATIVE : f.script.fileName;
    return getMsg (MSN_FMT_FRAME,
                   [f.functionName, formatArguments(f.scope), url, f.line]);
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

function displayCallStack ()
{
    for (var i = 0; i < console.frames.length; ++i)
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
    }
    
    var rec = console.scripts[url];
 
    if (!rec)
    {
        display (getMsg(MSN_ERR_NO_SCRIPT, url), MT_ERROR);
        return;
    }
    
    var sourceText = rec.sourceText;
    
    if (sourceText.isLoaded)
        for (var i = line - contextLines; i <= line + contextLines; ++i)
        {
            if (i > 0 && i < sourceText.lines.length)
            {
                display (getMsg(MSN_SOURCE_LINE, [zeroPad (i, 3),
                                                  sourceText.lines[i - 1]]),
                         i == line ? MT_STEP : MT_SOURCE);
            }
        }
    else
    {
        sourceText.loadSource (onSourceLoaded);
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
    if (!f.isNative && f.script.fileName && showSource)
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
        return null;
    }

    bpr.enabled = false;

    var fileName = bpr.fileName;
    var line = bpr.line;

    var sourceText;
    
    if (fileName in console.scripts)
        sourceText = console.scripts[fileName].sourceText;
    else if (fileName in console.files)
        sourceText = console.files[fileName];

    if (sourceText && sourceText.isLoaded && sourceText.lines[line - 1])
    {
        delete sourceText.lines[line - 1].bpRecord;
        if (console.sourceView.childData.fileName == fileName)
            console.sourceView.tree.invalidateRow (line - 1);
    }
    
    console.breakpoints.removeChildAtIndex(number);
    return bpr;
}
    
function setBreakpoint (fileName, line)
{
    var scriptRec = console.scripts[fileName];
    
    if (!scriptRec)
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
    
    var ary = scriptRec.childData;
    var found = false;
    
    for (var i = 0; i < ary.length; ++i)
    {
        if (ary[i].containsLine(line) &&
            ary[i].script.isLineExecutable(line, PCMAP_SOURCETEXT))
        {
            found = true;
            bpr.addScriptRecord(ary[i]);
        }
    }

    var matches = bpr.scriptMatches;
    if (!matches)
    {
        display (getMsg(MSN_ERR_BP_NOLINE, [fileName, line]), MT_ERROR);
        return null;
    }
    
    if (scriptRec.sourceText.isLoaded &&
        scriptRec.sourceText.lines[line - 1])
    {
        scriptRec.sourceText.lines[line - 1].bpRecord = bpr;
        if (console.sourceView.childData.fileName == fileName)
            console.sourceView.tree.invalidateRow (line - 1);
    }
    
    console.breakpoints.appendChild (bpr);
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
    
    if (filePattern in console.files)
    {
        var sourceText = console.files[filePattern];
        if (sourceText.isLoaded)
        {
            sourceText.lines[line - 1].bpRecord = bpr;
            if (console.sourceView.childData.fileName == filePattern)
                console.sourceView.tree.invalidateRow (line - 1);
        }
    }
    
    console.breakpoints.appendChild (bpr);
    return bpr;
}
