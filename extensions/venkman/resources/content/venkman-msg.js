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
    
    try 
    {
        if (restCount == 1 && arguments[1] instanceof Array)
        {
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
    catch (ex)
    {
        ASSERT (0, "caught exception getting value for ``" + msgName +
                "''\n" + ex + "\n");
        return msgName;
    }
}

/* message types, don't localize */
const MT_CONT  = "CONT";
const MT_ERROR = "ERROR";
const MT_HELLO = "HELLO";
const MT_HELP  = "HELP";
const MT_INFO  = "INFO";
const MT_STOP  = "STOP";
const MT_USAGE = "USAGE";

/* exception number -> localized message name map, keep in sync with ERR_* from
 * venkman-static.js */
const exceptionMsgNames = ["err.notimplemented", 
                           "err.required.param",
                           "err.invalid.param",
                           "err.subscript.load"];

/* message values for non-parameterized messages */
const MSG_ERR_NO_STACK    = getMsg("msg.err.nostack");

const MSG_TYPE_NATIVE     = getMsg("msg.type.native");
const MSG_TYPE_PRIMITIVE  = getMsg("msg.type.primitive");
const MSG_TYPE_CLASS      = getMsg("msg.type.class");
const MSG_TYPE_LINE       = getMsg("msg.type.line");
const MSG_TYPE_PROPERTIES = getMsg("msg.type.properties");

const MSG_TYPE_DOUBLE     = getMsg("msg.type.double");
const MSG_TYPE_FUNCTION   = getMsg("msg.type.function");
const MSG_TYPE_INT        = getMsg("msg.type.int");
const MSG_TYPE_NULL       = getMsg("msg.type.null");
const MSG_TYPE_OBJECT     = getMsg("msg.type.object");
const MSG_TYPE_STRING     = getMsg("msg.type.string");
const MSG_TYPE_UNKNOWN    = getMsg("msg.type.unknown");
const MSG_TYPE_VOID       = getMsg("msg.type.void");

const MSG_VAL_BREAKPOINT  = getMsg("msg.val.breakpoint");
const MSG_VAL_DEBUG       = getMsg("msg.val.debug");
const MSG_VAL_DEBUGGER    = getMsg("msg.val.debugger");
const MSG_VAL_THROW       = getMsg("msg.val.throw");

const MSG_VAL_CONSOLE     = getMsg("msg.val.console");
const MSG_VAL_UNKNOWN     = getMsg("msg.val.unknown");
const MSG_VAL_SCOPE       = getMsg("msg.val.scope");

const MSG_HELLO1          = getMsg("msg.hello.1");
const MSG_HELLO2          = getMsg("msg.hello.2");

const MSG_TIP_HELP        = getMsg("msg.tip.help");

const CMD_CMDS            = getMsg("cmd.commands");
const CMD_CMDS_PARAMS     = getMsg("cmd.commands.params");
const CMD_CMDS_HELP       = getMsg("cmd.commands.help");
const CMD_CONT            = getMsg("cmd.cont");
const CMD_CONT_PARAMS     = getMsg("cmd.cont.params");
const CMD_CONT_HELP       = getMsg("cmd.cont.help");
const CMD_EVAL            = getMsg("cmd.eval");
const CMD_EVAL_PARAMS     = getMsg("cmd.eval.params");
const CMD_EVAL_HELP       = getMsg("cmd.eval.help");
const CMD_FRAME           = getMsg("cmd.frame");
const CMD_FRAME_PARAMS    = getMsg("cmd.frame.params");
const CMD_FRAME_HELP      = getMsg("cmd.frame.help");
const CMD_HELP            = getMsg("cmd.help");
const CMD_HELP_PARAMS     = getMsg("cmd.help.params");
const CMD_HELP_HELP       = getMsg("cmd.help.help");
const CMD_SCOPE           = getMsg("cmd.scope");
const CMD_SCOPE_PARAMS    = getMsg("cmd.scope.params");
const CMD_SCOPE_HELP      = getMsg("cmd.scope.help");
const CMD_WHERE           = getMsg("cmd.where");
const CMD_WHERE_PARAMS    = getMsg("cmd.where.params");
const CMD_WHERE_HELP      = getMsg("cmd.where.help");
const CMD_QUIT            = getMsg("cmd.quit");
const CMD_QUIT_PARAMS     = getMsg("cmd.quit.params");
const CMD_QUIT_HELP       = getMsg("cmd.quit.help");

/* message names for parameterized messages */
const MSN_ERR_NO_COMMAND     = "msg.err.nocommand";
const MSN_ERR_NOTIMPLEMENTED = "msg.err.notimplemented";
const MSN_ERR_AMBIGCOMMAND   = "msg.err.ambigcommand";

const MSN_NO_PROPERTIES      = "msg.noproperties";
const MSN_NO_CMDMATCH        = "msg.no-commandmatch";
const MSN_CMDMATCH           = "msg.commandmatch";
const MSN_CMDMATCH_ALL       = "msg.commandmatch.all";

const MSN_CONT             = "msg.cont";
const MSN_EVAL_ERROR       = "msg.eval.error";
const MSN_EVAL_THREW       = "msg.eval.threw";
const MSN_STOP             = "msg.stop";
const MSN_SUBSCRIPT_LOADED = "msg.subscript.load";
