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

if (DEBUG)
{
    dd = function (msg) { dumpln("-*- venkman: " + msg); }
    warn = function (msg) { dd ("** WARNING " + msg + " **"); }
    ASSERT = function (expr, msg) {
        if (!expr)
            dd ("** ASSERTION FAILED: " + msg + " **\n" + getStackTrace()); 
    }
}    
else
    dd = warn = ASSERT = function (){};

var console = new Object();

/* |this|less functions */

function htmlVA (attribs, href, contents)
{
    if (!attribs)
        attribs = {"class": "venkman-link", target: "_content"};
    else if (attribs["class"])
        attribs["class"] += " venkman-link";
    
    return htmlA (attribs, href, contents);
}
    
function init()
{
    initCommands();
    initPrefs();
    initDebugger();
    
    console._outputDocument = 
        document.getElementById("output-iframe").contentDocument;
    
    console._outputElement = 
        console._outputDocument.getElementById("output-tbody");    

    display(htmlSpan(null, MSG_HELLO1 + " ",
                     htmlVA(null, 
                            "chrome://venkman/content/tests/testpage.html"),
                     " " + MSG_HELLO2), MT_HELLO);
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

/* exceptions */

/* keep this list in sync with exceptionMsgNames in venkman-msg.js */
const ERR_NOT_IMPLEMENTED = 0;
const ERR_REQUIRED_PARAM  = 1;
const ERR_INVALID_PARAM   = 2;
const ERR_SUBSCRIPT_LOAD  = 3;

/* venkman exception factory, can be used with or without |new|.
 * throw BadMojo (ERR_FOO, MSG_VAL_UNDEFINED);
 * throw new BadMojo (ERR_NOT_IMPLEMENTED);
 */
function BadMojo (errno, params)
{
    var msg = getMsg(exceptionMsgNames[errno], params);
    
    dd ("new BadMojo (" + errno + ": " + msg + ") from\n" + getStackTrace());
    return {message: msg, name: "Error " + errno,
            fileName: Components.stack.caller.filename,
            lineNumber: Components.stack.caller.lineNumber,
            functionName: Components.stack.caller.functionName};
}

/* console object */

/* input history (up/down arrow) related vars */
console._inputHistory = new Array();
console._lastHistoryReferenced = -1;
console._incompleteLine = "";

/* tab complete */
console._lastTabUp = new Date();

var display = console.display =
function display(message, msgtype)
{
    if (typeof message == "undefined")
        throw BadMojo(ERR_REQUIRED_PARAM, "message");

    if (typeof message != "string" && typeof message != "object")
        throw BadMojo(ERR_INVALID_PARAM, "message");

    if (typeof msgtype == "undefined")
        msgtype = MT_INFO;
    
    function setAttribs (obj, c, attrs)
    {
        for (var a in attrs)
            obj.setAttribute (a, attrs[a]);
        obj.setAttribute("class", c);
        obj.setAttribute("msg-type", msgtype);
    }

    function stringToMsg (message)
    {
        var ary  = message.split("\n");
        var span = htmlSpan();

        for (var l in ary)
        {
            var wordParts = 
                splitLongWord (ary[l], console.prefs["output.wordbreak.length"]);
            for (var i in wordParts)
            {
                span.appendChild (htmlText(wordParts[i]));
                span.appendChild (htmlImg());
            }
                    
            span.appendChild (htmlBR());
        }
        return span;
    }

    var msgRow = htmlTR("msg");
    setAttribs(msgRow, "msg");

    var msgData = htmlTD();
    setAttribs(msgData, "msg-data");
    if (typeof message == "string")
        msgData.appendChild(stringToMsg(message));
    else
        msgData.appendChild(message);

    msgRow.appendChild(msgData);

    console._outputElement.appendChild(msgRow);
    console.scrollDown();
}

console.load = load;

console.scrollDown =
function con_scrolldn ()
{
    window.frames[0].scrollTo(0, window.frames[0].document.height);
}

