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

const DEFAULT_NICK = "IRCMonkey"

function initPrefs()
{
    function makeLogNameClient()
    {
        return makeLogName(client, "client");
    };

    client.prefManager = new PrefManager("extensions.irc.",
                                         client.defaultBundle);
    client.prefManagers = [client.prefManager];

    client.prefs = client.prefManager.prefs;

    var profilePath = getSpecialDirectory("ProfD");
    profilePath.append("chatzilla");

    client.prefManager.addPref("profilePath", profilePath.path, null, null,
                                                                      "global");

    profilePath = new nsLocalFile(client.prefs["profilePath"]);

    if (!profilePath.exists())
        mkdir(profilePath);

    client.prefManager.profilePath = profilePath;

    var scriptPath = profilePath.clone();
    scriptPath.append("scripts");
    if (!scriptPath.exists())
        mkdir(scriptPath);
    client.prefManager.scriptPath = scriptPath;

    var logPath = profilePath.clone();
    logPath.append("logs");
    if (!logPath.exists())
        mkdir(logPath);
    client.prefManager.logPath = logPath;

    var logDefault = client.prefManager.logPath.clone();
    logDefault.append(escapeFileName("client.log"));

    var prefs =
        [
         ["activityFlashDelay", 200,      "global"],
         ["aliases",            [],       "lists.aliases"],
         ["autoAwayCap",        300,      "global"],
         ["autoRejoin",         false,    ".connect"],
         ["bugURL",           "https://bugzilla.mozilla.org/show_bug.cgi?id=%s",
                                          "global"],
         ["channelHeader",      true,     "global.header"],
         ["channelLog",         false,    "global.log"],
         ["channelMaxLines",    500,      "global.maxLines"],
         ["charset",            "utf-8",  ".connect"],
         ["clientMaxLines",     200,      "global.maxLines"],
         ["collapseMsgs",       false,    "appearance.misc"],
         ["connectTries",       5,        ".connect"],
         ["copyMessages",       true,     "global"],
         ["dcc.enabled",        true,     "dcc"],
         ["dcc.listenPorts",    [],       "dcc.ports"],
         ["dcc.useServerIP",    true,     "dcc"],
         ["debugMode",          "",       "global"],
         ["desc",               "New Now Know How", ".ident"],
         ["deleteOnPart",       true,     "global"],
         ["displayHeader",      true,     "appearance.misc"],
         ["guessCommands",      true,     "global"],
         ["hasPrefs",           false,    "hidden"],
         ["font.family",        "default", "appearance.misc"],
         ["font.size",          0,        "appearance.misc"],
         ["initialURLs",        [],       "startup.initialURLs"],
         ["initialScripts",     [getURLSpecFromFile(scriptPath.path)],
                                          "startup.initialScripts"],
         ["log",                false,                                  ".log"],
         ["logFileName",        makeLogNameClient,                      ".log"],
         ["logFile.client",     "client.$y-$m-$d.log",                  ".log"],
         ["logFile.network",    "$(network)/$(network).$y-$m-$d.log",   ".log"],
         ["logFile.channel",    "$(network)/channels/$(channel).$y-$m-$d.log",
                                                                        ".log"],
         ["logFile.user",       "$(network)/users/$(user).$y-$m-$d.log",".log"],
         ["logFolder",          getURLSpecFromFile(logPath.path), ".log"],
         ["messages.click",     "goto-url",          "global.links"],
         ["messages.ctrlClick", "goto-url-newwin",   "global.links"],
         ["messages.metaClick", "goto-url-newtab",   "global.links"],
         ["messages.middleClick", "goto-url-newtab", "global.links"],
         ["motif.dark",         "chrome://chatzilla/skin/output-dark.css",
                                          "appearance.motif"],
         ["motif.light",        "chrome://chatzilla/skin/output-light.css",
                                          "appearance.motif"],
         ["motif.default",      "chrome://chatzilla/skin/output-default.css",
                                          "appearance.motif"],
         ["motif.current",      "chrome://chatzilla/skin/output-default.css",
                                          "appearance.motif"],
         //["msgBeep",            "beep beep", "global.sounds"],
         ["multiline",          false,    "global"],
         ["munger.bold",        true,     "munger"],
         ["munger.bugzilla-link", true,   "munger"],
         ["munger.channel-link",true,     "munger"],
         ["munger.colorCodes",  true,     "munger"],
         ["munger.ctrl-char",   true,     "munger"],
         ["munger.face",        true,     "munger"],
         ["munger.italic",      true,     "munger"],
         ["munger.link",        true,     "munger"],
         ["munger.mailto",      true,     "munger"],
         ["munger.quote",       true,     "munger"],
         ["munger.rheet",       true,     "munger"],
         ["munger.teletype",    true,     "munger"],
         ["munger.underline",   true,     "munger"],
         ["munger.word-hyphenator", true, "munger"],
         ["networkHeader",      true,     "global.header"],
         ["networkLog",         false,    "global.log"],
         ["networkMaxLines",    200,      "global.maxLines"],
         ["newTabLimit",        15,       "global"],
         ["notify.aggressive",  true,     "global"],
         ["nickCompleteStr",    ":",      "global"],
         ["nickname",           DEFAULT_NICK, ".ident"],
         ["outgoing.colorCodes",  false,  "global"],
         ["outputWindowURL",   "chrome://chatzilla/content/output-window.html",
                                          "appearance.misc"],
         ["sortUsersByMode",    true,     "appearance.userlist"],
         //["queryBeep",          "beep",   "global.sounds"],
         ["reconnect",          true,     ".connect"],
         ["showModeSymbols",    false,    "appearance.userlist"],
         //["stalkBeep",          "beep",   "global.sounds"],
         ["stalkWholeWords",    true,     "lists.stalkWords"],
         ["stalkWords",         [],       "lists.stalkWords"],
         // Start == When view it opened.
         // Event == "Superfluous" activity.
         // Chat  == "Activity" activity.
         // Stalk == "Attention" activity.
         ["sound.enabled",       true,        "global.sounds"],
         ["sound.overlapDelay",  2000,        "global.sounds"],
         //["sound.surpressActive",false,       "global.sounds"],
         ["sound.channel.start", "",          "global.soundEvts"],
         ["sound.channel.event", "",          "global.soundEvts"],
         ["sound.channel.chat",  "",          "global.soundEvts"],
         ["sound.channel.stalk", "beep",      "global.soundEvts"],
         ["sound.user.start",    "beep beep", "global.soundEvts"],
         ["sound.user.stalk",    "beep",      "global.soundEvts"],
         ["timestamps",         false,     "appearance.timestamps"],
         ["timestampFormat",    "[%h:%n]", "appearance.timestamps"],
         ["username",           "chatzilla", ".ident"],
         ["usermode",           "+i",     ".ident"],
         ["userHeader",         true,     "global.header"],
         ["userLog",            false,    "global.log"],
         ["userMaxLines",       200,      "global.maxLines"]
        ];

    client.prefManager.addPrefs(prefs);
    client.prefManager.addObserver({ onPrefChanged: onPrefChanged });

    CIRCNetwork.prototype.stayingPower  = client.prefs["reconnect"];
    CIRCNetwork.prototype.MAX_CONNECT_ATTEMPTS = client.prefs["connectTries"];
    CIRCNetwork.prototype.INITIAL_NICK  = client.prefs["nickname"];
    CIRCNetwork.prototype.INITIAL_NAME  = client.prefs["username"];
    CIRCNetwork.prototype.INITIAL_DESC  = client.prefs["desc"];
    CIRCNetwork.prototype.INITIAL_UMODE = client.prefs["usermode"];
    CIRCNetwork.prototype.MAX_MESSAGES  = client.prefs["networkMaxLines"];
    CIRCChannel.prototype.MAX_MESSAGES  = client.prefs["channelMaxLines"];
    CIRCChanUser.prototype.MAX_MESSAGES = client.prefs["userMaxLines"];
    client.MAX_MESSAGES                 = client.prefs["clientMaxLines"];
    client.charset                      = client.prefs["charset"];

    initAliases();
}

function makeLogName(obj, type)
{
    // We like some control on the number of digits.
    function formatTimeNumber(num, digits)
    {
        var rv = num.toString();
        while (rv.length < digits)
            rv = "0" + rv;
        return rv;
    };

    /*  /\$\(([^)]+)\)|\$(\w)/g   *
     *       <----->     <-->     *
     *      longName   shortName  *
     */
    function replaceParam(match, longName, shortName)
    {
        if (typeof longName != "undefined" && longName)
        {
            // Remember to encode these, don't want some dodgy # breaking stuff.
            if (longName in longCodes)
                return encodeURIComponent(longCodes[longName]);
            dd("Unknown long code: " + longName);
            return;
        }
        else if (typeof shortName != "undefined" && shortName)
        {
            if (shortName in shortCodes)
                return encodeURIComponent(shortCodes[shortName]);
            dd("Unknown short code: " + shortName);
            return;
        }
        dd("Unknown match: " + match);
    };

    var base = client.prefs["logFolder"];
    var specific = client.prefs["logFile." + type];

    // Make sure we got ourselves a slash, or we'll be in trouble with the
    // concatenation.
    if (!base.match(/\/$/))
        base = base + "/";
    var file = base + specific;

    // Get details for $-replacement variables.
    var info = getObjectDetails(obj);

    // Three longs codes: $(network), $(channel) and $(user).
    // Each is available only if appropriate for the object.
    var longCodes = new Object();
    if (info.network)
        longCodes["network"] = info.network.unicodeName;
    if (info.channel)
        longCodes["channel"] = info.channel.unicodeName;
    if (info.user)
        longCodes["user"] = info.user.unicodeName;

    // Six short codes: $y, $m, $d, $h, $n, $s.
    // These are time codes, each replaced with a fixed-length number.
    var d = new Date();
    var shortCodes = { y: formatTimeNumber(d.getFullYear(), 4),
                       m: formatTimeNumber(d.getMonth() + 1, 2),
                       d: formatTimeNumber(d.getDate(), 2),
                       h: formatTimeNumber(d.getHours(), 2),
                       n: formatTimeNumber(d.getMinutes(), 2),
                       s: formatTimeNumber(d.getSeconds(), 2)
                     };

    // Replace all $-variables in one go.
    file = file.replace(/\$\(([^)]+)\)|\$(\w)/g, replaceParam);

    // Convert from file: URL to local OS format.
    try
    {
        file = getFileFromURLSpec(file).path;
    }
    catch(ex)
    {
        dd("Error converting '" + base + specific + "' to a local file path.");
    }

    return file;
}

function pref_mungeName(name)
{
    var safeName = name.replace(/\./g, "-").replace(/:/g, "_").toLowerCase();
    return ecmaEscape(safeName);
}

function getNetworkPrefManager(network)
{
    function defer(prefName)
    {
        return client.prefs[prefName];
    };

    function makeLogNameNetwork()
    {
        return makeLogName(network, "network");
    };

    function onPrefChanged(prefName, newValue, oldValue)
    {
        onNetworkPrefChanged (network, prefName, newValue, oldValue);
    };

    var logDefault = client.prefManager.logPath.clone();
    logDefault.append(escapeFileName(pref_mungeName(network.encodedName)) + ".log");

    var prefs =
        [
         ["autoRejoin",       defer, ".connect"],
         ["charset",          defer, ".connect"],
         ["collapseMsgs",     defer, "appearance.misc"],
         ["connectTries",     defer, ".connect"],
         ["dcc.useServerIP",  defer, "dcc"],
         ["desc",             defer, ".ident"],
         ["displayHeader",    client.prefs["networkHeader"],
                                                             "appearance.misc"],
         ["font.family",      defer, "appearance.misc"],
         ["font.size",        defer, "appearance.misc"],
         ["hasPrefs",         false, "hidden"],
         ["log",              client.prefs["networkLog"], ".log"],
         ["logFileName",      makeLogNameNetwork,         ".log"],
         ["motif.current",    defer, "appearance.motif"],
         ["nickname",         defer, ".ident"],
         ["notifyList",       [],    "lists.notifyList"],
         ["outputWindowURL",  defer, "appearance.misc"],
         ["reconnect",        defer, ".connect"],
         ["timestamps",       defer, "appearance.timestamps"],
         ["timestampFormat",  defer, "appearance.timestamps"],
         ["username",         defer, ".ident"],
         ["usermode",         defer, ".ident"],
         ["autoperform",      [],    "lists.autoperform"]
        ];

    var branch = "extensions.irc.networks." + pref_mungeName(network.encodedName) +
        ".";
    var prefManager = new PrefManager(branch, client.defaultBundle);
    prefManager.addPrefs(prefs);
    prefManager.addObserver({ onPrefChanged: onPrefChanged });
    client.prefManager.addObserver(prefManager);

    var value = prefManager.prefs["nickname"];
    if (value != CIRCNetwork.prototype.INITIAL_NICK)
        network.INITIAL_NICK = value;

    value = prefManager.prefs["username"];
    if (value != CIRCNetwork.prototype.INITIAL_NAME)
        network.INITIAL_NAME = value;

    value = prefManager.prefs["desc"];
    if (value != CIRCNetwork.prototype.INITIAL_DESC)
        network.INITIAL_DESC = value;

    value = prefManager.prefs["usermode"];
    if (value != CIRCNetwork.prototype.INITIAL_UMODE)
        network.INITIAL_UMODE = value;

    network.stayingPower  = prefManager.prefs["reconnect"];
    network.MAX_CONNECT_ATTEMPTS = prefManager.prefs["connectTries"];

    client.prefManagers.push(prefManager);

    return prefManager;
}

function getChannelPrefManager(channel)
{
    var network = channel.parent.parent;

    function defer(prefName)
    {
        return network.prefs[prefName];
    };

    function makeLogNameChannel()
    {
        return makeLogName(channel, "channel");
    };

    function onPrefChanged(prefName, newValue, oldValue)
    {
        onChannelPrefChanged (channel, prefName, newValue, oldValue);
    };

    var logDefault = client.prefManager.logPath.clone();
    var filename = pref_mungeName(network.encodedName) + "," +
        pref_mungeName(channel.encodedName);

    logDefault.append(escapeFileName(filename) + ".log");

    var prefs =
        [
         ["autoRejoin",       defer, ".connect"],
         ["charset",          defer, ".connect"],
         ["collapseMsgs",     defer, "appearance.misc"],
         ["displayHeader",    client.prefs["channelHeader"],
                                                             "appearance.misc"],
         ["font.family",      defer, "appearance.misc"],
         ["font.size",        defer, "appearance.misc"],
         ["hasPrefs",         false, "hidden"],
         ["log",              client.prefs["channelLog"], ".log"],
         ["logFileName",      makeLogNameChannel,         ".log"],
         ["motif.current",    defer, "appearance.motif"],
         ["timestamps",       defer, "appearance.timestamps"],
         ["timestampFormat",  defer, "appearance.timestamps"],
         ["outputWindowURL",  defer, "appearance.misc"]
        ];

    var branch = "extensions.irc.networks." + pref_mungeName(network.encodedName) +
        ".channels." + pref_mungeName(channel.encodedName) + "."
    var prefManager = new PrefManager(branch, client.defaultBundle);
    prefManager.addPrefs(prefs);
    prefManager.addObserver({ onPrefChanged: onPrefChanged });
    network.prefManager.addObserver(prefManager);

    client.prefManagers.push(prefManager);

    return prefManager;
}

function getUserPrefManager(user)
{
    var network = user.parent.parent;

    function defer(prefName)
    {
        return network.prefs[prefName];
    };

    function makeLogNameUser()
    {
        return makeLogName(user, "user");
    };

    function onPrefChanged(prefName, newValue, oldValue)
    {
        onUserPrefChanged (user, prefName, newValue, oldValue);
    };

    var logDefault = client.prefManager.logPath.clone();
    var filename = pref_mungeName(network.encodedName);
    filename += "," + pref_mungeName(user.encodedName);
    logDefault.append(escapeFileName(filename) + ".log");

    var prefs =
        [
         ["charset",          defer, ".connect"],
         ["collapseMsgs",     defer, "appearance.misc"],
         ["displayHeader",    client.prefs["userHeader"], "appearance.misc"],
         ["font.family",      defer, "appearance.misc"],
         ["font.size",        defer, "appearance.misc"],
         ["hasPrefs",         false, "hidden"],
         ["motif.current",    defer, "appearance.motif"],
         ["outputWindowURL",  defer, "appearance.misc"],
         ["log",              client.prefs["userLog"], ".log"],
         ["logFileName",      makeLogNameUser,         ".log"],
         ["timestamps",       defer, "appearance.timestamps"],
         ["timestampFormat",  defer, "appearance.timestamps"]
        ];

    var branch = "extensions.irc.networks." + pref_mungeName(network.encodedName) +
        ".users." + pref_mungeName(user.encodedName) + ".";
    var prefManager = new PrefManager(branch, client.defaultBundle);
    prefManager.addPrefs(prefs);
    prefManager.addObserver({ onPrefChanged: onPrefChanged });
    network.prefManager.addObserver(prefManager);

    client.prefManagers.push(prefManager);

    return prefManager;
}

function destroyPrefs()
{
    if ("prefManagers" in client)
    {
        for (var i = 0; i < client.prefManagers.length; ++i)
            client.prefManagers[i].destroy();
    }
}

function onPrefChanged(prefName, newValue, oldValue)
{
    switch (prefName)
    {
        case "channelMaxLines":
            CIRCChannel.prototype.MAX_MESSAGES = newValue;
            break;

        case "charset":
            client.charset = newValue;
            break;

        case "clientMaxLines":
            client.MAX_MESSAGES = newValue;
            break;

        case "connectTries":
            CIRCNetwork.prototype.MAX_CONNECT_ATTEMPTS = newValue;
            break;

        case "font.family":
        case "font.size":
            client.dispatch("sync-font");
            break;

        case "showModeSymbols":
            if (newValue)
                setListMode("symbol");
            else
                setListMode("graphic");
            break;

        case "nickname":
            CIRCNetwork.prototype.INITIAL_NICK = newValue;
            break;

        case "username":
            CIRCNetwork.prototype.INITIAL_NAME = newValue;
            break;

        case "usermode":
            CIRCNetwork.prototype.INITIAL_UMODE = newValue;
            break;

        case "userMaxLines":
            CIRCChanUser.prototype.MAX_MESSAGES = newValue;
            break;

        case "debugMode":
            setDebugMode(newValue);
            break;

        case "desc":
            CIRCNetwork.prototype.INITIAL_DESC = newValue;
            break;

        case "stalkWholeWords":
        case "stalkWords":
            updateAllStalkExpressions();
            break;

        case "sortUsersByMode":
            if (client.currentObject.TYPE == "IRCChannel")
                updateUserList();

        case "motif.current":
            client.dispatch("sync-motif");
            break;

        case "multiline":
            multilineInputMode(newValue);
            break;

        case "munger.colorCodes":
            client.enableColors = newValue;
            break;

        case "networkMaxLines":
            CIRCNetwork.prototype.MAX_MESSAGES = newValue;
            break;

        case "outputWindowURL":
            client.dispatch("sync-window");
            break;

        case "displayHeader":
            client.dispatch("sync-header");
            break;

        case "timestamps":
        case "timestampFormat":
            client.dispatch("sync-timestamp");
            break;

        case "log":
            client.dispatch("sync-log");
            break;

        case "aliases":
            initAliases();
            break;
    }
}

function onNetworkPrefChanged(network, prefName, newValue, oldValue)
{
    if (network != client.networks[network.canonicalName])
    {
        /* this is a stale observer, remove it */
        network.prefManager.destroy();
        return;
    }

    network.updateHeader();

    switch (prefName)
    {
        case "nickname":
            network.INITIAL_NICK = newValue;
            break;

        case "username":
            network.INITIAL_NAME = newValue;
            break;

        case "usermode":
            network.INITIAL_UMODE = newValue;
            if (network.isConnected())
            {
                network.primServ.sendData("mode " + network.server.me + " :" +
                                          newValue + "\n");
            }
            break;

        case "desc":
            network.INITIAL_DESC = newValue;
            break;

        case "reconnect":
            network.stayingPower = newValue;
            break;

        case "font.family":
        case "font.size":
            network.dispatch("sync-font");
            break;

        case "motif.current":
            network.dispatch("sync-motif");
            break;

        case "outputWindowURL":
            network.dispatch("sync-window");
            break;

        case "displayHeader":
            network.dispatch("sync-header");
            break;

        case "timestamps":
        case "timestampFormat":
            network.dispatch("sync-timestamp");
            break;

        case "log":
            network.dispatch("sync-log");
            break;

        case "connectTries":
            network.MAX_CONNECT_ATTEMPTS = newValue;
            break;
    }
}

function onChannelPrefChanged(channel, prefName, newValue, oldValue)
{
    var network = channel.parent.parent;

    if (network != client.networks[network.canonicalName] ||
        channel.parent != network.primServ ||
        channel != network.primServ.channels[channel.canonicalName])
    {
        /* this is a stale observer, remove it */
        channel.prefManager.destroy();
        return;
    }

    channel.updateHeader();

    switch (prefName)
    {
        case "font.family":
        case "font.size":
            channel.dispatch("sync-font");
            break;

        case "motif.current":
            channel.dispatch("sync-motif");
            break;

        case "outputWindowURL":
            channel.dispatch("sync-window");
            break;

        case "displayHeader":
            channel.dispatch("sync-header");
            break;

        case "timestamps":
        case "timestampFormat":
            channel.dispatch("sync-timestamp");
            break;

        case "log":
            channel.dispatch("sync-log");
            break;
    }
}

function onUserPrefChanged(user, prefName, newValue, oldValue)
{
    var network = user.parent.parent;

    if (network != client.networks[network.canonicalName] ||
        user.parent != network.primServ ||
        user != network.primServ.users[user.canonicalName])
    {
        /* this is a stale observer, remove it */
        user.prefManager.destroy();
        return;
    }

    user.updateHeader();

    switch (prefName)
    {
        case "font.family":
        case "font.size":
            user.dispatch("sync-font");
            break;

        case "motif.current":
            user.dispatch("sync-motif");
            break;

        case "outputWindowURL":
            user.dispatch("sync-window");
            break;

        case "displayHeader":
            user.dispatch("sync-header");
            break;

        case "timestamps":
        case "timestampFormat":
            user.dispatch("sync-timestamp");
            break;

        case "log":
            user.dispatch("sync-log");
            break;
    }
}

function initAliases()
{
    var aliasDefs = client.prefs["aliases"];

    for (var i = 0; i < aliasDefs.length; ++i)
    {
        var ary = aliasDefs[i].match(/^(.*?)\s*=\s*(.*)$/);
        if (ary)
        {
            var name = ary[1];
            var list = ary[2];

            client.commandManager.defineCommand(name, list);
        }
        else
        {
            dd("Malformed alias: " + aliasDefs[i]);
        }
    }
}
