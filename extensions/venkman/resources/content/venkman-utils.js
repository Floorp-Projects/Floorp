/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimensions Consulting, Inc. All Rights Reserved.
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 *
 *
 * JavaScript utility functions.
 */

var dumpln;
var dd;

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
    dd = function _dd (str) {
             if (typeof str != "string") {
                 dumpln (str);
             } else if (str[str.length - 1] == "{") {
                 _dd_timeStack.push (new Date());
                 if (_dd_lastDumpWasOpen)
                     dump("\n");
                 dump (_dd_pfx + _dd_currentIndent + str);
                 _dd_currentIndent += _dd_singleIndent;
                 _dd_lastDumpWasOpen = true;
             } else if (str[0] == "}") {
                 var sufx = (new Date() - _dd_timeStack.pop()) / 1000 + " sec";
                 _dd_currentIndent = 
                     _dd_currentIndent.substr (0, _dd_currentIndent.length -
                                               _dd_indentLength);
                 if (_dd_lastDumpWasOpen)
                     dumpln ("} " + sufx);
                 else
                     dumpln (_dd_pfx + _dd_currentIndent + str + " " + sufx);
                 _dd_lastDumpWasOpen = false;
             } else {
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
        
        var t = typeof o[i];
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
                s += pfx + tee + i + " (object)\n";
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

        if (!compress)
            s += pfx + "|\n";

    }

    s += pfx + "*\n";
    
    return s;
    
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

/*
 * Clones an existing object (Only the enumerable properties
 * of course.) use as a function..
 * var c = Clone (obj);
 * or a constructor...
 * var c = new Clone (obj);
 */
function Clone (obj)
{
    robj = new Object();

    for (var p in obj)
        robj[p] = obj[p];

    return robj;
    
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

function stringTrim (s)
{
    if (!s)
        return "";
    s = s.replace (/^\s+/, "");
    return s.replace (/\s+$/, "");
    
}

function formatDateOffset (seconds, format)
{
    seconds = parseInt(seconds);
    var minutes = parseInt(seconds / 60);
    seconds = seconds % 60;
    var hours   = parseInt(minutes / 60);
    minutes = minutes % 60;
    var days    = parseInt(hours / 24);
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


function arrayContains (ary, elem)
{
    return (arrayIndexOf (ary, elem) != -1);
}

function arrayIndexOf (ary, elem)
{
    for (var i in ary)
        if (ary[i] == elem)
            return i;

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
    var i = parseInt (Math.random() * ary.length)
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

    return parseInt ( Math.round(num * Math.pow (10, prec))) /
        Math.pow (10, prec);   

}

function randomRange (min, max)
{

    if (typeof min == "undefined")
        min = 0;

    if (typeof max == "undefined")
        max = 1;

    var rv = (parseInt(Math.round((Math.random() * (max - min)) + min )));
    
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
