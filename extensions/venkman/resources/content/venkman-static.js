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

/* dd is declared first in venkman-utils.js */
var warn;
var ASSERT;

if (DEBUG)
{
    _dd_pfx = "vnk: ";
    warn = function (msg) { dumpln ("** WARNING " + msg + " **"); }
    ASSERT = function (expr, msg) {
        if (!expr)
            dump ("** ASSERTION FAILED: " + msg + " **\n" + getStackTrace() +
                  "\n"); 
    }
}    
else
    dd = warn = ASSERT = function (){};

var MAX_STR_LEN = 100;  /* max string length to display before changing to
                         * "n characters" display mode.  as a var so we can
                         * change it later. */
const MAX_WORD_LEN = 20; /* number of characters to display before forcing a 
                          * potential word break point (in the console.) */

var console = new Object();

console.version = "0.6.1";

/* |this|less functions */

const jsdIFilter = Components.interfaces.jsdIFilter;

function cont ()
{
    disableDebugCommands();
    --console._stopLevel;
    console.stackView.saveState();
    console.jsds.exitNestedEventLoop();
}

function next ()
{
    console._stepOverLevel = 0;
    step();
    console.jsds.functionHook = console._callHook;
}

function step ()
{
    setStopState(true);
    var topFrame = console.frames[0];
    console._stepPast = topFrame.script.fileName
    if (console.sourceView.prettyPrint)
    {
        console._stepPast +=
            topFrame.script.pcToLine(topFrame.pc, PCMAP_PRETTYPRINT);
    }
    else
    {
        console._stepPast += topFrame.line;
    }
    disableDebugCommands()
    --console._stopLevel;
    console.stackView.saveState();
    console.jsds.exitNestedEventLoop();
}

function stepOut ()
{
    console._stepOverLevel = 1;
    setStopState(false);
    console.jsds.functionHook = console._callHook;
    disableDebugCommands()
    --console._stopLevel;
    console.stackView.saveState();
    console.jsds.exitNestedEventLoop();
}

function toggleStopState()
{
    if (console.jsds.interruptHook)
        setStopState(false);
    else
        setStopState(true);
}

function setStopState(state)
{
    var tb = document.getElementById("stop-button");
    if (state)
    {
        console.jsds.interruptHook = console._executionHook;
        tb.setAttribute("willStop", "true");
    }
    else
    {
        console.jsds.interruptHook = null;
        tb.removeAttribute("willStop");
    }
}

function enableReloadCommand()
{
    var cmd = document.getElementById("cmd_reload");
    cmd.removeAttribute ("disabled");
}

function disableReloadCommand()
{
    var cmd = document.getElementById("cmd_reload");
    cmd.setAttribute ("disabled", "true");
}

function enableDebugCommands()
{
    var cmds = document.getElementById("venkmanStackCommands");
    var sib = cmds.firstChild;

    while (sib)
    {
        sib.removeAttribute ("disabled");
        sib = sib.nextSibling;
    }

    var tb = document.getElementById("cmd_stop");
    tb.setAttribute("disabled", "true");
    if (console.frames.length == 1)
    {
        tb = document.getElementById("cmd_step_out");
        tb.setAttribute("disabled", "true");
    }
}

function disableDebugCommands()
{
    var cmds = document.getElementById("venkmanStackCommands");
    var sib = cmds.firstChild;
    while (sib)
    {
        sib.setAttribute ("disabled", "true");
        sib = sib.nextSibling;
    }

    var tb = document.getElementById("cmd_stop");
    tb.removeAttribute("disabled");
}

function dispatchCommand (text)
{
    var ev = new Object();
    ev.line = text;
    console.onInputCompleteLine (ev);
}

function display(message, msgtype)
{
    if (typeof message == "undefined")
        throw new BadMojo(ERR_REQUIRED_PARAM, "message");

    if (typeof message != "string" &&
        !(message instanceof Components.interfaces.nsIDOMHTMLElement))
        throw new BadMojo(ERR_INVALID_PARAM, ["message", String(message)]);

    if (typeof msgtype == "undefined")
        msgtype = MT_INFO;
    
    function setAttribs (obj, c, attrs)
    {
        for (var a in attrs)
            obj.setAttribute (a, attrs[a]);
        obj.setAttribute("class", c);
        obj.setAttribute("msg-type", msgtype);
    }

    var msgRow = htmlTR("msg");
    setAttribs(msgRow, "msg");

    var msgData = htmlTD();
    setAttribs(msgData, "msg-data");
    if (typeof message == "string")
        msgData.appendChild(stringToDOM(message));
    else
        msgData.appendChild(message);

    msgRow.appendChild(msgData);

    console._outputElement.appendChild(msgRow);
    console.scrollDown();
}

function displayCommands (pattern)
{
    display (MSG_TIP_HELP);
    
    if (pattern)
        display (getMsg(MSN_CMDMATCH,
                        [pattern, "["  + 
                        console._commands.listNames(pattern).join(MSG_COMMASP) +
                        "]"]));
    else
        display (getMsg(MSN_CMDMATCH_ALL,
                        "[" + console._commands.listNames().join(MSG_COMMASP) +
                        "]"));
}

function setCurrentSource (url, line)
{
    var fileRec = console.scripts[url];
    ASSERT (fileRec, "Attempt to focus unknown source: " + url);

    var lastFile = console._sourceOutlinerView.url
    if (lastFile)
    {
        var lastFileRec = console.scripts[lastFile];
        if (lastFileRec)
        {
            lastFileRec.lastLine = 
                console._sourceOutlinerView.outliner.getFirstVisibleRow();
        }
    }
    
    console._sourceOutlinerView.setSourceArray(fileRec.source, url);
    var hdr = document.getElementById("source-line-text");
    hdr.setAttribute ("label", url);
    hdr = document.getElementById("source-line-number");
    hdr.setAttribute ("label", "");    
}

function scrollSourceLineTop (url, lineNumber)
{
    focusSource(url);
    if (toTop)
        console._sourceOutlinerView.outliner.scrollToRow(lineNumber - 1);
    else
        console._sourceOutlinerView.centerLine(lineNumber - 1);
}

function evalInDebuggerScope (script)
{
    try
    {
        return console.doEval (script);
    }
    catch (ex)
    {
        display (formatEvalException(ex), MT_ERROR);
        return null;
    }
}

function evalInTargetScope (script)
{
    if (!console.frames)
    {
        display (MSG_ERR_NO_STACK, MT_ERROR);
        return null;
    }
    
    try
    {
        return getCurrentFrame().eval (script, MSG_VAL_CONSOLE, 1);
    }
    catch (ex)
    {
        display (formatEvalException (ex), MT_ERROR);
        return null;
    }
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
    
    return getMsg (MSN_EVAL_THREW,  String(ex));
}

function htmlVA (attribs, href, contents)
{
    if (!attribs)
        attribs = {"class": "venkman-link", target: "_content"};
    else if (attribs["class"])
        attribs["class"] += " venkman-link";
    else
        attribs["class"] = "venkman-link";

    if (!contents)
    {
        contents = htmlSpan();
        insertHyphenatedWord (href, contents);
    }
    
    return htmlA (attribs, href, contents);
}
    
function init()
{    
    initPrefs();
    initCommands();
    initOutliners();
    
    disableDebugCommands();
    
    console._outputDocument = 
        document.getElementById("output-iframe").contentDocument;

    console._outputElement = 
        console._outputDocument.getElementById("output-tbody");    

    console._slInputElement = 
        document.getElementById("input-single-line");
    console._slInputElement.focus();
 
    console._munger = new CMunger();
    console._munger.enabled = true;
    console._munger.addRule
        ("link", /((\w+):\/\/[^<>()\'\"\s:]+|www(\.[^.<>()\'\"\s:]+){2,})/,
         insertLink);
    console._munger.addRule ("word-hyphenator",
                             new RegExp ("(\\S{" + MAX_WORD_LEN + ",})"),
                             insertHyphenatedWord);

    console._lastStackDepth = -1;

    initDebugger(); /* debugger may need display() to init */

    display(MSG_HELLO, MT_HELLO);
    display(getMsg(MSN_VERSION, console.version), MT_HELLO);
    displayCommands();

    console.ui = new Object();
    console.ui["menu_PrettyPrint"] =
        document.getElementById ("menu_PrettyPrint");
    console.ui["menu_InitAtStartup"] =
        document.getElementById ("menu_InitAtStartup");
    console.ui["menu_TModeIgnore"] = document.getElementById("menu_TModeIgnore");
    console.ui["menu_TModeTrace"] = document.getElementById("menu_TModeTrace");
    console.ui["menu_TModeBreak"] = document.getElementById("menu_TModeBreak");
    console.ui["status-text"] = document.getElementById ("status-text");

    console._statusStack = new Array();

    startupTests();
}

function destroy ()
{
    destroyOutliners();
    detachDebugger();
}

function insertHyphenatedWord (longWord, containerTag)
{
    var wordParts = splitLongWord (longWord, MAX_WORD_LEN);
    containerTag.appendChild (htmlSpacer());
    for (var i = 0; i < wordParts.length; ++i)
    {
        containerTag.appendChild (document.createTextNode (wordParts[i]));
        if (i != wordParts.length)
            containerTag.appendChild (htmlSpacer());
    }
}

function insertLink (matchText, containerTag)
{
    var href;
    
    if (matchText.indexOf ("://") == -1)
        href = "http://" + matchText;
    else
        href = matchText;
    
    var anchor = htmlVA (null, href);
    containerTag.appendChild (anchor);    
}

function load(url, obj)
{
    var rv;
    var ex;
    
    if (!console._loader)
    {
        const LOADER_CTRID = "@mozilla.org/moz/jssubscript-loader;1";
        const mozIJSSubScriptLoader = 
            Components.interfaces.mozIJSSubScriptLoader;

        var cls;
        if ((cls = Components.classes[LOADER_CTRID]))
            console._loader = cls.createInstance(mozIJSSubScriptLoader);
    }
    
    rv = console._loader.loadSubScript(url, obj);
        
    display(getMsg(MSN_SUBSCRIPT_LOADED, url), MT_INFO);
    
    return rv;
    
}

function matchFileName (pattern)
{
    var rv = new Array();
    
    for (var scriptName in console.scripts)
        if (scriptName.search(pattern) != -1)
            rv.push (scriptName);

    return rv;
}

function refreshValues()
{
    for (var i = 0; i < $.length; ++i)
        $[i].refresh();
    console.stackView.refresh();
}

function stringToDOM (message)
{
    var ary = message.split ("\n");
    var span = htmlSpan();
    
    for (var l in ary)
    {
        console._munger.munge(ary[l], span);
        span.appendChild (htmlBR());
    }

    return span;
}

/* some of the drag and drop code has an annoying apetite for exceptions.  any
 * exception raised during a dnd operation causes the operation to fail silently.
 * passing the function through one of these adapters lets you use "return
 * false on planned failure" symantics, and dumps any exceptions caught
 * to the console. */
function Prophylactic (parent, fun)
{
    function adapter ()
    {
        var ex;
        var rv = false;

        try
        {
            rv = fun.apply (parent, arguments);
        }
        catch (ex)
        {
            dd ("Prophylactic caught an exception:\n" +
                dumpObjectTree(ex));
        }

        if (!rv)
            throw "goodger";
    }
        
    return adapter;
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
    this.message= msg;
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

/* console object */

/* input history (up/down arrow) related vars */
console._inputHistory = new Array();
console._lastHistoryReferenced = -1;
console._incompleteLine = "";

/* tab complete */
console._lastTabUp = new Date();

console.display = display;
console.load = load;

console.__defineGetter__ ("status", con_getstatus);
function con_getstatus ()
{
    return console.ui["status-text"].getAttribute ("label");
}

console.__defineSetter__ ("status", con_setstatus);
function con_setstatus (msg)
{
    if (!msg)
        msg = console._statusStack[console._statusStack.length - 1];
    
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
    console._statusStack.push (console.status);
    console.status = msg;
}

console.popStatus =
function con_popstatus ()
{
    console.status = console._statusStack.pop();
}

console.scrollDown =
function con_scrolldn ()
{
    window.frames[0].scrollTo(0, window.frames[0].document.height);
}

function SourceText (scriptContainer)
{
    this.sourceText = new Array();
    this.tabWidth = console.prefs["sourcetext.tab.width"];
    this.fileName = scriptContainer.fileName;
    this.scriptContainer = scriptContainer;
    this.lineMap = new Array();
}

SourceText.prototype.invalidate =
function st_invalidate ()
{
    if (console.scriptsView.childData == this)
    {
        if (!("lastRowCount" in this) || 
            this.lastRowCount != this.sourceText.length)
        {
            this.lastRowCount = this.sourceText.length;
            console.scriptsView.outliner.rowCountChanged(this.lastRowCount);
        }    
        console.scriptsView.outliner.invalidate();
    }
}

SourceText.prototype.onMarginClick =
function st_marginclick (e, line)
{
    setBreakpoint (this.fileName, line + 1);
}

SourceText.prototype.onBreakpointCreated =
function st_newbp (line)
{
    dd ("breakpoint created at line " + line);
    
    if (!(line in this.lineMap))
        this.lineMap[line - 1] = SourceText.LINE_BREAKPOINT;
    else
        this.lineMap[line - 1] |= SourceText.LINE_BREAKPOINT;
    this.invalidate();
}

SourceText.prototype.onBreakpointCleared =
function st_newbp (line)
{
    ASSERT ((line - 1) in this.lineMap,
            "cleared breakpoint for non-existant line");
    this.lineMap[line - 1] &= !SourceText.LINE_BREAKPOINT;
    this.invalidate();
}

SourceText.prototype.isLoaded = false;

SourceText.prototype.reloadSource =
function st_reloadsrc (cb)
{
    var sourceRec = this;
    
    function reloadCB (status)
    {
        sourceRec.invalidate();
        cb(status);
    }

    this.isLoaded = false;
    this.sourceText = new Array();
    this.invalidate();
    this.loadSource(reloadCB);
}

SourceText.LINE_BREAKABLE  = 1;
SourceText.LINE_BREAKPOINT = 2;

SourceText.prototype.markBreakableLines =
function st_initmap()
{
    var sourceRec = this;
    
    function setFlag (line, flag)
    {
        if (line in sourceRec.lineMap)
            sourceRec.lineMap[line] |= flag;
        else
            sourceRec.lineMap[line] = flag;
    }
    
    console.pushStatus (getMsg(MSN_STATUS_MARKING,
                               this.scriptContainer.shortName));
    
    var scripts = this.scriptContainer.childData;
    for (var i = 0; i < scripts.length; ++i)
    {
        var scriptRec = scripts[i];
        var end = scriptRec.baseLineNumber + scriptRec.lineExtent;
        for (var j = scriptRec.baseLineNumber; j < end; ++j)
        {
            var script = scriptRec.script;
            if (script.isLineExecutable(j + 1, PCMAP_SOURCETEXT)) {
                setFlag (j, SourceText.LINE_BREAKABLE);
            }
        }
    }

    console.popStatus();
}

SourceText.prototype.loadSource =
function st_loadsrc (cb)
{
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
        if (!("extraCallbacks" in this))
            this.extraCallbacks = new Array();
        this.extraCallbacks.push (cb);
        return;
    }

    var sourceRec = this;
    this.isLoading = true;    
    
    var observer = {
        onComplete: function oncomplete (data, url, status) {
            function callall (status)
            {
                cb (status);
                if ("extraCallbacks" in sourceRec)
                {
                    while (sourceRec.extraCallbacks)
                    {
                        cb = sourceRec.extraCallbacks.pop();
                        cb (status);
                        if (sourceRec.extraCallbacks.length < 1)
                            delete sourceRec.extraCallbacks;
                    }
                }
            }
            
            delete sourceRec.isLoading;
            
            if (status != Components.results.NS_OK)
            {
                dd ("loadSource failed with status " + status + ", " + url);
                callall (status);
                console.popStatus();                
                return;
            }
            
            sourceRec.isLoaded = true;
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
            sourceRec.sourceText = ary;
            ary = ary[0].match (/tab-?width*:\s*(\d+)/i);
            if (ary)
                sourceRec.tabWidth = ary[1];
            
            sourceRec.scriptContainer.guessFunctionNames(sourceRec.sourceText);
            sourceRec.markBreakableLines();
            sourceRec.invalidate();
            callall(status);
            console.popStatus();
        }
    };

    var ex;
    var src;
    var url = this.scriptContainer.fileName;
    console.pushStatus (getMsg(MSN_STATUS_LOADING, url));
    try
    {
        src = loadURLNow(url);
    }
    catch (ex)
    {
        /* if we can't load it now, try to load it later */
        loadURLAsync (url, observer);
    }

    observer.onComplete (src, url, Components.results.NS_OK);
    this.invalidate();
    delete this.isLoading;

}

function PPSourceText (scriptRecord)
{
    var sourceRec = this;
    
    function setFlag (line, flag)
    {
        if (line in sourceRec.lineMap)
            sourceRec.lineMap[line] |= flag;
        else
            sourceRec.lineMap[line] = flag;
    }

    this.tabWidth = console.prefs["sourcetext.tab.width"];
    this.scriptRecord = scriptRecord;
    this.fileName = scriptRecord.script.fileName;
    this.sourceText = String(scriptRecord.script.functionSource).split("\n");
    this.lineMap = new Array();
    var len = this.sourceText.length;
    for (var i = 0; i < len; ++i)
    {
        if (scriptRecord.script.isLineExecutable(i + 1, PCMAP_PRETTYPRINT))
        {
            setFlag (i, SourceText.LINE_BREAKABLE);
        }
    }
}

PPSourceText.prototype.invalidate =
function ppst_invalidate ()
{
    if (console.scriptsView.childData == this)
    {
        if (!("lastRowCount" in this) || 
            this.lastRowCount != this.sourceText.length)
        {
            this.lastRowCount = this.sourceText.length;
            console.scriptsView.outliner.rowCountChanged(this.lastRowCount);
        }    
        console.scriptsView.outliner.invalidate();
    }
}

PPSourceText.prototype.onBreakpointCreated =
function ppst_newbp (line)
{
    if (!(line in this.lineMap))
        this.lineMap[line] = SourceText.LINE_BREAKPOINT;
    else
        this.lineMap[line] |= SourceText.LINE_BREAKPOINT;
    this.invalidate();
}

PPSourceText.prototype.onBreakpointCleared =
function ppst_newbp (line)
{
    ASSERT (line in this.lineMap, "cleared breakpoint for non-existant line");
    this.lineMap[line] &= !SourceText.LINE_BREAKPOINT;
    this.invalidate();
}

PPSourceText.prototype.isLoaded = true;
