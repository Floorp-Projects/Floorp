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
 */

function addCommands(commandObject)
{

    function add (name, func, usage, help)
    {
        commandObject.add (name, func, usage, help);
    }

    add ("help", "onInputHelp",
         "[<command-name>]",
         "Displays help on all commands matching <command-name>, if " +
         "<command-name> is not given, displays help on all commands");

    add ("testdisplay", "onInputTestDisplay",
         "",
         "Displays a sample text.  Used to preview styles");

    add ("network", "onInputNetwork",
         "<network-name>",
         "Sets the current network to <network-name>");

    add ("server", "onInputServer",
         "<server-hostname> [<port>] [<password>]",
         "Connects to server <server-hostname> on <port>, or 6667 if " +
         "<port> is not specified.  Provides the password <password> if " +
         "specified.");

    add ("quit", "onInputQuit", "[<reason>]",
         "Disconnects from the server represented by the active view when " +
         "the command is executed providing the reason <reason> " +
         "or the default reason if <reason> is not specified.");

    add ("exit", "onInputExit", "[<reason>]",
         "Disconnects from all active servers and networks,  providing the " +
         "reason <reason>, or the default reason if <reason> is not " +
         "specified.  Exits ChatZilla after disconnecting.");

    add ("clear", "onInputClear", "",
         "Clear the current view, discarding *all* content.");
    
    add ("delete", "onInputDelete", "",
         "Clear the current view, discarding *all* content, and drop it's " +
         "icon from the toolbar.");

    add ("hide", "onInputHide", "",
         "Drop the current view's icon from the toolbar, but save it's " +
         "contents.  The icon will reappear the next time there is activity " +
         "on the view.");

    add ("names", "onInputNames", "",
         "Toggles the visibility of the username list.");

    add ("toolbar", "onInputToolbar", "",
         "Toggles the visibility of the channel toolbar.");

    add ("statusbar", "onInputStatusbar", "",
         "Toggles the visibility of the status bar.");

    add ("commands", "onInputCommands", "[<pattern>]",
         "Lists all command names matching <pattern>, or all command names " +
         "if pattern is not specified.");
    
    add ("attach", "onInputAttach",
         "[<network-name>] [<password>]",
         "Attaches to the network specified by <network-name>, " +
         "or the current network, if no network is specified.  " +
         "Provides the password <password> if specified.");
      
    add ("me", "onInputMe",
         "<action>",
         "Performs an 'action' on the current channel.");
    
    add ("msg", "onInputMsg",
         "<user> <msg>",
         "Sends a private message <msg> to the user <user>.");
    
    add ("nick", "onInputNick",
         "<nickname>",
         "Changes your current nickname.");

    add ("name", "onInputName",
         "<username>",
         "Changes the username displayed before your hostmask if the server " +
         "you're connecting to allows it.  Some servers will only trust the " +
         "username reply from the ident service.  You must specify this " +
         "*before* connecting to the network.");

    add ("desc", "onInputDesc",
         "<description>",
         "Changes the 'ircname' line returned when someone performs a /whois " +
         "on you.  You must specify this *before* connecting to the network.");

    add ("quote", "onInputQuote",
         "<irc-command>",
         "Sends a raw command to the IRC server, not a good idea if you " +
         "don't know what you're doing. see IRC 1459 " +
         "( http://www.irchelp.org/irchelp/rfc1459.html ) for complete " +
         "details.");

    add ("eval", "onInputEval",
         "<script>",
         "Evaluates <script> as JavaScript code.  Not for the faint of heart.");

    add ("ctcp", "onInputCTCP",
         "<target> <code> [<params>]",
         "Sends the CTCP code <code> to the target (user or channel) " +
         "<target>.  If <params> are specified they are sent along as well.");
    
    add ("join", "onInputJoin",
         "[#|&|+]<channel-name> [<key>]",
         "Joins a the global (name starts with #), local (name starts " +
         "with &), or modeless (name starts with a +) channel named " +
         "<channel-name>.  If no prefix is given, # is " +
         "assumed.  Provides the key <key> if specified.");

    add ("leave", "onInputLeave",
         "",
         "Leaves the current channel, use /delete or /hide to force the " +
         "view to go away.");

    add ("part", "onInputLeave",
         "",
         "See /leave");

    add ("zoom", "onInputZoom",
         "<nick>",
         "Shows only messages <nick> has sent to the channel, filtering out " +
         "all others, (including yours.)");

    add ("whois", "onInputWhoIs",
         "<nick>",
         "Displays information about the user <nick>, including 'real name', " +
         "server connected to, idle time, and signon time.  Note that some " +
         "servers will lie about the idle time.");

    add ("topic", "onInputTopic",
         "[<new-topic>]",
         "If <new-topic> is specified and you are a chanop, or the channel " +
         "is not in 'private topic' mode (+t), the topic will be changed to " +
         "<new-topic>.  If <new-topic> is *not* specified, the current topic " +
         "will be displayed");

    /* JG: commands below jan 24 2000, update mar 15 */
    
    add ("away", "onInputAway",
         "[<reason>]",
         "If <reason> is spcecified, sets you away with that message. " +
         "Used without <reason>, you are marked back as no longer being away.");
    
    add ("op", "onInputOp",
         "<nick>",
          "Gives operator status to <nick> on current channel. Requires " +
         "operator status.");
    
    add ("deop", "onInputDeop",
         "<nick>",
         "Removes operator status from <nick> on current channel. " +
         "Requires operator status.");
    
    add ("voice", "onInputVoice",
         "<nick>",
         "Gives voice status to <nick> on current channel. " +
         "Requires operator status.");   
     
    add ("devoice", "onInputDevoice",
         "<nick>",
         "Removes voice status from <nick> on current channel. " +
         "Requires operator status.");    
    
    add ("stalk", "onInputStalk",
         "<text>",
         "Add text to list of words for which you would like to see alerts.");
    
    add ("unstalk", "onInputUnstalk",
         "<text>",
         "Remove word from list of terms for which you would like to see " +
         "alerts.");

    add ("echo", "onInputEcho",
         "<text>",
         "Displays <text> in the current view, but does not send it to " +
         "the server.");

    /* FIXME: JG: not implemented yet */
    /*
      add ("filter", "onInputFilter",
      "<regex>",
      "Shows only messages matching <regex> on current channel. When used " $
      "with no parameter, the contents are restored.");
    */  
    
    add ("invite", "onInputInvite",
         "<nick> [<channel>]",
         "Invites <nick> to <channel> or current channel if not " +
         "supplied. Requires operator status if +i is set.");
    
    add ("kick", "onInputKick",
         "[<channel>] <nick>",
         "Kicks <nick> from <channel> or current channel if not " +
         "supplied. Requires operator status.");    
}

