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
    
    add ("attach", "onInputAttach",
         "[<network-name>] [<password>]",
         "Attaches to the network specified by <network-name>, " +
         "or the current network, if no network is specified.  " +
         "Provides the password <password> if specified.  If you are already " +
         "attached, the view for <network-name> is made current.  If that " +
         "view has been deleted, it is recreated.");

    add ("away", "onInputAway",
         "[<reason>]",
         "If <reason> is spcecified, sets you away with that message. " +
         "Used without <reason>, you are marked back as no longer being away.");
    
    add ("cancel", "onInputCancel", "",
         "Cancels a /attach or /server command.  Use /cancel when ChatZilla " +
         "is repeatdly trying to attach to a network that is not responding, " +
         "to tell ChatZilla to give up before the normal number of retries.");

    add ("clear", "onInputClear", "",
         "Clear the current view, discarding *all* content.");

    add ("client", "onInputClient", "",
         "Make the ``*client*'' view current.  If the ``*client*'' view has " +
         "been deleted, it will be recreated.");
    
    add ("commands", "onInputCommands", "[<pattern>]",
         "Lists all command names matching <pattern>, or all command names " +
         "if pattern is not specified.");
      
    add ("ctcp", "onInputCTCP",
         "<target> <code> [<params>]",
         "Sends the CTCP code <code> to the target (user or channel) " +
         "<target>.  If <params> are specified they are sent along as well.");
    
    add ("delete", "onInputDelete", "",
         "Clear the current view, discarding *all* content, and drop it's " +
         "icon from the toolbar.");

    add ("deop", "onInputDeop",
         "<nick>",
         "Removes operator status from <nick> on current channel. " +
         "Requires operator status.");
    
    add ("desc", "onInputDesc",
         "<description>",
         "Changes the 'ircname' line returned when someone performs a /whois " +
         "on you.  You must specify this *before* connecting to the network.");

    add ("devoice", "onInputDevoice",
         "<nick>",
         "Removes voice status from <nick> on current channel. " +
         "Requires operator status.");    
    
    add ("disconnect", "onInputQuit", "[<reason>]",
         "Disconnects from the server represented by the active view when " +
         "the command is executed providing the reason <reason> " +
         "or the default reason if <reason> is not specified.");

    add ("echo", "onInputEcho",
         "<text>",
         "Displays <text> in the current view, but does not send it to " +
         "the server.");

    add ("eval", "onInputEval",
         "<script>",
         "Evaluates <script> as JavaScript code.  Not for the faint of heart.");

    add ("exit", "onInputExit", "[<reason>]",
         "Disconnects from all active servers and networks,  providing the " +
         "reason <reason>, or the default reason if <reason> is not " +
         "specified.  Exits ChatZilla after disconnecting.");

    /* FIXME: JG: not implemented yet */
    /*
      add ("filter", "onInputFilter",
      "<regex>",
      "Shows only messages matching <regex> on current channel. When used " $
      "with no parameter, the contents are restored.");
    */  
    
    add ("help", "onInputHelp",
         "[<command-name>]",
         "Displays help on all commands matching <command-name>, if " +
         "<command-name> is not given, displays help on all commands");

    add ("hide", "onInputHide", "",
         "Drop the current view's icon from the toolbar, but save it's " +
         "contents.  The icon will reappear the next time there is activity " +
         "on the view.");

    add ("infobar", "onInputInfobar", "",
         "Toggles the visibility of the username list.");

    add ("invite", "onInputInvite",
         "<nick> [<channel>]",
         "Invites <nick> to <channel> or current channel if not " +
         "supplied. Requires operator status if +i is set.");
    
    add ("join", "onInputJoin",
         "[#|&|+]<channel-name> [<key>]",
         "Joins a the global (name starts with #), local (name starts " +
         "with &), or modeless (name starts with a +) channel named " +
         "<channel-name>.  If no prefix is given, # is " +
         "assumed.  Provides the key <key> if specified.");

    add ("kick", "onInputKick",
         "[<channel>] <nick>",
         "Kicks <nick> from <channel> or current channel if not " +
         "supplied. Requires operator status.");    

    add ("leave", "onInputLeave",
         "",
         "Leaves the current channel, use /delete or /hide to force the " +
         "view to go away.");

    add ("list", "onInputSimpleCommand",
         "[channel]",
         "Lists channel name, user count, and topic information for the " +
         "network/server you are attached to.  If you omit the optional " +
         "channel argument, all channels will be listed.  On large networks, " +
         "the server may disconnect you for asking for a complete list.");

    add ("me", "onInputMe",
         "<action>",
         "Performs an 'action' on the current channel.");
    
    add ("msg", "onInputMsg",
         "<user> <msg>",
         "Sends a private message <msg> to the user <user>.");
    
    add ("name", "onInputName",
         "<username>",
         "Changes the username displayed before your hostmask if the server " +
         "you're connecting to allows it.  Some servers will only trust the " +
         "username reply from the ident service.  You must specify this " +
         "*before* connecting to the network.");

    add ("names", "onInputNames", "[<channel>]",
         "Lists the users in a channel.");

    add ("network", "onInputNetwork",
         "<network-name>",
         "Sets the current network to <network-name>");

    add ("networks", "onInputNetworks",
         "",
         "Lists all available networks as clickable links.");

    add ("nick", "onInputNick",
         "<nickname>",
         "Changes your current nickname.");

    add ("notify", "onInputNotify",
         "[<nickname> [...]]",
         "With no parameters, /notify shows you the online/offline status " +
         "of all the users on your notify list.  If one or more <nickname> " +
         "parameters are supplied, the nickname(s) will be added to your " +
         "notify list if they are not yet on it, or removed from it if they "+
         "are.");

    add ("op", "onInputOp",
         "<nick>",
          "Gives operator status to <nick> on current channel. Requires " +
         "operator status.");
    
    add ("part", "onInputLeave",
         "",
         "See /leave");

    add ("query", "onInputQuery", ",<user> [<msg>]",
         "Opens a one-on-one chat with <usr>.  If <msg> is supplied, it is " +
         "sent as the initial private message to <user>.");

    add ("quit", null, "[<reason>]",
         "This command has been replaced by /disconnect.");

    add ("quote", "onInputQuote",
         "<irc-command>",
         "Sends a raw command to the IRC server, not a good idea if you " +
         "don't know what you're doing. see IRC 1459 " +
         "( http://www.irchelp.org/irchelp/rfc1459.html ) for complete " +
         "details.");

    add ("server", "onInputServer",
         "<server-hostname> [<port>] [<password>]",
         "Connects to server <server-hostname> on <port>, or 6667 if " +
         "<port> is not specified.  Provides the password <password> if " +
         "specified. If you are already connected, " +
         "the view for <server-hostname> is made current.  If that view " +
         "has been deleted, it is recreated.");

    add ("stalk", "onInputStalk",
         "<text>",
         "Add text to list of words for which you would like to see alerts." +
         "Whenever a person with a nickname macthing <text> speaks, or " +
         "someone says a phrase containing <text>, your " +
         "ChatZilla window will become active (on some operating systems) " +
         "and it's taskbar icon will flash (on some operating systems.)");

    add ("status", "onInputStatus", "",
         "Shows status information for the current view.");
    
    add ("statusbar", "onInputStatusbar", "",
         "Toggles the visibility of the status bar.");

    add ("testdisplay", "onInputTestDisplay",
         "",
         "Displays a sample text.  Used to preview styles");

    add ("topic", "onInputTopic",
         "[<new-topic>]",
         "If <new-topic> is specified and you are a chanop, or the channel " +
         "is not in 'private topic' mode (+t), the topic will be changed to " +
         "<new-topic>.  If <new-topic> is *not* specified, the current topic " +
         "will be displayed");

    add ("toolbar", "onInputToolbar", "",
         "Toggles the visibility of the channel toolbar.");

    add ("unstalk", "onInputUnstalk",
         "<text>",
         "Remove word from list of terms for which you would like to see " +
         "alerts.");

    add ("voice", "onInputVoice",
         "<nick>",
         "Gives voice status to <nick> on current channel. " +
         "Requires operator status.");   

    add ("who", "onInputSimpleCommand",
         "<pattern>",
         "List users who have name, host, or description information matching" +
         " <pattern>.");
    
    add ("whois", "onInputWhoIs",
         "<nick>",
         "Displays information about the user <nick>, including 'real name', " +
         "server connected to, idle time, and signon time.  Note that some " +
         "servers will lie about the idle time.");


    
/*
    add ("zoom", "onInputZoom",
         "<nick>",
         "Shows only messages <nick> has sent to the channel, filtering out " +
         "all others, (including yours.)");
*/
    
}

