/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * New Dimensions Consulting, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, rginda@ndcico.com, original author
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

var dumpln;
var dd;

const nsIBaseWindow         = Components.interfaces.nsIBaseWindow;
const nsIXULWindow          = Components.interfaces.nsIXULWindow;
const nsIInterfaceRequestor = Components.interfaces.nsIInterfaceRequestor;
const nsIWebNavigation      = Components.interfaces.nsIWebNavigation;
const nsIDocShellTreeItem   = Components.interfaces.nsIDocShellTreeItem;

var utils = new Object();

if (typeof document == "undefined") /* in xpcshell */
{
    dumpln = print;
}
else
{
    if (typeof dump == "function")
        dumpln = function (str) {dump (str + "\n");}
    else if (jsenv.HAS_RHINO)
    {
        dumpln = function (str) {
                     var out = java.lang.System.out;
                     out.println(str); out.flush();
                 }
    }
    else
        dumpln = function () {} /* no suitable function */
}

if (DEBUG) {
    var _dd_pfx = "";
    var _dd_singleIndent = "  ";
    var _dd_indentLength = _dd_singleIndent.length;
    var _dd_currentIndent = "";
    var _dd_lastDumpWasOpen = false;
    var _dd_timeStack = new Array();
    var _dd_disableDepth = Number.MAX_VALUE;
    var _dd_currentDepth = 0;
    dd = function _dd (str) {
             if (typeof str != "string") {
                 dumpln (str);
             } else if (str[str.length - 1] == "{") {
                 ++_dd_currentDepth;
                 if (_dd_currentDepth >= _dd_disableDepth)
                     return;
                 if (str.indexOf("OFF") == 0)
                     _dd_disableDepth = _dd_currentDepth;
                 _dd_timeStack.push (new Date());
                 if (_dd_lastDumpWasOpen)
                     dump("\n");
                 dump (_dd_pfx + _dd_currentIndent + str);
                 _dd_currentIndent += _dd_singleIndent;
                 _dd_lastDumpWasOpen = true;
             } else if (str[0] == "}") {
                 if (--_dd_currentDepth >= _dd_disableDepth)
                     return;
                 _dd_disableDepth = Number.MAX_VALUE;
                 var sufx = (new Date() - _dd_timeStack.pop()) / 1000 + " sec";
                 _dd_currentIndent = 
                     _dd_currentIndent.substr (0, _dd_currentIndent.length -
                                               _dd_indentLength);
                 if (_dd_lastDumpWasOpen)
                     dumpln (str + " " + sufx);
                 else
                     dumpln (_dd_pfx + _dd_currentIndent + str + " " + sufx);
                 _dd_lastDumpWasOpen = false;
             } else {
                 if (_dd_currentDepth >= _dd_disableDepth)
                     return;
                 if (_dd_lastDumpWasOpen)
                     dump ("\n");
                 dumpln (_dd_pfx + _dd_currentIndent + str);
                 _dd_lastDumpWasOpen = false;
             }    
         }
} else {
    dd = function (){};
}

var jsenv = new Object();
jsenv.HAS_SECURITYMANAGER = ((typeof netscape == "object") &&
                             (typeof netscape.security == "object"));
jsenv.HAS_XPCOM = ((typeof Components == "object") &&
                   (typeof Components.classes == "object"));
jsenv.HAS_JAVA = (typeof java == "object");
jsenv.HAS_RHINO = (typeof defineClass == "function");
jsenv.HAS_DOCUMENT = (typeof document == "object");

/* Dumps an object in tree format, recurse specifiec the the number of objects
 * to recurse, compress is a boolean that can uncompress (true) the output
 * format, and level is the number of levels to intitialy indent (only useful
 * internally.)  A sample dumpObjectTree (o, 1) is shown below.
 *
 * + parent (object)
 * + users (object)
 * | + jsbot (object)
 * | + mrjs (object)
 * | + nakkezzzz (object)
 * | *
 * + bans (object)
 * | *
 * + topic (string) 'ircclient.js:59: nothing is not defined'
 * + getUsersLength (function) 9 lines
 * *
 */
function dumpObjectTree (o, recurse, compress, level)
{
    var s = "";
    var pfx = "";

    if (typeof recurse == "undefined")
        recurse = 0;
    if (typeof level == "undefined")
        level = 0;
    if (typeof compress == "undefined")
        compress = true;
    
    for (var i = 0; i < level; i++)
        pfx += (compress) ? "| " : "|  ";

    var tee = (compress) ? "+ " : "+- ";

    for (i in o)
    {
        var t;
        try
        {
            t = typeof o[i];
        
            switch (t)
            {
                case "function":
                    var sfunc = String(o[i]).split("\n");
                    if (sfunc[2] == "    [native code]")
                        sfunc = "[native code]";
                    else
                        sfunc = sfunc.length + " lines";
                    s += pfx + tee + i + " (function) " + sfunc + "\n";
                    break;
                    
                case "object":
                    s += pfx + tee + i + " (object) " + o[i] + "\n";
                    if (!compress)
                        s += pfx + "|\n";
                    if ((i != "parent") && (recurse))
                        s += dumpObjectTree (o[i], recurse - 1,
                                             compress, level + 1);
                    break;
                    
                case "string":
                    if (o[i].length > 200)
                        s += pfx + tee + i + " (" + t + ") " + 
                            o[i].length + " chars\n";
                    else
                        s += pfx + tee + i + " (" + t + ") '" + o[i] + "'\n";
                    break;
                    
                default:
                    s += pfx + tee + i + " (" + t + ") " + o[i] + "\n";
            }
        }
        catch (ex)
        {
            s += pfx + tee + i + " (exception) " + ex + "\n";
        }

        if (!compress)
            s += pfx + "|\n";

    }

    s += pfx + "*\n";
    
    return s;
    
}

function safeHTML(str)
{
    function replaceChars(ch)
    {
        switch (ch)
        {
            case "<":
                return "&lt;";
            
            case ">":
                return "&gt;";
                    
            case "&":
                return "&amp;";
        }

        return "?";
    };
        
    return String(str).replace(/[<>&]/g, replaceChars);
}

function alert(msg, parent, title)
{
    var PROMPT_CTRID = "@mozilla.org/embedcomp/prompt-service;1";
    var nsIPromptService = Components.interfaces.nsIPromptService;
    var ps = Components.classes[PROMPT_CTRID].getService(nsIPromptService);
    if (!parent)
        parent = window;
    if (!title)
        title = MSG_ALERT;
    ps.alert (parent, title, msg);
}

function confirm(msg, parent, title)
{
    var PROMPT_CTRID = "@mozilla.org/embedcomp/prompt-service;1";
    var nsIPromptService = Components.interfaces.nsIPromptService;
    var ps = Components.classes[PROMPT_CTRID].getService(nsIPromptService);
    if (!parent)
        parent = window;
    if (!title)
        title = MSG_CONFIRM;
    return ps.confirm (parent, title, msg);
}

function prompt(msg, initial, parent, title)
{
    var PROMPT_CTRID = "@mozilla.org/embedcomp/prompt-service;1";
    var nsIPromptService = Components.interfaces.nsIPromptService;
    var ps = Components.classes[PROMPT_CTRID].getService(nsIPromptService);
    if (!parent)
        parent = window;
    if (!title)
        title = MSG_PROMPT;
    rv = { value: initial };

    if (!ps.prompt (parent, title, msg, rv, null, {value: null}))
        return null;

    return rv.value
}

function getChildById (element, id)
{
    var nl = element.getElementsByAttribute("id", id);
    return nl.item(0);
}

function openTopWin (url)
{
    var window = getWindowByType ("navigator:browser");
    if (window)
    {
        var base = getBaseWindowFromWindow (window);
        if (base.enabled)
        {
            window.focus();
            window._content.location.href = url;
            return window;
        }
    }

    return openDialog (getBrowserURL(), "_blank", "chrome,all,dialog=no", url);
}
    
function getWindowByType (windowType)
{
    const MEDIATOR_CONTRACTID =
        "@mozilla.org/appshell/window-mediator;1";
    const nsIWindowMediator  = Components.interfaces.nsIWindowMediator;

    var windowManager =
        Components.classes[MEDIATOR_CONTRACTID].getService(nsIWindowMediator);

    return windowManager.getMostRecentWindow(windowType);
}

function htmlVA (attribs, href, contents)
{
    if (!attribs)
        attribs = {"class": "venkman-link", target: "_content"};
    else if (attribs["class"])
        attribs["class"] += " venkman-link";
    else
        attribs["class"] = "venkman-link";

    if (typeof contents == "undefined")
    {
        contents = htmlSpan();
        insertHyphenatedWord (href, contents);
    }
    
    return htmlA (attribs, href, contents);
}

function insertHyphenatedWord (longWord, containerTag)
{
    var wordParts = splitLongWord (longWord, MAX_WORD_LEN);
    containerTag.appendChild (htmlWBR());
    for (var i = 0; i < wordParts.length; ++i)
    {
        containerTag.appendChild (document.createTextNode (wordParts[i]));
        if (i != wordParts.length)
            containerTag.appendChild (htmlWBR());
    }
}

function insertLink (matchText, containerTag)
{
    var href;
    var linkText;
    
    var trailing;
    ary = matchText.match(/([.,]+)$/);
    if (ary)
    {
        linkText = RegExp.leftContext;
        trailing = ary[1];
    }
    else
    {
        linkText = matchText;
    }

    var ary = linkText.match (/^(\w[\w-]+):/);
    if (ary)
    {
        if (!("schemes" in utils))
        {
            var pfx = "@mozilla.org/network/protocol;1?name=";
            var len = pfx.length

            utils.schemes = new Object();
            for (var c in Components.classes)
            {
                if (c.indexOf(pfx) == 0)
                    utils.schemes[c.substr(len)] = true;
            }
        }
        
        if (!(ary[1] in utils.schemes))
        {
            insertHyphenatedWord(matchText, containerTag);
            return;
        }

        href = linkText;
    }
    else
    {
        href = "http://" + linkText;
    }

    var anchor = htmlVA (null, href, "");
    insertHyphenatedWord (linkText, anchor);
    containerTag.appendChild (anchor);
    if (trailing)
        insertHyphenatedWord (trailing, containerTag);
    
}

function insertQuote (matchText, containerTag, msgtype)
{
    if (msgtype[0] == "#")
    {
        containerTag.appendChild(document.createTextNode(matchText));
        return;
    }
    
    if (matchText == "``")
        containerTag.appendChild(document.createTextNode("\u201c"));
    else
        containerTag.appendChild(document.createTextNode("\u201d"));
}

/* length should be an even number >= 6 */
function abbreviateWord (str, length)
{
    if (str.length <= length || length < 6)
        return str;

    var left = str.substr (0, (length / 2) - 1);
    var right = str.substr (str.length - (length / 2) + 1);

    return left + "..." + right;
}

function toBool (val)
{
    switch (typeof val)
    {
        case "boolean":
            return val;
    
        case "number":
            return val != 0;
            
        default:
            val = String(val);
            /* fall through */

        case "string":
            return (val.search(/true|on|yes|1/i) != -1);
    }

    return null;
}

/* some of the drag and drop code has an annoying appetite for exceptions.  any
 * exception raised during a dnd operation causes the operation to fail silently.
 * passing the function through one of these adapters lets you use "return
 * false on planned failure" symantics, and dumps any exceptions caught
 * to the console. */
function Prophylactic (parentObj, fun)
{
    function adapter ()
    {
        var ex;
        var rv = false;
        
        try
        {
            rv = fun.apply (parentObj, arguments);
        }
        catch (ex)
        {
            dd ("Prophylactic caught an exception:\n" +
                dumpObjectTree(ex));
        }
        
        if (!rv)
            throw "goodger";

        return rv;
    };
    
    return adapter;
}

function argumentsAsArray (args, start)
{
    if (typeof start == "undefined")
        start = 0;

    if (start >= args.length)
        return null;
    
    var rv = new Array();
    
    for (var i = start; i < args.length; ++i)
        rv.push(args[i]);
    
    return rv;
}

function splitLongWord (str, pos)
{
    if (str.length <= pos)
        return [str];

    var ary = new Array();
    var right = str;
    
    while (right.length > pos)
    {
        /* search for a nice place to break the word, fuzzfactor of +/-5, 
         * centered around |pos| */
        var splitPos =
            right.substring(pos - 5, pos + 5).search(/[^A-Za-z0-9]/);

        splitPos = (splitPos != -1) ? pos - 4 + splitPos : pos;
        ary.push(right.substr (0, splitPos));
        right = right.substr (splitPos);
    }

    ary.push (right);

    return ary;
}

function wrapText (str, width)
{
    var rv = "";
    while (str.length > width)
    {
        rv += str.substr(0, width) + "\n";
        str = str.substr(width);
    }
    return rv + str;
}

function wordCap (str)
{
    if (!str)
        return str;
    
    return str[0].toUpperCase() + str.substr(1);
}

/*
 * Clones an existing object (Only the enumerable properties
 * of course.) use as a function..
 * var c = Clone (obj);
 * or a constructor...
 * var c = new Clone (obj);
 */
function Clone (obj)
{
    var robj = new Object();

    for (var p in obj)
        robj[p] = obj[p];

    return robj;
    
}

function getXULWindowFromWindow (win)
{
    var rv;
    //dd ("getXULWindowFromWindow: before: getInterface is " + win.getInterface);
    try
    {
        var requestor = win.QueryInterface(nsIInterfaceRequestor);
        var nav = requestor.getInterface(nsIWebNavigation);
        var dsti = nav.QueryInterface(nsIDocShellTreeItem);
        var owner = dsti.treeOwner;
        requestor = owner.QueryInterface(nsIInterfaceRequestor);
        rv = requestor.getInterface(nsIXULWindow);
    }
    catch (ex)
    {
        rv = null;
        //dd ("not a nsIXULWindow: " + formatException(ex));
        /* ignore no-interface exception */
    }

    //dd ("getXULWindowFromWindow: after: getInterface is " + win.getInterface);
    return rv;
}

function getBaseWindowFromWindow (win)
{
    var rv;
    //dd ("getBaseWindowFromWindow: before: getInterface is " + win.getInterface);
    try
    {
        var requestor = win.QueryInterface(nsIInterfaceRequestor);
        var nav = requestor.getInterface(nsIWebNavigation);
        var dsti = nav.QueryInterface(nsIDocShellTreeItem);
        var owner = dsti.treeOwner;
        requestor = owner.QueryInterface(nsIInterfaceRequestor);
        rv = requestor.getInterface(nsIBaseWindow);
    }
    catch (ex)
    {
        rv = null;
        //dd ("not a nsIXULWindow: " + formatException(ex));
        /* ignore no-interface exception */
    }

    //dd ("getBaseWindowFromWindow: after: getInterface is " + win.getInterface);
    return rv;
}

function getSpecialDirectory(name)
{
    if (!("directoryService" in utils))
    {
        const DS_CTR = "@mozilla.org/file/directory_service;1";
        const nsIProperties = Components.interfaces.nsIProperties;
    
        utils.directoryService =
            Components.classes[DS_CTR].getService(nsIProperties);
    }
    
    return utils.directoryService.get(name, Components.interfaces.nsIFile);
}

function getPathFromURL (url)
{
    var ary = url.match(/^(.*\/)([^\/?#]+)(\?|#|$)/);
    if (ary)
        return ary[1];

    return url;
}

function getFileFromPath (path)
{
    var ary = path.match(/\/([^\/?#;]+)(\?|#|$|;)/);
    if (ary)
        return ary[1];

    return path;
}

function getURLSpecFromFile (file)
{
    if (!file)
        return null;

    const IOS_CTRID = "@mozilla.org/network/io-service;1";
    const LOCALFILE_CTRID = "@mozilla.org/file/local;1";

    const nsIIOService = Components.interfaces.nsIIOService;
    const nsILocalFile = Components.interfaces.nsILocalFile;
    
    if (typeof file == "string")
    {
        var fileObj =
            Components.classes[LOCALFILE_CTRID].createInstance(nsILocalFile);
        fileObj.initWithPath(file);
        file = fileObj;
    }
    
    var service = Components.classes[IOS_CTRID].getService(nsIIOService);
    /* In sept 2002, bug 166792 moved this method to the nsIFileProtocolHandler
     * interface, but we need to support older versions too. */
    if ("getURLSpecFromFile" in service)
        return service.getURLSpecFromFile(file);

    var nsIFileProtocolHandler = Components.interfaces.nsIFileProtocolHandler;
    var fileHandler = service.getProtocolHandler("file");
    fileHandler = fileHandler.QueryInterface(nsIFileProtocolHandler);
    return fileHandler.getURLSpecFromFile(file);
}

function getCommonPfx (list)
{
    var pfx = list[0];
    var l = list.length;
    
    for (var i = 1; i < l; i++)
    {
        for (var c = 0; c < pfx.length; c++)
            if (pfx[c] != list[i][c])
                pfx = pfx.substr (0, c);
    }

    return pfx;

}

function renameProperty (obj, oldname, newname)
{

    if (oldname == newname)
        return;
    
    obj[newname] = obj[oldname];
    delete obj[oldname];
    
}

function newObject(contractID, iface)
{
    if (!jsenv.HAS_XPCOM)
        return null;

    var obj = Components.classes[contractID].createInstance();
    var rv;

    switch (typeof iface)
    {
        case "string":
            rv = obj.QueryInterface(Components.interfaces[iface]);
            break;

        case "object":
            rv = obj.QueryInterface[iface];
            break;

        default:
            rv = null;
            break;
    }

    return rv;
    
}

function keys (o)
{
    var rv = new Array();
    
    for (var p in o)
        rv.push(p);

    return rv;
    
}

function parseSections (str, sections)
{
    var rv = new Object();
    var currentSection;

    for (var s in sections)
    {
        if (!currentSection)
            currentSection = s;
        
        if (sections[s])
        {
            var i = str.search(sections[s]);
            if (i != -1)
            {
                rv[currentSection] = str.substr(0, i);
                currentSection = 0;
                str = RegExp.rightContext;
                str = str.replace(/^(\n|\r|\r\n)/, "");
            }
        }
        else
        {
            rv[currentSection] = str;
            str = "";
            break;
        }
    }

    return rv;
}

function replaceStrings (str, obj)
{
    if (!str)
        return str;
    for (var p in obj)
        str = str.replace(RegExp(p, "g"), obj[p]);
    return str;
}

function stringTrim (s)
{
    if (!s)
        return "";
    s = s.replace (/^\s+/, "");
    return s.replace (/\s+$/, "");
}

function formatDateOffset (seconds, format)
{
    seconds = Math.floor(seconds);
    var minutes = Math.floor(seconds / 60);
    seconds = seconds % 60;
    var hours   = Math.floor(minutes / 60);
    minutes = minutes % 60;
    var days    = Math.floor(hours / 24);
    hours = hours % 24;

    if (!format)
    {
        var ary = new Array();
        if (days > 0)
            ary.push (days + " days");
        if (hours > 0)
            ary.push (hours + " hours");
        if (minutes > 0)
            ary.push (minutes + " minutes");
        if (seconds > 0)
            ary.push (seconds + " seconds");

        format = ary.join(", ");
    }
    else
    {
        format = format.replace ("%d", days);
        format = format.replace ("%h", hours);
        format = format.replace ("%m", minutes);
        format = format.replace ("%s", seconds);
    }
    
    return format;
}

function arrayHasElementAt(ary, i)
{
    return typeof ary[i] != "undefined";
}

function arraySpeak (ary, single, plural)
{
    var rv = "";
    
    switch (ary.length)
    {
        case 0:
            break;
            
        case 1:
            rv = ary[0];
            if (single)
                rv += " " + single;            
            break;

        case 2:
            rv = ary[0] + " and " + ary[1];
            if (plural)
                rv += " " + plural;
            break;

        default:
            for (var i = 0; i < ary.length - 1; ++i)
                rv += ary[i] + ", ";
            rv += "and " + ary[ary.length - 1];
            if (plural)
                rv += " " + plural;
            break;
    }

    return rv;
    
}

function arrayOrFlag (ary, i, flag)
{
    if (i in ary)
        ary[i] |= flag;
    else
        ary[i] = flag;
}

function arrayAndFlag (ary, i, flag)
{
    if (i in ary)
        ary[i] &= flag;
    else
        ary[i] = 0;
}

function arrayContains (ary, elem)
{
    return (arrayIndexOf (ary, elem) != -1);
}

function arrayIndexOf (ary, elem, start)
{
    if (!ary)
        return -1;

    var len = ary.length;

    if (typeof start == "undefined")
        start = 0;

    for (var i = start; i < len; ++i)
    {
        if (ary[i] == elem)
            return i;
    }

    return -1;
}
    
function arrayInsertAt (ary, i, o)
{

    ary.splice (i, 0, o);

}

function arrayRemoveAt (ary, i)
{

    ary.splice (i, 1);

}

function getRandomElement (ary)
{
    var i = Math.floor (Math.random() * ary.length)
	if (i == ary.length) i = 0;

    return ary[i];

}

function zeroPad (num, decimals)
{
    var rv = String(num);
    var len = rv.length;
    for (var i = 0; i < decimals - len; ++i)
        rv = "0" + rv;
    
    return rv;
}

function leftPadString (str, num, ch)
{
    var rv = "";
    var len = rv.length;
    for (var i = len; i < num; ++i)
        rv += ch;
    
    return rv + str;
}
    
function roundTo (num, prec)
{
    return Math.round(num * Math.pow (10, prec)) / Math.pow (10, prec);
}

function randomRange (min, max)
{

    if (typeof min == "undefined")
        min = 0;

    if (typeof max == "undefined")
        max = 1;

    var rv = (Math.floor(Math.round((Math.random() * (max - min)) + min )));
    
    return rv;

}

function getStackTrace ()
{

    if (!jsenv.HAS_XPCOM)
        return "No stack trace available.";

    var frame = Components.stack.caller;
    var str = "<top>";

    while (frame)
    {
        var name = frame.name ? frame.name : "[anonymous]";
        str += "\n" + name + "@" + frame.lineNumber;
        frame = frame.caller;
    }

    return str;
    
}

function getInterfaces (cls)
{
    if (!jsenv.HAS_XPCOM)
        return null;

    var rv = new Object();
    var e;

    for (var i in Components.interfaces)
    {
        try
        {
            var ifc = Components.interfaces[i];
            cls.QueryInterface(ifc);
            rv[i] = ifc;
        }
        catch (e)
        {
            /* nada */
        }
    }

    return rv;
    
}

function makeExpression (items)
{
    function escapeItem (item, first)
    {
        // Numbers.
        if (item.match(/^[0-9]+$/i))
            return "[" + item + "]";
        // Words/other items that don't need quoting.
        if (item.match(/^[a-z_][a-z0-9_]+$/i))
            return (!first ? "." : "") + item;
        // Quote everything else.
        return "[" + item.quote() + "]";
    };
    
    var expression = escapeItem(items[0], true);
    
    for (var i = 1; i < items.length; i++)
        expression += escapeItem(items[i], false);
    
    return expression;
}
