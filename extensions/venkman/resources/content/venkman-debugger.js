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

const JSD_CTRID           = "@mozilla.org/js/jsd/debugger-service;1";
const jsdIDebuggerService = Components.interfaces.jsdIDebuggerService;
const jsdIExecutionHook   = Components.interfaces.jsdIExecutionHook;
const jsdIErrorHook       = Components.interfaces.jsdIErrorHook;
const jsdICallHook        = Components.interfaces.jsdICallHook;
const jsdIValue           = Components.interfaces.jsdIValue;
const jsdIProperty        = Components.interfaces.jsdIProperty;
const jsdIScript          = Components.interfaces.jsdIScript;
const jsdIStackFrame      = Components.interfaces.jsdIStackFrame;

const TYPE_VOID     = jsdIValue.TYPE_VOID;
const TYPE_NULL     = jsdIValue.TYPE_NULL;
const TYPE_BOOLEAN  = jsdIValue.TYPE_BOOLEAN;
const TYPE_INT      = jsdIValue.TYPE_INT;
const TYPE_DOUBLE   = jsdIValue.TYPE_DOUBLE;
const TYPE_STRING   = jsdIValue.TYPE_STRING;
const TYPE_FUNCTION = jsdIValue.TYPE_FUNCTION;
const TYPE_OBJECT   = jsdIValue.TYPE_OBJECT;

const PROP_ENUMERATE = jsdIProperty.FLAG_ENUMERATE;
const PROP_READONLY  = jsdIProperty.FLAG_READONLY;
const PROP_PERMANENT = jsdIProperty.FLAG_PERMANENT;
const PROP_ALIAS     = jsdIProperty.FLAG_ALIAS;
const PROP_ARGUMENT  = jsdIProperty.FLAG_ARGUMENT;
const PROP_VARIABLE  = jsdIProperty.FLAG_VARIABLE;
const PROP_EXCEPTION = jsdIProperty.FLAG_EXCEPTION;
const PROP_ERROR     = jsdIProperty.FLAG_ERROR;
const PROP_HINTED    = jsdIProperty.FLAG_HINTED;

const SCRIPT_NODEBUG   = jsdIScript.FLAG_DEBUG;
const SCRIPT_NOPROFILE = jsdIScript.FLAG_PROFILE;

const COLLECT_PROFILE_DATA  = jsdIDebuggerService.COLLECT_PROFILE_DATA;

const PCMAP_SOURCETEXT    = jsdIScript.PCMAP_SOURCETEXT;
const PCMAP_PRETTYPRINT   = jsdIScript.PCMAP_PRETTYPRINT;

const RETURN_CONTINUE   = jsdIExecutionHook.RETURN_CONTINUE;
const RETURN_CONT_THROW = jsdIExecutionHook.RETURN_CONTINUE_THROW;
const RETURN_VALUE      = jsdIExecutionHook.RETURN_RET_WITH_VAL;
const RETURN_THROW      = jsdIExecutionHook.RETURN_THROW_WITH_VAL;

const FTYPE_STD     = 0;
const FTYPE_SUMMARY = 1;
const FTYPE_ARRAY   = 2;

const BREAKPOINT_STOPNEVER   = 0;
const BREAKPOINT_STOPALWAYS  = 1;
const BREAKPOINT_STOPTRUE    = 2;
const BREAKPOINT_EARLYRETURN = 3;

var $ = new Array(); /* array to store results from evals in debug frames */

function initDebugger()
{   
    dd ("initDebugger {");

    console.instanceSequence = 0;
    console._continueCodeStack = new Array(); /* top of stack is the default  */
                                              /* return code for the most     */
                                              /* recent debugTrap().          */
    console.scriptWrappers = new Object();
    console.scriptManagers = new Object();
    console.breaks  = new Object();
    console.fbreaks = new Object();
    console.sbreaks = new Object();

    /* create the debugger instance */
    if (!(JSD_CTRID in Components.classes))
        throw new BadMojo (ERR_NO_DEBUGGER);
    
    console.jsds = Components.classes[JSD_CTRID].getService(jsdIDebuggerService);
    console.jsds.on();

    console.executionHook = { onExecute: jsdExecutionHook };
    console.errorHook     = { onError: jsdErrorHook };
    console.callHook      = { onCall: jsdCallHook };
    
    console.jsds.breakpointHook = console.executionHook;
    console.jsds.debuggerHook   = console.executionHook;
    console.jsds.debugHook      = console.executionHook;
    console.jsds.errorHook      = console.errorHook;
    console.jsds.flags          = jsdIDebuggerService.ENABLE_NATIVE_FRAMES;

    console.jsdConsole = console.jsds.wrapValue(console);    

    dispatch ("tmode", {mode: console.prefs["lastThrowMode"]});
    dispatch ("emode", {mode: console.prefs["lastErrorMode"]});
    
    var enumer = { enumerateScript: console.scriptHook.onScriptCreated };
    console.jsds.scriptHook = console.scriptHook;
    console.jsds.enumerateScripts(enumer);

    dd ("} initDebugger");
}

function detachDebugger()
{
    if ("frames" in console)
        console.jsds.exitNestedEventLoop();

    var b;
    for (b in console.breaks)
        console.breaks[b].clearBreakpoint();
    for (b in console.fbreaks)
        console.fbreaks[b].clearFutureBreakpoint();

    console.jsds.topLevelHook = null;
    console.jsds.functionHook = null;
    console.jsds.breakpointHook = null;
    console.jsds.debuggerHook = null;
    console.jsds.errorHook = null;
    console.jsds.scriptHook = null;
    console.jsds.interruptHook = null;
    console.jsds.clearAllBreakpoints();

    console.jsds.GC();

    if (!console.jsds.initAtStartup)
        console.jsds.off();
}

console.scriptHook = new Object();

console.scriptHook.onScriptCreated =
function sh_created (jsdScript)
{
    try
    {
        jsdScriptCreated(jsdScript);
    }
    catch (ex)
    {
        dd ("caught " + dumpObjectTree(ex) + " while creating script.");
    }
}

console.scriptHook.onScriptDestroyed =
function sh_destroyed (jsdScript)
{
    try
    {
        jsdScriptDestroyed(jsdScript);
    }
    catch (ex)
    {
        dd ("caught " + dumpObjectTree(ex) + " while destroying script.");
    }
}
    
function jsdScriptCreated (jsdScript)
{
    var url = jsdScript.fileName;
    var manager;
    
    if (!(url in console.scriptManagers))
    {
        manager = console.scriptManagers[url] = new ScriptManager(url);
        //dispatchCommand (console.coManagerCreated, { scriptManager: manager });
    }
    else
    {
        manager = console.scriptManagers[url];
    }
    
    manager.onScriptCreated(jsdScript);
}

function jsdScriptDestroyed (jsdScript)
{
    if (!(jsdScript.tag in console.scriptWrappers))
        return;
    
    var scriptWrapper = console.scriptWrappers[jsdScript.tag];
    scriptWrapper.scriptManager.onScriptInvalidated(scriptWrapper);
    
    if (scriptWrapper.scriptManager.instances.length == 0 && 
        scriptWrapper.scriptManager.transientCount == 0)
    {
        delete console.scriptManagers[scriptWrapper.scriptManager.url];
        //dispatchCommand (console.coManagerDestroyed,
        //                 { scriptManager: scriptWrapper.scriptManager });
    }
}

function jsdExecutionHook (frame, type, rv)
{
    var hookReturn = jsdIExecutionHook.RETURN_CONTINUE;

    if (!ASSERT(!("frames" in console), "Execution hook called while stopped") ||
        frame.isNative ||
        !ASSERT(frame.script, "Execution hook called with no script") ||
        frame.script.fileName == MSG_VAL_CONSOLE ||
        !ASSERT(!(frame.script.flags & SCRIPT_NODEBUG),
                "Stopped in a script marked as don't debug") ||
        !ASSERT(isURLVenkman(frame.script.fileName) ||
                !isURLFiltered(frame.script.fileName),
                "stopped in a filtered URL"))
    {
        return hookReturn;
    }

    var targetWindow = null;
    var wasModal = false;
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
    var debuggerWasEnabled = console.baseWindow.enabled;
    console.baseWindow.enabled = true;
    
    if (!ASSERT(cx, "no cx in execution hook"))
        return hookReturn;
    
    var glob = cx.globalObject;
    if (!ASSERT(glob, "no glob in execution hook"))
        return hookReturn;
    
    console.targetWindow = getBaseWindowFromWindow(glob.getWrappedValue());
    targetWasEnabled = console.targetWindow.enabled;
    if (console.targetWindow != console.baseWindow)
    {
        cx.scriptsEnabled = false;
        console.targetWindow.enabled = false;
    }

    try
    {
        //dd ("debug trap " + formatFrame(frame));
        hookReturn = debugTrap(frame, type, rv);
        //dd ("debug trap returned " + hookReturn);
    }
    catch (ex)
    {
        display (MSG_ERR_INTERNAL_BPT, MT_ERROR);
        display (formatException(ex), MT_ERROR);
    }
    

    
    if (console.targetWindow && console.targetWindow != console.baseWindow)
    {
        console.targetWindow.enabled = targetWasEnabled;
        cx.scriptsEnabled = true;
    }
    
    console.baseWindow.enabled = debuggerWasEnabled;
    delete console.frames;
    delete console.targetWindow;
    if ("__exitAfterContinue__" in console)
        window.close();

    return hookReturn;
}

function jsdCallHook (frame, type)
{
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
    //dd ("Call Hook: " + frame.functionName + ", type " +
    //    type + " callCount: " + console._stepOverLevel);
}

function jsdErrorHook (message, fileName, line, pos, flags, exception)
{
    if (isURLFiltered (fileName))
        return true;
    
    try
    {
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

function ScriptManager (url)
{
    this.url = url;
    this.instances = new Array();
    this.transients = new Object();
    this.transientCount = 0;
    this.disableTransients = isURLFiltered(url);
}

ScriptManager.prototype.onScriptCreated =
function smgr_created (jsdScript)
{
    var instance;

    if (!ASSERT(jsdScript.isValid, "invalid script created!"))
        return;
    
    if (this.instances.length != 0)
        instance = this.instances[this.instances.length - 1];

    if (!instance || (instance.isSealed && jsdScript.functionName))
    {
        //dd ("instance created for " + jsdScript.fileName);
        instance = new ScriptInstance(this);
        instance.sequence = console.instanceSequence++;
        this.instances.push(instance);
        dispatchCommand (console.coInstanceCreated,
                         { scriptInstance: instance });
    }

    if ("_lastScriptWrapper" in console)
    {
        if ((console._lastScriptWrapper.scriptManager != this ||
             console._lastScriptWrapper.scriptInstance != instance) &&
            console._lastScriptWrapper.scriptInstance.scriptCount &&
            !console._lastScriptWrapper.scriptInstance.isSealed)
        {
            console._lastScriptWrapper.scriptInstance.seal();
        }
    }
            
    var scriptWrapper = new ScriptWrapper(jsdScript);
    console._lastScriptWrapper = scriptWrapper;
    scriptWrapper.scriptManager = this;
    console.scriptWrappers[jsdScript.tag] = scriptWrapper;
    scriptWrapper.scriptInstance = instance;

    if (!instance.isSealed)
    {
        //dd ("function created " + formatScript(jsdScript));
        instance.onScriptCreated (scriptWrapper);
    }
    else
    {
        //dd ("transient created " + formatScript(jsdScript));
        ++this.transientCount;
        if (this.disableTransients)
            jsdScript.flags |= SCRIPT_NODEBUG | SCRIPT_NOPROFILE;

        this.transients[jsdScript.tag] = scriptWrapper;
        scriptWrapper.functionName = MSG_VAL_EVSCRIPT;
        //dispatch ("hook-transient-script", { scriptWrapper: scriptWrapper });
    }    
}

ScriptManager.prototype.onScriptInvalidated =
function smgr_invalidated (scriptWrapper)
{
    //dd ("script invalidated");
    
    delete console.scriptWrappers[scriptWrapper.tag];
    if (scriptWrapper.tag in this.transients)
    {
        //dd ("transient destroyed " + formatScript(scriptWrapper.jsdScript));
        --this.transientCount;
        delete this.transients[scriptWrapper.tag];
        //dispatch ("hook-script-invalidated", { scriptWrapper: scriptWrapper });
    }
    else
    {
        //dd ("function destroyed " + formatScript(scriptWrapper.jsdScript));
        scriptWrapper.scriptInstance.onScriptInvalidated(scriptWrapper);
        //dispatch ("hook-script-invalidated", { scriptWrapper: scriptWrapper });

        if (scriptWrapper.scriptInstance.scriptCount == 0)
        {
            var i = arrayIndexOf(this.instances, scriptWrapper.scriptInstance);
            arrayRemoveAt(this.instances, i);
            dispatchCommand (console.coInstanceDestroyed,
                             { scriptInstance: scriptWrapper.scriptInstance });
        }
    }
}    

ScriptManager.prototype.__defineGetter__ ("sourceText", smgr_sourcetext);
function smgr_sourcetext()
{
    return this.instances[this.instances.length - 1].sourceText;
}

ScriptManager.prototype.__defineGetter__ ("lineMap", smgr_linemap);
function smgr_linemap()
{
    return this.instances[this.instances.length - 1].lineMap;
}

ScriptManager.prototype.getInstanceBySequence =
function smgr_bysequence (seq)
{
    for (var i = 0; i < this.instances.length; ++i)
    {
        if (this.instances[i].sequence == seq)
            return this.instances[i];
    }
    
    return null;
}

ScriptManager.prototype.isLineExecutable =
function smgr_isexe (line)
{
    for (var i in this.instances)
    {
        if (this.instances[i].isLineExecutable(line))
            return true;
    }

    return false;
}

ScriptManager.prototype.hasBreakpoint =
function smgr_hasbp (line)
{
    for (var i in this.instances)
    {
        if (this.instances[i].hasBreakpoint(line))
            return true;
    }
    
    return false;
}

ScriptManager.prototype.setBreakpoint =
function smgr_break (line, parentBP)
{
    var found = false;
    
    for (var i in this.instances)
        found |= this.instances[i].setBreakpoint(line, parentBP);

    return found;
}

ScriptManager.prototype.clearBreakpoint =
function smgr_break (line)
{
    var found = false;

    for (var i in this.instances)
        found |= this.instances[i].clearBreakpoint(line);

    return found;
}

ScriptManager.prototype.hasFutureBreakpoint =
function smgr_hasbp (line)
{
    var key = this.url + "#" + line;
    return (key in console.fbreaks);
}

ScriptManager.prototype.getFutureBreakpoint =
function smgr_getfbp (line)
{
    return getFutureBreakpoint (this.url, line);
}

ScriptManager.prototype.noteFutureBreakpoint =
function smgr_fbreak (line, state)
{
    for (var i in this.instances)
    {
        if (this.instances[i]._lineMapInited)
        {
            if (state)
            {
                arrayOrFlag (this.instances[i]._lineMap, line - 1, LINE_FBREAK);
            }
            else
            {
                arrayAndFlag (this.instances[i]._lineMap, line - 1,
                              ~LINE_FBREAK);
            }
        }
    }
}

function ScriptInstance (manager)
{
    this.scriptManager = manager;
    this.url = manager.url;
    this.creationDate = new Date();
    this.topLevel = null;
    this.functions = new Object();
    this.isSealed = false;
    this.scriptCount = 0;
    this.breakpointCount = 0;
    this._lineMap = new Array();
    this._lineMapInited = false;
}

ScriptInstance.prototype.scanForMetaComments =
function si_scan (start)
{
    const CHUNK_SIZE = 500;
    const CHUNK_DELAY = 100;
    
    var scriptInstance = this;
    var sourceText = this.sourceText;
    
    function onSourceLoaded(result)
    {
        if (result == Components.results.NS_OK)
            scriptInstance.scanForMetaComments();
    };
    
    if (!sourceText.isLoaded)
    {
        sourceText.loadSource(onSourceLoaded);
        return;
    }

    if (typeof start == "undefined")
        start = 0;
    
    var end = Math.min (sourceText.lines.length, start + CHUNK_SIZE);
    var obj = new Object();
    
    for (var i = start; i < end; ++i)
    {
        var ary = sourceText.lines[i].match (/\/\/@(\S+)(.*)/);
        if (ary && ary[1] in console.metaDirectives && !(ary[1] in obj))
        {
            try
            {
                console.metaDirectives[ary[1]](scriptInstance, i + 1, ary);
            }
            catch (ex)
            {
                display (getMsg(MSN_ERR_META_FAILED, [ary[1], this.url, i + 1]),
                         MT_ERROR);
                display (formatException (ex), MT_ERROR);
            }
        }
    }

    if (i != sourceText.lines.length)
        setTimeout (this.scanForMetaComments, CHUNK_DELAY, i);
}

ScriptInstance.prototype.seal =
function si_seal ()
{
    this.sealDate = new Date();
    this.isSealed = true;

    if (isURLFiltered(this.url))
    {
        var nada = SCRIPT_NODEBUG | SCRIPT_NOPROFILE;
        if (this.topLevel && this.topLevel.isValid)
            this.topLevel.jsdScript.flags |= nada;

        for (var f in this.functions)
        {
            if (this.functions[f].jsdScript.isValid)
                this.functions[f].jsdScript.flags |= nada;
        }
    }

    dispatch ("hook-script-instance-sealed", { scriptInstance: this });
}

ScriptInstance.prototype.onScriptCreated =
function si_created (scriptWrapper)
{
    var tag = scriptWrapper.jsdScript.tag;
    
    if (scriptWrapper.functionName)
    {
        this.functions[tag] = scriptWrapper;
    }
    else
    {
        this.topLevel = scriptWrapper;
        scriptWrapper.functionName = MSG_VAL_TLSCRIPT;
        scriptWrapper.addToLineMap(this._lineMap);
        //var dummy = scriptWrapper.sourceText;
        this.seal();
    }

    ++this.scriptCount;
}

ScriptInstance.prototype.onScriptInvalidated =
function si_invalidated (scriptWrapper)
{
    //dd ("script invalidated");
    scriptWrapper.clearBreakpoints();
    --this.scriptCount;
}

ScriptInstance.prototype.__defineGetter__ ("sourceText", si_gettext);
function si_gettext ()
{
    if (!("_sourceText" in this))
        this._sourceText = new SourceText (this);

    return this._sourceText;
}

ScriptInstance.prototype.__defineGetter__ ("lineMap", si_linemap);
function si_linemap()
{
    if (!this._lineMapInited)
    {
        for (var i in this.functions)
        {
            if (this.functions[i].jsdScript.isValid)
                this.functions[i].addToLineMap(this._lineMap);
        }
        
        for (var fbp in console.fbreaks)
        {
            var fbreak = console.fbreaks[fbp];
            if (fbreak.url == this.url)
                arrayOrFlag (this._lineMap, fbreak.lineNumber - 1, LINE_FBREAK);
        }
        
        this._lineMapInited = true;
    }

    return this._lineMap;            
}

ScriptInstance.prototype.isLineExecutable =
function si_isexe (line)
{
    if (this.topLevel && this.topLevel.jsdScript.isValid &&
        this.topLevel.jsdScript.isLineExecutable (line, PCMAP_SOURCETEXT))
    {
        return true;
    }
    
    for (var f in this.functions)
    {
        var jsdScript = this.functions[f].jsdScript;
        if (line >= jsdScript.baseLineNumber &&
            line <= jsdScript.baseLineNumber + jsdScript.lineExtent &&
            jsdScript.isLineExecutable (line, PCMAP_SOURCETEXT))
        {
            return true;
        }
    }

    return false;
}

ScriptInstance.prototype.hasBreakpoint =
function si_hasbp (line)
{
    return Boolean (this.getBreakpoint(line));
}

ScriptInstance.prototype.getBreakpoint =
function si_getbp (line)
{
    for (var b in console.breaks)
    {
        if (console.breaks[b].scriptWrapper.scriptInstance == this)
        {
            if (typeof line == "undefined")
                return true;
            
            var jsdScript = console.breaks[b].scriptWrapper.jsdScript;
            if (jsdScript.pcToLine(console.breaks[b].pc, PCMAP_SOURCETEXT) ==
                line)
            {
                return console.breaks[b];
            }
        }
    }

    return false;
}

ScriptInstance.prototype.setBreakpoint =
function si_setbp (line, parentBP)
{
    function setBP (scriptWrapper)
    {
        var jsdScript = scriptWrapper.jsdScript;
        if (!jsdScript.isValid)
            return false;

        if (line >= jsdScript.baseLineNumber &&
            line <= jsdScript.baseLineNumber + jsdScript.lineExtent &&
            jsdScript.isLineExecutable (line, PCMAP_SOURCETEXT))
        {
            scriptWrapper.setBreakpoint(jsdScript.lineToPc(line,
                                                           PCMAP_SOURCETEXT),
                                        parentBP);
            return true;
        }
        return false;
    };

    var found;
    
    if (this.topLevel)
        found = setBP (this.topLevel);
    for (var f in this.functions)
        found |= setBP (this.functions[f]);

    if (this._lineMapInited && found)
        arrayOrFlag(this._lineMap, line - 1, LINE_BREAK);

    return found;
}

ScriptInstance.prototype.clearBreakpoint =
function si_setbp (line)
{
    var found = false;
    
    function clearBP (scriptWrapper)
    {
        var jsdScript = scriptWrapper.jsdScript;
        if (!jsdScript.isValid)
            return;
        
        var pc = jsdScript.lineToPc(line, PCMAP_SOURCETEXT);
        if (line >= jsdScript.baseLineNumber &&
            line <= jsdScript.baseLineNumber + jsdScript.lineExtent &&
            scriptWrapper.hasBreakpoint(pc))
        {
            found |= scriptWrapper.clearBreakpoint(pc);
        }
    };

    if (this._lineMapInited)
        arrayAndFlag(this._lineMap, line - 1, ~LINE_BREAK);    
    
    if (this.topLevel)
        clearBP (this.topLevel);

    for (var f in this.functions)
        clearBP (this.functions[f]);

    return found;
}

ScriptInstance.prototype.getScriptWrapperAtLine =
function si_getscript (line)
{
    var targetScript = null;
    var scriptWrapper;
    
    if (this.topLevel)
    {
        scriptWrapper = this.topLevel;
        if (line >= scriptWrapper.jsdScript.baseLineNumber &&
            line <= scriptWrapper.jsdScript.baseLineNumber +
            scriptWrapper.jsdScript.lineExtent)
        {
            targetScript = scriptWrapper;
        }
    }    

    for (var f in this.functions)
    {
        scriptWrapper = this.functions[f];
        if ((line >= scriptWrapper.jsdScript.baseLineNumber &&
             line <= scriptWrapper.jsdScript.baseLineNumber +
             scriptWrapper.jsdScript.lineExtent) &&
            (!targetScript ||
             scriptWrapper.jsdScript.lineExtent <
             targetScript.jsdScript.lineExtent))
        {
            targetScript = scriptWrapper;
        }
    }

    return targetScript;
}

ScriptInstance.prototype.containsScriptTag =
function si_contains (tag)
{
    return ((this.topLevel && this.topLevel.tag == tag) ||
            (tag in this.functions));
}

ScriptInstance.prototype.guessFunctionNames =
function si_guessnames ()
{
    var sourceLines = this._sourceText.lines;
    var context = console.prefs["guessContext"];
    var pattern = new RegExp (console.prefs["guessPattern"]);
    var scanText;
    
    function getSourceContext (end)
    {
        var startLine = end - context;
        if (startLine < 0)
            startLine = 0;

        var text = "";
        
        for (i = startLine; i <= targetLine; ++i)
            text += String(sourceLines[i]);
    
        var pos = text.lastIndexOf ("function");
        if (pos != -1)
            text = text.substring(0, pos);

        return text;
    };
        
    for (var i in this.functions)
    {
        var scriptWrapper = this.functions[i];
        if (scriptWrapper.jsdScript.functionName != "anonymous")
            continue;
        
        var targetLine = scriptWrapper.jsdScript.baseLineNumber;
        if (targetLine > sourceLines.length)
        {
            dd ("not enough source to guess function at line " + targetLine);
            return;
        }

        scanText = getSourceContext(targetLine);
        var ary = scanText.match (pattern);
        if (ary)
        {
            if ("charset" in this._sourceText)
                ary[1] = toUnicode(ary[1], this._sourceText.charset);
            
            scriptWrapper.functionName = getMsg(MSN_FMT_GUESSEDNAME, ary[1]);
            this.isGuessedName = true;
        }
        else
        {
            dd ("unable to guess function name based on text ``" + 
                scanText + "''");
        }
    }

    dispatch ("hook-guess-complete", { scriptInstance: this });
}

function ScriptWrapper (jsdScript)
{    
    this.jsdScript = jsdScript;
    this.tag = jsdScript.tag;
    this.functionName = jsdScript.functionName;
    this.breakpointCount = 0;
    this._lineMap = null;
    this.breaks = new Object();
}

ScriptWrapper.prototype.__defineGetter__ ("sourceText", sw_getsource);
function sw_getsource ()
{
    if (!("_sourceText" in this))
    {
        if (!this.jsdScript.isValid)
            return null;
        this._sourceText = new PPSourceText(this);
    }
    
    return this._sourceText;
}

ScriptWrapper.prototype.__defineGetter__ ("lineMap", sw_linemap);
function sw_linemap ()
{
    if (!this._lineMap)
        this.addToLineMap(this._lineMap);
    
    return this._lineMap;
}

ScriptWrapper.prototype.hasBreakpoint =
function sw_hasbp (pc)
{
    var key = this.jsdScript.tag + ":" + pc;
    return key in console.breaks;
}

ScriptWrapper.prototype.getBreakpoint =
function sw_hasbp (pc)
{
    var key = this.jsdScript.tag + ":" + pc;
    if (key in console.breaks)
        return console.breaks[key];
    
    return null;
}

ScriptWrapper.prototype.setBreakpoint =
function sw_setbp (pc, parentBP)
{
    var key = this.jsdScript.tag + ":" + pc;
    
    //dd ("setting breakpoint in " + this.functionName + " " + key);
    
    if (key in console.breaks)
        return null;

    var brk = new BreakInstance (parentBP, this, pc);
    console.breaks[key] = brk;
    this.breaks[key] = brk;
    
    if (parentBP)
    {
        parentBP.childrenBP[key] = brk;
        brk.lineNumber = parentBP.lineNumber;
        brk.url = parentBP.url;
    }

    if ("_sourceText" in this)
    {
        var line = this.jsdScript.pcToLine(brk.pc, PCMAP_PRETTYPRINT);
        arrayOrFlag (this._sourceText.lineMap, line - 1, LINE_BREAK);
    }

    ++this.scriptInstance.breakpointCount;
    ++this.breakpointCount;
    
    if (this.scriptInstance._lineMapInited)
    {
        line = this.jsdScript.pcToLine (pc, PCMAP_SOURCETEXT);
        arrayOrFlag (this.scriptInstance._lineMap, line - 1, LINE_BREAK);
    }

    dispatch ("hook-break-set", { breakWrapper: brk });
    
    return brk;
}

ScriptWrapper.prototype.clearBreakpoints =
function sw_clearbps ()
{
    var found = false;
    
    for (b in this.breaks)
        found |= this.clearBreakpoint(this.breaks[b].pc);

    return found;
}

ScriptWrapper.prototype.clearBreakpoint =
function sw_clearbp (pc)
{
    var key = this.jsdScript.tag + ":" + pc;
    if (!(key in console.breaks))
        return false;

    var brk = console.breaks[key];

    if ("propsWindow" in brk)
        brk.propsWindow.close();
    
    delete console.breaks[key];
    delete this.breaks[key];

    if (brk.parentBP)
        delete brk.parentBP.childrenBP[key];

    var line;
    
    if ("_sourceText" in this && this.jsdScript.isValid)
    {
        line = this.jsdScript.pcToLine(brk.pc, PCMAP_PRETTYPRINT);
        this._sourceText.lineMap[line - 1] &= ~LINE_BREAK;
    }
    
    --this.scriptInstance.breakpointCount;
    --this.breakpointCount;

    if (this.scriptInstance._lineMapInited)
    {
        if (this.jsdScript.isValid)
        {
            line = this.jsdScript.pcToLine (pc, PCMAP_SOURCETEXT);
            if (!this.scriptInstance.hasBreakpoint(line))
                this.scriptInstance._lineMap[line - 1] &= ~LINE_BREAK;
        }
        else
        {
            /* script is gone, no way to find out where the break actually
             * was, so we have to redo the whole map. */
            this.scriptInstance._lineMapInited = false;
            this.scriptInstance._lineMap.length = 0;
            var dummy = this.scriptInstance.lineMap;
        }
    }

    dispatch ("hook-break-clear", { breakWrapper: brk });

    if (this.jsdScript.isValid)
        this.jsdScript.clearBreakpoint (pc);

    return true;
}

ScriptWrapper.prototype.addToLineMap =
function sw_addmap (lineMap)
{    
    var jsdScript = this.jsdScript;
    var end = jsdScript.baseLineNumber + jsdScript.lineExtent;
    for (var i = jsdScript.baseLineNumber; i < end; ++i)
    {
        if (jsdScript.isLineExecutable(i, PCMAP_SOURCETEXT))
            arrayOrFlag (lineMap, i - 1, LINE_BREAKABLE);
    }

    for (i in this.breaks)
    {
        var line = jsdScript.pcToLine(this.breaks[i].pc, PCMAP_SOURCETEXT);
        arrayOrFlag (lineMap, line - 1, LINE_BREAK);
    }
}

function getScriptWrapper(jsdScript)
{
    if (!ASSERT(jsdScript, "getScriptWrapper: null jsdScript"))
        return null;
    
    var tag = jsdScript.tag;
    if (tag in console.scriptWrappers)
        return console.scriptWrappers[tag];

    dd ("Can't find a wrapper for " + formatScript(jsdScript));
    return null;
}

function BreakInstance (parentBP, scriptWrapper, pc)
{
    this._enabled = true;
    this.parentBP = parentBP;
    this.scriptWrapper = scriptWrapper;
    this.pc = pc;
    this.url = scriptWrapper.jsdScript.fileName;
    this.lineNumber = scriptWrapper.jsdScript.pcToLine (pc, PCMAP_SOURCETEXT);
    this.oneTime = false;
    this.triggerCount = 0;
    
    scriptWrapper.jsdScript.setBreakpoint (pc);
}

BreakInstance.prototype.__defineGetter__ ("jsdURL", bi_getURL);
function bi_getURL ()
{
    return ("x-jsd:break?url=" + escape(this.url) +
            "&lineNumber=" + this.lineNumber +
            "&conditionEnabled=" + this.conditionEnabled +
            "&condition=" + escape(this.condition) +
            "&passExceptions=" + this.passExceptions +
            "&logResult=" + this.logResult +
            "&resultAction=" + this.resultAction +
            "&enabled=" + this.enabled);
}

BreakInstance.prototype.clearBreakpoint =
function bi_clear()
{
    this.scriptWrapper.clearBreakpoint(this.pc);
}

BreakInstance.prototype.__defineGetter__ ("enabled", bi_getEnabled);
function bi_getEnabled ()
{
    return this._enabled;
}

BreakInstance.prototype.__defineSetter__ ("enabled", bi_setEnabled);
function bi_setEnabled (state)
{
    if (state != this._enabled)
    {
        this._enabled = state;
        if (state)
            this.jsdScript.setBreakpoint(pc);
        else
            this.jsdScript.clearBreakpoint(pc);
    }
    
    return state;
}

BreakInstance.prototype.__defineGetter__ ("conditionEnabled", bi_getCondEnabled);
function bi_getCondEnabled ()
{
    if ("_conditionEnabled" in this)
        return this._conditionEnabled;
    
    if (this.parentBP)
        return this.parentBP.conditionEnabled;

    return false;
}

BreakInstance.prototype.__defineSetter__ ("conditionEnabled", bi_setCondEnabled);
function bi_setCondEnabled (state)
{
    return this._conditionEnabled = state;
}

BreakInstance.prototype.__defineGetter__ ("condition", bi_getCondition);
function bi_getCondition ()
{
    if ("_condition" in this)
        return this._condition;
    
    if (this.parentBP)
        return this.parentBP.condition;

    return "";
}

BreakInstance.prototype.__defineSetter__ ("condition", bi_setCondition);
function bi_setCondition (value)
{
    return this._condition = value;
}


BreakInstance.prototype.__defineGetter__ ("passExceptions", bi_getException);
function bi_getException ()
{
    if ("_passExceptions" in this)
        return this._passExceptions;
    
    if (this.parentBP)
        return this.parentBP.passExceptions;

    return false;
}

BreakInstance.prototype.__defineSetter__ ("passExceptions", bi_setException);
function bi_setException (state)
{
    return this._passExceptions = state;
}


BreakInstance.prototype.__defineGetter__ ("logResult", bi_getLogResult);
function bi_getLogResult ()
{
    if ("_logResult" in this)
        return this._logResult;
    
    if (this.parentBP)
        return this.parentBP.logResult;

    return false;
}

BreakInstance.prototype.__defineSetter__ ("logResult", bi_setLogResult);
function bi_setLogResult (state)
{
    return this._logResult = state;
}


BreakInstance.prototype.__defineGetter__ ("resultAction", bi_getResultAction);
function bi_getResultAction ()
{
    if ("_resultAction" in this)
        return this._resultAction;
    
    if (this.parentBP)
        return this.parentBP.resultAction;

    return BREAKPOINT_STOPALWAYS;
}

BreakInstance.prototype.__defineSetter__ ("resultAction", bi_setResultAction);
function bi_setResultAction (state)
{
    return this._resultAction = state;
}

function FutureBreakpoint (url, lineNumber)
{
    this.url = url;
    this.lineNumber = lineNumber;
    this.enabled = true;
    this.childrenBP = new Object;
    this.conditionEnabled = false;
    this.condition = "";
    this.passExceptions = false;
    this.logResult = false;
    this.resultAction = BREAKPOINT_STOPALWAYS;
}

FutureBreakpoint.prototype.__defineGetter__ ("jsdURL", fb_getURL);
function fb_getURL ()
{
    return ("x-jsd:fbreak?url=" + escape(this.url) +
            "&lineNumber=" + this.lineNumber +
            "&conditionEnabled=" + this.conditionEnabled +
            "&condition=" + escape(this.condition) +
            "&passExceptions=" + this.passExceptions +
            "&logResult=" + this.logResult +
            "&resultAction=" + this.resultAction +
            "&enabled=" + this.enabled);
}

FutureBreakpoint.prototype.clearFutureBreakpoint =
function fb_clear ()
{
    clearFutureBreakpoint (this.url, this.lineNumber);
}

FutureBreakpoint.prototype.resetInstances =
function fb_reseti ()
{
    for (var url in console.scriptManagers)
    {
        if (url.search(this.url) != -1)
            console.scriptManagers[url].setBreakpoint(this.lineNumber);
    }
}

FutureBreakpoint.prototype.clearInstances =
function fb_cleari ()
{
    for (var url in console.scriptManagers)
    {
        if (url.search(this.url) != -1)
            console.scriptManagers[url].clearBreakpoint(this.lineNumber);
    }
}

function testBreakpoint(currentFrame, rv)
{
    var tag = currentFrame.script.tag;
    if (!(tag in console.scriptWrappers))
        return -1;
    
    var scriptWrapper = console.scriptWrappers[tag];
    var breakpoint = scriptWrapper.getBreakpoint(currentFrame.pc);
    if (!ASSERT(breakpoint, "can't find breakpoint for " +
                formatFrame(currentFrame)))
    {
        return -1;
    }

    ++breakpoint.triggerCount;
    if ("propsWindow" in breakpoint)
        breakpoint.propsWindow.onBreakpointTriggered();
    
    if (breakpoint.oneTime)
        scriptWrapper.clearBreakpoint(currentFrame.pc);

    if (breakpoint.conditionEnabled && breakpoint.condition)
    {
        var result = new Object();
        var script = "var __trigger__ = function (__count__) {" +
            breakpoint.condition + "}; __trigger__.apply(this, [" +
            breakpoint.triggerCount + "]);";
        if (!currentFrame.eval (script,
                                JSD_URL_SCHEME + "breakpoint-condition",
                                1, result))
        {
            /* condition raised an exception */
            if (breakpoint.passExceptions)
            {
                rv.value = result.value;
                return RETURN_THROW;
            }
                
            display (MSG_ERR_CONDITION_FAILED, MT_ERROR);
            display (formatException(result.value.getWrappedValue()), MT_ERROR);
        }
        else
        {
            /* condition executed ok */
            if (breakpoint.logResult)
            {
                display (result.value.stringValue, MT_LOG);
            }

            if (breakpoint.resultAction == BREAKPOINT_EARLYRETURN)
            {
                rv.value = result.value;
                return RETURN_VALUE;
            }

            if (breakpoint.resultAction == BREAKPOINT_STOPNEVER ||
                (breakpoint.resultAction == BREAKPOINT_STOPTRUE &&
                 !result.value.booleanValue))
            {
                return RETURN_CONTINUE;
            }            
        }
    }

    return -1;
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
            var bpResult = testBreakpoint(frame, rv);
            if (bpResult != -1)
                return bpResult;
            tn = MSG_VAL_BREAKPOINT;
            break;
        case jsdIExecutionHook.TYPE_DEBUG_REQUESTED:
            tn = MSG_VAL_DEBUG;
            break;
        case jsdIExecutionHook.TYPE_DEBUGGER_KEYWORD:
            tn = MSG_VAL_DEBUGGER;
            break;
        case jsdIExecutionHook.TYPE_THROW:
            dd (dumpObjectTree(rv));
            display (getMsg(MSN_EXCEPTION_TRACE,
                            [rv.value.stringValue, formatFrame(frame)]),
                     MT_ETRACE);
            if (rv.value.jsClassName == "Error")
                display (formatProperty(rv.value.getProperty("message")));

            if (console.throwMode != TMODE_BREAK)
                return jsdIExecutionHook.RETURN_CONTINUE_THROW;

            console.currentException = rv.value;
            retcode = jsdIExecutionHook.RETURN_CONTINUE_THROW;
            
            tn = MSG_VAL_THROW;
            break;
        case jsdIExecutionHook.TYPE_INTERRUPTED:
            if (!frame.script.functionName && 
                isURLFiltered(frame.script.fileName))
            {
                dd ("filtered url: " + frame.script.fileName);
                frame.script.flags |= SCRIPT_NOPROFILE | SCRIPT_NODEBUG;
                return retcode;
            }
            
            var line;
            if (console.prefs["prettyprint"])
                line = frame.script.pcToLine (frame.pc, PCMAP_PRETTYPRINT);
            else
                line = frame.line;
            if (console._stepPast == frame.script.fileName + line)
                return retcode;
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
    
    /* build an array of frames */
    console.frames = new Array(frame);
    
    while ((frame = frame.callingFrame))
        console.frames.push(frame);
    
    console.trapType = type;
    
    try
    {    
        console.jsds.enterNestedEventLoop({onNest: eventLoopNested}); 
    }
    catch (ex)
    {
        dd ("caught " + ex + " while nested");
    }
    
    /* execution pauses here until someone calls exitNestedEventLoop() */

    clearCurrentFrame();
    rv.value = ("currentException" in console) ? console.currentException : null;

    delete console.frames;
    delete console.trapType;
    delete console.currentException;
    $ = new Array();
    
    dispatch ("hook-debug-continue");

    if (tn)
        display (getMsg(MSN_CONT, tn), MT_CONT);

    return console._continueCodeStack.pop();
}

function eventLoopNested ()
{
    window.focus();
    window.getAttention();

    dispatch ("hook-debug-stop");
}

function getCurrentFrame()
{
    if ("frames" in console)
        return console.frames[console._currentFrameIndex];

    return null;
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
    dispatch ("set-eval-obj", { jsdValue: cf });
    console.stopFile = (cf.isNative) ? MSG_URL_NATIVE : cf.script.fileName;
    console.stopLine = cf.line;
    delete console._pp_stopLine;
    return cf;
}

function clearCurrentFrame ()
{
    if (!console.frames)
        throw new BadMojo (ERR_NO_STACK);

    if (console.currentEvalObject instanceof jsdIStackFrame)
        dispatch ("set-eval-obj", { jsdValue: console.jsdConsole });
    
    delete console.stopLine;
    delete console._pp_stopLine;
    delete console.stopFile;
    delete console._currentFrameIndex;
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
    
    if (flags & PROP_ENUMERATE)
        s += MSG_VF_ENUMERABLE;
    if (flags & PROP_READONLY)
        s += MSG_VF_READONLY;        
    if (flags & PROP_PERMANENT)
        s += MSG_VF_PERMANENT;
    if (flags & PROP_ALIAS)
        s += MSG_VF_ALIAS;
    if (flags & PROP_ARGUMENT)
        s += MSG_VF_ARGUMENT;
    if (flags & PROP_VARIABLE)
        s += MSG_VF_VARIABLE;
    if (flags & PROP_ERROR)
        s += MSG_VF_ERROR;
    if (flags & PROP_EXCEPTION)
        s += MSG_VF_EXCEPTION;
    if (flags & PROP_HINTED)
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

    var functionName;
    if (script.tag in console.scriptWrappers)
        functionName = console.scriptWrappers[script.tag].functionName;
    else
        functionName = script.functionName;
    return getMsg (MSN_FMT_SCRIPT, [functionName, script.fileName]);
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
            var strval = v.stringValue;
            if (strval.length > console.prefs["maxStringLength"])
                strval = getMsg(MSN_FMT_LONGSTR, strval.length);
            else
                strval = strval.quote()
            value = strval;
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

function displaySourceContext (sourceText, line, contextLines)
{
    function onSourceLoaded (status)
    {
        if (status == Components.results.NS_OK)
            displaySourceContext (sourceText, line, contextLines);
    }
    
    if (sourceText.isLoaded)
    {
        for (var i = line - contextLines; i <= line + contextLines; ++i)
        {
            if (i > 0 && i < sourceText.lines.length)
            {
                var sourceLine;
                if ("charset" in sourceText)
                {
                    sourceLine = toUnicode(sourceText.lines[i - 1],
                                     sourceText.charset);
                }
                else
                {
                    sourceLine = sourceText.lines[i - 1];
                }
                
                display (getMsg(MSN_SOURCE_LINE, [zeroPad (i, 3), sourceLine]),
                         i == line ? MT_STEP : MT_SOURCE);
            }
        }
    }
    else
    {
        sourceText.loadSource (onSourceLoaded);
    }
}
    
function displayFrame (jsdFrame, idx, showHeader, sourceContext)
{
    if (typeof idx == "undefined")
    {
        for (idx = 0; idx < console.frames.length; ++idx)
            if (jsdFrame == console.frames[idx])
                break;
    
        if (idx >= console.frames.length)
            idx = MSG_VAL_UNKNOWN;
    }

    if (typeof showHeader == "undefined")
        showHeader = true;

    if (typeof sourceContext == "undefined")
        sourceContext = null;

    display(getMsg(MSN_FMT_FRAME_LINE, [idx, formatFrame(jsdFrame)]));

    if (!jsdFrame.isNative && sourceContext != null)
    {
        var jsdScript = jsdFrame.script;
        var scriptWrapper = getScriptWrapper(jsdScript);

        if (!ASSERT(scriptWrapper, "Couldn't get a script wrapper"))
            return;
        if (console.prefs["prettyprint"] && jsdScript.isValid)
        {
            displaySourceContext (scriptWrapper.sourceText,
                                  jsdScript.pcToLine(jsdFrame.pc,
                                                     PCMAP_PRETTYPRINT),
                                  sourceContext);
        }
        else
        {
            displaySourceContext (scriptWrapper.scriptInstance.sourceText,
                                  jsdFrame.line, sourceContext);
        }
    }
}

function getFutureBreakpoint (urlPattern, lineNumber)
{
    var key = urlPattern + "#" + lineNumber;
    if (key in console.fbreaks)
        return console.fbreaks[key];
    
    return null;
}

function setFutureBreakpoint (urlPattern, lineNumber)
{
    var key = urlPattern + "#" + lineNumber;

    if (key in console.fbreaks)
        return false;
    
    for (var url in console.scriptManagers)
    {
        if (url == urlPattern)
            console.scriptManagers[url].noteFutureBreakpoint(lineNumber, true);
    }    

    var fbreak = new FutureBreakpoint (urlPattern, lineNumber);
    console.fbreaks[key] = fbreak;

    dispatch ("hook-fbreak-set", { fbreak: fbreak });

    return fbreak;
}

function clearFutureBreakpoint (urlPattern, lineNumber)
{
    var key = urlPattern + "#" + lineNumber;
    if (!(key in console.fbreaks))
        return false;

    var i;
    var fbreak = console.fbreaks[key];
    if ("propsWindow" in fbreak)
        fbreak.propsWindow.close();
    
    delete console.fbreaks[key];

    for (i in fbreak.childrenBP)
        fbreak.childrenBP[i].parentBP = null;

    for (var url in console.scriptManagers)
    {
        if (url.search(urlPattern) != -1)
            console.scriptManagers[url].noteFutureBreakpoint(lineNumber, false);
    }    

    dispatch ("hook-fbreak-clear", { fbreak: fbreak });

    return true;
}
