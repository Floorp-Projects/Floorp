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
 * The Original Code is JSIRC Test Client #3
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 *  Chiaki Koufugata, chiaki@mozilla.gr.jp, UI i18n
 */

function addCommands(commandObject)
{

    function add (name, func)
    {
        var usage = getMsg (name + "Usage");
        var help = getMsg(name + "Help");
        commandObject.add (name, func, usage, help);
    }
    
    add ("about", "onInputAbout");
    add ("attach", "onInputAttach");
    add ("away", "onInputAway");
    add ("cancel", "onInputCancel");
    add ("clear", "onInputClear");
    add ("client", "onInputClient");
    add ("commands", "onInputCommands");
    add ("ctcp", "onInputCTCP");
    add ("css", "onInputCSS");
    add ("delete", "onInputDelete");
    add ("deop", "onInputDeop");
    add ("desc", "onInputDesc");
    add ("devoice", "onInputDevoice");
    add ("disconnect", "onInputQuit");
    add ("echo", "onInputEcho");
    add ("eval", "onInputEval");
    add ("exit", "onInputExit");
    add ("help", "onInputHelp");
    add ("headers", "onInputHeaders");
    add ("hide", "onInputHide");
    add ("infobar", "onInputInfobar");
    add ("invite", "onInputInvite"); 
    add ("join", "onInputJoin");
    add ("kick", "onInputKick");
    add ("leave", "onInputLeave");
    add ("list", "onInputSimpleCommand");
    add ("me", "onInputMe");
    add ("msg", "onInputMsg");
    add ("name", "onInputName");
    add ("names", "onInputNames"); 
    add ("network", "onInputNetwork");
    add ("networks", "onInputNetworks");
    add ("nick", "onInputNick");
    add ("notify", "onInputNotify");
    add ("op", "onInputOp");
    add ("part", "onInputLeave");
    add ("ping", "onInputPing");
    add ("query", "onInputQuery"); 
    add ("quit", "onInputExit");
    add ("quote", "onInputQuote");
    add ("server", "onInputServer");
    add ("stalk", "onInputStalk");
    add ("status", "onInputStatus");
    add ("statusbar", "onInputStatusbar");
    add ("testdisplay", "onInputTestDisplay");
    add ("topic", "onInputTopic");
    add ("tabstrip", "onInputTabstrip");
    add ("unstalk", "onInputUnstalk");
    add ("version", "onInputVersion");
    add ("voice", "onInputVoice");
    add ("who", "onInputSimpleCommand");
    add ("whois", "onInputWhoIs");
    add ("whowas", "onInputSimpleCommand");
    
}

