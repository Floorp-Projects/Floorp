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

    add ("attach", "onInputAttach",
         "[<network-name>]",
         "Attaches to the network specified by <network-name>, " +
         "or the current network, if no network is specified.");
      
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

    add ("join", "onInputJoin",
         "[#|&]<channel-name>",
         "Joins a the global (name starts with #) or local (name starts " +
         "with &) channel named <channel-name>.  If no prefix is given, # is " +
         "assumed.");

    add ("leave", "onInputLeave",
         "",
         "Parts the current channel");

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
    
    /* NOT implemented yet 
    add ("server", "onInputServer",
         "<server> [<port>]",
         "Connects you to <server> using port 6667 if <port> is not" +
         " specified.");
    */
    
    /* NOT implemented yet
    add ("quit", "onInputExit",
         "[<message>]",
         "Terminates the connection with the server associated with the" +
         " current view, overriding the default quit message with" +
         " message if specified.");
    */
}

