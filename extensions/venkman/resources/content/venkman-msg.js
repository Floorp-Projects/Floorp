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
const MT_CONT      = "CONT";
const MT_ERROR     = "ERROR";
const MT_HELLO     = "HELLO";
const MT_HELP      = "HELP";
const MT_INFO      = "INFO";
const MT_SOURCE    = "SOURCE";
const MT_STEP      = "STEP";
const MT_STOP      = "STOP";
const MT_ETRACE    = "ETRACE";
const MT_USAGE     = "USAGE";
const MT_EVAL_IN   = "EVAL-IN";
const MT_EVAL_OUT  = "EVAL-OUT";
const MT_FEVAL_IN  = "FEVAL-IN";
const MT_FEVAL_OUT = "FEVAL-OUT";

/* exception number -> localized message name map, keep in sync with ERR_* from
 * venkman-static.js */
const exceptionMsgNames = ["err.notimplemented", 
                           "err.required.param",
                           "err.invalid.param",
                           "err.subscript.load",
                           "err.no.debugger",
                           "err.failure",
                           "err.no.stack"];

/* message values for non-parameterized messages */
const MSG_ERR_NO_STACK    = getMsg("msg.err.nostack");
const MSG_ERR_NO_SOURCE   = getMsg("msg.err.nosource");
const MSG_ERR_CANT_CLOSE  = getMsg("msg.err.cant.close");

const MSG_TYPE_BOOLEAN    = getMsg("msg.type.boolean");
const MSG_TYPE_DOUBLE     = getMsg("msg.type.double");
const MSG_TYPE_FUNCTION   = getMsg("msg.type.function");
const MSG_TYPE_INT        = getMsg("msg.type.int");
const MSG_TYPE_NULL       = getMsg("msg.type.null");
const MSG_TYPE_OBJECT     = getMsg("msg.type.object");
const MSG_TYPE_STRING     = getMsg("msg.type.string");
const MSG_TYPE_UNKNOWN    = getMsg("msg.type.unknown");
const MSG_TYPE_VOID       = getMsg("msg.type.void");

const MSG_CLASS_XPCOBJ    = getMsg("msg.class.xpcobj");
const MSG_BLACKLIST       = getMsg("msg.blacklist");
const MSG_BREAK_REC       = getMsg("msg.break.rec");
const MSG_CALL_STACK      = getMsg("msg.callstack");

const MSG_WORD_NATIVE      = getMsg("msg.val.native");
const MSG_WORD_SCRIPT      = getMsg("msg.val.script");
const MSG_WORD_THIS        = getMsg("msg.val.this");
const MSG_WORD_BREAKPOINT  = getMsg("msg.val.breakpoint");
const MSG_WORD_DEBUG       = getMsg("msg.val.debug");
const MSG_WORD_DEBUGGER    = getMsg("msg.val.debugger");
const MSG_WORD_THROW       = getMsg("msg.val.throw");
const MSG_WORD_SCOPE       = getMsg("msg.val.scope");

const MSG_COMMASP          = getMsg("msg.val.commasp", " ");
const MSG_VAL_CONSOLE      = getMsg("msg.val.console");
const MSG_VAL_UNKNOWN      = getMsg("msg.val.unknown");
const MSG_VAL_NA           = getMsg("msg.val.na");
const MSG_VAL_OBJECT       = getMsg("msg.val.object");
const MSG_VAL_EXPR         = getMsg("msg.val.expression");
const MSG_VAL_PROTO        = getMsg("msg.val.proto");
const MSG_VAL_PARENT       = getMsg("msg.val.parent");
const MSG_VAL_EXCEPTION    = getMsg("msg.val.exception");
const MSG_VAL_ON           = getMsg("msg.val.on");
const MSG_VAL_OFF          = getMsg("msg.val.off");
const MSG_VAL_TLSCRIPT     = getMsg("msg.val.tlscript");

const MSG_VF_ENUMERABLE    = getMsg("vf.enumerable");
const MSG_VF_READONLY      = getMsg("vf.readonly");
const MSG_VF_PERMANENT     = getMsg("vf.permanent");
const MSG_VF_ALIAS         = getMsg("vf.alias");
const MSG_VF_ARGUMENT      = getMsg("vf.argument");
const MSG_VF_VARIABLE      = getMsg("vf.variable");
const MSG_VF_HINTED        = getMsg("vf.hinted");

const MSG_HELLO            = getMsg("msg.hello");

const MSG_STATUS_DEFAULT   = getMsg("msg.status.default");

const MSG_TIP_HELP           = getMsg("msg.tip.help");
const MSG_NO_BREAKPOINTS_SET = getMsg("msg.no.breakpoints.set");
const MSG_NO_FBREAKS_SET     = getMsg("msg.no.fbreaks.set");

const MSG_TMODE_IGNORE     = getMsg("msg.tmode.ignore");
const MSG_TMODE_TRACE      = getMsg("msg.tmode.trace");
const MSG_TMODE_BREAK      = getMsg("msg.tmode.break");

const CMD_BREAK           = getMsg("cmd.break");
const CMD_BREAK_PARAMS    = getMsg("cmd.break.params");
const CMD_BREAK_HELP      = getMsg("cmd.break.help");
const CMD_CLEAR           = getMsg("cmd.clear");
const CMD_CLEAR_PARAMS    = getMsg("cmd.clear.params");
const CMD_CLEAR_HELP      = getMsg("cmd.clear.help");
const CMD_CMDS            = getMsg("cmd.commands");
const CMD_CMDS_PARAMS     = getMsg("cmd.commands.params");
const CMD_CMDS_HELP       = getMsg("cmd.commands.help");
const CMD_CONT            = getMsg("cmd.cont");
const CMD_CONT_PARAMS     = getMsg("cmd.cont.params");
const CMD_CONT_HELP       = getMsg("cmd.cont.help");
const CMD_EVAL            = getMsg("cmd.eval");
const CMD_EVAL_PARAMS     = getMsg("cmd.eval.params");
const CMD_EVAL_HELP       = getMsg("cmd.eval.help");
const CMD_EVALD           = getMsg("cmd.evald");
const CMD_EVALD_PARAMS    = getMsg("cmd.evald.params");
const CMD_EVALD_HELP      = getMsg("cmd.evald.help");
const CMD_FBREAK          = getMsg("cmd.fbreak");
const CMD_FBREAK_PARAMS   = getMsg("cmd.fbreak.params");
const CMD_FBREAK_HELP     = getMsg("cmd.fbreak.help");
const CMD_FINISH          = getMsg("cmd.finish");
const CMD_FINISH_PARAMS   = getMsg("cmd.finish.params");
const CMD_FINISH_HELP     = getMsg("cmd.finish.help");
const CMD_FRAME           = getMsg("cmd.frame");
const CMD_FRAME_PARAMS    = getMsg("cmd.frame.params");
const CMD_FRAME_HELP      = getMsg("cmd.frame.help");
const CMD_HELP            = getMsg("cmd.help");
const CMD_HELP_PARAMS     = getMsg("cmd.help.params");
const CMD_HELP_HELP       = getMsg("cmd.help.help");
const CMD_NEXT            = getMsg("cmd.next");
const CMD_NEXT_PARAMS     = getMsg("cmd.next.params");
const CMD_NEXT_HELP       = getMsg("cmd.next.help");
const CMD_PROPS           = getMsg("cmd.props");
const CMD_PROPS_PARAMS    = getMsg("cmd.props.params");
const CMD_PROPS_HELP      = getMsg("cmd.props.help");
const CMD_PROPSD          = getMsg("cmd.propsd");
const CMD_PROPSD_PARAMS   = getMsg("cmd.propsd.params");
const CMD_PROPSD_HELP     = getMsg("cmd.propsd.help");
const CMD_SCOPE           = getMsg("cmd.scope");
const CMD_SCOPE_PARAMS    = getMsg("cmd.scope.params");
const CMD_SCOPE_HELP      = getMsg("cmd.scope.help");
const CMD_STEP            = getMsg("cmd.step");
const CMD_STEP_PARAMS     = getMsg("cmd.step.params");
const CMD_STEP_HELP       = getMsg("cmd.step.help");
const CMD_TMODE           = getMsg("cmd.tmode");
const CMD_TMODE_PARAMS    = getMsg("cmd.tmode.params");
const CMD_TMODE_HELP      = getMsg("cmd.tmode.help");
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
const MSN_ERR_BP_NOSCRIPT    = "msg.err.bp.noscript";
const MSN_ERR_BP_NOLINE      = "msg.err.bp.noline";
const MSN_ERR_BP_NODICE      = "msg.err.bp.nodice";
const MSN_ERR_BP_NOINDEX     = "msg.err.bp.noindex";
const MSN_ERR_REQUIRED_PARAM = "err.required.param"; /* also used as exception */
const MSN_ERR_INVALID_PARAM  = "err.invalid.param";  /* also used as exception */
const MSN_ERR_SOURCE_LOAD_FAILED = "msg.err.source.load.failed";
const MSN_ERR_STARTUP        = "msg.err.startup";

const MSN_FMT_ARGUMENT       = "fmt.argument";
const MSN_FMT_PROPERTY       = "fmt.property";
const MSN_FMT_SCRIPT         = "fmt.script";
const MSN_FMT_FRAME          = "fmt.frame";
const MSN_FMT_VALUE_LONG     = "fmt.value.long";
const MSN_FMT_VALUE_MED      = "fmt.value.med";
const MSN_FMT_VALUE_SHORT    = "fmt.value.short";
const MSN_FMT_OBJECT         = "fmt.object";
const MSN_FMT_JSEXCEPTION    = "fmt.jsexception";
const MSN_FMT_BADMOJO        = "fmt.badmojo";
const MSN_FMT_TMP_ASSIGN     = "fmt.tmp.assign";
const MSN_FMT_LONGSTR        = "fmt.longstr";
const MSN_FMT_USAGE          = "fmt.usage";
const MSN_FMT_GUESSEDNAME    = "fmt.guessedname";

const MSN_NO_PROPERTIES      = "msg.noproperties";
const MSN_NO_CMDMATCH        = "msg.no-commandmatch";
const MSN_CMDMATCH           = "msg.commandmatch";
const MSN_CMDMATCH_ALL       = "msg.commandmatch.all";
const MSN_PROPS_HEADER       = "msg.props.header";
const MSN_PROPSD_HEADER      = "msg.propsd.header";
const MSN_BP_HEADER          = "msg.bp.header";
const MSN_BP_LINE            = "msg.bp.line";
const MSN_BP_CREATED         = "msg.bp.created";
const MSN_BP_DISABLED        = "msg.bp.disabled";
const MSN_BP_CLEARED         = "msg.bp.cleared";
const MSN_BP_EXISTS          = "msg.bp.exists";
const MSN_FBP_HEADER         = "msg.fbp.header";
const MSN_FBP_LINE           = "msg.fbp.line";
const MSN_FBP_CREATED        = "msg.fbp.created";
const MSN_FBP_DISABLED       = "msg.fbp.disabled";
const MSN_FBP_EXISTS         = "msg.fbp.exists";
const MSN_SOURCE_LINE        = "msg.source.line";
const MSN_EXCP_TRACE         = "msg.exception.trace";
const MSN_VERSION            = "msg.version";

const MSN_CONT             = "msg.cont";
const MSN_EVAL_ERROR       = "msg.eval.error";
const MSN_EVAL_THREW       = "msg.eval.threw";
const MSN_STOP             = "msg.stop";
const MSN_SUBSCRIPT_LOADED = "msg.subscript.load";

const MSN_STATUS_LOADING   = "msg.status.loading";
const MSN_STATUS_MARKING   = "msg.status.marking";
const MSN_STATUS_STOPPED   = "msg.status.stopped";
