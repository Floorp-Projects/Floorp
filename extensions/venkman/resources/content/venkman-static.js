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
    dd = function (msg) { dumpln("-*- venkman: " + msg); }
    warn = function (msg) { dd ("** WARNING " + msg + " **"); }
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

function startupTests()
{
    if (0)
    {   
        openDialog("chrome://venkman/content/tests/tree.xul", "", "");
    }
    
}

function cont ()
{
    disableDebugCommands();
    --console._stopLevel;
    console.jsds.exitNestedEventLoop();
    return true;
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
    console._stepPast = topFrame.script.fileName + topFrame.line;
    disableDebugCommands()
    --console._stopLevel;
    console.jsds.exitNestedEventLoop();
}

function stepOut ()
{
    console._stepOverLevel = 1;
    setStopState(false);
    console.jsds.functionHook = console._callHook;
    disableDebugCommands()
    --console._stopLevel;
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
    var cmds = document.getElementById("venkmanDebuggerCommands");
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
    var cmds = document.getElementById("venkmanDebuggerCommands");
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

    
function fillInTooltip(tipElement)
{
    const XULNS =
        "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    const XLinkNS = "http://www.w3.org/1999/xlink";
    const Node = { ELEMENT_NODE : 1 }; // XXX Components.interfaces.Node;
    
    var retVal = false;
    var tipNode = document.getElementById("tooltipBox");
    while (tipNode.hasChildNodes())
        tipNode.removeChild(tipNode.firstChild);
    var titleText = "";
    var XLinkTitleText = "";
    while (!titleText && !XLinkTitleText && tipElement) {
        if (tipElement.nodeType == Node.ELEMENT_NODE) {
            titleText = tipElement.getAttribute("title");
            XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
        }
        tipElement = tipElement.parentNode;
    }
    var texts = [titleText, XLinkTitleText];
    for (var i = 0; i < texts.length; ++i) {
        var t = texts[i];
        if (t.search(/\S/) >= 0) {
            var tipLineElem =
                tipNode.ownerDocument.createElementNS(XULNS, "text");
            tipLineElem.setAttribute("value", t);
            tipNode.appendChild(tipLineElem);
            retVal = true;
        }
    }
    
    return retVal;
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

console.scrollDown =
function con_scrolldn ()
{
    window.frames[0].scrollTo(0, window.frames[0].document.height);
}
