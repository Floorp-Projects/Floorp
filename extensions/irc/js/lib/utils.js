/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is JSIRC Library
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. Copyright (C) 1999
 * New Dimenstions Consulting, Inc. All Rights Reserved.
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 *
 *
 * JavaScript utility functions.
 *
 * 1999-08-15 rginda@ndcico.com           v1.0
 */

var DEBUG = true;

if (typeof document == "undefined") /* in xpcshell */
    dumpln = print;
else
    dumpln = function (str) {dump (str + "\n");}

if (DEBUG)
    dd = dumpln;
else
    dd = function (){};

var jsenv = new Object();

jsenv.HAS_XPCOM = ((typeof Components == "function") && 
                   (typeof Components.classes == "function"));
jsenv.HAS_JAVA = (typeof java == "object");
jsenv.HAS_RHINO = (typeof defineClass == "function");

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
 * matches a real object against one or more pattern objects.
 * if you pass an array of pattern objects, |negate| controls wether to check
 * if the object matches ANY of the patterns, or NONE of the patterns.
 */
function matchObject (o, pattern, negate)
{
    negate = Boolean(negate);
    
    function _match (o, pattern)
    {

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

            if (typeof pattern[p] == "function")
            {
                if (!pattern[p](val))
                    return false;
            }
            else
            {
                if ((new String(val)).search(pattern[p]) == -1)
                    return false;
            }
        }

        return true;

    }

    if (!(pattern instanceof Array))
        return Boolean (negate ^ _match(o, pattern));
            
    for (var i in pattern)
    {
        if (_match (o, pattern[i]))
            if (!negate)
            {
                return true;
            }
            else
                if (negate)
                    return false;
    }

    return negate;
    
}

function renameProperty (obj, oldname, newname)
{

    obj[newname] = obj[oldname];
    delete obj[oldname];
    
}

function newObject(progID, iface)
{
    var obj = Components.classes[progID].createInstance();
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
    var rv = "";
    
    for (var p in o)
        rv += p + ", ";

    return rv;
    
}

function stringTrim (s)
{
    if (!s)
        return "";
    s = s.replace (/^\s+/);
    return s.replace (/\s+$/);
    
}


function arrayInsertAt (ary, i, o)
{

    if (ary.length < i)
    {
        this[i] = o;
        return;
    }

    for (var j = ary.length; j > i; j--)
        ary[j] = ary[j - 1];

    ary[i] = o;
    
}

function arrayRemoveAt (ary, i)
{

    if (ary.length < i)
        return false;

    for (var j = i; j < ary.length; j++)
        ary[j] = ary[j + 1];

    ary.length--;

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

function getInterfaces (cls)
{
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
