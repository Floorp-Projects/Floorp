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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, rginda@nestcape.com, original author
 */

/*
 * This file is (at the time of this writing) an exact copy of the chatzilla
 * utility functions.  It should actually be globally accessible (not just
 * part of chatzilla.)  This hasn't happened yet, so it's copied here to
 * avoid a dependancy on chatzilla
 */

if (typeof document == "undefined") /* in xpcshell */
    dumpln = print;
else
    if (typeof dump == "function")
        dumpln = function (str) {dump (str + "\n");}
    else if (jsenv.HAS_RHINO)
        dumpln = function (str) {var out = java.lang.System.out;
                                 out.println(str); out.flush(); }
    else
        dumpln = function () {} /* no suitable function */

if (DEBUG)
    dd = dumpln;
else
    dd = function (){};

var jsenv = new Object();
jsenv.HAS_SECURITYMANAGER = ((typeof netscape == "object") &&
                             (typeof netscape.security == "object"));
jsenv.HAS_XPCOM = ((typeof Components == "function") &&
                   (typeof Components.classes == "function"));
jsenv.HAS_JAVA = (typeof java == "object");
jsenv.HAS_RHINO = (typeof defineClass == "function");
jsenv.HAS_DOCUMENT = (typeof document == "object");

function dumpObject (o, pfx, sep)
{
    var p;
    var s = "";

    sep = (typeof sep == "undefined") ? " = " : sep;
    pfx = (typeof pfx == "undefined") ? "" : pfx;    

    for (p in o)
    {
        if (typeof (o[p]) != "function")
            s += pfx + p + sep + o[p] + "\n";
        else
            s += pfx + p + sep + "function\n";
    }

    return s;

}

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
                var sfunc = o[i].toString().split("\n");
                if (sfunc[2] == "    [native code]")
                    var sfunc = "[native code]";
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

/*
 * matches a real object against one or more pattern objects.
 * if you pass an array of pattern objects, |negate| controls wether to check
 * if the object matches ANY of the patterns, or NONE of the patterns.
 */
function matchObject (o, pattern, negate)
{
    negate = Boolean(negate);
    
    function _match (o, pattern)
    {
        if (pattern instanceof Function)
            return pattern(o);
        
        for (p in pattern)
        {
            var val;
                /* nice to have, but slow as molases, allows you to match
                 * properties of objects with obj$prop: "foo" syntax      */
                /*
                  if (p[0] == "$")
                  val = eval ("o." + 
                  p.substr(1,p.length).replace (/\$/g, "."));
                  else
                */
            val = o[p];
            
            if (pattern[p] instanceof Function)
            {
                if (!pattern[p](val))
                    return false;
            }
            else
            {
                var ary = (new String(val)).match(pattern[p]);
                if (ary == null)
                    return false;
                else
                    o.matchresult = ary;
            }
        }

        return true;

    }

    if (!(pattern instanceof Array))
        return Boolean (negate ^ _match(o, pattern));
            
    for (var i in pattern)
        if (_match (o, pattern[i]))
            return !negate;

    return negate;
    
}

function matchEntry (partialName, list)
{
    
    if ((typeof partialName == "undefined") ||
        (String(partialName) == ""))
        return list;

    var ary = new Array();

    for (var i in list)
    {
        if (list[i].indexOf(partialName) == 0)
            ary.push (list[i]);
    }

    return ary;
    
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

function getPriv (priv)
{
    if (!jsenv.HAS_SECURITYMANAGER)
        return true;

    var rv = true;

    try
    {
        netscape.security.PrivilegeManager.enablePrivilege(priv);
    }
    catch (e)
    {
        dd ("getPriv: unable to get privlege '" + priv + "': " + e);
        rv = false;
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


function arrayInsertAt (ary, i, o)
{

    ary.splice (i, 0, o);

    /* doh, forgot about that 'splice' thing
    if (ary.length < i)
    {
        this[i] = o;
        return;
    }

    for (var j = ary.length; j > i; j--)
        ary[j] = ary[j - 1];

    ary[i] = o;
    */
}

function arrayRemoveAt (ary, i)
{

    ary.splice (i, 1);

    /* doh, forgot about that 'splice' thing
    if (ary.length < i)
        return false;

    for (var j = i; j < ary.length; j++)
        ary[j] = ary[j + 1];

    ary.length--;
    */

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

function getRandomElement (ary)
{
    var i = parseInt (Math.random() * ary.length)
	if (i == ary.length) i = 0;

    return ary[i];

}

function roundTo (num, prec)
{

    return parseInt (( Math.round(num) * Math.pow (10, prec))) /
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
        var name = frame.functionName ? frame.functionName : "[anonymous]";
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

/**
 * Calls a named function for each element in an array, sending
 * the same parameter each call.
 *
 * @param ary           an array of objects
 * @param func_name     string name of function to call.
 * @param data          data object to pass to each object.
 */      
function mapObjFunc(ary, func_name, data)
{
    /* 
     * WARNING: Caller assumes resonsibility to verify ary
     * and func_name
     */

    for (var i in ary)
        ary[i][func_name](data);
}

/**
 * Passes each element of an array to a given function object.
 *
 * @param func  a function object.
 * @param ary   an array of values.
 */
function map(func, ary) {

    /*
     * WARNING: Caller assumnes responsibility to verify
     * func and ary.
     */

    for (var i in ary)
        func(ary[i]);

}

