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

}
