/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

const NS_XHTML   = "http://www.w3.org/1999/xhtml";

const HTML_BR    = "html:br";
const HTML_IMG   = "html:img";
const HTML_SPAN  = "html:span";
const HTML_TABLE = "html:table";
const HTML_TBODY = "html:tbody";
const HTML_TD    = "html:td";
const HTML_TH    = "html:th";
const HTML_TR    = "html:tr";

function HTML (tagName, attribs, args)
{
    var elem = document.createElementNS (NS_XHTML, tagName);    

    if (typeof attribs == "string")
        elem.setAttribute ("class", attribs);
    else if (attribs && typeof attribs == "object")
        for (var p in attribs)
            elem.setAttribute (p, attribs[p]);
    
    var start = 0;
    
    if (args)
    {
        if (!(args instanceof Array))
            args = [args];
        else if (arguments.length > 3)
        {
            start = 2; args = arguments;
        }

        for (var i = start; i < args.length; ++i)
            if (typeof args[i] == "string")
                elem.appendChild (document.createTextNode(args[i]));
            else if (args[i])
                elem.appendChild (args[i]);
        
    }
    
    return elem;
}

function htmlA(attribs, href, contents)
{
    if (typeof contents == "undefined")
        contents = href;
    
    var a = HTML("html:a", attribs, contents);
    a.setAttribute ("href", href);

    return a;
}
    
function htmlBR(attribs)
{
    return HTML("html:br", attribs, argumentsAsArray(arguments, 1));
}

function htmlWBR(attribs)
{
    return HTML("html:wbr", attribs, argumentsAsArray(arguments, 1));
}
    
function htmlImg(attribs, src)
{
    var img = HTML("html:img", attribs, argumentsAsArray(arguments, 2));
    if (src)
        img.setAttribute ("src", src);
    return img;
}

function htmlSpan(attribs)
{
    return HTML("html:span", attribs, argumentsAsArray(arguments, 1));
}

function htmlTable(attribs)
{
    return HTML("html:table", attribs, argumentsAsArray(arguments, 1));
}

function htmlTBody(attribs)
{
    return HTML("html:tbody", attribs, argumentsAsArray(arguments, 1));
}

function htmlText(text)
{
    return document.createTextNode(text);
}

function htmlTD(attribs)
{
    return HTML("html:td", attribs, argumentsAsArray(arguments, 1));
}

function htmlTR(attribs)
{
    return HTML("html:tr", attribs, argumentsAsArray(arguments, 1));
}

function htmlTH(attribs)
{
    return HTML("html:th", attribs, argumentsAsArray(arguments, 1));
}

