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

const __vnk_version        = "0.9.23";
const __vnk_requiredLocale = "0.9.x";
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
    return ((console.prefs["enableChromeFilter"] &&
            url.search(/^chrome:/) == 0) ||
            (url.search (/^chrome:\/\/venkman/) == 0 &&
             url.search (/test/) == -1));
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
                if ("stack" in ex)
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
        {
            names = command.beforeHookNames;
            hooks = command.beforeHooks;
        }
        else
        {
            names = command.afterHookNames;
            hooks = command.afterHooks;
        }

        var len = names.length;
        
        for (var i = 0; i < len; ++i)
        {
            var name = names[i];
            if ("dbgDispatch" in console && console.dbgDispatch)
            {
                dd ("calling " + (isBefore ? "before" : "after") + 
                    " hook " + name);
            }
            try
            {
                hooks[name](e);
            }
            catch (ex)
            {
                display (getMsg(MSN_ERR_INTERNAL_HOOK, name), MT_ERROR);
                display (formatException(ex), MT_ERROR);
                dd (formatException(ex), MT_ERROR);
                if ("stack" in ex)
                    dd (ex.stack);
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

    console.baseWindow = getBaseWindowFromWindow(window);
    console.mainWindow = window;
    console.dnd = nsDragAndDrop;
    
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

    fetchLaunchCount();
    
    console.sourceText = new HelpText();

    console.pushStatus(MSG_STATUS_DEFAULT);

    dispatch ("restore-layout default");

    var startupMsgs = console.capturedMsgs;
    delete console.capturedMsgs;
    
    dispatch ("version");

    for (i = 0; i < startupMsgs.length; ++i)
        display (startupMsgs[i][0], startupMsgs[i][1]);
    
    display (MSG_TIP1_HELP);
    display (MSG_TIP2_HELP);
    //dispatch ("commands");
    //dispatch ("help");

    dispatch ("pprint", { toggle: console.prefs["prettyprint"] });

    if (MSG_LOCALE_VERSION != __vnk_requiredLocale)
    {
        display (getMsg(MSN_BAD_LOCALE,
                        [__vnk_requiredLocale, MSG_LOCALE_VERSION]),
                 MT_WARN);
    }

    console.initialized = true;
    dispatch ("hook-venkman-started");

    dd ("}");
}

function destroy ()
{
    if (console.prefs["saveLayoutOnExit"])
        dispatch ("save-layout default");
    
    destroyViews();
    destroyHandlers();
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
            display (getMsg(MSN_LAUNCH_COUNT,
                            [console.prefs["startupCount"], ary[1]]));
    };
    
    var r = new XMLHttpRequest();
    r.onload = onLoad;
    r.open ("GET",
            __vnk_counter_url + "?local=" +  console.prefs["startupCount"] +
            "&version=" + __vnk_version);
    r.send (null);
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
    for (var fbp in console.fbreaks)
    {
        if (e.scriptInstance.url.search(console.fbreaks[fbp].url) != -1)
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
    paintHack();
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

function createDOMForSourceText (sourceText, container)
{
    if (!ASSERT(sourceText.isLoaded, "sourceText is not loaded"))
        return;
    
    var document = container.ownerDocument;
    if (!ASSERT(document, "container has no owner document, using default"))
        document = window.document

    var sourceContainer = document.createElement ("source-listing");
    
    const COMMENT = 0;
    const STRING1 = 1;
    const STRING2 = 2;
    const WORD    = 3;
    const NUMBER  = 4;
    const OTHER   = 5;
    
    var keywords = {
        "abstract": 1, "boolean": 1, "break": 1, "byte": 1, "case": 1,
        "catch": 1, "char": 1, "class": 1, "const": 1, "continue": 1,
        "debugger": 1, "default": 1, "delete": 1, "do": 1, "double": 1,
        "else": 1, "enum": 1, "export": 1, "export": 1, "extends": 1,
        "false": 1, "final": 1, "finally": 1, "float": 1, "for": 1,
        "function": 1, "goto": 1, "if": 1, "implements": 1, "import": 1,
        "in": 1, "instanceof": 1, "int": 1, "interface": 1, "long": 1,
        "native": 1, "new": 1, "null": 1, "package": 1, "private": 1,
        "protected": 1, "public": 1, "return": 1, "short": 1, "static": 1,
        "switch": 1, "synchronized": 1, "this": 1, "throw": 1, "throws": 1,
        "transient": 1, "true": 1, "try": 1, "typeof": 1, "var": 1, "void": 1,
        "while": 1, "with": 1
    };

    function mungeLine (line, parentNode, previousState, previousNode)
    {
        if (!previousState)
            previousState = OTHER;
        
        function closePhrase (phrase)
        {
            /* put |phrase| into a DOM node, and append that node to
             * |sourceContainer|.  If |phrase| is the same type as the
             * |previousNode|, insert it there instead. */
            if (!phrase)
            {
                previousState = OTHER;
                return;
            }

            var textNode = document.createTextNode (phrase);
            var nodeName;
            
            switch (previousState)
            {
                case COMMENT:
                    nodeName = "c";
                    break;            
                case STRING1:
                case STRING2:
                    nodeName = "s";
                    break;
                case WORD:
                    if (phrase in keywords)
                        nodeName = "k";
                    else
                        nodeName = "w";
                    break;
                case NUMBER:
                case OTHER:
                    parentNode.appendChild (textNode);
                    previousState = OTHER;
                    previousNode = null;
                    return;
            }

            if (previousNode && previousNode.localName == nodeName)
            {
                previousNode.appendChild (textNode);
            }
            else
            {
                previousNode = document.createElement (nodeName);
                previousNode.appendChild (textNode);
                parentNode.appendChild (previousNode);
            }

            previousState = OTHER;
            return;
        };
    
        var pos;
        var ch, ch2;
        var expr;
    
        while (line.length > 0)
        {
            /* scan a line of text.  |pos| always one *past* the end of the
             * phrase we just scanned. */

            switch (previousState)
            {
                case COMMENT:
                    /* look for the end of a slash-star comment,
                     * slash-slash comments are handled down below, because
                     * we know for sure that it's the last phrase on this line.
                     */
                    pos = line.search (/\*\//);
                    if (pos != -1)
                        pos += 2;
                    break;
                    
                case STRING1:
                case STRING2:
                    /* look for the end of a single or double quoted string. */
                    if (previousState == STRING1)
                    {
                        ch = "'";
                        expr = /\'/;
                    }
                    else
                    {
                        ch = "\"";
                        expr = /\"/;
                    }

                    if (line[0] == ch)
                    {
                        pos = 1;
                    }
                    else
                    {
                        pos = line.search (expr);
                        if (pos > 0 && line[pos - 1] == "\\")
                        {
                            /* arg, the quote we found was escaped, fall back
                             * to scanning a character at a time. */
                            var done = false;
                            for (pos = 0; !done && pos < line.length; ++pos)
                            {
                                if (line[pos] == "\\")
                                    ++pos;
                                else if (line[pos] == ch)
                                    done = true;
                            }
                        }
                        else
                        {
                            /* no slash before the end quote, it *must* be
                             * the end of the string. */
                            if (pos != -1)
                                ++pos;
                        }
                    }
                    break;

                case WORD:
                    /* look for the end of something that qualifies as
                     * an identifier. */
                    var wordPattern = /[^a-zA-Z0-9_\$]/;
                    pos = line.search(wordPattern);
                    while (pos > -1 && line[pos] == "\\")
                    {
                        /* if we ended with a \ character, then the slash
                         * and the character after it are part of this word.
                         * the characters following may also be part of the
                         * word. */
                        pos += 2;
                        var newPos = line.substr(pos).search(wordPattern);
                        if (newPos > -1)
                            pos += newPos;
                    }
                    break;

                case NUMBER:
                    /* look for the end of a number */
                    pos = line.search (/[^0-9\.]/);
                    break;

                case OTHER:
                    /* look for the end of anything else, like whitespace
                     * or operators. */
                    pos = line.search (/[0-9a-zA-Z_\$\"\']|\\|\/\/|\/\*/);
                    break;
            }

            if (pos == -1)
            {
                /* couldn't find an end for the current state, close out the 
                 * rest of the line.
                 */
                if (previousState == COMMENT || previousState == STRING1 ||
                    previousState == STRING2)
                {
                    var savedState = previousState;
                    closePhrase(line);
                    previousState = savedState;
                }
                else
                {
                    closePhrase(line);
                }

                line = "";
            }
            else
            {
                /* pos has a non -1 value, close out what we found, and move
                 * along. */
                if (previousState == STRING1 || previousState == STRING2)
                {
                    /* strings are a special case because they actually are
                     * considered to start *after* the leading quote,
                     * and they end *before* the trailing quote. */
                    if (pos == 1)
                    {
                        /* empty string */
                        previousState = OTHER;
                    }
                    else
                    {
                        /* non-empty string, close out the contents of the
                         * string. */
                        closePhrase(line.substr (0, pos - 1));
                    }
                    
                    /* close the trailing quote. */
                    closePhrase (line[pos - 1]);
                }
                else
                {
                    /* non-string phrase, close the whole deal. */
                    closePhrase(line.substr (0, pos));
                }
                
                if (pos)
                {
                    /* continue on with the rest of the line */
                    line = line.substr (pos);
                }
            }
            
            if (line)
            {
                /* figure out what the next token looks like. */
                ch = line[0];
                ch2 = (line.length > 1) ? line[1] : "";
                
                if (ch == "/" && ch2 == "/")
                {
                    /* slash-slash comment, the last thing on this line. */
                    previousState = COMMENT;
                    closePhrase(line);
                    line = "";
                }
                else if (ch == "'")
                {
                    closePhrase("'");
                    line = line.substr(1);
                    previousState = STRING1;
                }
                else if (ch == "\"")
                {
                    closePhrase("\"");
                    line = line.substr(1);
                    previousState = STRING2;
                }
                else if (ch == "/" && ch2 == "*")
                {
                    previousState = COMMENT;
                }
                else if (ch.search (/[a-zA-Z_\\\$]/) == 0)
                {
                    previousState = WORD;
                }
                else if (ch.search (/[0-9]/) == 0)
                {
                    previousState = NUMBER;
                }
            }
        }

        parentNode.appendChild (document.createTextNode ("\n"));

        return { previousState: previousState, previousNode: previousNode };
    };

    function processMarginChunk (start, callback, margin)
    {
        /* build a chunk of the left margin. */
        dd ("processMarginChunk " + start + " {");
        const CHUNK_SIZE = 500;
        const CHUNK_DELAY = 100;
        
        if (!margin)
        {
            margin = document.createElement ("left-margin");
            sourceContainer.appendChild (margin);
        }
        
        var stop = Math.min (sourceText.lines.length, start + CHUNK_SIZE);
        var element;
        for (var i = start; i < stop; ++i)
        {
            element = document.createElement ("num");
            if ("lineMap" in sourceText && i in sourceText.lineMap &&
                (sourceText.lineMap[i] & LINE_BREAKABLE))
            {
                element.setAttribute ("x", "t");
            }

            element.appendChild (document.createTextNode(i));
            margin.appendChild (element);
        }

        if (i != sourceText.lines.length)
        {
            setTimeout (processMarginChunk, CHUNK_DELAY, i, callback, 
                        margin);
        }
        else
        {
            callback();
        }
        dd ("}");
    };
    
    function processSourceChunk (start, callback, container)
    {
        dd ("processSourceChunk " + start + " {");
        /* build a chunk of the source text */
        const CHUNK_SIZE = 250;
        const CHUNK_DELAY = 100;

        if (!container)
        {
            container = document.createElement ("source-lines");
            sourceContainer.appendChild (container);
        }

        var stop = Math.min (sourceText.lines.length, start + CHUNK_SIZE);
        var rv = { previousState: null, previousNode: null };
        for (var i = start; i < stop; ++i)
        {
            rv = mungeLine (sourceText.lines[i], container, rv.previousState,
                            rv.previousNode);
        }

        if (i != sourceText.lines.length)
        {
            setTimeout (processSourceChunk, CHUNK_DELAY, i, callback, 
                        container);
        }
        else
        {
            callback();
        }
        dd ("}");
    };
    
    function processSourceText ()
    {
        processSourceChunk (0, onComplete);
    };
    
    function onComplete()
    {
        container.appendChild (sourceContainer);
        dd ("}");
    };

    dd ("createDOMForSourceText " + sourceText.lines.length + " lines {");
    processMarginChunk (0, processSourceText);
}

function st_contentGetter ()
{
    if (!("_contentNode" in this))
    {
        this._contentNode = htmlSpan();
        createDOMForSourceText (this, this._contentNode);
    }
    
    return this._contentNode;
}

function SourceText (scriptInstance)
{
    this.lines = new Array();
    this.tabWidth = console.prefs["tabWidth"];
    if (scriptInstance instanceof ScriptInstance)
    {
        this.scriptInstance = scriptInstance;
        this.url = scriptInstance.url;
        this.jsdURL = JSD_URL_SCHEME + "source?location=" + escape(this.url) +
            "&instance=" + scriptInstance.sequence;
    }
    else
    {
        /* assume scriptInstance is a string containing the filename */
        this.url = scriptInstance;
        this.jsdURL = JSD_URL_SCHEME + "source?location=" + escape(this.url);
    }

    this.shortName = getFileFromPath (this.url);
}

SourceText.prototype.__defineGetter__ ("contentNode", st_contentGetter);

SourceText.prototype.onMarginClick =
function st_marginclick (e, line)
{
    //dd ("onMarginClick");
    
    if (!("scriptInstance" in this))
    {
        dispatch ("fbreak", { url: this.url, line: line });
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
    this.loadSource(reloadCB);
}

SourceText.prototype.onSourceLoaded =
function st_oncomplete (data, url, status)
{
    dd ("source loaded " + url + ", " + status);
    
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
    
    var ary = data.split(/\r\n|\n|\r/m);
    for (var i = 0; i < ary.length; ++i)
    {
        /*
         * The replace() strips control characters, we leave the tabs in
         * so we can expand them to a per-file width before actually
         * displaying them.
         */
        ary[i] = new String(ary[i].replace(/[\x0-\x8]|[\xA-\x1A]/g, ""));
    }

    this.lines = ary;

    ary = ary[0].match (/tab-?width*:\s*(\d+)/i);
    if (ary)
        this.tabWidth = ary[1];
    
    if ("scriptInstance" in this)
    {
        this.scriptInstance.guessFunctionNames(sourceText);
        this.lineMap = this.scriptInstance.lineMap;
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
    try
    {
        if (url.search (/^javascript:/i) == 0)
            src = url;
        else
            src = loadURLNow(url);
        delete this.isLoading;
    }
    catch (ex)
    {
        /* if we can't load it now, try to load it later */
        try
        {
            dd ("trying async");
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

PPSourceText.prototype.__defineGetter__ ("contentNode", st_contentGetter);

PPSourceText.prototype.reloadSource =
function pp_reload (cb)
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
function st_marginclick (e, line)
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

HelpText.prototype.__defineGetter__ ("contentNode", st_contentGetter);

HelpText.prototype.reloadSource =
function (cb)
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
