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

function initCommands(commandObject)
{

    console._commands = new CCommandManager();

    function add (name, func, usage, help)
    {
        console._commands.add (name, func, usage, help);
    }

    add (CMD_BREAK,  "onInputBreak",     CMD_BREAK_PARAMS,  CMD_BREAK_HELP);
    add (CMD_CLEAR,  "onInputClear",     CMD_CLEAR_PARAMS,  CMD_CLEAR_HELP);
    add (CMD_CMDS,   "onInputCommands",  CMD_CMDS_PARAMS,   CMD_CMDS_HELP);
    add (CMD_CONT,   "onInputCont",      CMD_CONT_PARAMS,   CMD_CONT_HELP);
    add (CMD_EVAL,   "onInputEval",      CMD_EVAL_PARAMS,   CMD_EVAL_HELP);
    add (CMD_EVALD,  "onInputEvalD",     CMD_EVALD_PARAMS,  CMD_EVALD_HELP);
    add (CMD_FBREAK, "onInputFBreak",    CMD_FBREAK_PARAMS, CMD_FBREAK_HELP);
    add (CMD_FINISH, "onInputFinish",    CMD_FINISH_PARAMS, CMD_FINISH_HELP);
    add (CMD_FRAME,  "onInputFrame",     CMD_FRAME_PARAMS,  CMD_FRAME_HELP);
    add (CMD_HELP,   "onInputHelp",      CMD_HELP_PARAMS,   CMD_HELP_HELP);
    add (CMD_NEXT,   "onInputNext",      CMD_NEXT_PARAMS,   CMD_NEXT_HELP);
    add (CMD_PROPS,  "onInputProps",     CMD_PROPS_PARAMS,  CMD_PROPS_HELP);
    add (CMD_PROPSD, "onInputPropsD",    CMD_PROPSD_PARAMS, CMD_PROPSD_HELP);
    add (CMD_SCOPE,  "onInputScope",     CMD_SCOPE_PARAMS,  CMD_SCOPE_HELP);
    add (CMD_STEP,   "onInputStep",      CMD_STEP_PARAMS,   CMD_STEP_HELP);
    add (CMD_TMODE,  "onInputTMode",     CMD_TMODE_PARAMS,  CMD_TMODE_HELP);
    add (CMD_WHERE,  "onInputWhere",     CMD_WHERE_PARAMS,  CMD_WHERE_HELP);
    add (CMD_QUIT,   "onInputQuit",      CMD_QUIT_PARAMS,   CMD_QUIT_HELP);
    
}

