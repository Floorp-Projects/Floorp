/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is The JavaScript Debugger.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>, original author
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

const __vnk_version        = "0.9.83";
const __vnk_requiredLocale = "0.9.83";
var   __vnk_versionSuffix  = "";

const __vnk_counter_url = 
"http://www.hacksrus.com/~ginda/venkman/launch-counter/next-sequence.cgi"

/* dd is declared first in venkman-utils.js */
var warn;
var ASSERT;

if (DEBUG)
{
    _dd_pfx = "vnk: ";
    warn = function (msg) { dumpln ("** WARNING " + msg + " **"); }
    ASSERT = function (expr, msg) {
                 if (!expr) {
                     dump ("** ASSERTION FAILED: " + msg + " **\n" + 
                           getStackTrace() + "\n"); 
                     return false;
                 } else {
                     return true;
                 }
             }
}    
else
    dd = warn = ASSERT = function (){};


const MAX_WORD_LEN = 20; /* number of characters to display before forcing a 
                          * potential word break point (in the console.) */

const LINE_BREAKABLE = 0x01;
const LINE_BREAK     = 0x02;
const LINE_FBREAK    = 0x04;

var console = new Object();

console.initialized = false;

/* |this|less functions */

function setStopState(state)
{
    var tb = console.ui["stop-button"];
    if (state)
    {
        console.jsds.interruptHook = console.executionHook;
        tb.setAttribute("willStop", "true");
    }
    else
    {
        console.jsds.interruptHook = null;
        tb.removeAttribute("willStop");
    }
}

function setProfileState(state)
{
    var tb = console.ui["profile-button"];
    if (state)
    {
        console.profiler.enabled = true;
        tb.setAttribute("profile", "true");
    }
    else
    {
        console.profiler.enabled = false;
        tb.removeAttribute("profile");
    }
}

function enableDebugCommands()
{
    var cmds = console.commandManager.list ("", CMD_NEED_STACK);
    for (var i in cmds)
        cmds[i].enabled = true;
    
    cmds = console.commandManager.list ("", CMD_NO_STACK);
    for (i in cmds)
        cmds[i].enabled = false;
}

function disableDebugCommands()
{
    var cmds = console.commandManager.list ("", CMD_NEED_STACK);
    for (var i in cmds)
        cmds[i].enabled = false;
    
    cmds = console.commandManager.list ("", CMD_NO_STACK);
    for (i in cmds)
        cmds[i].enabled = true;
}

function isURLVenkman (url)
{
    return (url.search (/^chrome:\/\/venkman/) == 0 &&
            url.search (/test/) == -1);
}

function isURLFiltered (url)
{
    return (!url ||
            (console.prefs["enableChromeFilter"] &&
             (url.search(/^chrome:/) == 0 ||
              ("componentPath" in console &&
               url.indexOf(console.componentPath) == 0))) ||
            (url.search (/^chrome:\/\/venkman/) == 0 &&
             url.search (/test/) == -1));
}

function toUnicode (msg, charset)
{
    try
    {
        console.ucConverter.charset = charset;
        return console.ucConverter.ConvertToUnicode(msg);
    }
    catch (ex)
    {
        dd ("caught " + ex + " converting to unicode");
        return msg;
    }
}

function fromUnicode (msg, charset)
{
    try
    {
        console.ucConverter.charset = charset;
        return console.ucConverter.ConvertFromUnicode(msg);
    }
    catch (ex)
    {
        dd ("caught " + ex + " converting from unicode");
        return msg;
    }
}

function displayUsageError (e, details)
{
    if (!("isInteractive" in e) || !e.isInteractive)
    {
        var caller = Components.stack.caller.caller;
        if (caller.name == "dispatch")
            caller = caller.caller;
        var error = new Error (details);
        error.fileName = caller.filename;
        error.lineNumber = caller.lineNumber;
        error.name = caller.name;
        display (formatException(error), MT_ERROR);
    }
    else
    {
        display (details, MT_ERROR);
    }
    
    display (getMsg(MSN_FMT_USAGE, [e.command.name, e.command.usage]), MT_USAGE);
}

function dispatch (text, e, flags)
{
    if (!e)
        e = new Object();

    if (!("inputData" in e))
        e.inputData = "";
    
    /* split command from arguments */
    var ary = text.match (/(\S+) ?(.*)/);
    e.commandText = ary[1];
    e.inputData =  ary[2] ? stringTrim(ary[2]) : "";
    
    /* list matching commands */
    ary = console.commandManager.list (e.commandText, flags);
    var rv = null;
    var i;
    
    switch (ary.length)
    {            
        case 0:
            /* no match, try again */
            display (getMsg(MSN_NO_CMDMATCH, e.commandText), MT_ERROR);
            break;
            
        case 1:
            /* one match, good for you */
            var ex;
            try
            {
                rv = dispatchCommand(ary[0], e, flags);
            }
            catch (ex)
            {
                display (getMsg(MSN_ERR_INTERNAL_DISPATCH, ary[0].name),
                         MT_ERROR);
                display (formatException(ex), MT_ERROR);
                dd (formatException(ex), MT_ERROR);
                if (typeof ex == "object" && "stack" in ex)
                    dd (ex.stack);
            }
            break;
            
        default:
            /* more than one match, show the list */
            var str = "";
            for (i in ary)
                str += (str) ? MSG_COMMASP + ary[i].name : ary[i].name;
            display (getMsg (MSN_ERR_AMBIGCOMMAND,
                             [e.commandText, ary.length, str]), MT_ERROR);
    }

    return rv;
}

function dispatchCommand (command, e, flags)
{
    function callHooks (command, isBefore)
    {
        var names, hooks;
        
        if (isBefore)
            hooks = command.beforeHooks;
        else
            hooks = command.afterHooks;

        for (var h in hooks)
        {
            if ("dbgDispatch" in console && console.dbgDispatch)
            {
                dd ("calling " + (isBefore ? "before" : "after") + 
                    " hook " + h);
            }
            try
            {
                hooks[h](e);
            }
            catch (ex)
            {
                if (e.command.name != "hook-session-display")
                {
                    display (getMsg(MSN_ERR_INTERNAL_HOOK, h), MT_ERROR);
                    display (formatException(ex), MT_ERROR);
                }
                else
                {
                    dd (getMsg(MSN_ERR_INTERNAL_HOOK, h));
                }

                dd ("Caught exception calling " +
                    (isBefore ? "before" : "after") + " hook " + h);
                dd (formatException(ex));
                if (typeof ex == "object" && "stack" in ex)
                    dd (ex.stack);
                else
                    dd (getStackTrace());
            }
        }
    };
    
    e.command = command;

    if ((e.command.flags & CMD_NEED_STACK) &&
        (!("frames" in console)))
    {
        /* doc, it hurts when I do /this/... */
        display (MSG_ERR_NO_STACK, MT_ERROR);
        return null;
    }
    
    if (!e.command.enabled)
    {
        /* disabled command */
        display (getMsg(MSN_ERR_DISABLED, e.command.name),
                 MT_ERROR);
        return null;
    }
    
    var h, i;
    
    if ("beforeHooks" in e.command)
        callHooks (e.command, true);
    
    if (typeof e.command.func == "function")
    {
        /* dispatch a real function */
        if (e.command.usage)
            console.commandManager.parseArguments (e);
        if ("parseError" in e)
        {
            displayUsageError (e, e.parseError);
        }
        else
        {
            if ("dbgDispatch" in console && console.dbgDispatch)
            {
                var str = "";
                for (i = 0; i < e.command.argNames.length; ++i)
                {
                    var name = e.command.argNames[i];
                    if (name in e)
                        str += " " + name + ": " + e[name];
                    else if (name != ":")
                        str += " ?" + name;
                }
                dd (">>> " + e.command.name + str + " <<<");
                e.returnValue = e.command.func(e);
                /* set console.lastEvent *after* dispatching, so the dispatched
                 * function actually get's a chance to see the last event. */
                console.lastEvent = e;
            }
            else
            {
                e.returnValue = e.command.func(e);
            }

        }
    }
    else if (typeof e.command.func == "string")
    {
        /* dispatch an alias (semicolon delimited list of subcommands) */
        var commandList = e.command.func.split(";");
        for (i = 0; i < commandList.length; ++i)
        {
            var newEvent = Clone (e);
            delete newEvent.command;            
            commandList[i] = stringTrim(commandList[i]);
            dispatch (commandList[i], newEvent, flags);
        }
    }
    else
    {
        display (getMsg(MSN_ERR_NOTIMPLEMENTED, e.command.name),
                 MT_ERROR);
        return null;
    }

    if ("afterHooks" in e.command)
        callHooks (e.command, false);

    return ("returnValue" in e) ? e.returnValue : null;
}

function feedback(e, message, msgtype)
{
    if ("isInteractive" in e && e.isInteractive)
        display (message, msgtype);
}

function display(message, msgtype)
{
    if (typeof message == "undefined")
        throw new RequiredParam ("message");

    if (typeof message != "string" &&
        !(message instanceof Components.interfaces.nsIDOMHTMLElement))
        throw new InvalidParam ("message", message);

    if ("capturedMsgs" in console)
        console.capturedMsgs.push ([message, msgtype]);
        
    dispatchCommand(console.coDisplayHook, 
                    { message: message, msgtype: msgtype });
}

function evalInDebuggerScope (script, rethrow)
{
    try
    {
        return console.doEval (script);
    }
    catch (ex)
    {
        if (rethrow)
            throw ex;

        display (formatEvalException(ex), MT_ERROR);
        return null;
    }
}

function evalInTargetScope (script, rethrow)
{
    if (!console.frames)
    {
        display (MSG_ERR_NO_STACK, MT_ERROR);
        return null;
    }
    
    var rval = new Object();
    
    if (!getCurrentFrame().eval (script, MSG_VAL_CONSOLE, 1, rval))
    {
        if (rethrow)
            throw rval.value;
        
        //dd ("exception: " + dumpObjectTree(rval.value));
        display (formatEvalException (rval.value), MT_ERROR);
        return null;
    }

    return ("value" in rval) ? rval.value : null;
}

function formatException (ex)
{
    if (ex instanceof BadMojo)
        return getMsg (MSN_FMT_BADMOJO,
                       [ex.errno, ex.message, ex.fileName, ex.lineNumber, 
                        ex.functionName]);

    if (ex instanceof Error)
        return getMsg (MSN_FMT_JSEXCEPTION, [ex.name, ex.message, ex.fileName, 
                                             ex.lineNumber]);

    return String(ex);
}

function formatEvalException (ex)
{
    if (ex instanceof BadMojo || ex instanceof Error)
        return formatException (ex);
    else if (ex instanceof jsdIValue)
        ex = ex.stringValue;
    
    return getMsg (MSN_EVAL_THREW,  String(ex));
}

function init()
{
    dd ("init {");
    
    var i;
    const WW_CTRID = "@mozilla.org/embedcomp/window-watcher;1";
    const nsIWindowWatcher = Components.interfaces.nsIWindowWatcher;

    console.windowWatcher =
        Components.classes[WW_CTRID].getService(nsIWindowWatcher);

    const UC_CTRID = "@mozilla.org/intl/scriptableunicodeconverter";
    const nsIUnicodeConverter = 
        Components.interfaces.nsIScriptableUnicodeConverter;
    console.ucConverter =
        Components.classes[UC_CTRID].getService(nsIUnicodeConverter);

    console.baseWindow = getBaseWindowFromWindow(window);
    console.mainWindow = window;
    console.dnd = nsDragAndDrop;
    console.currentEvalObject = console;
    
    console.files = new Object();
    console.pluginState = new Object();

    console._statusStack = new Array();
    console._lastStackDepth = -1;

    initMsgs();
    initPrefs();
    initCommands();
    initJSDURL();
    
    /* Some commonly used commands, cached now, for use with dispatchCommand. */
    var cm = console.commandManager;
    console.coManagerCreated    = cm.commands["hook-script-manager-created"];
    console.coManagerDestroyed  = cm.commands["hook-script-manager-destroyed"];
    console.coInstanceCreated   = cm.commands["hook-script-instance-created"];
    console.coInstanceSealed    = cm.commands["hook-script-instance-sealed"];
    console.coInstanceDestroyed = cm.commands["hook-script-instance-destroyed"];
    console.coDisplayHook       = cm.commands["hook-session-display"];
    console.coFindScript        = cm.commands["find-script"];

    console.commandManager.addHooks (console.hooks);

    initMenus();

    console.capturedMsgs = new Array();
    var ary = console.prefs["initialScripts"].split(/\s*;\s*/);
    for (i = 0; i < ary.length; ++i)
    {
        var url = stringTrim(ary[i]);
        if (url)
            dispatch ("loadd", { url: ary[i] });
    }

    dispatch ("hook-venkman-started");

    initViews();
    initRecords();
    initHandlers(); // handlers may notice windows, which need views and records

    createMainMenu(document);
    createMainToolbar(document);

    console.ui = {
        "status-text": document.getElementById ("status-text"),
        "profile-button": document.getElementById ("maintoolbar:profile-tb"),
        "stop-button": document.getElementById ("maintoolbar:stop")
    };    

    disableDebugCommands();

    initDebugger();
    initProfiler();

    // read prefs that have not been explicitly created, such as the layout prefs
    console.prefManager.readPrefs();
    
    fetchLaunchCount();
    
    console.sourceText = new HelpText();

    console.pushStatus(MSG_STATUS_DEFAULT);

    dispatch ("restore-layout default");

    var startupMsgs = console.capturedMsgs;
    delete console.capturedMsgs;
    
    dispatch ("version");

    for (i = 0; i < startupMsgs.length; ++i)
        display (startupMsgs[i][0], startupMsgs[i][1]);

    display (getMsg(MSN_TIP1_HELP, 
                    console.prefs["sessionView.requireSlash"] ? "/" : ""));
    display (MSG_TIP2_HELP);
    if (console.prefs["sessionView.requireSlash"])
        display (MSG_TIP3_HELP);

    if (console.prefs["rememberPrettyprint"])
        dispatch ("pprint", { toggle: console.prefs["prettyprint"] });
    else
        dispatch ("pprint", { toggle: false });

    if (MSG_LOCALE_VERSION != __vnk_requiredLocale)
    {
        display (getMsg(MSN_BAD_LOCALE,
                        [__vnk_requiredLocale, MSG_LOCALE_VERSION]),
                 MT_WARN);
    }

    console.initialized = true;

    if (console.prefs["saveSettingsOnExit"])
    {
        dispatch ("restore-settings",
                  {settingsFile: console.prefs["settingsFile"]});
    }

    dispatch ("hook-venkman-started");

    dd ("}");
}

function destroy ()
{
    if (console.prefs["saveLayoutOnExit"])
        dispatch ("save-layout default");

    if (console.prefs["saveSettingsOnExit"])
    {
        dispatch ("save-settings",
                  {settingsFile: console.prefs["settingsFile"]});
    }

    delete console.currentEvalObject;
    delete console.jsdConsole;

    destroyViews();
    destroyHandlers();
    destroyPrefs();
    detachDebugger();
}

function paintHack (i)
{
    /* when stopping at a timeout, we don't repaint correctly.
     * by jamming a character into this hidden text box, we can force
     * a repaint.
     */
    for (var w in console.viewManager.windows)
    {
        var window = console.viewManager.windows[w];
        var textbox = window.document.getElementById("paint-hack");
        if (textbox)
        {
            textbox.value = " ";
            textbox.value = "";
        }
    }
    
    if (!i)
        i = 0;
    
    if (i < 4)
        setTimeout (paintHack, 250, i + 1);
}   

function fetchLaunchCount()
{
    ++console.prefs["startupCount"];

    if (!toBool(console.prefs["permitStartupHit"]))
        return;

    function onLoad ()
    {
        var ary = String(r.responseText).match(/(\d+)/);
        if (ary)
        {
            display (getMsg(MSN_LAUNCH_COUNT,
                            [console.prefs["startupCount"], ary[1]]));
        }
    };
    
    try
    {
        var r = new XMLHttpRequest();
        r.onload = onLoad;
        r.open ("GET",
                __vnk_counter_url + "?local=" + console.prefs["startupCount"] +
                "&version=" + __vnk_version);
        r.send (null);
    }
    catch (ex)
    {
        // Oops. Probably missing the xmlextras extension, can't really do 
        // much about that.
        display(getMsg(MSN_LAUNCH_COUNT,
                       [console.prefs["startupCount"], MSG_VAL_UNKNOWN]));
    }
}
    
console.__defineGetter__ ("userAgent", con_ua);
function con_ua ()
{
    var ary = navigator.userAgent.match (/;\s*([^;\s]+\s*)\).*\/(\d+)/);
    if (ary)
    {
        return ("Venkman " + __vnk_version + __vnk_versionSuffix + 
                " [Mozilla " + ary[1] + "/" + ary[2] + "]");
    }

    return ("Venkman " + __vnk_version + __vnk_versionSuffix + " [" + 
            navigator.userAgent + "]");
}

console.hooks = new Object();

console.hooks["hook-script-instance-sealed"] =
function hookScriptSealed (e)
{
    if (!("componentPath" in console))
    {
        var ary = e.scriptInstance.url.match (/(.*)venkman-service\.js$/);
        if (ary)
        {
            if (!console.prefs["enableChromeFilter"])
            {
                dispatch ("debug-instance-on",
                          { scriptInstance: e.scriptInstance });
            }
            console.componentPath = ary[1];
        }
    }

    for (var fbp in console.fbreaks)
    {
        if (console.fbreaks[fbp].enabled &&
            e.scriptInstance.url.indexOf(console.fbreaks[fbp].url) != -1)
        {
            e.scriptInstance.setBreakpoint(console.fbreaks[fbp].lineNumber,
                                           console.fbreaks[fbp]);
        }
    }
}

console.hooks["hook-debug-stop"] =
function hookDebugStop (e)
{
    var jsdFrame = setCurrentFrameByIndex(0);
    var type = console.trapType;

    //var frameRec = console.stackView.stack.childData[0];
    //console.pushStatus (getMsg(MSN_STATUS_STOPPED, [frameRec.functionName,
    //                                                frameRec.location]));
    var showHeader = (type != jsdIExecutionHook.TYPE_INTERRUPTED ||
                      console._lastStackDepth != console.frames.length);
    var sourceContext = (type == jsdIExecutionHook.TYPE_INTERRUPTED) ? 0 : 2
        
    displayFrame (jsdFrame, 0, showHeader, sourceContext);    

    var scriptWrapper = getScriptWrapper(jsdFrame.script);
    if (scriptWrapper)
    {
        dispatchCommand (console.coFindScript,
                         { scriptWrapper: scriptWrapper, targetPc: jsdFrame.pc })
    }

    console._lastStackDepth = console.frames.length;

    enableDebugCommands();

    //XXX
    //paintHack();
}

console.hooks["hook-venkman-query-exit"] =
function hookQueryExit (e)
{
    if ("frames" in console)
    {
        if (confirm(MSG_QUERY_CLOSE))
        {
            dispatch ("cont");
            console.__exitAfterContinue__ = true;
        }
        e.returnValue = false;
    }
}

console.hooks["eval"] =
function hookEval()
{
    for (var i = 0; i < $.length; ++i)
        $[i].refresh();
}
    
/* exceptions */

/* keep this list in sync with exceptionMsgNames in venkman-msg.js */
const ERR_NOT_IMPLEMENTED = 0;
const ERR_REQUIRED_PARAM  = 1;
const ERR_INVALID_PARAM   = 2;
const ERR_SUBSCRIPT_LOAD  = 3;
const ERR_NO_DEBUGGER     = 4;
const ERR_FAILURE         = 5;
const ERR_NO_STACK        = 6;

/* venkman exception class */
function BadMojo (errno, params)
{
    var msg = getMsg(exceptionMsgNames[errno], params);
    
    dd ("new BadMojo (" + errno + ": " + msg + ") from\n" + getStackTrace());
    this.message = msg;
    this.errno = errno;
    this.fileName = Components.stack.caller.filename;
    this.lineNumber = Components.stack.caller.lineNumber;
    this.functionName = Components.stack.caller.name;
}

BadMojo.prototype.toString =
function bm_tostring ()
{
    return formatException (this);
}

function Failure (reason)
{
    if (typeof reason == "undefined")
        reason = MSG_ERR_DEFAULT_REASON;

    var obj = new BadMojo(ERR_FAILURE, [reason]);
    obj.fileName = Components.stack.caller.filename;
    obj.lineNumber = Components.stack.caller.lineNumber;
    obj.functionName = Components.stack.caller.name;
    
    return obj;
}

function InvalidParam (name, value)
{
    var obj = new BadMojo(ERR_INVALID_PARAM, [name, String(value)]);
    obj.fileName = Components.stack.caller.filename;
    obj.lineNumber = Components.stack.caller.lineNumber;
    obj.functionName = Components.stack.caller.name;
    return obj;
}

function RequiredParam (name)
{
    var obj = new BadMojo(ERR_REQUIRED_PARAM, name);
    obj.fileName = Components.stack.caller.filename;
    obj.lineNumber = Components.stack.caller.lineNumber;
    obj.functionName = Components.stack.caller.name;
    return obj;
}

/* console object */

console.display = display;
console.dispatch = dispatch;
console.dispatchCommand = dispatchCommand;

console.evalCount = 1;

console.metaDirectives = new Object();

console.metaDirectives["JSD_LOG"] =
console.metaDirectives["JSD_BREAK"] =
console.metaDirectives["JSD_EVAL"] =
function metaBreak (scriptInstance, line, matchResult)
{
    var scriptWrapper = scriptInstance.getScriptWrapperAtLine(line);
    if (!scriptWrapper)
    {
        display (getMsg(MSN_ERR_NO_FUNCTION, [line, scriptInstance.url]),
                 MT_ERROR);
        return;
    }
                        
    var pc = scriptWrapper.jsdScript.lineToPc(line, PCMAP_SOURCETEXT);
    if (scriptWrapper.getBreakpoint(pc))
        return;

    var sourceLine = scriptWrapper.jsdScript.pcToLine(pc, PCMAP_SOURCETEXT);
    var breakpoint = setFutureBreakpoint (scriptWrapper.jsdScript.fileName,
                                          sourceLine);
    scriptWrapper.setBreakpoint (pc, breakpoint);
    matchResult[2] = stringTrim(matchResult[2]);
    
    switch (matchResult[1])
    {
        case "JSD_LOG":
            breakpoint.conditionEnabled = true;
            breakpoint.condition = "return eval(" + matchResult[2].quote() + ")";
            breakpoint.resultAction = BREAKPOINT_STOPNEVER;
            breakpoint.logResult = true;
            break;

        case "JSD_BREAK":
            if (matchResult[2])
            {
                breakpoint.conditionEnabled = true;
                breakpoint.condition = "return eval(" + matchResult[2].quote() +
                    ")";
                breakpoint.resultAction = BREAKPOINT_STOPTRUE;
            }
            else
            {
                breakpoint.resultAction = BREAKPOINT_STOPALWAYS;
            }
            break;
            
        case "JSD_EVAL":
            breakpoint.conditionEnabled = true;
            breakpoint.condition = "return eval(" + matchResult[2].quote() + ")";
            breakpoint.resultAction = BREAKPOINT_STOPNEVER;
            break;
    }
}

console.__defineGetter__ ("status", con_getstatus);
function con_getstatus ()
{
    return console.ui["status-text"].getAttribute ("label");
}

console.__defineSetter__ ("status", con_setstatus);
function con_setstatus (msg)
{
    var topMsg = console._statusStack[console._statusStack.length - 1];

    if (msg)
    {        
        if ("_statusTimeout" in console)
        {
            clearTimeout (console._statusTimeout);
            delete console._statusTimeout;
        }
        if (msg != topMsg)
        {
            console._statusTimeout = setTimeout (con_setstatus,
                                                 console.prefs["statusDuration"],
                                                 null);
        }
    }
    else
    {
        msg = topMsg;
    }

    console.ui["status-text"].setAttribute ("label", msg);
}

console.__defineGetter__ ("pp_stopLine", con_getppline);
function con_getppline ()
{
    if (!("frames" in console))
        return (void 0);
    
    if (!("_pp_stopLine" in this))
    {
        var frame = getCurrentFrame();
        this._pp_stopLine = frame.script.pcToLine(frame.pc, PCMAP_PRETTYPRINT);
    }
    return this._pp_stopLine;
}

console.pushStatus =
function con_pushstatus (msg)
{
    console._statusStack.push (msg);
    console.status = msg;
}

console.popStatus =
function con_popstatus ()
{
    console._statusStack.pop();
    console.status = console._statusStack[console._statusStack.length - 1];
}

function SourceText (scriptInstance)
{
    this.lines = new Array();
    this.tabWidth = console.prefs["tabWidth"];
    if (scriptInstance instanceof ScriptInstance)
    {
        this.scriptInstance = scriptInstance;
        this.url = scriptInstance.url;
        this.jsdURL = JSD_URL_SCHEME + "source?location=" + 
            encodeURIComponent(this.url) +
            "&instance=" + scriptInstance.sequence;
    }
    else
    {
        /* assume scriptInstance is a string containing the filename */
        this.url = scriptInstance;
        this.jsdURL = JSD_URL_SCHEME + "source?location=" + 
                      encodeURIComponent(this.url);
    }

    this.shortName = abbreviateWord(getFileFromPath (this.url), 30);
}

SourceText.prototype.noteFutureBreakpoint =
function st_notefbreak(line, state)
{
    if (!ASSERT(!("scriptInstance" in this),
                "Don't call noteFutureBreakpoint on a SourceText with a " +
                "scriptInstance, use the scriptManager instead."))
    {
        return;
    }
    
    if (state)
        arrayOrFlag (this.lineMap, line - 1, LINE_FBREAK);
    else
        arrayAndFlag (this.lineMap, line - 1, ~LINE_FBREAK);
}

SourceText.prototype.onMarginClick =
function st_marginclick (e, line)
{
    //dd ("onMarginClick");
    
    if (!("scriptInstance" in this))
    {
        if (getFutureBreakpoint(this.url, line))
        {
            clearFutureBreakpoint(this.url, line);
        }
        else
        {
            setFutureBreakpoint(this.url, line);
            //dispatch ("fbreak", { urlPattern: this.url, lineNumber: line });
        }
    }
    else
    {
        var parentBP = getFutureBreakpoint(this.url, line);
        if (parentBP)
        {
            if (this.scriptInstance.hasBreakpoint(line))
                this.scriptInstance.clearBreakpoint(line);
            else
                clearFutureBreakpoint(this.url, line);
        }
        else
        {
            if (this.scriptInstance.hasBreakpoint(line))
            {
                this.scriptInstance.clearBreakpoint(line);
            }
            else
            {
                parentBP = setFutureBreakpoint(this.url, line);
                this.scriptInstance.setBreakpoint (line, parentBP);
            }
        }
    }
}

SourceText.prototype.isLoaded = false;

SourceText.prototype.reloadSource =
function st_reloadsrc (cb)
{
    function reloadCB (status)
    {
        if (typeof cb == "function")
            cb(status);
    }

    this.isLoaded = false;
    this.lines = new Array();
    delete this.markup;
    delete this.charset;
    this.loadSource(reloadCB);
}

SourceText.prototype.onSourceLoaded =
function st_oncomplete (data, url, status)
{
    //dd ("source loaded " + url + ", " + status);
    
    var sourceText = this;
    
    function callall (status)
    {
        while (sourceText.loadCallbacks.length)
        {
            var cb = sourceText.loadCallbacks.pop();
            cb (status);
        }
        delete sourceText.loadCallbacks;
    };
            
    delete this.isLoading;
    
    if (status != Components.results.NS_OK)
    {
        dd ("loadSource failed with status " + status + ", " + url);
        callall (status);
        return;
    }

    if (!ASSERT(data, "loadSource succeeded but got no data"))
        data = "";

    var matchResult;
    
    // search for xml charset
    if (data.substring(0, 5) == "<?xml" && !("charset" in this))
    {
        var s = data.substring(6, data.indexOf("?>"));
        matchResult = s.match(/encoding\s*=\s*([\"\'])([^\"\'\s]+)\1/i);
        if (matchResult)
            this.charset = matchResult[2];
    }

    // kill control characters, except \t, \r, and \n
    data = data.replace(/[\x00-\x08]|[\x0B\x0C]|[\x0E-\x1F]/g, "?");

    // check for a html style charset declaration
    if (!("charset" in this))
    {
        matchResult =
            data.match(/meta\s+http-equiv.*content-type.*charset\s*=\s*([^\;\"\'\s]+)/i);
        if (matchResult)
            this.charset = matchResult[1];
    }

    // look for an emacs mode line
    matchResult = data.match (/-\*-.*tab-width\:\s*(\d+).*-\*-/);
    if (matchResult)
        this.tabWidth = matchResult[1];

    // replace tabs
    data = data.replace(/\x09/g, leftPadString ("", this.tabWidth, " "));
    
    var ary = data.split(/\r\n|\n|\r/m);        

    if (0)
    {
        for (var i = 0; i < ary.length; ++i)
        {
            /*
             * The replace() strips control characters, we leave the tabs in
             * so we can expand them to a per-file width before actually
             * displaying them.
             */
            ary[i] = ary[i].replace(/[\x00-\x08]|[\x0A-\x1F]/g, "?");
            if (!("charset" in this))
            {
                matchResult = ary[i].match(charsetRE);
                if (matchResult)
                    this.charset = matchResult[1];
            }
        }
    }
    
    this.lines = ary;

    if ("scriptInstance" in this)
    {
        this.scriptInstance.guessFunctionNames(sourceText);
        this.lineMap = this.scriptInstance.lineMap;
    }
    else
    {
        this.lineMap = new Array();
        for (var fbp in console.fbreaks)
        {
            var fbreak = console.fbreaks[fbp];
            if (fbreak.url == this.url)
                arrayOrFlag (this.lineMap, fbreak.lineNumber - 1, LINE_FBREAK);
        }
    }

    this.isLoaded = true;
    dispatch ("hook-source-load-complete",
              { sourceText: sourceText, status: status });
    callall(status);
}


SourceText.prototype.loadSource =
function st_loadsrc (cb)
{
    var sourceText = this;
    
    function onComplete (data, url, status)
    {
        //dd ("loaded " + url + " with status " + status + "\n" + data);
        sourceText.onSourceLoaded (data, url, status);
    };

    if (this.isLoaded)
    {
        /* if we're loaded, callback right now, and return. */
        cb (Components.results.NS_OK);
        return;
    }

    if ("isLoading" in this)
    {
        /* if we're in the process of loading, make a note of the callback, and
         * return. */
        if (typeof cb == "function")
            this.loadCallbacks.push (cb);
        return;
    }
    else
        this.loadCallbacks = new Array();

    if (typeof cb == "function")
        this.loadCallbacks.push (cb);
    this.isLoading = true;    
    
    var ex;
    var src;
    var url = this.url;
    if (url.search (/^javascript:/i) == 0)
    {
        src = url;
        delete this.isLoading;
    }
    else
    {
        try
        {
            loadURLAsync (url, { onComplete: onComplete });
            return;
        }
        catch (ex)
        {
            display (getMsg(MSN_ERR_SOURCE_LOAD_FAILED, [url, ex]), MT_ERROR);
            onComplete (src, url, Components.results.NS_ERROR_FAILURE);
            return;
        }
    }

    onComplete (src, url, Components.results.NS_OK);
}

function PPSourceText (scriptWrapper)
{
    this.scriptWrapper = scriptWrapper;
    this.reloadSource();
    this.shortName = scriptWrapper.functionName;
    this.jsdURL = JSD_URL_SCHEME + "pprint?scriptWrapper=" + scriptWrapper.tag;
}

PPSourceText.prototype.isLoaded = true;

PPSourceText.prototype.reloadSource =
function ppst_reload (cb)
{
    try
    {
        var jsdScript = this.scriptWrapper.jsdScript;
        this.tabWidth = console.prefs["tabWidth"];
        this.url = jsdScript.fileName;
        var lines = String(jsdScript.functionSource).split("\n");
        var lineMap = new Array();
        var len = lines.length;
        for (var i = 0; i < len; ++i)
        {
            if (jsdScript.isLineExecutable(i + 1, PCMAP_PRETTYPRINT))
                lineMap[i] = LINE_BREAKABLE;
        }
        
        for (var b in this.scriptWrapper.breaks)
        {
            var line = jsdScript.pcToLine(this.scriptWrapper.breaks[b].pc,
                                          PCMAP_PRETTYPRINT);
            arrayOrFlag (lineMap, line - 1, LINE_BREAK);
        }
        
        this.lines = lines;
        this.lineMap = lineMap;
        delete this.markup;
        
        if (typeof cb == "function")
            cb (Components.results.NS_OK);
    }
    catch (ex)
    {
        ASSERT(0, "caught exception reloading pretty printed source " + ex);
        if (typeof cb == "function")
            cb (Components.results.NS_ERROR_FAILURE);
        this.lines = [String(MSG_CANT_PPRINT)];
        this.lineMap = [0];
    }           
}

PPSourceText.prototype.onMarginClick =
function ppst_marginclick (e, line)
{
    try
    {
        var jsdScript = this.scriptWrapper.jsdScript;
        var pc = jsdScript.lineToPc(line, PCMAP_PRETTYPRINT);
        if (this.scriptWrapper.hasBreakpoint(pc))
            this.scriptWrapper.clearBreakpoint(pc);
        else
            this.scriptWrapper.setBreakpoint(pc);
    }
    catch (ex)
    {
        ASSERT(0, "onMarginClick caught exception " + ex);
    }
}

function HelpText ()
{
    this.tabWidth = console.prefs["tabWidth"];
    this.jsdURL = this.url = JSD_URL_SCHEME + "help";
    this.shortName = this.url;
    this.reloadSource();
}

HelpText.prototype.isLoaded = true;

HelpText.prototype.reloadSource =
function ht_reload (cb)
{
    var ary = console.commandManager.list();
    var str = "";
    for (var c in ary)
    {
        str += "\n                                        ===\n"
            + ary[c].getDocumentation(formatCommandFlags);
    }
    
    this.lines = str.split("\n");
    if (typeof cb == "function")
        cb (Components.results.NS_OK);
}
