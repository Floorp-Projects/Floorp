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

function getMsg (msgName, params, deflt)
{
    try 
    {
        var rv;
        
        if (params && params instanceof Array)
        {
            rv = console._bundle.formatStringFromName (msgName, params,
                                                       params.length);
        }
        else if (params)
        {
            rv = console._bundle.formatStringFromName (msgName, [params], 1);
        }
        else
        {
            rv = console._bundle.GetStringFromName (msgName);
        }
        
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
}

/* message types, don't localize */
const MT_CONT      = "CONT";
const MT_ERROR     = "ERROR";
const MT_HELLO     = "HELLO";
const MT_HELP      = "HELP";
const MT_WARN      = "WARN";
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
const MSG_ERR_NO_STACK     = getMsg("msg.err.nostack");
const MSG_ERR_DISABLED     = getMsg("msg.err.disabled");
const MSG_ERR_INTERNAL_BPT = getMsg("msg.err.internal.bpt");

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
const MSG_WINDOW_REC      = getMsg("msg.window.rec");
const MSG_FILES_REC       = getMsg("msg.files.rec");
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

const MSG_URL_NATIVE       = getMsg("msg.url.native");

const MSG_VF_ENUMERABLE    = getMsg("vf.enumerable");
const MSG_VF_READONLY      = getMsg("vf.readonly");
const MSG_VF_PERMANENT     = getMsg("vf.permanent");
const MSG_VF_ALIAS         = getMsg("vf.alias");
const MSG_VF_ARGUMENT      = getMsg("vf.argument");
const MSG_VF_VARIABLE      = getMsg("vf.variable");
const MSG_VF_HINTED        = getMsg("vf.hinted");

const MSG_HELLO            = getMsg("msg.hello");

const MSG_QUERY_CLOSE      = getMsg("msg.query.close");
const MSG_STATUS_DEFAULT   = getMsg("msg.status.default");

const MSG_TIP_HELP           = getMsg("msg.tip.help");
const MSG_NO_BREAKPOINTS_SET = getMsg("msg.no.breakpoints.set");
const MSG_NO_FBREAKS_SET     = getMsg("msg.no.fbreaks.set");

const MSG_EMODE_IGNORE     = getMsg("msg.emode.ignore");
const MSG_EMODE_TRACE      = getMsg("msg.emode.trace");
const MSG_EMODE_BREAK      = getMsg("msg.emode.break");

const MSG_TMODE_IGNORE     = getMsg("msg.tmode.ignore");
const MSG_TMODE_TRACE      = getMsg("msg.tmode.trace");
const MSG_TMODE_BREAK      = getMsg("msg.tmode.break");

const MSG_NO_HELP          = getMsg("msg.no.help");
const MSG_NOTE_CONSOLE     = getMsg("msg.note.console");
const MSG_NOTE_NEEDSTACK   = getMsg("msg.note.needstack");
const MSG_NOTE_NOSTACK     = getMsg("msg.note.nostack");
const MSG_DOC_NOTES        = getMsg("msg.doc.notes");
const MSG_DOC_DESCRIPTION  = getMsg("msg.doc.description");

const MSG_PROFILE_CLEARED  = getMsg("msg.profile.cleared");
const MSG_OPEN_FILE        = getMsg("msg.open.file");
const MSG_OPEN_URL         = getMsg("msg.open.url");
const MSG_SAVE_PROFILE     = getMsg("msg.save.profile");
const MSG_SAVE_SOURCE      = getMsg("msg.save.source");

/* message names for parameterized messages */
const MSN_ERR_INTERNAL_DISPATCH = "msg.err.internal.dispatch";
const MSN_CHROME_FILTER    = "msg.chrome.filter";
const MSN_ERR_NO_SCRIPT    = "msg.err.noscript";
const MSN_IASMODE          = "msg.iasmode";
const MSN_EXTRA_PARAMS     = "msg.extra.params";
const MSN_DOC_COMMANDLABEL = "msg.doc.commandlabel";
const MSN_DOC_KEY          = "msg.doc.key";
const MSN_DOC_SYNTAX       = "msg.doc.syntax";

const MSN_ERR_SCRIPTLOAD     = "err.subscript.load";
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
const MSN_FMT_PREFVALUE      = "fmt.prefvalue";

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
const MSN_ERPT_ERROR         = "msg.erpt.error";
const MSN_ERPT_WARN          = "msg.erpt.warn";
const MSN_PROFILE_LOST       = "msg.profile.lost";
const MSN_PROFILE_STATE      = "msg.profile.state";
const MSN_PROFILE_SAVED      = "msg.profile.saved";
const MSN_VERSION            = "msg.version";
const MSN_DEFAULT_ALIAS_HELP = "msg.default.alias.help";

const MSN_CONT             = "msg.cont";
const MSN_EVAL_ERROR       = "msg.eval.error";
const MSN_EVAL_THREW       = "msg.eval.threw";
const MSN_STOP             = "msg.stop";
const MSN_SUBSCRIPT_LOADED = "msg.subscript.load";
const MSN_STATUS_LOADING   = "msg.status.loading";
const MSN_STATUS_MARKING   = "msg.status.marking";
const MSN_STATUS_STOPPED   = "msg.status.stopped";
