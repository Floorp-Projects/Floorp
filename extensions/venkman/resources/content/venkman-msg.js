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

console._bundle = srGetStrBundle("chrome://venkman/locale/venkman.properties");

function getMsg (msgName)
{
    var restCount = arguments.length - 1;
    
    if (restCount == 1 && arguments[1] instanceof Array)
    {
        dd ("formatting from array of values, " + arguments[1]);
        
        return console._bundle.formatStringFromName (msgName, arguments[1], 
                                                     arguments[1].length);
    }
    else if (restCount > 0)
    {
        var subPhrases = new Array();
        for (var i = 1; i < arguments.length; ++i)
            subPhrases.push(arguments[i]);
        return console._bundle.formatStringFromName (msgName, subPhrases, 
                                                     subPhrases.length);
    }

    return console._bundle.GetStringFromName (msgName);
}

/* message types, don't localize */
const MT_STOP  = "STOP";
const MT_CONT  = "CONT";
const MT_ERROR = "ERROR";
const MT_INFO  = "INFO";

/* exception number -> localized message name map, keep in sync with ERR_* from
 * venkman-static.js */
const exceptionMsgNames = ["err.notimplemented", 
                           "err.display.arg1",
                           "err.subscript.load"];

/* message values for non-parameterized messages */
const MSG_STOP_DEBUGGER = getMsg("msg.stop.debugger");
const MSG_CONT_DEBUGGER = getMsg("msg.cont.debugger");

const MSG_VAL_UNKNOWN   = getMsg("msg.val.unknown");
const MSG_VAL_CONSOLE   = getMsg("msg.val.console");

/* message names for parameterized messages */
const MSN_SUBSCRIPT_LOADED = "msg.subscript.load";
const MSN_EVAL_ERROR       = "msg.eval.error";
const MSN_EVAL_THREW       = "msg.eval.threw";

