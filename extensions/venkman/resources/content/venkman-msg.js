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

function initMsgs ()
{
    console.bundleList = new Array();
    console.defaultBundle = 
        initStringBundle("chrome://venkman/locale/venkman.properties");
}

function initStringBundle (bundlePath)
{
    const nsIPropertyElement = Components.interfaces.nsIPropertyElement;

    var pfx;
    if (console.bundleList.length == 0)
        pfx = "";
    else
        pfx = console.bundleList.length + ":";

    var bundle = srGetStrBundle(bundlePath);
    console.bundleList.push(bundle);
    var enumer = bundle.getSimpleEnumeration();

    while (enumer.hasMoreElements())
    {
        var prop = enumer.getNext().QueryInterface(nsIPropertyElement);
        var ary = prop.key.match (/^(msg|msn)/);
        if (ary)
        {
            var constValue;
            var constName = prop.key.toUpperCase().replace (/\./g, "_");
            if (ary[1] == "msn")
                constValue = pfx + prop.key;
            else
                constValue = prop.value.replace (/^\"/, "").replace (/\"$/, "");

            window[constName] = constValue;
        }
    }

    return bundle;
}

function getMsg (msgName, params, deflt)
{
    try
    {    
        var bundle;
        var ary = msgName.match (/(\d+):(.+)/);
        if (ary)
        {
            return (getMsgFrom(console.bundleList[ary[1]], ary[2], params,
                               deflt));
        }
        
        return (getMsgFrom(console.bundleList[0], msgName, params, deflt));
    }
    catch (ex)
    {
        ASSERT (0, "Caught exception getting message: " + msgName + "/" +
                params);
        return deflt ? deflt : msgName;
    }
}

function getMsgFrom (bundle, msgName, params, deflt)
{
    try 
    {
        var rv;
        
        if (params && params instanceof Array)
            rv = bundle.formatStringFromName (msgName, params, params.length);
        else if (params || params == 0)
            rv = bundle.formatStringFromName (msgName, [params], 1);
        else
            rv = bundle.GetStringFromName (msgName);
        
        /* strip leading and trailing quote characters, see comment at the
         * top of venkman.properties.
         */
        rv = rv.replace (/^\"/, "");
        rv = rv.replace (/\"$/, "");

        return rv;
    }
    catch (ex)
    {
        if (typeof deflt == "undefined")
        {
            ASSERT (0, "caught exception getting value for ``" + msgName +
                    "''\n" + ex + "\n");
            return msgName;
        }
        return deflt;
    }

    return null;    
}

/* message types, don't localize */
const MT_ATTENTION = "ATTENTION";
const MT_CONT      = "CONT";
const MT_ERROR     = "ERROR";
const MT_HELLO     = "HELLO";
const MT_HELP      = "HELP";
const MT_WARN      = "WARN";
const MT_INFO      = "INFO";
const MT_OUTPUT    = "#OUTPUT";
const MT_SOURCE    = "#SOURCE";
const MT_STEP      = "#STEP";
const MT_STOP      = "STOP";
const MT_ETRACE    = "#ETRACE";
const MT_LOG       = "#LOG";
const MT_USAGE     = "USAGE";
const MT_EVAL_IN   = "#EVAL-IN";
const MT_EVAL_OUT  = "#EVAL-OUT";
const MT_FEVAL_IN  = "#FEVAL-IN";
const MT_FEVAL_OUT = "#FEVAL-OUT";

/* these messages might be needed to report an exception at startup, before
 * initMsgs() has been called. */
window.MSN_ERR_STARTUP        = "msg.err.startup";
window.MSN_FMT_JSEXCEPTION    = "msn.fmt.jsexception";

/* exception number -> localized message name map, keep in sync with ERR_* from
 * venkman-static.js */
const exceptionMsgNames = ["err.notimplemented", 
                           "err.required.param",
                           "err.invalid.param",
                           "err.subscript.load",
                           "err.no.debugger",
                           "err.failure",
                           "err.no.stack"];
