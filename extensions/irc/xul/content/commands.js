/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is ChatZilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>, original author
 *   Chiaki Koufugata, chiaki@mozilla.gr.jp, UI i18n
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const CMD_CONSOLE    = 0x01;
const CMD_NEED_NET   = 0x02;
const CMD_NEED_SRV   = 0x04;
const CMD_NEED_CHAN  = 0x08;
const CMD_NEED_USER  = 0x10;
const CMD_NO_HELP    = 0x20;

function initCommands()
{
    var cmdary =
        [/* "real" commands */
         ["about",             cmdAbout,                           CMD_CONSOLE],
         ["alias",             cmdAlias,                           CMD_CONSOLE],
         ["attach",            cmdAttach,                          CMD_CONSOLE],
         ["away",              cmdAway,             CMD_NEED_SRV | CMD_CONSOLE],
         ["cancel",            cmdCancel,           CMD_NEED_NET | CMD_CONSOLE],
         ["charset",           cmdCharset,                         CMD_CONSOLE],
         ["channel-motif",     cmdMotif,           CMD_NEED_CHAN | CMD_CONSOLE],
         ["channel-pref",      cmdPref,            CMD_NEED_CHAN | CMD_CONSOLE],
         ["cmd-undo",          "cmd-docommand cmd_undo",                     0],
         ["cmd-redo",          "cmd-docommand cmd_redo",                     0],
         ["cmd-cut",           "cmd-docommand cmd_cut",                      0],
         ["cmd-copy",          "cmd-docommand cmd_copy",                     0],
         ["cmd-paste",         "cmd-docommand cmd_paste",                    0],
         ["cmd-delete",        "cmd-docommand cmd_delete",                   0],
         ["cmd-selectall",     "cmd-docommand cmd_selectAll",                0],
         ["cmd-copy-link-url", "cmd-docommand cmd_copyLink",                 0],
         ["cmd-mozilla-prefs",   "cmd-docommand cmd_mozillaPrefs",           0],
         ["cmd-prefs",           "cmd-docommand cmd_chatzillaPrefs",         0],
         ["cmd-chatzilla-prefs", "cmd-docommand cmd_chatzillaPrefs",         0],
         ["cmd-chatzilla-opts",  "cmd-docommand cmd_chatzillaPrefs",         0],
         ["cmd-docommand",     cmdDoCommand,                                 0],
         ["create-tab-for-view", cmdCreateTabForView,                        0],
         ["op",                cmdChanUserMode,    CMD_NEED_CHAN | CMD_CONSOLE],
         ["dcc-accept",        cmdDCCAccept,                       CMD_CONSOLE],
         ["dcc-chat",          cmdDCCChat,          CMD_NEED_NET | CMD_CONSOLE],
         ["dcc-close",         cmdDCCClose,                        CMD_CONSOLE],
         ["dcc-decline",       cmdDCCDecline,                      CMD_CONSOLE],
         ["dcc-list",          cmdDCCList,                         CMD_CONSOLE],
         ["dcc-send",          cmdDCCSend,          CMD_NEED_NET | CMD_CONSOLE],
         ["deop",              cmdChanUserMode,    CMD_NEED_CHAN | CMD_CONSOLE],
         ["describe",          cmdDescribe,         CMD_NEED_SRV | CMD_CONSOLE],
         ["hop",               cmdChanUserMode,    CMD_NEED_CHAN | CMD_CONSOLE],
         ["dehop",             cmdChanUserMode,    CMD_NEED_CHAN | CMD_CONSOLE],
         ["voice",             cmdChanUserMode,    CMD_NEED_CHAN | CMD_CONSOLE],
         ["devoice",           cmdChanUserMode,    CMD_NEED_CHAN | CMD_CONSOLE],
         ["clear-view",        cmdClearView,                       CMD_CONSOLE],
         ["client",            cmdClient,                          CMD_CONSOLE],
         ["commands",          cmdCommands,                        CMD_CONSOLE],
         ["ctcp",              cmdCTCP,             CMD_NEED_SRV | CMD_CONSOLE],
         ["default-charset",   cmdCharset,                         CMD_CONSOLE],
         ["delete-view",       cmdDeleteView,                      CMD_CONSOLE],
         ["disable-plugin",    cmdAblePlugin,                      CMD_CONSOLE],
         ["disconnect",        cmdDisconnect,       CMD_NEED_SRV | CMD_CONSOLE],
         ["echo",              cmdEcho,                            CMD_CONSOLE],
         ["enable-plugin",     cmdAblePlugin,                      CMD_CONSOLE],
         ["eval",              cmdEval,                            CMD_CONSOLE],
         ["focus-input",       cmdFocusInput,                      CMD_CONSOLE],
         ["font-family",       cmdFont,                            CMD_CONSOLE],
         ["font-family-other", cmdFont,                                      0],
         ["font-size",         cmdFont,                            CMD_CONSOLE],
         ["font-size-other",   cmdFont,                                      0],
         ["goto-url",          cmdGotoURL,                                   0],
         ["goto-url-newwin",   cmdGotoURL,                                   0],
         ["goto-url-newtab",   cmdGotoURL,                                   0],
         ["help",              cmdHelp,                            CMD_CONSOLE],
         ["hide-view",         cmdHideView,                        CMD_CONSOLE],
         ["ignore",            cmdIgnore,           CMD_NEED_NET | CMD_CONSOLE],
         ["input-text-direction", cmdInputTextDirection,                     0],
         ["invite",            cmdInvite,           CMD_NEED_SRV | CMD_CONSOLE],
         ["join",              cmdJoin,             CMD_NEED_SRV | CMD_CONSOLE],
         ["join-charset",      cmdJoin,             CMD_NEED_SRV | CMD_CONSOLE],
         ["kick",              cmdKick,            CMD_NEED_CHAN | CMD_CONSOLE],
         ["kick-ban",          cmdKick,            CMD_NEED_CHAN | CMD_CONSOLE],
         ["leave",             cmdLeave,           CMD_NEED_CHAN | CMD_CONSOLE],
         ["links",             cmdSimpleCommand,    CMD_NEED_SRV | CMD_CONSOLE],
         ["list",              cmdList,             CMD_NEED_SRV | CMD_CONSOLE],
         ["list-plugins",      cmdListPlugins,                     CMD_CONSOLE],
         ["load",              cmdLoad,                            CMD_CONSOLE],
         ["log",               cmdLog,                             CMD_CONSOLE],
         ["me",                cmdMe,                              CMD_CONSOLE],
         ["motd",              cmdSimpleCommand,    CMD_NEED_SRV | CMD_CONSOLE],
         ["motif",             cmdMotif,                           CMD_CONSOLE],
         ["msg",               cmdMsg,              CMD_NEED_SRV | CMD_CONSOLE],
         ["names",             cmdNames,            CMD_NEED_SRV | CMD_CONSOLE],
         ["network",           cmdNetwork,                         CMD_CONSOLE],
         ["network-motif",     cmdMotif,            CMD_NEED_NET | CMD_CONSOLE],
         ["network-pref",      cmdPref,             CMD_NEED_NET | CMD_CONSOLE],
         ["networks",          cmdNetworks,                        CMD_CONSOLE],
         ["nick",              cmdNick,                            CMD_CONSOLE],
         ["notify",            cmdNotify,           CMD_NEED_SRV | CMD_CONSOLE],
         ["open-at-startup",   cmdOpenAtStartup,                   CMD_CONSOLE],
         ["ping",              cmdPing,             CMD_NEED_SRV | CMD_CONSOLE],
         ["pref",              cmdPref,                            CMD_CONSOLE],
         ["query",             cmdQuery,            CMD_NEED_SRV | CMD_CONSOLE],
         ["quit",              cmdQuit,                            CMD_CONSOLE],
         ["quit-mozilla",      cmdQuitMozilla,                     CMD_CONSOLE],
         ["quote",             cmdQuote,            CMD_NEED_SRV | CMD_CONSOLE],
         ["rlist",             cmdRlist,            CMD_NEED_SRV | CMD_CONSOLE],
         ["reload-ui",         cmdReloadUI,                                  0],
         ["say",               cmdSay,              CMD_NEED_SRV | CMD_CONSOLE],
         ["server",            cmdServer,                          CMD_CONSOLE],
         ["set-current-view",  cmdSetCurrentView,                            0],
         ["squery",            cmdSquery,           CMD_NEED_SRV | CMD_CONSOLE],
         ["sslserver",         cmdSSLServer,                       CMD_CONSOLE],
         ["stalk",             cmdStalk,                           CMD_CONSOLE],
         ["supports",          cmdSupports,         CMD_NEED_SRV | CMD_CONSOLE],
         ["sync-font",         cmdSync,                                      0],
         ["sync-header",       cmdSync,                                      0],
         ["sync-log",          cmdSync,                                      0],
         ["sync-motif",        cmdSync,                                      0],
         ["sync-timestamp",    cmdSync,                                      0],
         ["sync-window",       cmdSync,                                      0],
         ["testdisplay",       cmdTestDisplay,                     CMD_CONSOLE],
         ["text-direction",    cmdTextDirection,                             0],
         ["timestamps",        cmdTimestamps,                      CMD_CONSOLE],
         ["timestamp-format",  cmdTimestampFormat,                 CMD_CONSOLE],
         ["toggle-ui",         cmdToggleUI,                        CMD_CONSOLE],
         ["toggle-pref",       cmdTogglePref,                                0],
         ["topic",             cmdTopic,           CMD_NEED_CHAN | CMD_CONSOLE],
         ["unignore",          cmdIgnore,           CMD_NEED_NET | CMD_CONSOLE],
         ["unstalk",           cmdUnstalk,                         CMD_CONSOLE],
         ["usermode",          cmdUsermode,                        CMD_CONSOLE],
         ["user-motif",        cmdMotif,           CMD_NEED_USER | CMD_CONSOLE],
         ["user-pref",         cmdPref,            CMD_NEED_USER | CMD_CONSOLE],
         ["version",           cmdVersion,                         CMD_CONSOLE],
         ["who",               cmdWho,              CMD_NEED_SRV | CMD_CONSOLE],
         ["whois",             cmdWhoIs,            CMD_NEED_SRV | CMD_CONSOLE],
         ["whowas",            cmdSimpleCommand,    CMD_NEED_SRV | CMD_CONSOLE],

         /* aliases */
         ["css",              "motif",                             CMD_CONSOLE],
         ["exit",             "quit",                              CMD_CONSOLE],
         ["exit-mozilla",     "quit-mozilla",                      CMD_CONSOLE],
         ["desc",             "pref desc",                         CMD_CONSOLE],
         ["j",                "join",                              CMD_CONSOLE],
         ["name",             "pref username",                     CMD_CONSOLE],
         ["part",             "leave",                             CMD_CONSOLE],
         ["raw",              "quote",              CMD_NEED_SRV | CMD_CONSOLE],
         // These are all the font family/size menu commands...
         ["font-family-default",    "font-family default",                   0],
         ["font-family-serif",      "font-family serif",                     0],
         ["font-family-sans-serif", "font-family sans-serif",                0],
         ["font-family-monospace",  "font-family monospace",                 0],
         ["font-size-default",      "font-size default",                     0],
         ["font-size-small",        "font-size small",                       0],
         ["font-size-medium",       "font-size medium",                      0],
         ["font-size-large",        "font-size large",                       0],
         ["font-size-bigger",       "font-size bigger",                      0],
         // This next command is not visible; it maps to Ctrl-=, which is what
         // you get when the user tries to do Ctrl-+ (previous command's key).
         ["font-size-bigger2",      "font-size bigger",                      0],
         ["font-size-smaller",      "font-size smaller",                     0],
         ["toggle-oas",       "open-at-startup toggle",                      0],
         ["toggle-ccm",       "toggle-pref collapseMsgs",                    0],
         ["toggle-copy",      "toggle-pref copyMessages",                    0],
         ["toggle-usort",     "toggle-pref sortUsersByMode",                 0],
         ["toggle-umode",     "toggle-pref showModeSymbols",                 0],
         ["toggle-timestamps","timestamps toggle",                           0],
         ["motif-dark",       "motif dark",                                  0],
         ["motif-light",      "motif light",                                 0],
         ["motif-default",    "motif default",                               0],
         ["sync-output",      "eval syncOutputFrame(this)",        CMD_CONSOLE],
         ["userlist",         "toggle-ui userlist",                CMD_CONSOLE],
         ["tabstrip",         "toggle-ui tabstrip",                CMD_CONSOLE],
         ["statusbar",        "toggle-ui status",                  CMD_CONSOLE],
         ["header",           "toggle-ui header",                  CMD_CONSOLE],

         // text-direction aliases
         ["rtl",              "text-direction rtl",                CMD_CONSOLE],
         ["ltr",              "text-direction ltr",                CMD_CONSOLE],
         ["toggle-text-dir",  "text-direction toggle",                       0],
         ["irtl",             "input-text-direction rtl",          CMD_CONSOLE],
         ["iltr",             "input-text-direction ltr",          CMD_CONSOLE]
        ];

    // set the stringbundle associated with these commands.
    cmdary.stringBundle = client.defaultBundle;

    client.commandManager = new CommandManager(client.defaultBundle);
    client.commandManager.defaultFlags = CMD_NO_HELP | CMD_CONSOLE;
    client.commandManager.isCommandSatisfied = isCommandSatisfied;
    client.commandManager.defineCommands(cmdary);

    client.commandManager.argTypes.__aliasTypes__(["reason", "action", "text",
                                                   "message", "params", "font",
                                                   "reason", "expression",
                                                   "ircCommand", "prefValue",
                                                   "newTopic", "commandList",
                                                   "file"], "rest");
    client.commandManager.argTypes["plugin"] = parsePlugin;
}

function isCommandSatisfied(e, command)
{
    if (typeof command == "undefined")
        command = e.command;
    else if (typeof command == "string")
        command = this.commands[command];

    if (command.flags & CMD_NEED_USER)
    {
        if (!("user" in e) || !e.user)
        {
            e.parseError = getMsg(MSG_ERR_NEED_USER, command.name);
            return false;
        }
    }

    if (command.flags & CMD_NEED_CHAN)
    {
        if (!("channel" in e) || !e.channel)
        {
            e.parseError = getMsg(MSG_ERR_NEED_CHANNEL, command.name);
            return false;
        }
    }

    if (command.flags & CMD_NEED_SRV)
    {
        if (!("server" in e) || !e.server)
        {
            e.parseError = getMsg(MSG_ERR_NEED_SERVER, command.name);
            return false;
        }

        if (!e.server.isConnected)
        {
            e.parseError = MSG_ERR_NOT_CONNECTED;
            return false;
        }
    }

    if (command.flags & (CMD_NEED_NET | CMD_NEED_SRV | CMD_NEED_CHAN))
    {
        if (!("network" in e) || !e.network)
        {
            e.parseError = getMsg(MSG_ERR_NEED_NETWORK, command.name);
            return false;
        }
    }

    return CommandManager.prototype.isCommandSatisfied(e, command);
}

CIRCChannel.prototype.dispatch =
CIRCNetwork.prototype.dispatch =
CIRCUser.prototype.dispatch =
CIRCDCCChat.prototype.dispatch =
CIRCDCCFileTransfer.prototype.dispatch =
client.dispatch =
function this_dispatch(text, e, isInteractive, flags)
{
    e = getObjectDetails(this, e);
    return dispatch(text, e, isInteractive, flags);
}

function dispatch(text, e, isInteractive, flags)
{
    if (typeof isInteractive == "undefined")
        isInteractive = false;

    if (!e)
        e = new Object();

    if (!("sourceObject" in e))
        e.__proto__ = getObjectDetails(client.currentObject);

    if (!("isInteractive" in e))
        e.isInteractive = isInteractive;

    if (!("inputData" in e))
        e.inputData = "";

    /* split command from arguments */
    var ary = text.match(/(\S+) ?(.*)/);
    e.commandText = ary[1];
    if (ary[2])
        e.inputData = stringTrim(ary[2]);

    /* list matching commands */
    ary = client.commandManager.list(e.commandText, flags);
    var rv = null;
    var i;

    switch (ary.length)
    {
        case 0:
            /* no match, try again */
            if (e.server && e.server.isConnected &&
                client.prefs["guessCommands"])
            {
                /* Want to keep the source details. */
                var e2 = getObjectDetails(e.sourceObject);
                e2.inputData = e.commandText + " " + e.inputData;
                return dispatch("quote", e2);
            }

            display(getMsg(MSG_NO_CMDMATCH, e.commandText), MT_ERROR);
            break;

        case 1:
            /* one match, good for you */
            var ex;
            try
            {
                rv = dispatchCommand(ary[0], e, flags);
            }
            catch (ex)
            {
                display(getMsg(MSG_ERR_INTERNAL_DISPATCH, ary[0].name),
                        MT_ERROR);
                display(formatException(ex), MT_ERROR);
                if (typeof ex == "object" && "stack" in ex)
                    dd(formatException(ex) + "\n" + ex.stack);
                else
                    dd(formatException(ex), MT_ERROR);
            }
            break;

        default:
            /* more than one match, show the list */
            var str = "";
            for (i in ary)
                str += (str) ? MSG_COMMASP + ary[i].name : ary[i].name;
            display(getMsg(MSG_ERR_AMBIGCOMMAND,
                           [e.commandText, ary.length, str]), MT_ERROR);
    }

    return rv;
}

function dispatchCommand (command, e, flags)
{
    function displayUsageError (e, details)
    {
        if (!("isInteractive" in e) || !e.isInteractive)
        {
            var caller = Components.stack.caller.caller;
            if (caller.name == "dispatch")
                caller = caller.caller;
            var error = new Error (details);
            error.fileName = caller.filename;
            error.lineNumber = caller.lineNumber;
            error.name = caller.name;
            display (formatException(error), MT_ERROR);
        }
        else
        {
            display (details, MT_ERROR);
        }

        //display (getMsg(MSG_FMT_USAGE, [e.command.name, e.command.usage]),
        //         MT_USAGE);
    };

    function callHooks (command, isBefore)
    {
        var names, hooks;

        if (isBefore)
            hooks = command.beforeHooks;
        else
            hooks = command.afterHooks;

        for (var h in hooks)
        {
            if ("dbgDispatch" in client && client.dbgDispatch)
            {
                dd ("calling " + (isBefore ? "before" : "after") +
                    " hook " + h);
            }
            try
            {
                hooks[h](e);
            }
            catch (ex)
            {
                if (e.command.name != "hook-session-display")
                {
                    display(getMsg(MSG_ERR_INTERNAL_HOOK, h), MT_ERROR);
                    display(formatException(ex), MT_ERROR);
                }
                else
                {
                    dd(getMsg(MSG_ERR_INTERNAL_HOOK, h));
                }

                dd("Caught exception calling " +
                   (isBefore ? "before" : "after") + " hook " + h);
                dd(formatException(ex));
                if (typeof ex == "object" && "stack" in ex)
                    dd(ex.stack);
                else
                    dd(getStackTrace());
            }
        }
    };

    e.command = command;

    if (!e.command.enabled)
    {
        /* disabled command */
        display (getMsg(MSG_ERR_DISABLED, e.command.name),
                 MT_ERROR);
        return null;
    }

    var h, i;

    if (typeof e.command.func == "function")
    {
        /* dispatch a real function */
        if (e.command.usage)
            client.commandManager.parseArguments (e);
        if ("parseError" in e)
        {
            displayUsageError(e, e.parseError);
        }
        else
        {
            if ("beforeHooks" in e.command)
                callHooks (e.command, true);

            if ("dbgDispatch" in client && client.dbgDispatch)
            {
                var str = "";
                for (i = 0; i < e.command.argNames.length; ++i)
                {
                    var name = e.command.argNames[i];
                    if (name in e)
                        str += " " + name + ": " + e[name];
                    else if (name != ":")
                        str += " ?" + name;
                }
                dd (">>> " + e.command.name + str + " <<<");
                e.returnValue = e.command.func(e);
                /* set client.lastEvent *after* dispatching, so the dispatched
                 * function actually get's a chance to see the last event. */
                client.lastEvent = e;
            }
            else
            {
                e.returnValue = e.command.func(e);
            }

        }
    }
    else if (typeof e.command.func == "string")
    {
        /* dispatch an alias (semicolon delimited list of subcommands) */
        if ("beforeHooks" in e.command)
            callHooks (e.command, true);

        var commandList = e.command.func.split(";");
        for (i = 0; i < commandList.length; ++i)
        {
            var newEvent = Clone(e);
            delete newEvent.command;
            commandList[i] = stringTrim(commandList[i]);
            if (i == commandList.length - 1)
                dispatch(commandList[i] + " " + e.inputData, newEvent, flags);
            else
                dispatch(commandList[i], newEvent, flags);
        }
    }
    else
    {
        display (getMsg(MSG_ERR_NOTIMPLEMENTED, e.command.name),
                 MT_ERROR);
        return null;
    }

    if ("afterHooks" in e.command)
        callHooks (e.command, false);

    return ("returnValue" in e) ? e.returnValue : null;
}

/* parse function for <plugin> parameters */
function parsePlugin(e, name)
{
    var ary = e.unparsedData.match (/(?:(\d+)|(\S+))(?:\s+(.*))?$/);
    if (!ary)
        return false;

    var plugin;

    if (ary[1])
    {
        var i = parseInt(ary[1]);
        if (!(i in client.plugins))
            return false;

        plugin = client.plugins[i];
    }
    else
    {
        plugin = getPluginById(ary[2]);
        if (!plugin)
            return false;

    }

    e.unparsedData = arrayHasElementAt(ary, 4) ? ary[4] : "";
    e[name] = plugin;
    return true;
}

function getToggle (toggle, currentState)
{
    if (toggle == "toggle")
        toggle = !currentState;

    return toggle;
}

/******************************************************************************
 * command definitions from here on down.
 */

function cmdAblePlugin(e)
{
    if (e.command.name == "disable-plugin")
    {
        if (!("disablePlugin" in e.plugin.scope))
        {
            display(getMsg(MSG_CANT_DISABLE, e.plugin.id));
            return;
        }

        e.plugin.enabled = false;
        e.plugin.scope.disablePlugin();
    }
    else
    {
        if (!("enablePlugin" in e.plugin.scope))
        {
            display(getMsg(MSG_CANT_ENABLE, e.plugin.id));
            return;
        }

        e.plugin.enabled = true;
        e.plugin.scope.enablePlugin();
    }
}

function cmdCancel(e)
{
    var network = e.network;

    if (!network.connecting)
    {
        display(MSG_NOTHING_TO_CANCEL, MT_ERROR);
        return;
    }

    network.cancelConnect = true;
    display(getMsg(MSG_CANCELLING, network.unicodeName));
    if (network.isConnected())
    {
        // Pull the plug on the current connection, or...
        network.dispatch("disconnect");
    }
    else
    {
        // ...try a reconnect (which will fail us).
        var ev = new CEvent("network", "do-connect", network, "onDoConnect");
        network.eventPump.addEvent(ev);
    }
}

function cmdChanUserMode(e)
{
    var modestr;
    switch (e.command.name)
    {
        case "op":
            modestr = "+oooo";
            break;

        case "deop":
            modestr = "-oooo";
            break;

        case "hop":
            modestr = "+hhhh";
            break;

        case "dehop":
            modestr = "-hhhh";
            break;

        case "voice":
            modestr = "+vvvv";
            break;

        case "devoice":
            modestr = "-vvvv";
            break;

        default:
            ASSERT(0, "Dispatch from unknown name " + e.command.name);
            return;
    }

    var nicks;
    // Prefer pre-canonicalised list, then a normal list, then finally a
    // sigular item (canon. or otherwise).
    if (e.canonNickList)
    {
        nicks = combineNicks(e.canonNickList);
    }
    else if (e.nicknameList)
    {
        var nickList = new Array();
        for (i = 0; i < e.nicknameList.length; i++)
        {
            var user = e.channel.getUser(e.nicknameList[i]);
            if (!user)
            {
                display(getMsg(MSG_ERR_UNKNOWN_USER, e.nicknameList[i]), MT_ERROR);
                return;
            }
            nickList.push(user.encodedName);
        }
        nicks = combineNicks(nickList);
    }
    else if (e.nickname)
    {
        var user = e.channel.getUser(e.nickname);
        if (!user)
        {
            display(getMsg(MSG_ERR_UNKNOWN_USER, e.nicknameList[i]), MT_ERROR);
            return;
        }
        var str = new String(user.encodedName);
        str.count = 1;
        nicks = [str];
    }
    else
    {
        // Panic?
        dd("Help! Channel user mode command with no users...?");
    }

    for (var i = 0; i < nicks.length; ++i)
    {
        e.server.sendData("MODE " + e.channel.encodedName + " " +
                          modestr.substr(0, nicks[i].count + 1) +
                          " " + nicks[i] + "\n");
    }
}

function cmdCharset(e)
{
    var pm;

    if (e.command.name == "default-charset")
    {
        pm = client.prefManager;
        msg = MSG_CURRENT_CHARSET;
    }
    else
    {
        pm = e.sourceObject.prefManager;
        msg = MSG_CURRENT_CHARSET_VIEW;
    }

    if (e.newCharset)
    {
        if (e.newCharset == "-")
        {
            pm.clearPref("charset");
        }
        else
        {
            if(!checkCharset(e.newCharset))
            {
                display(getMsg(MSG_ERR_INVALID_CHARSET, e.newCharset),
                        MT_ERROR);
                return;
            }
            pm.prefs["charset"] = e.newCharset;
        }
    }

    display(getMsg(msg, pm.prefs["charset"]));
}

function cmdCreateTabForView(e)
{
    return getTabForObject(e.view, true);
}

function cmdSync(e)
{
    var fun;

    switch (e.command.name)
    {
        case "sync-font":
            fun = function ()
                  {
                      if (view.prefs["displayHeader"])
                          view.setHeaderState(false);
                      view.changeCSS(view.getFontCSS("data"),
                                     "cz-fonts");
                      if (view.prefs["displayHeader"])
                          view.setHeaderState(true);
                  };
            break;

        case "sync-header":
            fun = function ()
                  {
                      view.setHeaderState(view.prefs["displayHeader"]);
                  };
            break;

        case "sync-motif":
            fun = function ()
                  {
                      view.changeCSS(view.prefs["motif.current"]);
                  };
            break;

        case "sync-timestamp":
            fun = function ()
                  {
                      view.changeCSS(view.getTimestampCSS("data"),
                                     "cz-timestamp-format");
                  };
            break;

        case "sync-window":
            fun = function ()
                  {
                      if (window && window.location &&
                          window.location.href != view.prefs["outputWindowURL"])
                      {
                          syncOutputFrame(view);
                      }
                  };
            break;

        case "sync-log":
            fun = function ()
                  {
                      if (view.prefs["log"] ^ Boolean(view.logFile))
                      {
                          if (view.prefs["log"])
                              client.openLogFile(view);
                          else
                              client.closeLogFile(view);
                      }
                  };
            break;
    }

    var view = e.sourceObject;
    var window;
    if (("frame" in view) && view.frame)
        window = view.frame.contentWindow;

    try
    {
        fun();
    }
    catch(ex)
    {
        dd("Exception in " + e.command.name + " for " + e.sourceObject.unicodeName + ": " + ex);
    }
}

function cmdSimpleCommand(e)
{
    e.server.sendData(e.command.name + " " + e.inputData + "\n");
}

function cmdSquery(e)
{
    var data;

    if (e.commands)
        data = "SQUERY " + e.service + " :" + e.commands + "\n";
    else
        data = "SQUERY " + e.service + "\n";

    e.server.sendData(data);
}

function cmdStatus(e)
{
    function serverStatus (s)
    {
        if (!s.connection.isConnected)
        {
            display(getMsg(MSG_NOT_CONNECTED, s.parent.name), MT_STATUS);
            return;
        }

        var serverType = (s.parent.primServ == s) ? MSG_PRIMARY : MSG_SECONDARY;
        display(getMsg(MSG_CONNECTION_INFO,
                       [s.parent.name, s.me.unicodeName, s.connection.host,
                        s.connection.port, serverType]),
                MT_STATUS);

        var connectTime = Math.floor((new Date() - s.connection.connectDate) /
                                     1000);
        connectTime = formatDateOffset(connectTime);

        var pingTime = ("lastPing" in s) ?
            formatDateOffset(Math.floor((new Date() - s.lastPing) / 1000)) :
            MSG_NA;
        var lag = (s.lag >= 0) ? s.lag : MSG_NA;

        display(getMsg(MSG_SERVER_INFO,
                       [s.parent.name, connectTime, pingTime, lag]),
                MT_STATUS);
    }

    function channelStatus (c)
    {
        var cu;
        var net = c.parent.parent.name;

        if ((cu = c.users[c.parent.me.canonicalName]))
        {
            var mtype;

            if (cu.isOp && cu.isVoice)
                mtype = MSG_VOICEOP;
            else if (cu.isOp)
                mtype = MSG_OPERATOR;
            else if (cu.isVoice)
                mtype = MSG_VOICED;
            else
                mtype = MSG_MEMBER;

            var mode = c.mode.getModeStr();
            if (!mode)
                mode = MSG_NO_MODE;

            display(getMsg(MSG_CHANNEL_INFO,
                           [net, mtype, c.unicodeName, mode,
                            (c.parent.isSecure ? "irc://" : "ircs://" )
                             + escape(net) + "/" + escape(c.encodedName) + "/"]),
                    MT_STATUS);
            display(getMsg(MSG_CHANNEL_DETAIL,
                           [net, c.unicodeName, c.getUsersLength(),
                            c.opCount, c.voiceCount]),
                    MT_STATUS);

            if (c.topic)
            {
                display(getMsg(MSG_TOPIC_INFO, [net, c.unicodeName, c.topic]),
                         MT_STATUS);
            }
            else
            {
                display(getMsg(MSG_NOTOPIC_INFO, [net, c.unicodeName]),
                        MT_STATUS);
            }
        }
        else
        {
            display(getMsg(MSG_NONMEMBER, [net, c.unicodeName]), MT_STATUS);
        }
    };

    display(client.userAgent, MT_STATUS);
    display(getMsg(MSG_USER_INFO,
                   [client.prefs["nickname"], client.prefs["username"],
                    client.prefs["desc"]]),
            MT_STATUS);

    var n, s, c;

    if (e.channel)
    {
        serverStatus(e.server);
        channelStatus(e.channel);
    }
    else if (e.network)
    {
        for (s in e.network.servers)
        {
            serverStatus(e.network.servers[s]);
            for (c in e.network.servers[s].channels)
                channelStatus (e.network.servers[s].channels[c]);
        }
    }
    else
    {
        for (n in client.networks)
        {
            for (s in client.networks[n].servers)
            {
                var server = client.networks[n].servers[s]
                    serverStatus(server);
                for (c in server.channels)
                    channelStatus(server.channels[c]);
            }
        }
    }

    display(MSG_END_STATUS, MT_STATUS);
}

function cmdHelp (e)
{
    var ary;
    ary = client.commandManager.list (e.pattern, CMD_CONSOLE);

    if (ary.length == 0)
    {
        display (getMsg(MSG_ERR_NO_COMMAND, e.pattern), MT_ERROR);
        return false;
    }

    for (var i in ary)
    {
        display (getMsg(MSG_FMT_USAGE, [ary[i].name, ary[i].usage]), MT_USAGE);
        display (ary[i].help, MT_HELP);
    }

    return true;
}

function cmdTestDisplay(e)
{
    display(MSG_TEST_HELLO, MT_HELLO);
    display(MSG_TEST_INFO, MT_INFO);
    display(MSG_TEST_ERROR, MT_ERROR);
    display(MSG_TEST_HELP, MT_HELP);
    display(MSG_TEST_USAGE, MT_USAGE);
    display(MSG_TEST_STATUS, MT_STATUS);

    if (e.server && e.server.me)
    {
        var me = e.server.me;
        var sampleUser = {TYPE: "IRCUser",
                          encodedName: "ircmonkey", canonicalName: "ircmonkey",
                          unicodeName: "IRCMonkey", viewName: "IRCMonkey",
                          host: "", name: "chatzilla"};
        var sampleChannel = {TYPE: "IRCChannel",
                             encodedName: "#mojo", canonicalName: "#mojo",
                             unicodeName: "#Mojo", viewName: "#Mojo"};

        function test (from, to)
        {
            var fromText = (from != me) ? from.TYPE + " ``" + from.name + "''" :
                MSG_YOU;
            var toText   = (to != me) ? to.TYPE + " ``" + to.name + "''" :
                MSG_YOU;

            display (getMsg(MSG_TEST_PRIVMSG, [fromText, toText]),
                     "PRIVMSG", from, to);
            display (getMsg(MSG_TEST_ACTION, [fromText, toText]),
                     "ACTION", from, to);
            display (getMsg(MSG_TEST_NOTICE, [fromText, toText]),
                     "NOTICE", from, to);
        }

        test (sampleUser, me); /* from user to me */
        test (me, sampleUser); /* me to user */

        display(MSG_TEST_URL, "PRIVMSG", sampleUser, me);
        display(MSG_TEST_STYLES, "PRIVMSG", sampleUser, me);
        display(MSG_TEST_EMOTICON, "PRIVMSG", sampleUser, me);
        display(MSG_TEST_RHEET, "PRIVMSG", sampleUser, me);
        display(unescape(MSG_TEST_CTLCHR), "PRIVMSG", sampleUser, me);
        display(unescape(MSG_TEST_COLOR), "PRIVMSG", sampleUser, me);
        display(MSG_TEST_QUOTE, "PRIVMSG", sampleUser, me);

        if (e.channel)
        {
            test (sampleUser, sampleChannel); /* user to channel */
            test (me, sampleChannel);         /* me to channel */
            display(MSG_TEST_TOPIC, "TOPIC", sampleUser, sampleChannel);
            display(MSG_TEST_JOIN, "JOIN", sampleUser, sampleChannel);
            display(MSG_TEST_PART, "PART", sampleUser, sampleChannel);
            display(MSG_TEST_KICK, "KICK", sampleUser, sampleChannel);
            display(MSG_TEST_QUIT, "QUIT", sampleUser, sampleChannel);
            display(getMsg(MSG_TEST_STALK, me.unicodeName),
                    "PRIVMSG", sampleUser, sampleChannel);
            display(MSG_TEST_STYLES, "PRIVMSG", me, sampleChannel);
        }
    }
}

function cmdNetwork(e)
{
    if (!(e.networkName in client.networks))
    {
        display (getMsg(MSG_ERR_UNKNOWN_NETWORK, e.networkName), MT_ERROR);
        return;
    }

    var network = client.networks[e.networkName];

    if (!("messages" in network))
        network.displayHere(getMsg(MSG_NETWORK_OPENED, network.name));

    dispatch("set-current-view", { view: network });
}

function cmdNetworks(e)
{
    const ns = "http://www.w3.org/1999/xhtml";

    var span = document.createElementNS(ns, "html:span");

    span.appendChild(newInlineText(MSG_NETWORKS_HEADA));

    var netnames = keys(client.networks).sort();
    var lastname = netnames[netnames.length - 1];

    for (n in netnames)
    {
        var net = client.networks[netnames[n]];
        var a = document.createElementNS(ns, "html:a");
        /* Test for an all-SSL network */
        var isSecure = true;
        for (var s in client.networks[netnames[n]].serverList)
        {
            if (!client.networks[netnames[n]].serverList[s].isSecure)
            {
                isSecure = false;
                break;
            }
        }
        a.setAttribute("class", "chatzilla-link");
        a.setAttribute("href", (isSecure ? "ircs://" : "irc://") + net.canonicalName);
        var t = newInlineText(net.unicodeName);
        a.appendChild(t);
        span.appendChild(a);
        if (netnames[n] != lastname)
            span.appendChild(newInlineText (MSG_COMMASP));
    }

    span.appendChild(newInlineText(MSG_NETWORKS_HEADB));

    display(span, MT_INFO);
}

function cmdServer(e)
{
    var ary = e.hostname.match(/^(.*):(\d+)$/);
    if (ary)
    {
        // Foolish user obviously hasn't read the instructions, but we're nice.
        e.password = e.port;
        e.port = ary[2];
        e.hostname = ary[1];
    }

    var name = e.hostname.toLowerCase();

    if (!e.port)
        e.port = 6667;
    else if (e.port != 6667)
        name += ":" + e.port;

    if (!(name in client.networks))
    {
        /* if there wasn't already a network created for this server,
         * make one. */
        client.addNetwork(name, [{name: e.hostname, port: e.port,
                                        password: e.password}]);
    }
    else if (e.password)
    {
        /* update password on existing server. */
        client.networks[name].serverList[0].password = e.password;
    }

    return client.connectToNetwork(name, false);
}

function cmdSSLServer(e)
{
    var ary = e.hostname.match(/^(.*):(\d+)$/);
    if (ary)
    {
        // Foolish user obviously hasn't read the instructions, but we're nice.
        e.password = e.port;
        e.port = ary[2];
        e.hostname = ary[1];
    }

    var name = e.hostname.toLowerCase();

    if (!e.port)
        e.port = 9999;
    if (e.port != 6667)
        name += ":" + e.port;

    if (!(name in client.networks))
    {
        /* if there wasn't already a network created for this server,
         * make one. */
        client.addNetwork(name, [{name: e.hostname, port: e.port,
                                  password: e.password, isSecure: true}]);
    }
    else if (e.password)
    {
        /* update password on existing server. */
        client.networks[name].serverList[0].password = e.password;
    }

    return client.connectToNetwork(name, true);
}


function cmdQuit(e)
{
    if (!e.reason)
        e.reason = client.userAgent;

    client.quit(e.reason);
    window.close();
}

function cmdQuitMozilla(e)
{
    client.quit(e.reason);
    goQuitApplication();
}

function cmdDisconnect(e)
{
    if (typeof e.reason != "string")
        e.reason = client.userAgent;

    e.network.quit(e.reason);
}

function cmdDeleteView(e)
{
    if (!e.view)
        e.view = e.sourceObject;

    if (e.view.TYPE == "IRCChannel" && e.view.active)
        e.view.part();
    if (e.view.TYPE == "IRCDCCChat" && e.view.active)
        e.view.disconnect();

    if (client.viewsArray.length < 2)
    {
        display(MSG_ERR_LAST_VIEW, MT_ERROR);
        return;
    }

    var tb = getTabForObject(e.view);
    if (tb)
    {
        var i = deleteTab (tb);
        if (i != -1)
        {
            if (e.view.logFile)
            {
                e.view.logFile.close();
                e.view.logFile = null;
            }
            delete e.view.messageCount;
            delete e.view.messages;
            client.deck.removeChild(e.view.frame);
            delete e.view.frame;

            var oldView = client.currentObject;
            if (client.currentObject == e.view)
            {
                if (i >= client.viewsArray.length)
                    i = client.viewsArray.length - 1;
                oldView = client.viewsArray[i].source
            }
            client.currentObject = null;
            oldView.dispatch("set-current-view", { view: oldView });
        }
    }
}

function cmdHideView(e)
{
    if (!e.view)
        e.view = e.sourceObject;

    var tb = getTabForObject(e.view);

    if (tb)
    {
        var i = deleteTab (tb);
        if (i != -1)
        {
            client.deck.removeChild(e.view.frame);
            delete e.view.frame;

            var oldView = client.currentObject;
            if (client.currentObject == e.view)
            {
                if (i >= client.viewsArray.length)
                    i = client.viewsArray.length - 1;
                oldView = client.viewsArray[i].source
            }
            client.currentObject = null;
            oldView.dispatch("set-current-view", { view: oldView });
        }
    }
}

function cmdClearView(e)
{
    if (!e.view)
        e.view = e.sourceObject;

    e.view.messages = null;
    e.view.messageCount = 0;

    e.view.displayHere(MSG_MESSAGES_CLEARED);

    syncOutputFrame(e.view);
}

function cmdNames(e)
{
    if (e.channelName)
    {
        e.channel = new CIRCChannel(e.server, e.channelName);
    }
    else
    {
        if (!e.channel)
        {
            display(getMsg(MSG_ERR_REQUIRED_PARAM, "channel-name"), MT_ERROR);
            return;
        }
    }

    e.channel.pendingNamesReply = true;
    e.server.sendData("NAMES " + e.channel.encodedName + "\n");
}

function cmdTogglePref (e)
{
    var state = !client.prefs[e.prefName];
    client.prefs[e.prefName] = state;
    feedback(e, getMsg (MSG_FMT_PREF,
                        [e.prefName, state ? MSG_VAL_ON : MSG_VAL_OFF]));
}

function cmdToggleUI(e)
{
    var ids = new Array();

    switch (e.thing)
    {
        case "tabstrip":
            ids = ["view-tabs"];
            break;

        case "userlist":
            ids = ["main-splitter", "user-list-box"];
            break;

        case "header":
            client.currentObject.prefs["displayHeader"] =
                !client.currentObject.prefs["displayHeader"];
            return;

        case "status":
            ids = ["status-bar"];
            break;

        default:
            ASSERT (0,"Unknown element ``" + e.thing +
                    "'' passed to onToggleVisibility.");
            return;
    }

    var newState;
    var elem = document.getElementById(ids[0]);
    var sourceObject = e.sourceObject;

    if (elem.getAttribute("collapsed") == "true")
    {
        if (e.thing == "userlist")
        {
            if (sourceObject.TYPE == "IRCChannel")
            {
                client.rdf.setTreeRoot("user-list",
                                       sourceObject.getGraphResource());
            }
            else
            {
                client.rdf.setTreeRoot("user-list", client.rdf.resNullChan);
            }
        }

        newState = "false";
    }
    else
    {
        newState = "true";
    }

    for (var i in ids)
    {
        elem = document.getElementById(ids[i]);
        elem.setAttribute ("collapsed", newState);
    }

    updateTitle();
    dispatch("focus-input");
}

function cmdCommands(e)
{
    display(MSG_COMMANDS_HEADER);

    var matchResult = client.commandManager.listNames(e.pattern, CMD_CONSOLE);
    matchResult = matchResult.join(MSG_COMMASP);

    if (e.pattern)
        display(getMsg(MSG_MATCHING_COMMANDS, [e.pattern, matchResult]));
    else
        display(getMsg(MSG_ALL_COMMANDS, matchResult));
}

function cmdAttach(e)
{
    if (e.ircUrl.search(/ircs?:\/\//i) != 0)
        e.ircUrl = "irc://" + e.ircUrl;

    var parsedURL = parseIRCURL(e.ircUrl);
    if (!parsedURL)
    {
        display(getMsg(MSG_ERR_BAD_IRCURL, e.ircUrl), MT_ERROR);
        return;
    }

    gotoIRCURL(e.ircUrl);
}

function cmdMe(e)
{
    if (!("act" in e.sourceObject))
    {
        display(getMsg(MSG_ERR_IMPROPER_VIEW, "me"), MT_ERROR);
        return;
    }

    var msg = filterOutput(e.action, "ACTION", "ME!");
    e.sourceObject.display(msg, "ACTION", "ME!", e.sourceObject);
    e.sourceObject.act(msg);
}

function cmdDescribe(e)
{
    var target = e.server.addTarget(e.target);

    var msg = filterOutput(e.action, "ACTION", "ME!");
    e.sourceObject.display(msg, "ACTION", "ME!", target);
    target.act(msg);
}

function cmdMotif(e)
{
    var pm;
    var msg;

    if (e.command.name == "channel-motif")
    {
        pm = e.channel.prefManager;
        msg = MSG_CURRENT_CSS_CHAN;
    }
    else if (e.command.name == "network-motif")
    {
        pm = e.network.prefManager;
        msg = MSG_CURRENT_CSS_NET;
    }
    else if (e.command.name == "user-motif")
    {
        pm = e.user.prefManager;
        msg = MSG_CURRENT_CSS_USER;
    }
    else
    {
        pm = client.prefManager;
        msg = MSG_CURRENT_CSS;
    }

    if (e.motif)
    {
        if (e.motif == "-")
        {
            // delete local motif in favor of default
            pm.clearPref("motif.current");
            e.motif = pm.prefs["motif.current"];
        }
        else if (e.motif.search(/\.css$/i) != -1)
        {
            // specific css file
            pm.prefs["motif.current"] = e.motif;
        }
        else
        {
            // motif alias
            var prefName = "motif." + e.motif;
            if (client.prefManager.isKnownPref(prefName))
            {
                e.motif = client.prefManager.prefs[prefName];
            }
            else
            {
                display(getMsg(MSG_ERR_UNKNOWN_MOTIF, e.motif), MT_ERROR);
                return;
            }

            pm.prefs["motif.current"] = e.motif;
        }

    }

    display (getMsg(msg, pm.prefs["motif.current"]));
}

function cmdList(e)
{
    e.network.list = new Array();
    e.network.list.regexp = null;
    if (!e.channelName || !e.inputData)
        e.channelName = "";
    e.server.sendData("LIST " + fromUnicode(e.channelName, e.network) + "\n");
}

function cmdListPlugins(e)
{
    function listPlugin(plugin, i)
    {
        var enabled;
        if ("disablePlugin" in plugin.scope)
            enabled = plugin.enabled;
        else
            enabled = MSG_ALWAYS;

        display(getMsg(MSG_FMT_PLUGIN1, [i, plugin.url]));
        display(getMsg(MSG_FMT_PLUGIN2,
                       [plugin.id, plugin.version, enabled, plugin.status]));
        display(getMsg(MSG_FMT_PLUGIN3, plugin.description));
    }

    if (e.plugin)
    {
        listPlugin(e.plugin, 0);
        return;
    }

    if (client.plugins.length > 0)
    {
        for (var i = 0; i < client.plugins.length; ++i)
            listPlugin(client.plugins[i], i);
    }
    else
    {
        display(MSG_NO_PLUGINS);
    }
}

function cmdRlist(e)
{
    e.network.list = new Array();
    e.network.list.regexp = new RegExp(e.regexp, "i");
    e.server.sendData ("list\n");
}

function cmdReloadUI(e)
{
    if (!("getConnectionCount" in client) ||
        client.getConnectionCount() == 0)
    {
        window.location.href = window.location.href;
    }
}

function cmdQuery(e)
{
    // We'd rather *not* trigger the user.start event this time.
    blockEventSounds("user", "start");
    var user = openQueryTab(e.server, e.nickname);
    dispatch("set-current-view", { view: user });

    if (e.message)
    {
        e.message = filterOutput(e.message, "PRIVMSG", "ME!");
        user.display(e.message, "PRIVMSG", "ME!", user);
        user.say(e.message, e.sourceObject);
    }

    return user;
}

function cmdSay(e)
{
    if (!("say" in e.sourceObject))
    {
        display(getMsg(MSG_ERR_IMPROPER_VIEW, "say"), MT_ERROR);
        return;
    }

    var msg = filterOutput(e.message, "PRIVMSG", "ME!");
    e.sourceObject.display(msg, "PRIVMSG", "ME!", e.sourceObject);
    e.sourceObject.say(msg, e.sourceObject);
}

function cmdMsg(e)
{
    var target = e.server.addTarget(e.nickname);

    var msg = filterOutput(e.message, "PRIVMSG", "ME!");
    e.sourceObject.display(msg, "PRIVMSG", "ME!", target);
    target.say(msg, target);
}

function cmdNick(e)
{
    if (e.server)
        e.server.changeNick(e.nickname);

    if (e.network)
        e.network.prefs["nickname"] = e.nickname;
    else
        client.prefs["nickname"] = e.nickname;
}

function cmdQuote(e)
{
    e.server.sendData(fromUnicode(e.ircCommand) + "\n", e.sourceObject);
}

function cmdEval(e)
{
    var sourceObject = e.sourceObject;

    try
    {
        sourceObject.doEval = function (__s) { return eval(__s); }
        sourceObject.display(e.expression, MT_EVALIN);
        var rv = String(sourceObject.doEval (e.expression));
        sourceObject.display (rv, MT_EVALOUT);

    }
    catch (ex)
    {
        sourceObject.display (String(ex), MT_ERROR);
    }
}

function cmdFocusInput(e)
{
    const WWATCHER_CTRID = "@mozilla.org/embedcomp/window-watcher;1";
    const nsIWindowWatcher = Components.interfaces.nsIWindowWatcher;

    var watcher =
        Components.classes[WWATCHER_CTRID].getService(nsIWindowWatcher);
    if (watcher.activeWindow == window)
        client.input.focus();
    else
        document.commandDispatcher.focusedElement = client.input;
}

function cmdGotoURL(e)
{
    if (e.url.search(/^ircs?:/i) == 0)
    {
        gotoIRCURL(e.url);
        return;
    }

    if (e.url.search(/^x-cz-command:/i) == 0)
    {
        var ary = e.url.match(/^x-cz-command:(.*)$/i);
        e.sourceObject.frame.contentWindow.location = "javascript:void(view.dispatch('" +
                                                      decodeURI(ary[1]) + "'))";
        return;
    }

    if (e.command.name == "goto-url-newwin")
    {
        openTopWin(e.url);
        return;
    }

    var window = getWindowByType("navigator:browser");

    if (!window)
    {
        openTopWin(e.url);
        return;
    }

    if (e.command.name == "goto-url-newtab")
    {
        window.openNewTabWith(e.url, false, false);
        return;
    }

    var location = window.content.document.location;
    if (location.href.indexOf("chrome://chatzilla/content/") == 0)
    {
        // don't replace chatzilla running in a tab
        openTopWin(e.url);
    }

    location.href = e.url;
}

function cmdCTCP(e)
{
    var obj = e.server.addTarget(e.target);
    obj.ctcp(e.code, e.params);
}

function cmdJoin(e)
{
    if (!("charset" in e))
    {
        e.charset = null;
    }
    else if (e.charset && !checkCharset(e.charset))
    {
        display (getMsg(MSG_ERR_INVALID_CHARSET, e.charset), MT_ERROR);
        return null;
    }

    if (e.channelName && (e.channelName.search(",") != -1))
    {
        // We can join multiple channels! Woo!
        var chan;
        var chans = e.channelName.split(",");
        var keys = [];
        if (e.key)
            keys = e.key.split(",");
        for (var c in chans)
        {
            chan = dispatch("join", { charset: e.charset,
                                      channelName: chans[c],
                                      key: keys.shift() });
        }
        return chan;
    }
    if (!e.channelName)
    {
        var channel = e.channel;
        if (!channel)
        {
            display(getMsg(MSG_ERR_REQUIRED_PARAM, "channel"), MT_ERROR);
            return null;
        }

        e.channelName = channel.unicodeName;
        if (channel.mode.key)
            e.key = channel.mode.key
    }

    if (arrayIndexOf(e.server.channelTypes, e.channelName[0]) == -1)
        e.channelName = e.server.channelTypes[0] + e.channelName;

    var charset = e.charset ? e.charset : e.network.prefs["charset"];
    e.channel = e.server.addChannel(e.channelName, charset);
    if (e.charset)
        e.channel.prefs["charset"] = e.charset;

    e.channel.join(e.key);

    /* !-channels are "safe" channels, and get a server-generated prefix. For
     * this reason, we shouldn't do anything client-side until the server
     * replies (since the reply will have the appropriate prefix). */
    if (e.channelName[0] != "!")
    {
        if (!("messages" in e.channel))
        {
            e.channel.displayHere(getMsg(MSG_CHANNEL_OPENED,
                                         e.channel.unicodeName), MT_INFO);
        }

        dispatch("set-current-view", { view: e.channel });
    }

    return e.channel;
}

function cmdLeave(e)
{
    if (!e.server)
    {
        display(MSG_ERR_IMPROPER_VIEW, MT_ERROR);
        return;
    }

    // FIXME: Smart param handling... //
    if (e.channelName)
    {
        if (arrayIndexOf(e.server.channelTypes, e.channelName[0]) == -1)
        {
            // No valid prefix character. Check they really meant a channel...
            var valid = false;
            for (var i = 0; i < e.server.channelTypes.length; i++)
            {
                // Hmm, not ideal...
                var chan = e.server.getChannel(e.server.channelTypes[i] +
                                               e.channelName);
                if (chan)
                {
                    // Yes! They just missed that single character.
                    e.channel = chan;
                    valid = true;
                    break;
                }
            }

            // We can only let them get away here if we've got a channel.
            if (!valid)
            {
                if (e.channel)
                {
                    e.reason = e.channelName + " " + e.reason;
                }
                else
                {
                    display("Huh?", MT_ERROR);
                    return;
                }
            }
        }
        else
        {
            // Valid prefix, so get real channel (if it exists...).
            e.channel = e.server.getChannel(e.channelName);
        }
    }

    /* If it's not active, we're not actually in it, even though the view is
     * still here.
     */
    if (e.channel && e.channel.active)
    {
        if (e.noDelete)
            e.channel.noDelete = true;

        if (!e.reason)
            e.reason = "";

        e.server.sendData("PART " + e.channel.encodedName + " :" +
                          fromUnicode(e.reason, e.channel) + "\n");
    }
    else
    {
        if (!e.noDelete && client.prefs["deleteOnPart"])
            e.channel.dispatch("delete");
    }
}

function cmdLoad (e)
{
    var ex;

    if (!e.scope)
        e.scope = new Object();

    if (!("plugin" in e.scope))
    {
        e.scope.plugin = { url: e.url, id: MSG_UNKNOWN, version: -1,
                           description: "", status: MSG_LOADING, enabled: true};
    }

    e.scope.plugin.scope = e.scope;

    try
    {
        var rvStr;
        var rv = rvStr = client.load(e.url, e.scope);
        if ("initPlugin" in e.scope)
            rv = rvStr = e.scope.initPlugin(e.scope);

        /* do this again, in case the plugin trashed it */
        if (!("plugin" in e.scope))
        {
            e.scope.plugin = { url: e.url, id: MSG_UNKNOWN, version: -1,
                               description: "", status: MSG_ERROR,
                               enabled: true };
        }
        else
        {
            e.scope.plugin.status = "loaded";
        }

        e.scope.plugin.scope = e.scope;

        if (typeof rv == "function")
            rvStr = "function";
        if (!("__id" in e.scope))
            e.scope.__id = null;
        if (!("__description" in e.scope))
            e.scope.__description = null;

        var i = getPluginIndexByURL(e.url);
        if (i != -1)
            client.plugins[i] = e.scope.plugin;
        else
            client.plugins.push(e.scope.plugin);

        feedback(e, getMsg(MSG_SUBSCRIPT_LOADED, [e.url, rvStr]), MT_INFO);
        return rv;
    }
    catch (ex)
    {
        display (getMsg(MSG_ERR_SCRIPTLOAD, e.url));
        display (formatException(ex), MT_ERROR);
    }

    return null;
}

function cmdWho(e)
{
    e.network.pendingWhoReply = true;
    e.server.LIGHTWEIGHT_WHO = false;
    e.server.who(e.pattern);
}

function cmdWhoIs (e)
{
    e.server.whois(e.nickname);
}

function cmdTopic(e)
{
    if (!e.newTopic)
        e.server.sendData("TOPIC " + e.channel.encodedName + "\n");
    else
        e.channel.setTopic(e.newTopic);
}

function cmdAbout(e)
{
    display(CIRCServer.prototype.VERSION_RPLY);
    display(MSG_HOMEPAGE);
}

function cmdAlias(e)
{
    var aliasDefs = client.prefs["aliases"];
    function getAlias(commandName)
    {
        for (var i = 0; i < aliasDefs.length; ++i)
        {
            var ary = aliasDefs[i].match(/^(.*?)\s*=\s*(.*)$/);
            if (ary[1] == commandName)
                return [i, ary[2]];
        }

        return null;
    };

    var ary;

    if (e.commandList == "-")
    {
        /* remove alias */
        ary = getAlias(e.aliasName);
        if (!ary)
        {
            display(getMsg(MSG_NOT_AN_ALIAS, e.aliasName), MT_ERROR);
            return;
        }

        delete client.commandManager.commands[e.aliasName];
        arrayRemoveAt(aliasDefs, ary[0]);
        aliasDefs.update();

        feedback(e, getMsg(MSG_ALIAS_REMOVED, e.aliasName));
    }
    else if (e.aliasName)
    {
        /* add/change alias */
        client.commandManager.defineCommand(e.aliasName, e.commandList);
        ary = getAlias(e.aliasName);
        if (ary)
            aliasDefs[ary[0]] = e.aliasName + " = " + e.commandList;
        else
            aliasDefs.push(e.aliasName + " = " + e.commandList);

        aliasDefs.update();

        feedback(e, getMsg(MSG_ALIAS_CREATED, [e.aliasName, e.commandList]));
    }
    else
    {
        /* list aliases */
        if (aliasDefs.length == 0)
        {
            display(MSG_NO_ALIASES);
        }
        else
        {
            for (var i = 0; i < aliasDefs.length; ++i)
            {
                ary = aliasDefs[i].match(/^(.*?)\s*=\s*(.*)$/);
                if (ary)
                    display(getMsg(MSG_FMT_ALIAS, [ary[1], ary[2]]));
                else
                    display(getMsg(MSG_ERR_BADALIAS, aliasDefs[i]));
            }
        }
    }
}

function cmdAway(e)
{
    if (e.reason)
    {
        /* going away */
        if (client.prefs["awayNick"])
            e.server.sendData("NICK " + e.network.prefs["awayNick"] + "\n");

        e.server.sendData ("AWAY :" + fromUnicode(e.reason, e.network) + "\n");
    }
    else
    {
        /* returning */
        if (client.prefs["awayNick"])
            e.server.sendData("NICK " + e.network.prefs["nickname"] + "\n");

        e.server.sendData ("AWAY\n");
    }
}

function cmdOpenAtStartup(e)
{
    var url = e.sourceObject.getURL();
    var list = client.prefs["initialURLs"];
    var index = arrayIndexOf(list, url);

    if (e.toggle == null)
    {
        if (index == -1)
            display(getMsg(MSG_STARTUP_NOTFOUND, url));
        else
            display(getMsg(MSG_STARTUP_EXISTS, url));
        return;
    }

    e.toggle = getToggle(e.toggle, (index != -1));

    if (e.toggle)
    {
        // yes, please open at startup
        if (index == -1)
        {
            list.push(url);
            list.update();
            display(getMsg(MSG_STARTUP_ADDED, url));
        }
        else
        {
            display(getMsg(MSG_STARTUP_EXISTS, url));
        }
    }
    else
    {
        // no, please don't open at startup
        if (index != -1)
        {
            arrayRemoveAt(list, index);
            list.update();
            display(getMsg(MSG_STARTUP_REMOVED, url));
        }
        else
        {
            display(getMsg(MSG_STARTUP_NOTFOUND, url));
        }
    }
}

function cmdPing (e)
{
    e.network.dispatch("ctcp", { target: e.target, code: "PING" });
}

function cmdPref (e)
{
    var msg;
    var pm;

    if (e.command.name == "network-pref")
    {
        pm = e.network.prefManager;
        msg = MSG_FMT_NETPREF;
    }
    else if (e.command.name == "channel-pref")
    {
        pm = e.channel.prefManager;
        msg = MSG_FMT_CHANPREF;
    }
    else if (e.command.name == "user-pref")
    {
        pm = e.user.prefManager;
        msg = MSG_FMT_USERPREF;
    }
    else
    {
        pm = client.prefManager;
        msg = MSG_FMT_PREF;
    }

    var ary = pm.listPrefs(e.prefName);
    if (ary.length == 0)
    {
        display (getMsg(MSG_ERR_UNKNOWN_PREF, [e.prefName]),
                 MT_ERROR);
        return false;
    }

    if (e.prefValue == "-")
        e.deletePref = true;

    if (e.deletePref)
    {
        try
        {
            pm.clearPref(e.prefName);
        }
        catch (ex)
        {
            // ignore exception generated by clear of nonexistant pref
            if (!("result" in ex) ||
                ex.result != Components.results.NS_ERROR_UNEXPECTED)
            {
                throw ex;
            }
        }

        var prefValue = pm.prefs[e.prefName];
        feedback (e, getMsg(msg, [e.prefName, pm.prefs[e.prefName]]));
        return true;
    }

    if (e.prefValue)
    {
        var r = pm.prefRecords[e.prefName];
        var type;

        if (typeof r.defaultValue == "function")
            type = typeof r.defaultValue(e.prefName);
        else
            type = typeof r.defaultValue;

        switch (type)
        {
            case "number":
                e.prefValue = Number(e.prefValue);
                break;
            case "boolean":
                e.prefValue = (e.prefValue.toLowerCase() == "true");
                break;
            case "string":
                break;
            default:
                if (r.defaultValue instanceof Array)
                    e.prefValue = pm.stringToArray(e.prefValue);
                else
                    e.prefValue = e.prefValue.join("; ");
                break;
        }

        pm.prefs[e.prefName] = e.prefValue;
        feedback (e, getMsg(msg, [e.prefName, e.prefValue]));
    }
    else
    {
        for (var i = 0; i < ary.length; ++i)
        {
            var value;
            if (pm.prefs[ary[i]] instanceof Array)
                value = pm.prefs[ary[i]].join("; ");
            else
                value = pm.prefs[ary[i]];

            feedback(e, getMsg(msg, [ary[i], value]));
        }
    }

    return true;
}

function cmdVersion(e)
{
    e.network.dispatch("ctcp", { target: e.nickname, code: "VERSION"});
}

function cmdEcho(e)
{
    display(e.message);
}

function cmdInvite(e)
{
    var channel;

    if (!e.channelName)
    {
        channel = e.channel;
    }
    else
    {
        channel = e.server.getChannel(e.channelName);
        if (!channel)
        {
            display(getMsg(MSG_ERR_UNKNOWN_CHANNEL, e.channelName), MT_ERROR);
            return;
        }
    }

    channel.invite(e.nickname);
}

function cmdKick(e)
{
    if (!e.user)
        e.user = e.channel.getUser(e.nickname);

    if (!e.user)
    {
        display(getMsg(MSG_ERR_UNKNOWN_USER, e.nickname), MT_ERROR);
        return;
    }

    if (e.command.name == "kick-ban")
    {
        var hostmask = e.user.host.replace(/^[^.]+/, "*");
        e.server.sendData("MODE " + e.channel.encodedName + " +b *!" +
                          e.user.name + "@" + hostmask + "\n");
    }

    e.user.kick(e.reason);
}

function cmdClient(e)
{
    dispatch("create-tab-for-view", { view: client });

    if (!client.messages)
    {
        client.display(MSG_CLIENT_OPENED);
        dispatch("set-current-view", { view: client });
        client.display(MSG_WELCOME, "HELLO");
        dispatch("networks");
        dispatch("commands");
    } else {
        dispatch("set-current-view", { view: client });
    }
}

function cmdNotify(e)
{
    var net = e.network;

    if (!e.nickname)
    {
        if (net.prefs["notifyList"].length > 0)
        {
            /* delete the lists and force a ISON check, this will
             * print the current online/offline status when the server
             * responds */
            delete net.onList;
            delete net.offList;
            onNotifyTimeout();
        }
        else
        {
            display(MSG_NO_NOTIFY_LIST);
        }
    }
    else
    {
        var adds = new Array();
        var subs = new Array();

        for (var i in e.nicknameList)
        {
            var nickname = e.server.toLowerCase(e.nicknameList[i]);
            var idx = arrayIndexOf (net.prefs["notifyList"], nickname);
            if (idx == -1)
            {
                net.prefs["notifyList"].push (nickname);
                adds.push(nickname);
            }
            else
            {
                arrayRemoveAt (net.prefs["notifyList"], idx);
                subs.push(nickname);
            }
        }
        net.prefs["notifyList"].update();

        var msgname;

        if (adds.length > 0)
        {
            msgname = (adds.length == 1) ? MSG_NOTIFY_ADDONE :
                                           MSG_NOTIFY_ADDSOME;
            display(getMsg(msgname, arraySpeak(adds)));
        }

        if (subs.length > 0)
        {
            msgname = (subs.length == 1) ? MSG_NOTIFY_DELONE :
                                           MSG_NOTIFY_DELSOME;
            display(getMsg(msgname, arraySpeak(subs)));
        }

        delete net.onList;
        delete net.offList;
        onNotifyTimeout();
    }
}

function cmdStalk(e)
{
    if (!e.text)
    {
        if (list.length == 0)
            display(MSG_NO_STALK_LIST);
        else
            display(getMsg(MSG_STALK_LIST, list.join(MSG_COMMASP)));
        return;
    }

    client.prefs["stalkWords"].push(e.text);
    client.prefs["stalkWords"].update();

    display(getMsg(MSG_STALK_ADD, e.text));
}

function cmdUnstalk(e)
{
    e.text = e.text.toLowerCase();
    var list = client.prefs["stalkWords"];

    for (i in list)
    {
        if (list[i].toLowerCase() == e.text)
        {
            list.splice(i, 1);
            list.update();
            display(getMsg(MSG_STALK_DEL, e.text));
            return;
        }
    }

    display(getMsg(MSG_ERR_UNKNOWN_STALK, e.text), MT_ERROR);
}

function cmdUsermode(e)
{
    if (e.newMode)
    {
        if (e.sourceObject.network)
            e.sourceObject.network.prefs["usermode"] = e.newMode;
        else
            client.prefs["usermode"] = e.newMode;
    }
    else
    {
        if (e.server && e.server.isConnected)
        {
            e.server.sendData("mode " + e.server.me.encodedName + "\n");
        }
        else
        {
            var prefs;

            if (e.network)
                prefs = e.network.prefs;
            else
                prefs = client.prefs;

            display(getMsg(MSG_USER_MODE,
                           [prefs["nickname"], prefs["usermode"]]),
                    MT_MODE);
        }
    }
}

function cmdLog(e)
{
    var view = e.sourceObject;

    if (e.state != null)
    {
        e.state = getToggle(e.state, view.prefs["log"])
        view.prefs["log"] = e.state;
    }
    else
    {
        if (view.prefs["log"])
            display(getMsg(MSG_LOGGING_ON, view.logFile.path));
        else
            display(MSG_LOGGING_OFF);
    }
}

function cmdSupports(e)
{
    var server = e.server;
    var data = server.supports;

    if ("channelTypes" in server)
        display(getMsg(MSG_SUPPORTS_CHANTYPES,
                       server.channelTypes.join(MSG_COMMASP)));
    if ("channelModes" in server)
    {
        display(getMsg(MSG_SUPPORTS_CHANMODESA,
                       server.channelModes.a.join(MSG_COMMASP)));
        display(getMsg(MSG_SUPPORTS_CHANMODESB,
                       server.channelModes.b.join(MSG_COMMASP)));
        display(getMsg(MSG_SUPPORTS_CHANMODESC,
                       server.channelModes.c.join(MSG_COMMASP)));
        display(getMsg(MSG_SUPPORTS_CHANMODESD,
                       server.channelModes.d.join(MSG_COMMASP)));
    }

    if ("userModes" in server)
    {
        var list = new Array();
        for (var m in server.userModes)
        {
            list.push(getMsg(MSG_SUPPORTS_USERMODE, [
                                                      server.userModes[m].mode,
                                                      server.userModes[m].symbol
                                                    ]));
        }
        display(getMsg(MSG_SUPPORTS_USERMODES, list.join(MSG_COMMASP)));
    }

    var listB1 = new Array();
    var listB2 = new Array();
    var listN = new Array();
    for (var k in data)
    {
        if (typeof data[k] == "boolean")
        {
            if (data[k])
                listB1.push(k);
            else
                listB2.push(k);
        }
        else
        {
            listN.push(getMsg(MSG_SUPPORTS_MISCOPTION, [ k, data[k] ] ));
        }
    }
    listB1.sort();
    listB2.sort();
    listN.sort();
    display(getMsg(MSG_SUPPORTS_FLAGSON, listB1.join(MSG_COMMASP)));
    display(getMsg(MSG_SUPPORTS_FLAGSOFF, listB2.join(MSG_COMMASP)));
    display(getMsg(MSG_SUPPORTS_MISCOPTIONS, listN.join(MSG_COMMASP)));
}

function cmdDoCommand(e)
{
    if (e.cmdName == "cmd_mozillaPrefs")
    {
        goPreferences('navigator',
                      'chrome://chatzilla/content/prefpanel/pref-irc.xul',
                      'navigator');
    }
    else if (e.cmdName == "cmd_chatzillaPrefs")
    {
        window.openDialog('chrome://chatzilla/content/config.xul', '',
                          'chrome,resizable,dialog=no', window);
    }
    else
    {
        doCommand(e.cmdName);
    }
}

function cmdTimestamps(e)
{
    var view = e.sourceObject;

    if (e.toggle != null)
    {
        e.toggle = getToggle(e.toggle, view.prefs["timestamps"])
        view.prefs["timestamps"] = e.toggle;
    }
    else
    {
        display(getMsg(MSG_FMT_PREF, ["timestamps",
                                      view.prefs["timestamps"]]));
    }
}

function cmdTimestampFormat(e)
{
    var view = e.sourceObject;

    if (e.format != null)
    {
        view.prefs["timestampFormat"] = e.format;
    }
    else
    {
        display(getMsg(MSG_FMT_PREF, ["timestampFormat",
                                      view.prefs["timestampFormat"]]));
    }
}

function cmdSetCurrentView(e)
{
    setCurrentObject(e.view);
}

function cmdIgnore(e)
{
    if (("mask" in e) && e.mask)
    {
        e.mask = e.server.toLowerCase(e.mask);

        if (e.command.name == "ignore")
        {
            if (e.network.ignore(e.mask))
                display(getMsg(MSG_IGNORE_ADD, e.mask));
            else
                display(getMsg(MSG_IGNORE_ADDERR, e.mask));
        }
        else
        {
            if (e.network.unignore(e.mask))
                display(getMsg(MSG_IGNORE_DEL, e.mask));
            else
                display(getMsg(MSG_IGNORE_DELERR, e.mask));
        }
    }
    else
    {
        var list = new Array();
        for (var m in e.network.ignoreList)
            list.push(m);
        if (list.length == 0)
            display(MSG_IGNORE_LIST_1);
        else
            display(getMsg(MSG_IGNORE_LIST_2, arraySpeak(list)));
    }
}

function cmdFont(e)
{
    var view = client;
    var pref, val, pVal;

    if (e.command.name == "font-family")
    {
        pref = "font.family";
        val = e.font;

        // Save new value, then display pref value.
        if (val)
            view.prefs[pref] = val;

        display(getMsg(MSG_FONTS_FAMILY_FMT, view.prefs[pref]));
    }
    else if (e.command.name == "font-size")
    {
        pref = "font.size";
        val = e.fontSize;

        // Ok, we've got an input.
        if (val)
        {
            // Get the current value, use user's default if needed.
            pVal = view.prefs[pref];
            if (pVal == 0)
                pVal = getDefaultFontSize();

            // Handle user's input...
            switch(val) {
                case "default":
                    val = 0;
                    break;

                case "small":
                    val = getDefaultFontSize() - 2;
                    break;

                case "medium":
                    val = getDefaultFontSize();
                    break;

                case "large":
                    val = getDefaultFontSize() + 2;
                    break;

                case "smaller":
                    val = pVal - 2;
                    break;

                case "bigger":
                    val = pVal + 2;
                    break;

                default:
                    val = Number(val);
            }
            // Save the new value.
            view.prefs[pref] = val;
        }

        // Show the user what the pref is set to.
        if (view.prefs[pref] == 0)
            display(MSG_FONTS_SIZE_DEFAULT);
        else
            display(getMsg(MSG_FONTS_SIZE_FMT, view.prefs[pref]));
    }
    else if (e.command.name == "font-family-other")
    {
        val = prompt(MSG_FONTS_FAMILY_PICK, view.prefs["font.family"]);
        if (!val)
            return;

        dispatch("font-family", { font: val });
    }
    else if (e.command.name == "font-size-other")
    {
        pVal = view.prefs["font.size"];
        if (pVal == 0)
            pVal = getDefaultFontSize();

        val = prompt(MSG_FONTS_SIZE_PICK, pVal);
        if (!val)
            return;

        dispatch("font-size", { fontSize: val });
    }
}

function cmdDCCChat(e)
{
    if (!jsenv.HAS_SERVER_SOCKETS)
        return display(MSG_DCC_NOT_POSSIBLE);
    if (!client.prefs["dcc.enabled"])
        return display(MSG_DCC_NOT_ENABLED);

    if (!e.nickname && !e.user)
        return display(MSG_DCC_ERR_NOUSER);

    var user;
    if (e.nickname)
        user = e.server.addUser(e.nickname);
    else
        user = e.server.addUser(e.user.unicodeName);

    var u = client.dcc.addUser(user);
    var c = client.dcc.addChat(u, client.dcc.getNextPort());
    c.request();

    client.munger.entries[".inline-buttons"].enabled = true;
    var cmd = getMsg(MSG_DCC_COMMAND_CANCEL, "dcc-close " + c.id);
    display(getMsg(MSG_DCCCHAT_SENT_REQUEST, [u.unicodeName, c.localIP,
                                              c.port, cmd]), "DCC-CHAT");
    client.munger.entries[".inline-buttons"].enabled = false;

    return true;
}

function cmdDCCClose(e)
{
    if (!jsenv.HAS_SERVER_SOCKETS)
        return display(MSG_DCC_NOT_POSSIBLE);
    if (!client.prefs["dcc.enabled"])
        return display(MSG_DCC_NOT_ENABLED);

    // If there is no nickname specified, use current view.
    if (!e.nickname)
    {
        if (client.currentObject.TYPE == "IRCDCCChat")
            return client.currentObject.abort();
        // ...if there is one.
        return display(MSG_DCC_ERR_NOCHAT);
    }

    var o = client.dcc.findByID(e.nickname);
    if (o)
        // Direct ID --> object request.
        return o.abort();

    if (e.type)
        e.type = [e.type.toLowerCase()];
    else
        e.type = ["chat", "file"];

    // Go ask the DCC code for some matching requets.
    var list = client.dcc.getMatches
              (e.nickname, e.file, e.type, [DCC_DIR_GETTING, DCC_DIR_SENDING],
               [DCC_STATE_REQUESTED, DCC_STATE_ACCEPTED, DCC_STATE_CONNECTED]);

    // Disconnect if only one match.
    if (list.length == 1)
        return list[0].abort();

    // Oops, couldn't figure the user's requets out, so give them some help.
    display(getMsg(MSG_DCC_ACCEPTED_MATCHES, [list.length]));
    display(MSG_DCC_MATCHES_HELP);
    return true;
}

function cmdDCCSend(e)
{
    if (!jsenv.HAS_SERVER_SOCKETS)
        return display(MSG_DCC_NOT_POSSIBLE);
    if (!client.prefs["dcc.enabled"])
        return display(MSG_DCC_NOT_ENABLED);

    const DIRSVC_CID = "@mozilla.org/file/directory_service;1";
    const nsIProperties = Components.interfaces.nsIProperties;

    if (!e.nickname && !e.user)
        return display(MSG_DCC_ERR_NOUSER);

    // Accept the request passed in...
    var file;
    if (!e.file)
    {
        var pickerRv = pickOpen(MSG_DCCFILE_SEND);
        if (pickerRv.reason == PICK_CANCEL)
            return false;
        file = pickerRv.file;
    }
    else
    {
        // Wrap in try/catch because nsILocalFile creation throws a freaking
        // error if it doesn't get a FULL path.
        try
        {
            file = nsLocalFile(e.file);
        }
        catch(ex)
        {
            // Ok, try user's home directory.
            var fl = Components.classes[DIRSVC_CID].getService(nsIProperties);
            file = fl.get("Home", Components.interfaces.nsILocalFile);

            // Another freaking try/catch wrapper.
            try
            {
                // NOTE: This is so pathetic it can't cope with any path
                // separators in it, so don't even THINK about lobing a
                // relative path at it.
                file.append(e.file);

                // Wow. We survived.
            }
            catch (ex)
            {
                return display(MSG_DCCFILE_ERR_NOTFOUND);
            }
        }
    }
    if (!file.exists())
        return display(MSG_DCCFILE_ERR_NOTFOUND);
    if (!file.isFile())
        return display(MSG_DCCFILE_ERR_NOTAFILE);
    if (!file.isReadable())
        return display(MSG_DCCFILE_ERR_NOTREADABLE);

    var user;
    if (e.nickname)
        user = e.server.addUser(e.nickname);
    else
        user = e.server.addUser(e.user.unicodeName);

    var u = client.dcc.addUser(user);
    var c = client.dcc.addFileTransfer(u, client.dcc.getNextPort());
    c.request(file);

    client.munger.entries[".inline-buttons"].enabled = true;
    var cmd = getMsg(MSG_DCC_COMMAND_CANCEL, "dcc-close " + c.id);
    display(getMsg(MSG_DCCFILE_SENT_REQUEST, [u.unicodeName, c.localIP, c.port,
                                              file.leafName, c.size, cmd]),
            "DCC-FILE");
    client.munger.entries[".inline-buttons"].enabled = false;

    return true;
}

function cmdDCCList(e) {
    if (!jsenv.HAS_SERVER_SOCKETS)
        return display(MSG_DCC_NOT_POSSIBLE);
    if (!client.prefs["dcc.enabled"])
        return display(MSG_DCC_NOT_ENABLED);

    var counts = { pending: 0, connected: 0, failed: 0 };
    var k;

    // Get all the DCC sessions.
    var list = client.dcc.getMatches();

    for (k = 0; k < list.length; k++) {
        var c = list[k];
        var type = c.TYPE.substr(6, c.TYPE.length - 6);

        var dir = MSG_UNKNOWN;
        var tf = MSG_UNKNOWN;
        if (c.state.dir == DCC_DIR_SENDING)
        {
            dir = MSG_DCCLIST_DIR_OUT;
            tf = MSG_DCCLIST_TO;
        }
        else if (c.state.dir == DCC_DIR_GETTING)
        {
            dir = MSG_DCCLIST_DIR_IN;
            tf = MSG_DCCLIST_FROM;
        }

        var state = MSG_UNKNOWN;
        var cmds = "";
        switch (c.state.state)
        {
            case DCC_STATE_REQUESTED:
                state = MSG_DCC_STATE_REQUEST;
                if (c.state.dir == DCC_DIR_GETTING)
                {
                    cmds = getMsg(MSG_DCC_COMMAND_ACCEPT, "dcc-accept " + c.id) + " " +
                           getMsg(MSG_DCC_COMMAND_DECLINE, "dcc-decline " + c.id);
                }
                else
                {
                    cmds = getMsg(MSG_DCC_COMMAND_CANCEL, "dcc-close " + c.id);
                }
                counts.pending++;
                break;
            case DCC_STATE_ACCEPTED:
                state = MSG_DCC_STATE_ACCEPT;
                counts.connected++;
                break;
            case DCC_STATE_DECLINED:
                state = MSG_DCC_STATE_DECLINE;
                break;
            case DCC_STATE_CONNECTED:
                state = MSG_DCC_STATE_CONNECT;
                cmds = getMsg(MSG_DCC_COMMAND_CLOSE, "dcc-close " + c.id);
                if (c.TYPE == "IRCDCCFileTransfer")
                {
                    state = getMsg(MSG_DCC_STATE_CONNECTPRO,
                                   [Math.floor(100 * c.position / c.size),
                                    c.position, c.size]);
                }
                counts.connected++;
                break;
            case DCC_STATE_DONE:
                state = MSG_DCC_STATE_DISCONNECT;
                break;
            case DCC_STATE_ABORTED:
                state = MSG_DCC_STATE_ABORT;
                counts.failed++;
                break;
            case DCC_STATE_FAILED:
                state = MSG_DCC_STATE_FAIL;
                counts.failed++;
                break;
        }
        client.munger.entries[".inline-buttons"].enabled = true;
        display(getMsg(MSG_DCCLIST_LINE, [k + 1, state, dir, type, tf,
                                          c.unicodeName, c.remoteIP, c.port,
                                          cmds]));
        client.munger.entries[".inline-buttons"].enabled = false;
    }
    display(getMsg(MSG_DCCLIST_SUMMARY, [counts.pending, counts.connected,
                                         counts.failed]));
    return true;
}

function cmdDCCAccept(e)
{
    if (!jsenv.HAS_SERVER_SOCKETS)
        return display(MSG_DCC_NOT_POSSIBLE);
    if (!client.prefs["dcc.enabled"])
        return display(MSG_DCC_NOT_ENABLED);

    function accept(c)
    {
        if (c.TYPE == "IRCDCCChat")
            return c.accept();

        // Accept the request passed in...
        var filename = c.filename;
        var ext = "*";
        var m = filename.match(/...\.([a-z]+)$/i);
        if (m)
            ext = "*." + m[1];

        var pickerRv = pickSaveAs(getMsg(MSG_DCCFILE_SAVE_TO, filename),
                                  ["$all", ext], filename);
        if (pickerRv.reason == PICK_CANCEL)
            return false;
        c.accept(pickerRv.file);

        if (c.TYPE == "CIRCDCCChat")
            display(getMsg(MSG_DCCCHAT_ACCEPTED, [c.unicodeName, c.remoteIP,
                                                  c.port]),
                    "DCC-CHAT");
        else
            display(getMsg(MSG_DCCFILE_ACCEPTED, [c.filename, c.unicodeName,
                                                  c.remoteIP, c.port]),
                    "DCC-FILE");
        return true;
    };

    // If there is no nickname specified, use the "last" item.
    // This is the last DCC request that arrvied.
    if (!e.nickname && client.dcc.last)
    {
        if ((new Date() - client.dcc.lastTime) >= 10000)
            return accept(client.dcc.last);
        return display(MSG_DCC_ERR_ACCEPT_TIME);
    }

    var o = client.dcc.findByID(e.nickname);
    if (o)
        // Direct ID --> object request.
        return accept(o);

    if (e.type)
        e.type = [e.type.toLowerCase()];
    else
        e.type = ["chat", "file"];

    // Go ask the DCC code for some matching requets.
    var list = client.dcc.getMatches(e.nickname, e.file, e.type,
                                     [DCC_DIR_GETTING], [DCC_STATE_REQUESTED]);
    // Accept if only one match.
    if (list.length == 1)
        return accept(list[0]);

    // Oops, couldn't figure the user's requets out, so give them some help.
    display(getMsg(MSG_DCC_PENDING_MATCHES, [list.length]));
    display(MSG_DCC_MATCHES_HELP);
    return true;
}

function cmdDCCDecline(e)
{
    if (!jsenv.HAS_SERVER_SOCKETS)
        return display(MSG_DCC_NOT_POSSIBLE);
    if (!client.prefs["dcc.enabled"])
        return display(MSG_DCC_NOT_ENABLED);

    function decline(c)
    {
        // Decline the request passed in...
        c.decline();
        if (c.TYPE == "CIRCDCCChat")
            display(getMsg(MSG_DCCCHAT_DECLINED, c._getParams()), "DCC-CHAT");
        else
            display(getMsg(MSG_DCCFILE_DECLINED, c._getParams()), "DCC-FILE");
    };

    // If there is no nickname specified, use the "last" item.
    // This is the last DCC request that arrvied.
    if (!e.nickname && client.dcc.last)
        return decline(client.dcc.last);

    var o = client.dcc.findByID(e.nickname);
    if (o)
        // Direct ID --> object request.
        return decline(o);

    if (!e.type)
        e.type = ["chat", "file"];

    // Go ask the DCC code for some matching requets.
    var list = client.dcc.getMatches(e.nickname, e.file, e.type,
                                     [DCC_DIR_GETTING], [DCC_STATE_REQUESTED]);
    // Decline if only one match.
    if (list.length == 1)
        return decline(list[0]);

    // Oops, couldn't figure the user's requets out, so give them some help.
    display(getMsg(MSG_DCC_PENDING_MATCHES, [list.length]));
    display(MSG_DCC_MATCHES_HELP);
    return true;
}

function cmdTextDirection(e)
{
    var direction;
    var sourceObject = e.sourceObject.frame.contentDocument.body;

    switch (e.dir)
    {
        case "toggle":
            if (sourceObject.getAttribute("dir") == "rtl")
                direction = 'ltr';
            else
                direction = 'rtl';
            break;
        case "rtl":
            direction = 'rtl';
            break;
        default:
            // that is "case "ltr":",
            // but even if !e.dir OR e.dir is an invalid value -> set to
            // default direction
            direction = 'ltr';
    }
    client.input.setAttribute("dir", direction);
    sourceObject.setAttribute("dir", direction);

    return true;
}

function cmdInputTextDirection(e)
{
    var direction;

    switch (e.dir)
    {
        case "rtl":
            client.input.setAttribute("dir", "rtl");
            break
        default:
            // that is "case "ltr":", but even if !e.dir OR e.dir is an
            //invalid value -> set to default direction
            client.input.setAttribute("dir", "ltr");
    }

    return true;
}
