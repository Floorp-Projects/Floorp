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


var MAX_STR_LEN = 100;  /* max string length to display before changing to
                         * "n characters" display mode.  as a var so we can
                         * change it later. */
const MAX_WORD_LEN = 20; /* number of characters to display before forcing a 
                          * potential word break point (in the console.) */

var console = new Object();

console.version = "0.8.5";

/* |this|less functions */

function setStopState(state)
{
    var tb = document.getElementById("maintoolbar-stop");
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

function setProfileState(state)
{
    var tb = document.getElementById("maintoolbar-profile-tb");
    if (state)
    {
        console.jsds.flags |= COLLECT_PROFILE_DATA;
        tb.setAttribute("profile", "true");
    }
    else
    {
        console.jsds.flags &= ~COLLECT_PROFILE_DATA;
        tb.removeAttribute("profile");
    }
}

function setPrettyPrintState(state)
{
    var tb = document.getElementById("maintoolbar-pprint");
    if (state)
    {
        console.sourceView.prettyPrint = true;
        tb.setAttribute("state", "true");
    }
    else
    {
        console.sourceView.prettyPrint = false;
        tb.removeAttribute("state");
    }

    if (console.sourceView.details)
        dispatch("find-script", {scriptRec: console.sourceView.details});

}
    
function enableReloadCommand()
{
    console.commandManager.commands["reload"].enabled = true;
}

function disableReloadCommand()
{
    console.commandManager.commands["reload"].enabled = false;
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


function displayUsageError (e, details)
{
    display (details, MT_ERROR);
    display (getMsg(MSN_FMT_USAGE, [e.command.name, e.command.usage]), MT_USAGE);
}

function dispatch (text, e, flags)
{
    if (!e)
    {
        if (typeof CommandManager.cx == "object")
            e = CommandManager.cx;
        else
            e = new Object();
    }

    if (!("inputData" in e))
        e.inputData = "";
    
    /* split command from arguments */
    var ary = text.match (/(\S+) ?(.*)/);
    e.commandText = ary[1];
    e.inputData =  ary[2] ? stringTrim(ary[2]) : "";
    
    /* list matching commands */
    ary = console.commandManager.list (e.commandText, flags);
    var i;
    
    switch (ary.length)
    {            
        case 0:
            /* no match, try again */
            display (getMsg(MSN_ERR_NO_COMMAND, e.commandText), MT_ERROR);
            break;
            
        case 1:
            /* one match, good for you */
            var rv;
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

    return null;
}

function dispatchCommand (command, e, flags)
{
    if (!e)
    {
        if (typeof CommandManager.cx == "object")
            e = CommandManager.cx;
        else
            e = new Object();
    }

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
        display (getMsg(MSG_ERR_DISABLED, e.command.name),
                 MT_ERROR);
        return null;
    }                
    
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
                dd ("dispatching command ``" + e.command.name+ "''\n" +
                    dumpObjectTree(e));
                e.returnValue = e.command.func(e);
                console.lastEvent = e;
            }
            else
            {
                e.returnValue = e.command.func(e);
            }

        }
        return e;
    }
    
    if (typeof e.command.func == "string")
    {
        /* dispatch an alias (semicolon delimited list of subcommands) */
        var commandList = e.command.func.split(";");
        for (var i = 0; i < commandList.length; ++i)
        {
            commandList[i] = stringTrim(commandList[i]);
            if (i == 1)
                dispatch (commandList[i] + " " + e.inputData, e, flags);
            else
                dispatch (commandList[i], null, flags);
        }
        return e;
    }

    /* by process of elimination... */
    display (getMsg(MSN_ERR_NOTIMPLEMENTED, e.command.name),
             MT_ERROR);

    return null;    
}

function feedback(e, message, msgtype)
{
    if ("isInteractive" in e && e.isInteractive)
        display (message, msgtype);
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
        if (attrs)
        {
            for (var a in attrs)
                obj.setAttribute (a, attrs[a]);
        }
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

function setCurrentSource (url, line)
{
    var fileRec = console.scripts[url];
    ASSERT (fileRec, "Attempt to focus unknown source: " + url);

    var lastFile = console._sourceTreeView.url
    if (lastFile)
    {
        var lastFileRec = console.scripts[lastFile];
        if (lastFileRec)
        {
            lastFileRec.lastLine = 
                console._sourceTreeView.tree.getFirstVisibleRow();
        }
    }
    
    console._sourceTreeView.setSourceArray(fileRec.source, url);
    var hdr = document.getElementById("source-line-text");
    hdr.setAttribute ("label", url);
    hdr = document.getElementById("source-line-number");
    hdr.setAttribute ("label", "");    
}

function scrollSourceLineTop (url, lineNumber)
{
    focusSource(url);
    if (toTop)
        console._sourceTreeView.tree.scrollToRow(lineNumber - 1);
    else
        console._sourceTreeView.centerLine(lineNumber - 1);
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
    
    var rval = new Object();
    
    if (!getCurrentFrame().eval (script, MSG_VAL_CONSOLE, 1, rval))
    {
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
    var ary = navigator.userAgent.match (/;\s*([^;\s]+\s*)\).*\/(\d+)/);
    if (ary)
    {
        console.userAgent = "Venkman " + console.version + " [Mozilla " + 
            ary[1] + "/" + ary[2] + "]";
    }
    else
    {
        console.userAgent = "Venkman " + console.version + " [" + 
            navigator.userAgent + "]";
    }

    const WW_CTRID = "@mozilla.org/embedcomp/window-watcher;1";
    const nsIWindowWatcher = Components.interfaces.nsIWindowWatcher;
    console.windowWatcher =
        Components.classes[WW_CTRID].getService(nsIWindowWatcher);

    console.debuggerWindow = getBaseWindowFromWindow(window);

    initPrefs();
    initCommands();
    initMenus();
    initTrees();
    
    disableDebugCommands();
    
    console.files = new Object();

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

    console.ui = new Object();
    console.ui["status-text"] = document.getElementById ("status-text");
    console.ui["sl-input"] = document.getElementById ("input-single-line");
    console._statusStack = new Array();
    console.pluginState = new Object();
    dispatch("version");

    var ary = console.prefs["initialScripts"].split();
    for (var i = 0; i < ary.length; ++i)
    {
        var url = stringTrim(ary[i]);
        if (url)
            dispatch ("loadd", {url: ary[i]});
    }
    
    console.sourceText = new HelpText();

    dispatch("commands");
    dispatch("help");

    initHandlers();
}

function destroy ()
{
    destroyTrees();
    destroyHandlers();
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

/* some of the drag and drop code has an annoying appetite for exceptions.  any
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
console.inputHistory = new Array();
console.lastHistoryReferenced = -1;
console.incompleteLine = "";

/* tab complete */
console._lastTabUp = new Date();

console.display = display;

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

console.getProfileSummary =
function con_getProfileSummary (fileName, key)
{
    if (typeof key == "undefined")
        key = "max";
    
    function compare (a, b)
    {
        if (a.key > b.key)
            return 1;
        if (a.key < b.key)
            return -1;
        return 0;
    };
    
    function addScriptRec(s)
    {
        var ex;
        
        try
        {
            var ccount = s.script.callCount;
            var tot_ms = roundTo(s.script.totalExecutionTime, 2);
            var min_ms = roundTo(s.script.minExecutionTime, 2);
            var max_ms = roundTo(s.script.maxExecutionTime, 2);
            var avg_ms = roundTo(s.script.totalExecutionTime / ccount, 2);
            var recurse = s.script.maxRecurseDepth;

            var obj = new Object();
            obj.total = tot_ms;
            obj.ccount = ccount;
            obj.avg = avg_ms;
            obj.min = min_ms;
            obj.max = max_ms;
            obj.recurse = recurse;
            obj.path = s.script.fileName;
            obj.file = getFileFromPath(obj.path);
            obj.base = s.script.baseLineNumber;
            obj.end = obj.base + s.script.lineExtent;
            obj.fun = s.functionName;
            obj.str = obj.fun  + ":" + obj.base + "-" + obj.end +
                ", calls " + ccount +
                (obj.recurse ? " (depth " + recurse +")" : "") +
                ", total " + tot_ms + 
                "ms, min " + min_ms +
                "ms, max " + max_ms +
                "ms, avg " + avg_ms + "ms.";
            obj.key = obj[key];
            list.push (obj);
        }
        catch (ex)
        {
            /* This function is called under duress, and the script representd
             * by |s| may get collected at any point.  When that happens,
             * attempting to access to the profile data will throw this
             * exception.
             */
            if (ex == Components.results.NS_ERROR_NOT_AVAILABLE)
            {
                display (getMsg(MSG_PROFILE_LOST, formatScript(s)), MT_WARN);
            }
            else
            {
                throw ex;
            }
            
        }
        
    };

    function addScriptContainer (container)
    {
        for (var i = 0; i < container.childData.length; ++i)
        {
            if (container.childData[i].script.callCount)
                addScriptRec(container.childData[i]);
        }
    };
    
    var list = new Array();
    list.key = key;
    
    if (!fileName)
    {
        for (var c in console.scripts)
            addScriptContainer (console.scripts[c]);
    } else {
        if (!(fileName in console.scripts))
            return null;
        addScriptContainer (console.scripts[fileName]);
    }
    
    list.sort(compare);
    return list;
}
        
function loadTemplate(url)
{
    var lines = loadURLNow(url);
    if (!lines)
        return null;

    var obj = new Object();
    var i;

    var sections = 
        {"fileHeader"    : /^<!--@section-start-->/m,
         "sectionHeader" : /^<!--@range-start-->/m,
         "rangeHeader"   : /^<!--@item-start-->/m,
         "itemBody"      : /^<!--@item-end-->/m,
         "rangeFooter"   : /^<!--@range-end-->/m,
         "sectionFooter" : /^<!--@section-end-->/m,
         "fileFooter"    : 0
        };

    for (var s in sections)
    {
        if (sections[s])
        {
            i = lines.search(sections[s]);
            if (i == -1)
                throw "Cant match " + String(sections[s]);
            obj[s] = lines.substr(0, i - 1);
            i = lines.indexOf("\n", i);
            lines = lines.substr(i);
        }
        else
        {
            obj[s] = lines;
            lines = "";
        }
    }

    return obj;
}
    
function writeHeaderHTML(file, tpl)
{
    file.tpl = loadTemplate(console.prefs["profile.template.html"]);
    file.fileData = {
        "\\$full-date"    : String(Date()),
        "\\$user-agent"   : navigator.userAgent,
        "\\$venkman-agent": console.userAgent
    };
    file.write(replaceStrings(file.tpl.fileHeader, file.fileData));
};

function writeSummaryHTML(file, summary, fileName)
{
    function scale(x) { return roundTo(K * x, 2); };

    function writeSummaryEntry()
    {
        var entryData = {
            "\\$item-number-next": summary.length - i + 1,
            "\\$item-number-prev": summary.length - i - 1,
            "\\$item-number"     : summary.length - i,
            "\\$item-name"       : r.path,
            "\\$item-summary"    : r.str,
            "\\$item-min-pct"    : scale(r.min),
            "\\$item-below-pct"  : scale(r.avg - r.min),
            "\\$item-above-pct"  : scale(r.max - r.avg),
            "\\$time-max"        : r.max,
            "\\$time-min"        : r.min,
            "\\$time-avg"        : r.avg,
            "\\$time-tot"        : r.total,
            "\\$call-count"      : r.ccount,
            "\\$funcion-name"    : r.fun,
            "\\$file-name"       : r.file,
            "\\$full-url"        : r.path,
            "\\$line-start"      : r.base,
            "\\$line-end"        : r.end,
            "__proto__": rangeData
        };
    
        file.write(replaceStrings(file.tpl.itemBody, entryData));
    };
    
    if (!summary || summary.length < 1)
        return;

    if ("sumNo" in file)
        ++file.sumNo;
    else
        file.sumNo = 1;

    var headerData = {
        "\\$section-number-prev": file.sumNo - 1,
        "\\$section-number-next": file.sumNo + 1,
        "\\$section-number"     : file.sumNo,
        "\\$section-link"       : fileName ? "<a class='section-link' href='" +
                                  fileName + "'>" + fileName + "</a>" :
                                  "** All Files **",
        "__proto__"             : file.fileData
    };

    file.write(replaceStrings(file.tpl.sectionHeader, headerData));

    const MAX_WIDTH = 90;
    var ranges = console.prefs["profile.ranges"].split(",");
    if (!ranges.length)
        throw "Bad value for pref profile.ranges";
    for (i = 0; i < ranges.length; ++i)
        ranges[i] = Number(ranges[i]);
    ranges.push(0); // push two 0's to the end of the list so the user doesn't
    ranges.push(0); // have to.
    var rangeIndex = 1;
    var lastRangeIndex = 0;
    var K = 1;
    var rangeIter = 0;
    for (var i = summary.length - 1; i >= 0; --i)
    {
        var r = summary[i];
        while (r.key && r.key <= ranges[rangeIndex])
            ++rangeIndex;

        if (lastRangeIndex != rangeIndex)
        {
            ++rangeIter;
            K = MAX_WIDTH / ranges[rangeIndex - 1];
            var rangeData = {
                "\\$range-min"        : ranges[rangeIndex],
                "\\$range-max"        : ranges[rangeIndex - 1],
                "\\$range-number-prev": rangeIter - 1,
                "\\$range-number-next": rangeIter,
                "\\$range-number"     : rangeIter,
                "__proto__"           : headerData
            };
            if (rangeIndex > 0)
                file.write(replaceStrings(file.tpl.rangeFooter, rangeData));
            file.write(replaceStrings(file.tpl.rangeHeader, rangeData));
            lastRangeIndex = rangeIndex;
        }
        writeSummaryEntry();
    }

    file.write(replaceStrings(file.tpl.rangeFooter, rangeData));
    file.write(replaceStrings(file.tpl.sectionFooter, headerData));
}

function writeFooterHTML(file)
{
    file.write(replaceStrings(file.tpl.fileFooter, file.fileData));
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

function SourceText (scriptContainer, url)
{
    this.lines = new Array();
    this.tabWidth = console.prefs["sourcetext.tab.width"];
    this.fileName = url;
    this.scriptContainer = scriptContainer;
    this.lineMap = new Array();
}

SourceText.prototype.invalidate =
function st_invalidate ()
{
    if ("childData" in console.sourceView &&
        console.sourceView.childData == this)
    {
        if (!("lastRowCount" in this) || 
            this.lastRowCount != this.lines.length)
        {
            this.lastRowCount = this.lines.length;
            console.sourceView.tree.rowCountChanged(0, this.lastRowCount);
        }    
        console.sourceView.tree.invalidate();
    }
}

SourceText.prototype.onMarginClick =
function st_marginclick (e, line)
{
    if (getBreakpoint(this.fileName, line))
    {
        clearBreakpoint (this.fileName, line);
    }
    else
    {
        if (this.scriptContainer)
            setBreakpoint (this.fileName, line);
        else
            setFutureBreakpoint (this.fileName, line);
    }
    console.sourceView.tree.invalidateRow(line - 1);
}

SourceText.prototype.isLoaded = false;

SourceText.prototype.reloadSource =
function st_reloadsrc (cb)
{
    var sourceRec = this;
    
    function reloadCB (status)
    {
        sourceRec.invalidate();
        if (typeof cb == "function")
            cb(status);
    }

    this.isLoaded = false;
    this.lines = new Array();
    this.invalidate();
    this.loadSource(reloadCB);
}

SourceText.prototype.markBreakableLines =
function st_initmap()
{
    var sourceText = this;
    
    function setFlag (line, flag)
    {
        if (line in sourceText.lineMap)
            sourceText.lineMap[line] |= flag;
        else
            sourceText.lineMap[line] = flag;
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
                setFlag (j, console.sourceView.LINE_BREAKABLE);
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

    var sourceText = this;
    this.isLoading = true;    
    
    var observer = {
        onComplete: function oncomplete (data, url, status) {
            function callall (status)
            {
                cb (status);
                if ("extraCallbacks" in sourceText)
                {
                    while (sourceText.extraCallbacks)
                    {
                        cb = sourceText.extraCallbacks.pop();
                        cb (status);
                    }
                    delete sourceText.extraCallbacks;
                }
            }
            
            delete sourceText.isLoading;
            
            if (status != Components.results.NS_OK)
            {
                dd ("loadSource failed with status " + status + ", " + url);
                callall (status);
                console.popStatus();                
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
            sourceText.lines = ary;
            ary = ary[0].match (/tab-?width*:\s*(\d+)/i);
            if (ary)
                sourceText.tabWidth = ary[1];
            
            if (sourceText.scriptContainer)
            {
                sourceText.scriptContainer.guessFunctionNames(sourceText);
                sourceText.markBreakableLines();
            }
            sourceText.isLoaded = true;
            sourceText.invalidate();
            callall(status);
            console.popStatus();
        }
    };

    var ex;
    var src;
    var url = this.fileName;
    console.pushStatus (getMsg(MSN_STATUS_LOADING, url));
    try
    {
        src = loadURLNow(url);
        observer.onComplete (src, url, Components.results.NS_OK);
        this.invalidate();
        delete this.isLoading;
    }
    catch (ex)
    {
        /* if we can't load it now, try to load it later */
        try
        {
            loadURLAsync (url, observer);
        }
        catch (ex)
        {
            display (getMsg(MSN_ERR_SOURCE_LOAD_FAILED, [url, ex]), MT_ERROR);
            observer.onComplete (src, url, Components.results.NS_ERROR_FAILURE);
        }    
    }
}

function PPSourceText (scriptRecord)
{
    this.scriptRecord = scriptRecord;
    this.reloadSource();
}

PPSourceText.prototype.isLoaded = true;

PPSourceText.prototype.reloadSource =
function (cb)
{
    var sourceText = this;
    
    function setFlag (line, flag)
    {
        if (line in sourceText.lineMap)
            sourceText.lineMap[line] |= flag;
        else
            sourceText.lineMap[line] = flag;
    }

    this.tabWidth = console.prefs["sourcetext.tab.width"];
    this.fileName = this.scriptRecord.script.fileName;
    this.lines = String(this.scriptRecord.script.functionSource).split("\n");
    this.lineMap = new Array();
    var len = this.lines.length;
    for (var i = 0; i < len; ++i)
    {
        if (this.scriptRecord.script.isLineExecutable(i + 1, PCMAP_PRETTYPRINT))
        {
            setFlag (i, console.sourceView.LINE_BREAKABLE);
        }
    }

    if (typeof cb == "function")
        cb (Components.results.NS_OK);
}

PPSourceText.prototype.invalidate =
function st_invalidate ()
{
    dd ("invalidate ppsourcetext");
    
    if ("childData" in console.sourceView &&
        console.sourceView.childData == this)
    {
        var topRow = console.sourceView.tree.getFirstVisibleRow() + 1;
        if (!("lastRowCount" in this) || 
            this.lastRowCount != this.lines.length)
        {
            this.lastRowCount = this.lines.length;
            console.sourceView.tree.rowCountChanged(0, this.lastRowCount);
        }    
        console.sourceView.scrollTo(topRow, -1);
        console.sourceView.tree.invalidate();
    }
}

function HelpText ()
{
    this.tabWidth = console.prefs["sourcetext.tab.width"];
    this.fileName = "x-jsd:help";
    this.reloadSource();
}

HelpText.prototype.isLoaded = true;

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
    this.invalidate();
    if (typeof cb == "function")
        cb (Components.results.NS_OK);
}
        
HelpText.prototype.invalidate =
function st_invalidate ()
{
    if ("childData" in console.sourceView &&
        console.sourceView.childData == this)
    {
        var topRow = console.sourceView.tree.getFirstVisibleRow() + 1;
        if (!("lastRowCount" in this) || 
            this.lastRowCount != this.lines.length)
        {
            this.lastRowCount = this.lines.length;
            console.sourceView.tree.rowCountChanged(0, this.lastRowCount);
        }    
        console.sourceView.scrollTo(topRow, -1);
        console.sourceView.tree.invalidate();
    }
}
