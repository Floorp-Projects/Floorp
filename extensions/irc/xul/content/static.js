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
 *   Chiaki Koufugata chiaki@mozilla.gr.jp UI i18n
 *   Samuel Sieb, samuel@sieb.net, MIRC color codes, munger menu, and various
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

const __cz_version   = "0.9.77";
const __cz_condition = "green";
const __cz_suffix    = "";
const __cz_guid      = "59c81df5-4b7a-477b-912d-4e0fdf64e5f2";
const __cz_locale    = "0.9.76.0";

var warn;
var ASSERT;
var TEST;

if (DEBUG)
{
    _dd_pfx = "cz: ";
    warn = function (msg) { dumpln ("** WARNING " + msg + " **"); }
    TEST = ASSERT = function _assert(expr, msg) {
                 if (!expr) {
                     dd("** ASSERTION FAILED: " + msg + " **\n" +
                        getStackTrace() + "\n");
                     return false;
                 } else {
                     return true;
                 }
             }
}
else
    dd = warn = TEST = ASSERT = function (){};

var client = new Object();

client.TYPE = "IRCClient";
client.COMMAND_CHAR = "/";
client.STEP_TIMEOUT = 500;
client.MAX_MESSAGES = 200;
client.MAX_HISTORY = 50;
/* longest nick to show in display before forcing the message to a block level
 * element */
client.MAX_NICK_DISPLAY = 14;
/* longest word to show in display before abbreviating */
client.MAX_WORD_DISPLAY = 20;

client.MAX_MSG_PER_ROW = 3; /* default number of messages to collapse into a
                             * single row, max. */
client.INITIAL_COLSPAN = 5; /* MAX_MSG_PER_ROW cannot grow to greater than
                             * one half INITIAL_COLSPAN + 1. */
client.NOTIFY_TIMEOUT = 5 * 60 * 1000; /* update notify list every 5 minutes */

// Check every minute which networks have away statuses that need an update.
client.AWAY_TIMEOUT = 60 * 1000;

client.SLOPPY_NETWORKS = true; /* true if msgs from a network can be displayed
                                * on the current object if it is related to
                                * the network (ie, /whois results will appear
                                * on the channel you're viewing, if that channel
                                * is on the network that the results came from)
                                */
client.DOUBLETAB_TIME = 500;
client.IMAGEDIR = "chrome://chatzilla/skin/images/";
client.HIDE_CODES = true;      /* true if you'd prefer to show numeric response
                                * codes as some default value (ie, "===") */
/* true if the browser widget shouldn't be allowed to take focus.  windows, and
 * probably the mac, need to be able to give focus to the browser widget for
 * copy to work properly. */
client.NO_BROWSER_FOCUS = (navigator.platform.search(/mac|win/i) == -1);
client.DEFAULT_RESPONSE_CODE = "===";
/* Minimum number of users above or below the conference limit the user count
 * must go, before it is changed. This allows the user count to fluctuate
 * around the limit without continously going on and off.
 */
client.CONFERENCE_LOW_PASS = 10;


client.viewsArray = new Array();
client.activityList = new Object();
client.hostCompat = new Object();
client.inputHistory = new Array();
client.lastHistoryReferenced = -1;
client.incompleteLine = "";
client.lastTabUp = new Date();
client.awayMsgs = new Array();
client.awayMsgCount = 5;

CIRCNetwork.prototype.INITIAL_CHANNEL = "";
CIRCNetwork.prototype.MAX_MESSAGES = 100;
CIRCNetwork.prototype.IGNORE_MOTD = false;
CIRCNetwork.prototype.RECLAIM_WAIT = 15000;
CIRCNetwork.prototype.RECLAIM_TIMEOUT = 400000;
CIRCNetwork.prototype.MIN_RECONNECT_MS = 15 * 1000;             // 15s
CIRCNetwork.prototype.MAX_RECONNECT_MS = 2 * 60 * 60 * 1000;    // 2h

CIRCServer.prototype.READ_TIMEOUT = 0;
CIRCServer.prototype.PRUNE_OLD_USERS = 0; // prune on user quit.

CIRCUser.prototype.MAX_MESSAGES = 200;

CIRCChannel.prototype.MAX_MESSAGES = 300;

CIRCChanUser.prototype.MAX_MESSAGES = 200;

function init()
{
    if (("initialized" in client) && client.initialized)
        return;

    client.initialized = false;

    client.networks = new Object();
    client.entities = new Object();
    client.eventPump = new CEventPump (200);

    if (DEBUG)
    {
        /* hook all events EXCEPT server.poll and *.event-end types
         * (the 4th param inverts the match) */
        client.debugHook =
            client.eventPump.addHook([{type: "poll", set:/^(server|dcc-chat)$/},
                                    {type: "event-end"}], event_tracer,
                                    "event-tracer", true /* negate */,
                                    false /* disable */);
    }

    initApplicationCompatibility();
    initMessages();
    if (client.host == "")
        showErrorDlg(getMsg(MSG_ERR_UNKNOWN_HOST, client.unknownUID));

    initRDF();
    initCommands();
    initPrefs();
    initMunger();
    initNetworks();
    initMenus();
    initStatic();
    initHandlers();

    // Create DCC handler.
    client.dcc = new CIRCDCC(client);

    client.ident = new IdentServer(client);

    // start logging.  nothing should call display() before this point.
    if (client.prefs["log"])
        client.openLogFile(client);
    // kick-start a log-check interval to make sure we change logfiles in time:
    // It will fire 2 seconds past the next full hour.
    setTimeout("checkLogFiles()", 3602000 - (Number(new Date()) % 3600000));

    // Make sure the userlist is on the correct side.
    updateUserlistSide(client.prefs["userlistLeft"]);

    client.display(MSG_WELCOME, "HELLO");
    client.dispatch("set-current-view", { view: client });

    importFromFrame("updateHeader");
    importFromFrame("setHeaderState");
    importFromFrame("changeCSS");
    importFromFrame("updateMotifSettings");
    importFromFrame("addUsers");
    importFromFrame("updateUsers");
    importFromFrame("removeUsers");

    processStartupScripts();

    client.commandManager.installKeys(document);
    createMenus();

    initIcons();

    client.busy = false;
    updateProgress();

    client.initialized = true;

    dispatch("help", { hello: true });
    dispatch("networks");

    initInstrumentation();
    setTimeout(processStartupURLs, 0);
}

function initStatic()
{
    client.mainWindow = window;

    try
    {
        var io = Components.classes['@mozilla.org/network/io-service;1'];
        client.iosvc = io.getService(Components.interfaces.nsIIOService);
    }
    catch (ex)
    {
        dd("IO service failed to initialize: " + ex);
    }

    try
    {
        const nsISound = Components.interfaces.nsISound;
        client.sound =
            Components.classes["@mozilla.org/sound;1"].createInstance(nsISound);

        client.soundList = new Object();
    }
    catch (ex)
    {
        dd("Sound failed to initialize: " + ex);
    }

    try
    {
        const nsIGlobalHistory = Components.interfaces.nsIGlobalHistory;
        const GHIST_CONTRACTID = "@mozilla.org/browser/global-history;1";
        client.globalHistory =
            Components.classes[GHIST_CONTRACTID].getService(nsIGlobalHistory);
    }
    catch (ex)
    {
        dd("Global History failed to initialize: " + ex);
    }

    try
    {
        const nsISDateFormat = Components.interfaces.nsIScriptableDateFormat;
        const DTFMT_CID = "@mozilla.org/intl/scriptabledateformat;1";
        client.dtFormatter =
            Components.classes[DTFMT_CID].createInstance(nsISDateFormat);

        // Mmmm, fun. This ONLY affects the ChatZilla window, don't worry!
        Date.prototype.toStringInt = Date.prototype.toString;
        Date.prototype.toString = function() {
            var dtf = client.dtFormatter;
            return dtf.FormatDateTime("", dtf.dateFormatLong,
                                      dtf.timeFormatSeconds,
                                      this.getFullYear(), this.getMonth() + 1,
                                      this.getDate(), this.getHours(),
                                      this.getMinutes(), this.getSeconds()
                                     );
        }
    }
    catch (ex)
    {
        dd("Locale-correct date formatting failed to initialize: " + ex);
    }

    multilineInputMode(client.prefs["multiline"]);
    updateSpellcheck(client.prefs["inputSpellcheck"]);
    if (client.prefs["showModeSymbols"])
        setListMode("symbol");
    else
        setListMode("graphic");

    var tree = document.getElementById('user-list');
    tree.setAttribute("ondraggesture",
                      "nsDragAndDrop.startDrag(event, userlistDNDObserver);");

    setDebugMode(client.prefs["debugMode"]);

    var ver = __cz_version + (__cz_suffix ? "-" + __cz_suffix : "");

    var ua = navigator.userAgent;
    var app = getService("@mozilla.org/xre/app-info;1", "nsIXULAppInfo");
    if (app)
    {
        // Use the XUL host app info, and Gecko build ID.
        if (app.ID == "{" + __cz_guid + "}")
        {
            // We ARE the app, in other words, we're running in XULrunner.
            // Because of this, we must disregard app.(name|vendor|version).
            // "XULRunner 1.7+/2005071506"
            ua = "XULRunner " + app.platformVersion + "/" + app.platformBuildID;

            // "XULRunner 1.7+/2005071506, Windows"
            CIRCServer.prototype.HOST_RPLY = ua + ", " + client.platform;
        }
        else
        {
            // "Firefox 1.0+/2005071506"
            ua = app.name + " " + app.version + "/";
            if ("platformBuildID" in app) // 1.1 and up
                ua += app.platformBuildID;
            else if ("geckoBuildID" in app) // 1.0 - 1.1 trunk only
                ua += app.geckoBuildID;
            else // Uh oh!
                ua += "??????????";

            // "Mozilla Firefox 1.0+, Windows"
            CIRCServer.prototype.HOST_RPLY = app.vendor + " " + app.name + " " +
                                             app.version + ", " + client.platform;
        }
    }
    else
    {
        // Extract the revision number, and Gecko build ID.
        var ary = navigator.userAgent.match(/(rv:[^;)\s]+).*?Gecko\/(\d+)/);
        if (ary)
        {
            if (navigator.vendor)
                ua = navigator.vendor + " " + navigator.vendorSub; // FF 1.0
            else
                ua = client.entities.brandShortName + " " + ary[1]; // Suite
            ua = ua + "/" + ary[2];
        }
        CIRCServer.prototype.HOST_RPLY = client.entities.brandShortName + ", " +
                                         client.platform;
    }

    client.userAgent = getMsg(MSG_VERSION_REPLY, [ver, ua]);
    CIRCServer.prototype.VERSION_RPLY = client.userAgent;
    CIRCServer.prototype.SOURCE_RPLY = MSG_SOURCE_REPLY;

    client.statusBar = new Object();

    client.statusBar["server-nick"] = document.getElementById("server-nick");

    client.statusElement = document.getElementById("status-text");
    client.defaultStatus = MSG_DEFAULT_STATUS;

    client.progressPanel = document.getElementById("status-progress-panel");
    client.progressBar = document.getElementById("status-progress-bar");

    client.logFile = null;
    setInterval("onNotifyTimeout()", client.NOTIFY_TIMEOUT);
    // Call every minute, will check only the networks necessary.
    setInterval("onWhoTimeout()", client.AWAY_TIMEOUT);

    client.awayMsgs = [{ message: MSG_AWAY_DEFAULT }];
    var awayFile = new nsLocalFile(client.prefs["profilePath"]);
    awayFile.append("awayMsgs.txt");
    if (awayFile.exists())
    {
        var awayLoader = new TextSerializer(awayFile);
        if (awayLoader.open("<"))
        {
            // Load the first item from the file.
            var item = awayLoader.deserialize();
            if (isinstance(item, Array))
            {
                // If the first item is an array, it is the entire thing.
                client.awayMsgs = item;
            }
            else
            {
                /* Not an array, so we have the old format of a single object
                 * per entry.
                 */
                client.awayMsgs = [item];
                while ((item = awayLoader.deserialize()))
                    client.awayMsgs.push(item);
            }
            awayLoader.close();
        }
    }

    client.defaultCompletion = client.COMMAND_CHAR + "help ";

    client.deck = document.getElementById('output-deck');
}

function initApplicationCompatibility()
{
    // This routine does nothing more than tweak the UI based on the host
    // application.

    /* client.hostCompat.typeChromeBrowser indicates whether we should use
     * type="chrome" <browser> elements for the output window documents.
     * Using these is necessary to work properly with xpcnativewrappers, but
     * broke selection in older builds.
     */
    client.hostCompat.typeChromeBrowser = false;

    // Set up simple host and platform information.
    client.host = "Unknown";
    var app = getService("@mozilla.org/xre/app-info;1", "nsIXULAppInfo");
    if (app)
    {
        // Use the XULAppInfo.ID to find out what host we run on.
        switch (app.ID)
        {
            case "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}":
                client.host = "Firefox";
                if (compareVersions(app.version, "1.4") <= 0)
                    client.hostCompat.typeChromeBrowser = true;
                break;
            case "{" + __cz_guid + "}":
                // We ARE the app, in other words, we're running in XULrunner.
                client.host = "XULrunner";
                client.hostCompat.typeChromeBrowser = true;
                break;
            case "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}": // SeaMonkey
                client.host = "Mozilla";
                client.hostCompat.typeChromeBrowser = true;
                break;
            case "{a463f10c-3994-11da-9945-000d60ca027b}": // Flock
                client.host = "Flock";
                client.hostCompat.typeChromeBrowser = true;
                break;
            default:
                client.unknownUID = app.ID;
                client.host = ""; // Unknown host, show an error later.
        }
    }
    else if ("getBrowserURL" in window)
    {
        var url = getBrowserURL();
        if (url == "chrome://navigator/content/navigator.xul")
        {
            client.host = "Mozilla";
        }
        else if (url == "chrome://browser/content/browser.xul")
        {
            client.host = "Firefox";
        }
        else
        {
            client.host = ""; // We don't know this host. Show an error later.
            client.unknownUID = url;
        }
    }

    client.platform = "Unknown";
    if (navigator.platform.search(/mac/i) > -1)
        client.platform = "Mac";
    if (navigator.platform.search(/win/i) > -1)
        client.platform = "Windows";
    if (navigator.platform.search(/linux/i) > -1)
        client.platform = "Linux";
    if (navigator.platform.search(/os\/2/i) > -1)
        client.platform = "OS/2";

    client.hostPlatform = client.host + client.platform;

    CIRCServer.prototype.OS_RPLY = navigator.oscpu + " (" +
                                   navigator.platform + ")";

    // Windows likes \r\n line endings, as wussy-notepad can't cope with just
    // \n logs.
    if (client.platform == "Windows")
        client.lineEnd = "\r\n";
    else
        client.lineEnd = "\n";
}

function initIcons()
{
    // Make sure we got the ChatZilla icon(s) in place first.
    const iconName = "chatzilla-window";
    const suffixes = [".ico", ".xpm", "16.xpm"];

    /* when installing on Mozilla, the XPI has the power to put the icons where
     * they are needed - in Firefox, it doesn't. So we move them here, instead.
     * In XULRunner, things are more fun, as we're not an extension.
     */
    var sourceDir;
    if ((client.host == "Firefox") || (client.host == "Flock"))
    {
        sourceDir = getSpecialDirectory("ProfD");
        sourceDir.append("extensions");
        sourceDir.append("{" + __cz_guid + "}");
        sourceDir.append("defaults");
    }
    else if (client.host == "XULrunner")
    {
        sourceDir = getSpecialDirectory("resource:app");
        sourceDir.append("chrome");
        sourceDir.append("icons");
    }
    else
    {
        return;
    }

    var destDir = getSpecialDirectory("AChrom");
    destDir.append("icons");
    destDir.append("default");
    if (!destDir.exists())
    {
        try
        {
            mkdir(destDir);
        }
        catch(ex)
        {
            return;
        }
    }

    for (var i = 0; i < suffixes.length; i++)
    {
        var iconDest = destDir.clone();
        iconDest.append(iconName + suffixes[i]);
        var iconSrc = sourceDir.clone();
        iconSrc.append(iconName + suffixes[i]);

        if (iconSrc.exists() && !iconDest.exists())
        {
            try
            {
                iconSrc.copyTo(iconDest.parent, iconDest.leafName);
            }
            catch(ex){}
        }
    }
}

function initInstrumentation()
{
    // Make sure we assign the user a random key - this is not used for
    // anything except percentage chance of participation.
    if (client.prefs["instrumentation.key"] == 0)
    {
        var rand = 1 + Math.round(Math.random() * 10000);
        client.prefs["instrumentation.key"] = rand;
    }

    runInstrumentation("inst1");
}

function runInstrumentation(name, firstRun)
{
    if (!/^inst\d+$/.test(name))
        return;

    // Values:
    //   0 = not answered question
    //   1 = allowed inst
    //   2 = denied inst

    if (client.prefs["instrumentation." + name] == 0)
    {
        // We only want 1% of people to be asked here.
        if (client.prefs["instrumentation.key"] > 100)
            return;

        // User has not seen the info about this system. Show them the info.
        var cmdYes = "allow-" + name;
        var cmdNo = "deny-" + name;
        var btnYes = getMsg(MSG_INST1_COMMAND_YES, cmdYes);
        var btnNo  = getMsg(MSG_INST1_COMMAND_NO,  cmdNo);
        client.munger.entries[".inline-buttons"].enabled = true;
        client.display(getMsg("msg." + name + ".msg1", [btnYes, btnNo]));
        client.display(getMsg("msg." + name + ".msg2", [cmdYes, cmdNo]));
        client.munger.entries[".inline-buttons"].enabled = false;

        // Don't hide *client* if we're asking the user about the startup ping.
        client.lockView = true;
        return;
    }

    if (client.prefs["instrumentation." + name] != 1)
        return;

    if (name == "inst1")
        runInstrumentation1(firstRun);
}

function runInstrumentation1(firstRun)
{
    function inst1onLoad()
    {
        if (/OK/.test(req.responseText))
            client.display(MSG_INST1_MSGRPLY2);
        else
            client.display(getMsg(MSG_INST1_MSGRPLY1, MSG_UNKNOWN));
    };

    function inst1onError()
    {
        client.display(getMsg(MSG_INST1_MSGRPLY1, req.statusText));
    };

    try
    {
        const baseURI = "http://silver.warwickcompsoc.co.uk/" +
                        "mozilla/chatzilla/instrumentation/startup?";

        if (firstRun)
        {
            // Do a first-run ping here.
            var frReq = new XMLHttpRequest();
            frReq.open("GET", baseURI + "first-run");
            frReq.send(null);
        }

        var data = new Array();
        data.push("ver=" + encodeURIComponent(CIRCServer.prototype.VERSION_RPLY));
        data.push("host=" + encodeURIComponent(client.hostPlatform));
        data.push("chost=" + encodeURIComponent(CIRCServer.prototype.HOST_RPLY));
        data.push("cos=" + encodeURIComponent(CIRCServer.prototype.OS_RPLY));

        var url = baseURI + data.join("&");

        var req = new XMLHttpRequest();
        req.onload = inst1onLoad;
        req.onerror = inst1onError;
        req.open("GET", url);
        req.send(null);
    }
    catch (ex)
    {
        client.display(getMsg(MSG_INST1_MSGRPLY1, formatException(ex)));
    }
}

function getFindData(e)
{
    var findData = new nsFindInstData();
    findData.browser = e.sourceObject.frame;
    findData.rootSearchWindow = e.sourceObject.frame.contentWindow;
    findData.currentSearchWindow = e.sourceObject.frame.contentWindow;

    /* Yay, evil hacks! findData.init doesn't care about the findService, it
     * gets option settings from webBrowserFind. As we want the wrap option *on*
     * when we use /find foo, we set it on the findService there. However,
     * restoring the original value afterwards doesn't help, because init() here
     * overrides that value. Unless we make .init do something else, of course:
     */
    findData._init = findData.init;
    findData.init =
        function init()
        {
            this._init();
            const FINDSVC_ID = "@mozilla.org/find/find_service;1";
            var findService = getService(FINDSVC_ID, "nsIFindService");
            this.webBrowserFind.wrapFind = findService.wrapFind;
        };

    return findData;
}

function importFromFrame(method)
{
    client.__defineGetter__(method, import_wrapper);
    CIRCNetwork.prototype.__defineGetter__(method, import_wrapper);
    CIRCChannel.prototype.__defineGetter__(method, import_wrapper);
    CIRCUser.prototype.__defineGetter__(method, import_wrapper);
    CIRCDCCChat.prototype.__defineGetter__(method, import_wrapper);
    CIRCDCCFileTransfer.prototype.__defineGetter__(method, import_wrapper);

    function import_wrapper()
    {
        var dummy = function(){};

        if (!("frame" in this))
            return dummy;

        try
        {
            var window = getContentWindow(this.frame)
            if (window && "initialized" in window && window.initialized &&
                method in window)
            {
                return window[method];
            }
        }
        catch (ex)
        {
            ASSERT(0, "Caught exception calling: " + method + "\n" + ex);
        }

        return dummy;
    };
}

function processStartupScripts()
{
    client.plugins = new Array();
    var scripts = client.prefs["initialScripts"];
    for (var i = 0; i < scripts.length; ++i)
    {
        if (scripts[i].search(/^file:|chrome:/i) != 0)
        {
            display(getMsg(MSG_ERR_INVALID_SCHEME, scripts[i]), MT_ERROR);
            continue;
        }

        var path = getFileFromURLSpec(scripts[i]);

        if (!path.exists())
        {
            display(getMsg(MSG_ERR_ITEM_NOT_FOUND, scripts[i]), MT_WARN);
            continue;
        }

        if (path.isDirectory())
            loadPluginDirectory(path);
        else
            loadLocalFile(path);
    }
}

function loadPluginDirectory(localPath, recurse)
{
    if (typeof recurse == "undefined")
        recurse = 1;

    var initPath = localPath.clone();
    initPath.append("init.js");
    if (initPath.exists())
        loadLocalFile(initPath);

    if (recurse < 1)
        return;

    var enumer = localPath.directoryEntries;
    while (enumer.hasMoreElements())
    {
        var entry = enumer.getNext();
        entry = entry.QueryInterface(Components.interfaces.nsILocalFile);
        if (entry.isDirectory())
            loadPluginDirectory(entry, recurse - 1);
    }
}

function loadLocalFile(localFile)
{
    var url = getURLSpecFromFile(localFile);
    var glob = new Object();
    dispatch("load", {url: url, scope: glob});
}

function getPluginById(id)
{
    for (var i = 0; i < client.plugins.length; ++i)
    {
        if (client.plugins[i].id == id)
            return client.plugins[i];

    }

    return null;
}

function getPluginIndexById(id)
{
    for (var i = 0; i < client.plugins.length; ++i)
    {
        if (client.plugins[i].id == id)
            return i;

    }

    return -1;
}

function getPluginByURL(url)
{
    for (var i = 0; i < client.plugins.length; ++i)
    {
        if (client.plugins[i].url == url)
            return client.plugins[i];

    }

    return null;
}

function getPluginIndexByURL(url)
{
    for (var i = 0; i < client.plugins.length; ++i)
    {
        if (client.plugins[i].url == url)
            return i;

    }

    return -1;
}

function processStartupURLs()
{
    var wentSomewhere = false;

    if ("arguments" in window &&
        0 in window.arguments && typeof window.arguments[0] == "object" &&
        "url" in window.arguments[0])
    {
        var url = window.arguments[0].url;
        if (url.search(/^ircs?:\/?\/?\/?$/i) == -1)
        {
            /* if the url is not irc: irc:/, irc://, or ircs equiv then go to it. */
            gotoIRCURL(url);
            wentSomewhere = true;
        }
    }
    /* check to see whether the URL has been passed via the command line
       instead. */
    else if ("arguments" in window &&
        0 in window.arguments && typeof window.arguments[0] == "string")
    {
        var url = window.arguments[0]
        var urlMatches = url.match(/^ircs?:\/\/\/?(.*)$/)
        if (urlMatches)
        {
            if (urlMatches[1])
            {
                /* if the url is not "irc://", "irc:///" or an ircs equiv then
                   go to it. */
                gotoIRCURL(url);
                wentSomewhere = true;
            }
        }
        else if (url)
        {
            /* URL parameter is not blank, but does not not conform to the
               irc[s] scheme. */
            display(getMsg(MSG_ERR_INVALID_SCHEME, url), MT_ERROR);
        }
    }

    if (!wentSomewhere)
    {
        /* if we had nowhere else to go, connect to any default urls */
        var ary = client.prefs["initialURLs"];
        for (var i = 0; i < ary.length; ++i)
        {
            if (ary[i] && ary[i] == "irc:///")
            {
                // Clean out "default network" entries, which we don't
                // support any more; replace with the harmless irc:// URL.
                ary[i] = "irc://";
                client.prefs["initialURLs"].update();
            }
            if (ary[i] && ary[i] != "irc://")
                gotoIRCURL(ary[i]);
        }
    }

    if (client.viewsArray.length > 1 && !isStartupURL("irc://"))
    {
        dispatch("delete-view", {view: client});
    }
}

function destroy()
{
    destroyPrefs();
}

function setStatus (str)
{
    client.statusElement.setAttribute ("label", str);
    return str;
}

client.__defineSetter__ ("status", setStatus);

function getStatus ()
{
    return client.statusElement.getAttribute ("label");
}

client.__defineGetter__ ("status", getStatus);

function isVisible (id)
{
    var e = document.getElementById(id);

    if (!ASSERT(e,"Bogus id ``" + id + "'' passed to isVisible() **"))
        return false;

    return (e.getAttribute ("collapsed") != "true");
}

client.getConnectedNetworks =
function getConnectedNetworks()
{
    var rv = [];
    for (var n in client.networks)
    {
        if (client.networks[n].isConnected())
            rv.push(client.networks[n]);
    }
    return rv;
}

function insertLink (matchText, containerTag, data)
{
    var href;
    var linkText;

    var trailing;
    ary = matchText.match(/([.,?]+)$/);
    if (ary)
    {
        linkText = RegExp.leftContext;
        trailing = ary[1];
    }
    else
    {
        linkText = matchText;
    }

    var ary = linkText.match (/^(\w[\w-]+):/);
    if (ary)
    {
        if (!("schemes" in client))
        {
            var pfx = "@mozilla.org/network/protocol;1?name=";
            var len = pfx.length;

            client.schemes = new Object();
            for (var c in Components.classes)
            {
                if (c.indexOf(pfx) == 0)
                    client.schemes[c.substr(len)] = true;
            }
        }

        if (!(ary[1] in client.schemes))
        {
            insertHyphenatedWord(matchText, containerTag);
            return;
        }

        href = linkText;
    }
    else
    {
        href = "http://" + linkText;
    }

    /* This gives callers to the munger control over URLs being logged; the
     * channel topic munger uses this, as well as the "is important" checker.
     * If either of |dontLogURLs| or |noStateChange| is present and true, we
     * don't log.
     */
    if ((!("dontLogURLs" in data) || !data.dontLogURLs) &&
        (!("noStateChange" in data) || !data.noStateChange))
    {
        var max = client.prefs["urls.store.max"];
        if (client.prefs["urls.list"].unshift(href) > max)
            client.prefs["urls.list"].pop();
        client.prefs["urls.list"].update();
    }

    var anchor = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:a");
    anchor.setAttribute ("href", href);
    anchor.setAttribute ("class", "chatzilla-link");
    anchor.setAttribute ("target", "_content");
    insertHyphenatedWord (linkText, anchor);
    containerTag.appendChild (anchor);
    if (trailing)
        insertHyphenatedWord (trailing, containerTag);

}

function insertMailToLink (matchText, containerTag)
{

    var href;

    if (matchText.indexOf ("mailto:") != 0)
        href = "mailto:" + matchText;
    else
        href = matchText;

    var anchor = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:a");
    anchor.setAttribute ("href", href);
    anchor.setAttribute ("class", "chatzilla-link");
    //anchor.setAttribute ("target", "_content");
    insertHyphenatedWord (matchText, anchor);
    containerTag.appendChild (anchor);

}

function insertChannelLink (matchText, containerTag, eventData)
{
    var bogusChannels =
        /^#(include|error|define|if|ifdef|else|elsif|endif|\d+)$/i;

    if (!("network" in eventData) || !eventData.network ||
        matchText.search(bogusChannels) != -1)
    {
        containerTag.appendChild(document.createTextNode(matchText));
        return;
    }

    var encodedMatchText = fromUnicode(matchText, eventData.sourceObject);
    var anchor = document.createElementNS("http://www.w3.org/1999/xhtml",
                                          "html:a");
    anchor.setAttribute ("href", eventData.network.getURL() +
                         ecmaEscape(encodedMatchText));
    anchor.setAttribute ("class", "chatzilla-link");
    insertHyphenatedWord (matchText, anchor);
    containerTag.appendChild (anchor);
}

function insertTalkbackLink(matchText, containerTag, eventData)
{
    var anchor = document.createElementNS("http://www.w3.org/1999/xhtml",
                                          "html:a");

    anchor.setAttribute("href", "http://talkback-public.mozilla.org/" +
                        "search/start.jsp?search=2&type=iid&id=" + matchText);
    anchor.setAttribute("class", "chatzilla-link");
    insertHyphenatedWord(matchText, anchor);
    containerTag.appendChild(anchor);
}

function insertBugzillaLink (matchText, containerTag, eventData)
{
    var bugURL;
    if (eventData.channel)
        bugURL = eventData.channel.prefs["bugURL"];
    else if (eventData.network)
        bugURL = eventData.network.prefs["bugURL"];
    else
        bugURL = client.prefs["bugURL"];

    if (bugURL.length > 0)
    {
        var idOrAlias = matchText.match(/bug\s+#?(\d+|[^\s,]{1,20})/i)[1];
        var anchor = document.createElementNS("http://www.w3.org/1999/xhtml",
                                              "html:a");

        anchor.setAttribute("href", bugURL.replace("%s", idOrAlias));
        anchor.setAttribute("class", "chatzilla-link");
        anchor.setAttribute("target", "_content");
        insertHyphenatedWord(matchText, anchor);
        containerTag.appendChild(anchor);
    }
    else
    {
        insertHyphenatedWord(matchText, containerTag);
    }
}

function insertRheet (matchText, containerTag)
{

    var anchor = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:a");
    anchor.setAttribute ("href",
                         "http://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/bonus-tracks/rheet.wav");
    anchor.setAttribute ("class", "chatzilla-rheet chatzilla-link");
    //anchor.setAttribute ("target", "_content");
    insertHyphenatedWord (matchText, anchor);
    containerTag.appendChild (anchor);
}

function insertQuote (matchText, containerTag)
{
    if (matchText == "``")
        containerTag.appendChild(document.createTextNode("\u201c"));
    else
        containerTag.appendChild(document.createTextNode("\u201d"));
}

function insertSmiley(emoticon, containerTag)
{
    var type = "error";

    if (emoticon.search(/\>[-^v]?\)/) != -1)
        type = "face-alien";
    else if (emoticon.search(/\>[=:;][-^v]?[(|]/) != -1)
        type = "face-angry";
    else if (emoticon.search(/[=:;][-^v]?[Ss\\\/]/) != -1)
        type = "face-confused";
    else if (emoticon.search(/[B8][-^v]?[)\]]/) != -1)
        type = "face-cool";
    else if (emoticon.search(/[=:;][~'][-^v]?\(/) != -1)
        type = "face-cry";
    else if (emoticon.search(/o[._]O/) != -1)
        type = "face-dizzy";
    else if (emoticon.search(/O[._]o/) != -1)
        type = "face-dizzy-back";
    else if (emoticon.search(/o[._]o|O[._]O/) != -1)
        type = "face-eek";
    else if (emoticon.search(/\>[=:;][-^v]?D/) != -1)
        type = "face-evil";
    else if (emoticon.search(/[=:;][-^v]?DD/) != -1)
        type = "face-lol";
    else if (emoticon.search(/[=:;][-^v]?D/) != -1)
        type = "face-laugh";
    else if (emoticon.search(/\([-^v]?D|[xX][-^v]?D/) != -1)
        type = "face-rofl";
    else if (emoticon.search(/[=:;][-^v]?\|/) != -1)
        type = "face-normal";
    else if (emoticon.search(/[=:;][-^v]?\?/) != -1)
        type = "face-question";
    else if (emoticon.search(/[=:;]"[)\]]/) != -1)
        type = "face-red";
    else if (emoticon.search(/9[._]9/) != -1)
        type = "face-rolleyes";
    else if (emoticon.search(/[=:;][-^v]?[(\[]/) != -1)
        type = "face-sad";
    else if (emoticon.search(/[=:][-^v]?[)\]]/) != -1)
        type = "face-smile";
    else if (emoticon.search(/[=:;][-^v]?[0oO]/) != -1)
        type = "face-surprised";
    else if (emoticon.search(/[=:;][-^v]?[pP]/) != -1)
        type = "face-tongue";
    else if (emoticon.search(/;[-^v]?[)\]]/) != -1)
        type = "face-wink";

    if (type == "error")
    {
        // We didn't actually match anything, so it'll be a too-generic match
        // from the munger RegExp.
        containerTag.appendChild(document.createTextNode(emoticon));
        return;
    }

    var span = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                         "html:span");

    /* create a span to hold the emoticon text */
    span.setAttribute ("class", "chatzilla-emote-txt");
    span.setAttribute ("type", type);
    span.appendChild (document.createTextNode (emoticon));
    containerTag.appendChild (span);

    /* create an empty span after the text.  this span will have an image added
     * after it with a chatzilla-emote:after css rule. using
     * chatzilla-emote-txt:after is not good enough because it does not allow us
     * to turn off the emoticon text, but keep the image.  ie.
     * chatzilla-emote-txt { display: none; } turns off
     * chatzilla-emote-txt:after as well.*/
    span = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                     "html:span");
    span.setAttribute ("class", "chatzilla-emote");
    span.setAttribute ("type", type);
    span.setAttribute ("title", emoticon);
    containerTag.appendChild (span);

}

function mircChangeColor (colorInfo, containerTag, data)
{
    /* If colors are disabled, the caller doesn't want colors specifically, or
     * the caller doesn't want any state-changing effects, we drop out.
     */
    if (!client.enableColors ||
        (("noMircColors" in data) && data.noMircColors) ||
        (("noStateChange" in data) && data.noStateChange))
    {
        return;
    }

    var ary = colorInfo.match (/.(\d{1,2}|)(,(\d{1,2})|)/);

    // Do we have a BG color specified...?
    if (!arrayHasElementAt(ary, 1) || !ary[1])
    {
        // Oops, no colors.
        delete data.currFgColor;
        delete data.currBgColor;
        return;
    }

    var fgColor = String(Number(ary[1]) % 16);

    if (fgColor.length == 1)
        data.currFgColor = "0" + fgColor;
    else
        data.currFgColor = fgColor;

    // Do we have a BG color specified...?
    if (arrayHasElementAt(ary, 3) && ary[3])
    {
        var bgColor = String(Number(ary[3]) % 16);

        if (bgColor.length == 1)
            data.currBgColor = "0" + bgColor;
        else
            data.currBgColor = bgColor;
    }

    data.hasColorInfo = true;
}

function mircToggleBold (colorInfo, containerTag, data)
{
    if (!client.enableColors ||
        (("noMircColors" in data) && data.noMircColors) ||
        (("noStateChange" in data) && data.noStateChange))
    {
        return;
    }

    if ("isBold" in data)
        delete data.isBold;
    else
        data.isBold = true;
    data.hasColorInfo = true;
}

function mircToggleUnder (colorInfo, containerTag, data)
{
    if (!client.enableColors ||
        (("noMircColors" in data) && data.noMircColors) ||
        (("noStateChange" in data) && data.noStateChange))
    {
        return;
    }

    if ("isUnderline" in data)
        delete data.isUnderline;
    else
        data.isUnderline = true;
    data.hasColorInfo = true;
}

function mircResetColor (text, containerTag, data)
{
    if (!client.enableColors ||
        (("noMircColors" in data) && data.noMircColors) ||
        (("noStateChange" in data) && data.noStateChange) ||
        !("hasColorInfo" in data))
    {
        return;
    }

    delete data.currFgColor;
    delete data.currBgColor;
    delete data.isBold;
    delete data.isUnder;
    delete data.hasColorInfo;
}

function mircReverseColor (text, containerTag, data)
{
    if (!client.enableColors ||
        (("noMircColors" in data) && data.noMircColors) ||
        (("noStateChange" in data) && data.noStateChange))
    {
        return;
    }

    var tempColor = ("currFgColor" in data ? data.currFgColor : "");

    if ("currBgColor" in data)
        data.currFgColor = data.currBgColor;
    else
        delete data.currFgColor;
    if (tempColor)
        data.currBgColor = tempColor;
    else
        delete data.currBgColor;
    data.hasColorInfo = true;
}

function showCtrlChar(c, containerTag)
{
    var span = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                         "html:span");
    span.setAttribute ("class", "chatzilla-control-char");
    if (c == "\t")
    {
        containerTag.appendChild(document.createTextNode(c));
        return;
    }

    var ctrlStr = c.charCodeAt(0).toString(16);
    if (ctrlStr.length < 2)
        ctrlStr = "0" + ctrlStr;
    span.appendChild (document.createTextNode ("0x" + ctrlStr));
    containerTag.appendChild (span);
}

function insertHyphenatedWord (longWord, containerTag)
{
    var wordParts = splitLongWord (longWord, client.MAX_WORD_DISPLAY);
    for (var i = 0; i < wordParts.length; ++i)
    {
        containerTag.appendChild (document.createTextNode (wordParts[i]));
        if (i != wordParts.length)
        {
            var wbr = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                                "html:wbr");
            containerTag.appendChild (wbr);
        }
    }
}

function insertInlineButton(text, containerTag, data)
{
    var ary = text.match(/\[\[([^\]]+)\]\[([^\]]+)\]\[([^\]]+)\]\]/);

    if (!ary)
    {
        containerTag.appendChild(document.createTextNode(text));
        return;
    }

    var label = ary[1];
    var title = ary[2];
    var command = ary[3];

    var link = document.createElementNS("http://www.w3.org/1999/xhtml", "a");
    link.setAttribute("href", "x-cz-command:" + encodeURI(command));
    link.setAttribute("title", title);
    link.setAttribute("class", "chatzilla-link");
    link.appendChild(document.createTextNode(label));

    containerTag.appendChild(document.createTextNode("["));
    containerTag.appendChild(link);
    containerTag.appendChild(document.createTextNode("]"));
}

function combineNicks(nickList, max)
{
    if (!max)
        max = 4;

    var combinedList = [];

    for (var i = 0; i < nickList.length; i += max)
    {
        count = Math.min(max, nickList.length - i);
        var nicks = nickList.slice(i, i + count);
        var str = new String(nicks.join(" "));
        str.count = count;
        combinedList.push(str);
    }

    return combinedList;
}

function updateAllStalkExpressions()
{
    var list = client.prefs["stalkWords"];

    for (var name in client.networks)
    {
        if ("stalkExpression" in client.networks[name])
            updateStalkExpression(client.networks[name], list);
    }
}

function updateStalkExpression(network)
{
    function escapeChar(ch)
    {
        return "\\" + ch;
    };

    var list = client.prefs["stalkWords"];

    var ary = new Array();

    ary.push(network.primServ.me.unicodeName.replace(/[^\w\d]/g, escapeChar));

    for (var i = 0; i < list.length; ++i)
        ary.push(list[i].replace(/[^\w\d]/g, escapeChar));

    var re;
    if (client.prefs["stalkWholeWords"])
        re = "(^|[\\W\\s])((" + ary.join(")|(") + "))([\\W\\s]|$)";
    else
        re = "(" + ary.join(")|(") + ")";

    network.stalkExpression = new RegExp(re, "i");
}

function getDefaultFontSize()
{
    const PREF_CTRID = "@mozilla.org/preferences-service;1";
    const nsIPrefService = Components.interfaces.nsIPrefService;
    const nsIPrefBranch = Components.interfaces.nsIPrefBranch;
    const XHTML_NS = "http://www.w3.org/1999/xhtml";

    var prefSvc = Components.classes[PREF_CTRID].getService(nsIPrefService);
    var prefBranch = prefSvc.getBranch(null);

    // PX size pref: font.size.variable.x-western
    var pxSize = 16;
    try
    {
        pxSize = prefBranch.getIntPref("font.size.variable.x-western");
    }
    catch(ex) { }

    var dpi = 96;
    try
    {
        // Get the DPI the fun way (make Mozilla do the work).
        var b = document.createElement("box");
        b.style.width = "1in";
        dpi = window.getComputedStyle(b, null).width.match(/^\d+/);
    }
    catch(ex)
    {
        try
        {
            // Get the DPI the fun way (make Mozilla do the work).
            b = document.createElementNS("box", XHTML_NS);
            b.style.width = "1in";
            dpi = window.getComputedStyle(b, null).width.match(/^\d+/);
        }
        catch(ex) { }
    }

    return Math.round((pxSize / dpi) * 72);
}

function getDefaultContext(cx)
{
    if (!cx)
        cx = new Object();
    /* Use __proto__ here and in all other get*Context so that the command can
     * tell the difference between getObjectDetails and actual parameters. See
     * cmdJoin for more details.
     */
    cx.__proto__ = getObjectDetails(client.currentObject);
    return cx;
}

function getMessagesContext(cx, element)
{
    if (!cx)
        cx = new Object();
    cx.__proto__ = getObjectDetails(client.currentObject);
    if (!element)
        element = document.popupNode;

    while (element)
    {
        switch (element.localName)
        {
            case "a":
                var href = element.getAttribute("href");
                cx.url = href;
                break;

            case "tr":
                var nickname = element.getAttribute("msg-user");
                if (!nickname)
                    break;

                // strip out  a potential ME! suffix
                var ary = nickname.match(/(\S+)/);
                nickname = ary[1];

                if (!cx.network)
                    break;

                // NOTE: nickname is the unicodeName here!
                if (cx.channel)
                    cx.user = cx.channel.getUser(nickname);
                else
                    cx.user = cx.network.getUser(nickname);

                if (cx.user)
                {
                    cx.nickname = cx.user.unicodeName;
                    cx.canonNick = cx.user.canonicalName;
                }
                else
                {
                    cx.nickname = nickname;
                }
                break;
        }

        element = element.parentNode;
    }

    return cx;
}

function getTabContext(cx, element)
{
    if (!cx)
        cx = new Object();
    if (!element)
        element = document.popupNode;

    while (element)
    {
        if (element.localName == "tab")
        {
            cx.__proto__ = getObjectDetails(element.view);
            return cx;
        }
        element = element.parentNode;
    }

    return cx;
}

function getUserlistContext(cx)
{
    if (!cx)
        cx = new Object();
    cx.__proto__ = getObjectDetails(client.currentObject);
    if (!cx.channel)
        return cx;

    var user, tree = document.getElementById("user-list");
    cx.userList = new Array();
    cx.canonNickList = new Array();
    cx.nicknameList = getSelectedNicknames(tree);

    for (var i = 0; i < cx.nicknameList.length; ++i)
    {
        user = cx.channel.getUser(cx.nicknameList[i])
        cx.userList.push(user);
        cx.canonNickList.push(user.canonicalName);
        if (i == 0)
        {
            cx.user = user;
            cx.nickname = user.unicodeName;
            cx.canonNick = user.canonicalName;
        }
    }

    return cx;
}

function getSelectedNicknames(tree)
{
    var rv = [];
    if (!tree || !tree.view || !tree.view.selection)
        return rv;
    var rangeCount = tree.view.selection.getRangeCount();

    // Loop through the selection ranges.
    for (var i = 0; i < rangeCount; ++i)
    {
        var start = {}, end = {};
        tree.view.selection.getRangeAt(i, start, end);

        // If they == -1, we've got no selection, so bail.
        if ((start.value == -1) && (end.value == -1))
            continue;
        /* Workaround: Because we use select(-1) instead of clearSelection()
         * (see bug 197667) the tree will then give us selection ranges
         * starting from -1 instead of 0! (See bug 319066.)
         */
        if (start.value == -1)
            start.value = 0;

        // Loop through the contents of the current selection range.
        for (var k = start.value; k <= end.value; ++k)
        {
            var item = tree.contentView.getItemAtIndex(k).firstChild.firstChild;
            var userName = item.getAttribute("unicodeName");
            rv.push(userName);
        }
    }
    return rv;
}

function setSelectedNicknames(tree, nicknameAry)
{
    if (!tree || !tree.view || !tree.view.selection || !nicknameAry)
        return;
    var item, unicodeName, resultAry = [];
    // Clear selection:
    tree.view.selection.select(-1);
    // Loop through the tree to (re-)select nicknames
    for (var i = 0; i < tree.view.rowCount; i++)
    {
        item = tree.contentView.getItemAtIndex(i).firstChild.firstChild;
        unicodeName = item.getAttribute("unicodeName");
        if ((unicodeName != "") && arrayContains(nicknameAry, unicodeName))
        {
            tree.view.selection.toggleSelect(i);
            resultAry.push(unicodeName);
        }
    }
    // Make sure we pass back a correct array:
    nicknameAry.length = 0;
    for (var j = 0; j < resultAry.length; j++)
        nicknameAry.push(resultAry[j]);
}

function getFontContext(cx)
{
    if (!cx)
        cx = new Object();
    cx.__proto__ = getObjectDetails(client.currentObject);
    cx.fontSizeDefault = getDefaultFontSize();
    var view = client;

    if ("prefs" in cx.sourceObject)
    {
        cx.fontFamily = view.prefs["font.family"];
        if (cx.fontFamily.match(/^(default|(sans-)?serif|monospace)$/))
            delete cx.fontFamily;

        cx.fontSize = view.prefs["font.size"];
        if (cx.fontSize == 0)
            delete cx.fontSize;
    }

    return cx;
}

function msgIsImportant (msg, sourceNick, network)
{
    /* This is a huge hack, but it works. What we want is to match against the
     * plain text of a message, ignoring color codes, bold, etc. so we put it
     * through the munger. This produces a tree of HTML elements, which we use
     * |.innerHTML| to convert to a textual representation.
     *
     * Then we remove all the HTML tags, using a RegExp.
     *
     * It certainly isn't ideal, and there has to be a better way, but it:
     *   a) works, and
     *   b) is fast enough to not cause problems,
     * so it will do for now.
     *
     * Note also that we don't want to log URLs munged here, or generally do
     * any state-changing stuff.
     */
    var plainMsg = client.munger.munge(msg, null, { noStateChange: true });
    plainMsg = plainMsg.innerHTML.replace(/<[^>]+>/g, "");

    var re = network.stalkExpression;
    if (plainMsg.search(re) != -1 || sourceNick && sourceNick.search(re) == 0)
        return true;

    return false;
}

function isStartupURL(url)
{
    return arrayContains(client.prefs["initialURLs"], url);
}

function cycleView (amount)
{
    var len = client.viewsArray.length;
    if (len <= 1)
        return;

    var tb = getTabForObject (client.currentObject);
    if (!tb)
        return;

    var vk = Number(tb.getAttribute("viewKey"));
    var destKey = (vk + amount) % len; /* wrap around */
    if (destKey < 0)
        destKey += len;

    dispatch("set-current-view", { view: client.viewsArray[destKey].source });
}

// Plays the sound for a particular event on a type of object.
function playEventSounds(type, event)
{
    if (!client.sound || !client.prefs["sound.enabled"])
        return;

    // Converts .TYPE values into the event object names.
    // IRCChannel => channel, IRCUser => user, etc.
    if (type.match(/^IRC/))
        type = type.substr(3, type.length).toLowerCase();

    var ev = type + "." + event;

    if (ev in client.soundList)
        return;

    if (!(("sound." + ev) in client.prefs))
        return;

    var s = client.prefs["sound." + ev];

    if (!s)
        return;

    if (client.prefs["sound.overlapDelay"] > 0)
    {
        client.soundList[ev] = true;
        setTimeout("delete client.soundList['" + ev + "']",
                   client.prefs["sound.overlapDelay"]);
    }

    if (event == "start")
    {
        blockEventSounds(type, "event");
        blockEventSounds(type, "chat");
        blockEventSounds(type, "stalk");
    }

    playSounds(s);
}

// Blocks a particular type of event sound occuring.
function blockEventSounds(type, event)
{
    if (!client.sound || !client.prefs["sound.enabled"])
        return;

    // Converts .TYPE values into the event object names.
    // IRCChannel => channel, IRCUser => user, etc.
    if (type.match(/^IRC/))
        type = type.substr(3, type.length).toLowerCase();

    var ev = type + "." + event;

    if (client.prefs["sound.overlapDelay"] > 0)
    {
        client.soundList[ev] = true;
        setTimeout("delete client.soundList['" + ev + "']",
                   client.prefs["sound.overlapDelay"]);
    }
}

function playSounds(list)
{
    var ary = list.split (" ");
    if (ary.length == 0)
        return;

    playSound(ary[0]);
    for (var i = 1; i < ary.length; ++i)
        setTimeout(playSound, 250 * i, ary[i]);
}

function playSound(file)
{
    if (!client.sound || !client.prefs["sound.enabled"] || !file)
        return;

    if (file == "beep")
    {
        client.sound.beep();
    }
    else
    {
        try
        {
            var uri = client.iosvc.newURI(file, null, null);
            client.sound.play(uri);
        }
        catch (ex)
        {
            // ignore exceptions from this pile of code.
        }
    }
}

/* timer-based mainloop */
function mainStep()
{
    try
    {
        var count = client.eventPump.stepEvents();
        if (count > 0)
            setTimeout("mainStep()", client.STEP_TIMEOUT);
        else
            setTimeout("mainStep()", client.STEP_TIMEOUT / 5);
    }
    catch(ex)
    {
        dd("Exception in mainStep!");
        dd(formatException(ex));
        setTimeout("mainStep()", client.STEP_TIMEOUT);
    }
}

function openQueryTab(server, nick)
{
    var user = server.addUser(nick);
    if (client.globalHistory)
        client.globalHistory.addPage(user.getURL());
    if (!("messages" in user))
    {
        var value = "";
        var same = true;
        for (var c in server.channels)
        {
            var chan = server.channels[c];
            if (!(user.canonicalName in chan.users))
                continue;
            /* This takes a boolean value for each channel (true - channel has
             * same value as first), and &&-s them all together. Thus, |same|
             * will tell us, at the end, if all the channels found have the
             * same value for charset.
             */
            if (value)
                same = same && (value == chan.prefs["charset"]);
            else
                value = chan.prefs["charset"];
        }
        /* If we've got a value, and it's the same accross all channels,
         * we use it as the *default* for the charset pref. If not, it'll
         * just keep the "defer" default which pulls it off the network.
         */
        if (value && same)
        {
            user.prefManager.prefRecords["charset"].defaultValue = value;
        }

        user.displayHere (getMsg(MSG_QUERY_OPENED, user.unicodeName));
    }
    user.whois();
    return user;
}

function arraySpeak (ary, single, plural)
{
    var rv = "";
    var and = MSG_AND;

    switch (ary.length)
    {
        case 0:
            break;

        case 1:
            rv = ary[0];
            if (single)
                rv += " " + single;
            break;

        case 2:
            rv = ary[0] + " " + and + " " + ary[1];
            if (plural)
                rv += " " + plural;
            break;

        default:
            for (var i = 0; i < ary.length - 1; ++i)
                rv += ary[i] + ", ";
            rv += and + " " + ary[ary.length - 1];
            if (plural)
                rv += " " + plural;
            break;
    }

    return rv;

}

function getObjectDetails (obj, rv)
{
    if (!rv)
        rv = new Object();

    if (!ASSERT(obj && typeof obj == "object",
                "INVALID OBJECT passed to getObjectDetails (" + obj + "). **"))
    {
        return rv;
    }

    rv.sourceObject = obj;
    rv.TYPE = obj.TYPE;
    rv.parent = ("parent" in obj) ? obj.parent : null;
    rv.user = null;
    rv.channel = null;
    rv.server = null;
    rv.network = null;

    switch (obj.TYPE)
    {
        case "IRCChannel":
            rv.viewType = MSG_CHANNEL;
            rv.channel = obj;
            rv.channelName = obj.unicodeName;
            rv.server = rv.channel.parent;
            rv.network = rv.server.parent;
            break;

        case "IRCUser":
            rv.viewType = MSG_USER;
            rv.user = obj;
            rv.userName = obj.unicodeName;
            rv.server = rv.user.parent;
            rv.network = rv.server.parent;
            break;

        case "IRCChanUser":
            rv.viewType = MSG_USER;
            rv.user = obj;
            rv.userName = obj.unicodeName;
            rv.channel = rv.user.parent;
            rv.server = rv.channel.parent;
            rv.network = rv.server.parent;
            break;

        case "IRCNetwork":
            rv.network = obj;
            rv.viewType = MSG_NETWORK;
            if ("primServ" in rv.network)
                rv.server = rv.network.primServ;
            else
                rv.server = null;
            break;

        case "IRCClient":
            rv.viewType = MSG_TAB;
            break;

        case "IRCDCCUser":
            //rv.viewType = MSG_USER;
            rv.user = obj;
            rv.userName = obj.unicodeName;
            break;

        case "IRCDCCChat":
            //rv.viewType = MSG_USER;
            rv.chat = obj;
            rv.user = obj.user;
            rv.userName = obj.unicodeName;
            break;

        case "IRCDCCFileTransfer":
            //rv.viewType = MSG_USER;
            rv.file = obj;
            rv.user = obj.user;
            rv.userName = obj.unicodeName;
            rv.fileName = obj.filename;
            break;

        default:
            /* no setup for unknown object */
            break;
    }

    if (rv.network)
        rv.networkName = rv.network.unicodeName;

    return rv;

}

function findDynamicRule (selector)
{
    var rules = frames[0].document.styleSheets[1].cssRules;

    if (isinstance(selector, RegExp))
        fun = "search";
    else
        fun = "indexOf";

    for (var i = 0; i < rules.length; ++i)
    {
        var rule = rules.item(i);
        if (rule.selectorText && rule.selectorText[fun](selector) == 0)
            return {sheet: frames[0].document.styleSheets[1], rule: rule,
                    index: i};
    }

    return null;
}

function addDynamicRule (rule)
{
    var rules = frames[0].document.styleSheets[1];

    var pos = rules.cssRules.length;
    rules.insertRule (rule, pos);
}

function getCommandEnabled(command)
{
    try {
        var dispatcher = document.commandDispatcher;
        var controller = dispatcher.getControllerForCommand(command);

        return controller.isCommandEnabled(command);
    }
    catch (e)
    {
        return false;
    }
}

function doCommand(command)
{
    try {
        var dispatcher = document.commandDispatcher;
        var controller = dispatcher.getControllerForCommand(command);
        if (controller && controller.isCommandEnabled(command))
            controller.doCommand(command);
    }
    catch (e)
    {
    }
}

function doCommandWithParams(command, params)
{
    try {
        var dispatcher = document.commandDispatcher;
        var controller = dispatcher.getControllerForCommand(command);
        controller.QueryInterface(Components.interfaces.nsICommandController);

        if (!controller || !controller.isCommandEnabled(command))
            return;

        var cmdparams = newObject("@mozilla.org/embedcomp/command-params;1",
                                  "nsICommandParams");
        for (var i in params)
            cmdparams.setISupportsValue(i, params[i]);

        controller.doCommandWithParams(command, cmdparams);
    }
    catch (e)
    {
    }
}

var testURLs =
    ["irc:", "irc://", "irc:///", "irc:///help", "irc:///help,needkey",
    "irc://irc.foo.org", "irc://foo:6666",
    "irc://foo", "irc://irc.foo.org/", "irc://foo:6666/", "irc://foo/",
    "irc://irc.foo.org/,needpass", "irc://foo/,isserver",
    "irc://moznet/,isserver", "irc://moznet/",
    "irc://foo/chatzilla", "irc://foo/chatzilla/",
    "irc://irc.foo.org/?msg=hello%20there",
    "irc://irc.foo.org/?msg=hello%20there&ignorethis",
    "irc://irc.foo.org/%23mozilla,needkey?msg=hello%20there&ignorethis",
    "invalids",
    "irc://irc.foo.org/,isnick"];

function doURLTest()
{
    for (var u in testURLs)
    {
        dd ("testing url \"" + testURLs[u] + "\"");
        var o = parseIRCURL(testURLs[u]);
        if (!o)
            dd ("PARSE FAILED!");
        else
            dd (dumpObjectTree(o));
        dd ("---");
    }
}

function parseIRCURL (url)
{
    var specifiedHost = "";

    var rv = new Object();
    rv.spec = url;
    rv.scheme = url.split(":")[0];
    rv.host = null;
    rv.target = "";
    rv.port = (rv.scheme == "ircs" ? 9999 : 6667);
    rv.msg = "";
    rv.pass = null;
    rv.key = null;
    rv.charset = null;
    rv.needpass = false;
    rv.needkey = false;
    rv.isnick = false;
    rv.isserver = false;

    if (url.search(/^(ircs?:\/?\/?)$/i) != -1)
        return rv;

    /* split url into <host>/<everything-else> pieces */
    var ary = url.match (/^ircs?:\/\/([^\/\s]+)?(\/[^\s]*)?$/i);
    if (!ary || !ary[1])
    {
        dd ("parseIRCURL: initial split failed");
        return null;
    }
    var host = ary[1];
    var rest = arrayHasElementAt(ary, 2) ? ary[2] : "";

    /* split <host> into server (or network) / port */
    ary = host.match (/^([^\:]+)?(\:\d+)?$/);
    if (!ary)
    {
        dd ("parseIRCURL: host/port split failed");
        return null;
    }

    if (arrayHasElementAt(ary, 2))
    {
        if (!arrayHasElementAt(ary, 2))
        {
            dd ("parseIRCURL: port with no host");
            return null;
        }
        specifiedHost = rv.host = ary[1].toLowerCase();
        rv.isserver = true;
        rv.port = parseInt(ary[2].substr(1));
    }
    else if (arrayHasElementAt(ary, 1))
    {
        specifiedHost = rv.host = ary[1].toLowerCase();
        if (specifiedHost.indexOf(".") != -1)
            rv.isserver = true;
    }

    if (rest)
    {
        ary = rest.match (/^\/([^\?\s\/,]*)?\/?(,[^\?]*)?(\?.*)?$/);
        if (!ary)
        {
            dd ("parseIRCURL: rest split failed ``" + rest + "''");
            return null;
        }

        rv.target = arrayHasElementAt(ary, 1) ? ecmaUnescape(ary[1]) : "";

        if (rv.target.search(/[\x07,:\s]/) != -1)
        {
            dd ("parseIRCURL: invalid characters in channel name");
            return null;
        }

        var params = arrayHasElementAt(ary, 2) ? ary[2].toLowerCase() : "";
        var query = arrayHasElementAt(ary, 3) ? ary[3] : "";

        if (params)
        {
            rv.isnick =
                (params.search (/,isnick(?:,|$)/) != -1);
            if (rv.isnick && !rv.target)
            {
                dd ("parseIRCURL: isnick w/o target");
                /* isnick w/o a target is bogus */
                return null;
            }

            if (!rv.isserver)
            {
                rv.isserver =
                    (params.search (/,isserver(?:,|$)/) != -1);
            }

            if (rv.isserver && !specifiedHost)
            {
                dd ("parseIRCURL: isserver w/o host");
                    /* isserver w/o a host is bogus */
                return null;
            }

            rv.needpass =
                (params.search (/,needpass(?:,|$)/) != -1);

            rv.needkey =
                (params.search (/,needkey(?:,|$)/) != -1);

        }

        if (query)
        {
            ary = query.substr(1).split("&");
            while (ary.length)
            {
                var arg = ary.pop().split("=");
                /*
                 * we don't want to accept *any* query, or folks could
                 * say things like "target=foo", and overwrite what we've
                 * already parsed, so we only use query args we know about.
                 */
                switch (arg[0].toLowerCase())
                {
                    case "msg":
                        rv.msg = unescape(arg[1]).replace ("\n", "\\n");
                         break;

                    case "pass":
                        rv.needpass = true;
                        rv.pass = unescape(arg[1]).replace ("\n", "\\n");
                        break;

                    case "key":
                        rv.needkey = true;
                        rv.key = unescape(arg[1]).replace ("\n", "\\n");
                        break;

                    case "charset":
                        rv.charset = unescape(arg[1]).replace ("\n", "\\n");
                        break;
                }
            }
        }
    }

    return rv;

}

function gotoIRCURL (url)
{
    var urlspec = url;
    if (typeof url == "string")
        url = parseIRCURL(url);

    if (!url)
    {
        window.alert (getMsg(MSG_ERR_BAD_IRCURL, urlspec));
        return;
    }

    if (!url.host)
    {
        /* focus the *client* view for irc:, irc:/, and irc:// (the only irc
         * urls that don't have a host.  (irc:/// implies a connect to the
         * default network.)
         */
        dispatch("client");
        return;
    }

    var network;

    if (url.isserver)
    {
        var alreadyThere = false;
        for (var n in client.networks)
        {
            if ((client.networks[n].isConnected()) &&
                (client.networks[n].primServ.hostname == url.host) &&
                (client.networks[n].primServ.port == url.port))
            {
                /* already connected to this server/port */
                network = client.networks[n];
                alreadyThere = true;
                break;
            }
        }

        if (!alreadyThere)
        {
            /*
            dd ("gotoIRCURL: not already connected to " +
                "server " + url.host + " trying to connect...");
            */
            var pass = "";
            if (url.needpass)
            {
                if (url.pass)
                    pass = url.pass;
                else
                    pass = promptPassword(getMsg(MSG_HOST_PASSWORD, url.host));
            }

            network = dispatch((url.scheme == "ircs" ? "sslserver" : "server"),
                               {hostname: url.host, port: url.port,
                                password: pass});

            if (!url.target)
                return;

            if (!("pendingURLs" in network))
                network.pendingURLs = new Array();
            network.pendingURLs.unshift(url);
            return;
        }
    }
    else
    {
        /* parsed as a network name */
        if (!(url.host in client.networks))
        {
            display(getMsg(MSG_ERR_UNKNOWN_NETWORK, url.host));
            return;
        }

        network = client.networks[url.host];
        if (!network.isConnected())
        {
            /*
            dd ("gotoIRCURL: not already connected to " +
                "network " + url.host + " trying to connect...");
            */
            client.connectToNetwork(network, (url.scheme == "ircs" ? true : false));

            if (!url.target)
                return;

            if (!("pendingURLs" in network))
                network.pendingURLs = new Array();
            network.pendingURLs.unshift(url);
            return;
        }
    }

    /* already connected, do whatever comes next in the url */
    //dd ("gotoIRCURL: connected, time to finish parsing ``" + url + "''");
    if (url.target)
    {
        var targetObject;
        var ev;
        if (url.isnick)
        {
            /* url points to a person. */
            var nick = url.target;
            var ary = url.target.split("!");
            if (ary)
                nick = ary[0];

            targetObject = network.dispatch("query", {nickname: nick});
        }
        else
        {
            /* url points to a channel */
            var key;
            if (url.needkey)
            {
                if (url.key)
                    key = url.key;
                else
                    key = window.promptPassword(getMsg(MSG_URL_KEY, url.spec));
            }

            if (url.charset)
            {
                var d = { channelName: url.target, key: key,
                          charset: url.charset };
                targetObject = network.dispatch("join", d);
            }
            else
            {
                // Must do this the hard way... we have the server's format
                // for the channel name here, and all our commands only work
                // with the Unicode forms.
                var serv = network.primServ;
                var target = url.target;

                /* If we don't have a valid prefix, stick a "#" on it.
                 * NOTE: This is always a "#" so that URLs may be compared
                 * properly without involving the server (e.g. off-line).
                 */
                if ((arrayIndexOf(["#", "&", "+", "!"], target[0]) == -1) &&
                    (arrayIndexOf(serv.channelTypes, target[0]) == -1))
                {
                    target = "#" + target;
                }

                var chan = new CIRCChannel(serv, null, target);

                d = { channelName: chan.unicodeName, key: key,
                      charset: url.charset };
                targetObject = network.dispatch("join", d);
            }

            if (!targetObject)
                return;
        }

        if (url.msg)
        {
            var msg;
            if (url.msg.indexOf("\01ACTION") == 0)
            {
                msg = filterOutput(url.msg, "ACTION", targetObject);
                targetObject.display(msg, "ACTION", "ME!",
                                     client.currentObject);
            }
            else
            {
                msg = filterOutput(url.msg, "PRIVMSG", targetObject);
                targetObject.display(msg, "PRIVMSG", "ME!",
                                     client.currentObject);
            }
            targetObject.say(msg);
            dispatch("set-current-view", { view: targetObject });
        }
    }
    else
    {
        if (!network.messages)
            network.displayHere (getMsg(MSG_NETWORK_OPENED, network.unicodeName));
        dispatch("set-current-view", { view: network });
    }
}

function updateProgress()
{
    var busy;
    var progress = -1;

    if ("busy" in client.currentObject)
        busy = client.currentObject.busy;

    if ("progress" in client.currentObject)
        progress = client.currentObject.progress;

    if (!busy)
        progress = 0;

    client.progressPanel.collapsed = !busy;
    client.progressBar.mode = (progress < 0 ? "undetermined" : "determined");
    if (progress >= 0)
        client.progressBar.value = progress;
}

function updateSecurityIcon()
{
    var o = getObjectDetails(client.currentObject);
    var securityButton = window.document.getElementById("security-button");
    securityButton.firstChild.value = "";
    securityButton.removeAttribute("level");
    securityButton.removeAttribute("tooltiptext");
    if (!o.server || !o.server.isConnected) // No server or connection?
    {
        securityButton.setAttribute("tooltiptext", MSG_SECURITY_INFO);
        return;
    }

    var securityState = o.server.connection.getSecurityState()
    switch (securityState[0])
    {
        case STATE_IS_SECURE:
            securityButton.firstChild.value = o.server.hostname;
            if (securityState[1] == STATE_SECURE_HIGH)
                securityButton.setAttribute("level", "high");
            else // Because low security is the worst we have when being secure
                securityButton.setAttribute("level", "low");

            // Add the tooltip:
            var issuer = o.server.connection.getCertificate().issuerOrganization;
            var tooltiptext = getMsg(MSG_SECURE_CONNECTION, issuer);
            securityButton.setAttribute("tooltiptext", tooltiptext);
            securityButton.firstChild.setAttribute("tooltiptext", tooltiptext);
            securityButton.lastChild.setAttribute("tooltiptext", tooltiptext);
            break;
        case STATE_IS_BROKEN:
            securityButton.setAttribute("level", "broken");
            // No break to make sure we get the correct tooltip
        case STATE_IS_INSECURE:
        default:
            securityButton.setAttribute("tooltiptext", MSG_SECURITY_INFO);
    }
}

function updateAppMotif(motifURL)
{
    var node = document.firstChild;
    while (node && ((node.nodeType != node.PROCESSING_INSTRUCTION_NODE) ||
                    !(/name="dyn-motif"/).test(node.data)))
    {
        node = node.nextSibling;
    }

    motifURL = motifURL.replace(/"/g, "%22");
    var dataStr = "href=\"" + motifURL + "\" name=\"dyn-motif\"";
    try 
    {
        // No dynamic style node yet.
        if (!node)
        {
            node = document.createProcessingInstruction("xml-stylesheet", dataStr);
            document.insertBefore(node, document.firstChild);
        }
        else
        {
            node.data = dataStr;
        }
    }
    catch (ex)
    {
        dd(formatException(ex));
        var err = ex.name;
        // Mozilla 1.0 doesn't like document.insertBefore(...,
        // document.firstChild); though it has a prototype for it -
        // check for the right error:
        if (err == "NS_ERROR_NOT_IMPLEMENTED")
        {
            display(MSG_NO_DYNAMIC_STYLE, MT_INFO);
            updateAppMotif = function() {};
        }
    }
}

function updateSpellcheck(value)
{
    value = value.toString();
    document.getElementById("input").setAttribute("spellcheck", value);
    document.getElementById("multiline-input").setAttribute("spellcheck",
                                                            value);
}

function updateNetwork()
{
    var o = getObjectDetails (client.currentObject);

    var lag = MSG_UNKNOWN;
    var nick = "";
    if (o.server)
    {
        if (o.server.me)
            nick = o.server.me.unicodeName;
        lag = (o.server.lag != -1) ? o.server.lag : MSG_UNKNOWN;
    }
    client.statusBar["header-url"].setAttribute("value",
                                                 client.currentObject.getURL());
    client.statusBar["header-url"].setAttribute("href",
                                                 client.currentObject.getURL());
    client.statusBar["header-url"].setAttribute("name",
                                                 client.currentObject.unicodeName);
}

function updateTitle (obj)
{
    if (!(("currentObject" in client) && client.currentObject) ||
        (obj && obj != client.currentObject))
        return;

    var tstring = MSG_TITLE_UNKNOWN;
    var o = getObjectDetails(client.currentObject);
    var net = o.network ? o.network.unicodeName : "";
    var nick = "";
    client.statusBar["server-nick"].disabled = false;

    switch (client.currentObject.TYPE)
    {
        case "IRCNetwork":
            var serv = "", port = "";
            if (client.currentObject.isConnected())
            {
                serv = o.server.hostname;
                port = o.server.port;
                if (o.server.me)
                    nick = o.server.me.unicodeName;
                tstring = getMsg(MSG_TITLE_NET_ON, [nick, net, serv, port]);
            }
            else
            {
                nick = client.currentObject.INITIAL_NICK;
                tstring = getMsg(MSG_TITLE_NET_OFF, [nick, net]);
            }
            break;

        case "IRCChannel":
            var chan = "", mode = "", topic = "";
            if ("me" in o.parent)
            {
                nick = o.parent.me.unicodeName;
                if (o.parent.me.canonicalName in client.currentObject.users)
                {
                    var cuser = client.currentObject.users[o.parent.me.canonicalName];
                    if (cuser.isFounder)
                        nick = "~" + nick;
                    else if (cuser.isAdmin)
                        nick = "&" + nick;
                    else if (cuser.isOp)
                        nick = "@" + nick;
                    else if (cuser.isHalfOp)
                        nick = "%" + nick;
                    else if (cuser.isVoice)
                        nick = "+" + nick;
                }
            }
            else
            {
                nick = MSG_TITLE_NONICK;
            }
            chan = o.channel.unicodeName;
            mode = o.channel.mode.getModeStr();
            if (!mode)
                mode = MSG_TITLE_NO_MODE;
            topic = o.channel.topic ? o.channel.topic : MSG_TITLE_NO_TOPIC;
            var re = /\x1f|\x02|\x0f|\x16|\x03([0-9]{1,2}(,[0-9]{1,2})?)?/g;
            topic = topic.replace(re, "");

            tstring = getMsg(MSG_TITLE_CHANNEL, [nick, chan, mode, topic]);
            break;

        case "IRCUser":
            nick = client.currentObject.unicodeName;
            var source = "";
            if (client.currentObject.name)
            {
                source = "<" + client.currentObject.name + "@" +
                    client.currentObject.host +">";
            }
            tstring = getMsg(MSG_TITLE_USER, [nick, source]);
            nick = "me" in o.parent ? o.parent.me.unicodeName : MSG_TITLE_NONICK;
            break;

        case "IRCClient":
            nick = client.prefs["nickname"];
            break;

        case "IRCDCCChat":
            client.statusBar["server-nick"].disabled = true;
            nick = o.chat.me.unicodeName;
            tstring = getMsg(MSG_TITLE_DCCCHAT, o.userName);
            break;

        case "IRCDCCFileTransfer":
            client.statusBar["server-nick"].disabled = true;
            nick = o.file.me.unicodeName;
            var data = [o.file.progress, o.file.filename, o.userName];
            if (o.file.state.dir == 1)
                tstring = getMsg(MSG_TITLE_DCCFILE_SEND, data);
            else
                tstring = getMsg(MSG_TITLE_DCCFILE_GET, data);
            break;
    }

    if (0 && !client.uiState["tabstrip"])
    {
        var actl = new Array();
        for (var i in client.activityList)
            actl.push ((client.activityList[i] == "!") ?
                       (Number(i) + 1) + "!" : (Number(i) + 1));
        if (actl.length > 0)
            tstring = getMsg(MSG_TITLE_ACTIVITY,
                             [tstring, actl.join (MSG_COMMASP)]);
    }

    document.title = tstring;
    client.statusBar["server-nick"].setAttribute("label", nick);
}

// Where 'right' is orientation, not wrong/right:
function updateUserlistSide(shouldBeLeft)
{
    var listParent = document.getElementById("tabpanels-contents-box");
    var isLeft = (listParent.childNodes[0].id == "user-list-box");
    if (isLeft == shouldBeLeft)
        return;
    if (shouldBeLeft) // Move from right to left.
    {
        listParent.insertBefore(listParent.childNodes[1], listParent.childNodes[0]);
        listParent.insertBefore(listParent.childNodes[2], listParent.childNodes[0]);
        listParent.childNodes[1].setAttribute("collapse", "before");
    }
    else // Move from left to right.
    {
        listParent.appendChild(listParent.childNodes[1]);
        listParent.appendChild(listParent.childNodes[0]);
        listParent.childNodes[1].setAttribute("collapse", "after");
    }
}

function multilineInputMode (state)
{
    var multiInput = document.getElementById("multiline-input");
    var multiInputBox = document.getElementById("multiline-box");
    var singleInput = document.getElementById("input");
    var singleInputBox = document.getElementById("singleline-box");
    var splitter = document.getElementById("input-splitter");
    var iw = document.getElementById("input-widgets");
    var h;

    client._mlMode = state;

    if (state)  /* turn on multiline input mode */
    {

        h = iw.getAttribute ("lastHeight");
        if (h)
            iw.setAttribute ("height", h); /* restore the slider position */

        singleInputBox.setAttribute ("collapsed", "true");
        splitter.setAttribute ("collapsed", "false");
        multiInputBox.setAttribute ("collapsed", "false");
        // multiInput should have the same direction as singleInput
        multiInput.setAttribute("dir", singleInput.getAttribute("dir"));
        multiInput.value = (client.input ? client.input.value : "");
        client.input = multiInput;
    }
    else  /* turn off multiline input mode */
    {
        h = iw.getAttribute ("height");
        iw.setAttribute ("lastHeight", h); /* save the slider position */
        iw.removeAttribute ("height");     /* let the slider drop */

        splitter.setAttribute ("collapsed", "true");
        multiInputBox.setAttribute ("collapsed", "true");
        singleInputBox.setAttribute ("collapsed", "false");
        // singleInput should have the same direction as multiInput
        singleInput.setAttribute("dir", multiInput.getAttribute("dir"));
        singleInput.value = (client.input ? client.input.value : "");
        client.input = singleInput;
    }

    client.input.focus();
}

function displayCertificateInfo()
{
    var o = getObjectDetails(client.currentObject);
    if (!o.server)
        return;

    if (!o.server.isSecure)
    {
        alert(getMsg(MSG_INSECURE_SERVER, o.server.hostname));
        return;
    }

    viewCert(o.server.connection.getCertificate());
}

function newInlineText (data, className, tagName)
{
    if (typeof tagName == "undefined")
        tagName = "html:span";

    var a = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                      tagName);
    if (className)
        a.setAttribute ("class", className);

    switch (typeof data)
    {
        case "string":
            a.appendChild (document.createTextNode (data));
            break;

        case "object":
            for (var p in data)
                if (p != "data")
                    a.setAttribute (p, data[p]);
                else
                    a.appendChild (document.createTextNode (data[p]));
            break;

        case "undefined":
            break;

        default:
            ASSERT(0, "INVALID TYPE ('" + typeof data + "') passed to " +
                   "newInlineText.");
            break;

    }

    return a;

}

function stringToMsg (message, obj)
{
    var ary = message.split ("\n");
    var span = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                         "html:span");
    var data = getObjectDetails(obj);

    if (ary.length == 1)
        client.munger.munge(ary[0], span, data);
    else
    {
        for (var l = 0; l < ary.length - 1; ++l)
        {
            client.munger.munge(ary[l], span, data);
            span.appendChild
                (document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:br"));
        }
        client.munger.munge(ary[l], span, data);
    }

    return span;
}

function getFrame()
{
    if (client.deck.childNodes.length == 0)
        return undefined;
    var panel = client.deck.selectedPanel;
    return getContentWindow(panel);
}

client.__defineGetter__ ("currentFrame", getFrame);

function setCurrentObject (obj)
{
    function clearList()
    {
        client.rdf.Unassert (client.rdf.resNullChan, client.rdf.resChanUser,
                             client.rdf.resNullUser, true);
    };

    if (!ASSERT(obj.messages, "INVALID OBJECT passed to setCurrentObject **"))
        return;

    if ("currentObject" in client && client.currentObject == obj)
        return;

    var tb, userList;
    userList = document.getElementById("user-list");

    if ("currentObject" in client && client.currentObject)
    {
        var co = client.currentObject;
        // Save any nicknames selected
        if (client.currentObject.TYPE == "IRCChannel")
            co.userlistSelection = getSelectedNicknames(userList);
        tb = getTabForObject(co);
    }
    if (tb)
    {
        tb.selected = false;
        tb.setAttribute ("state", "normal");
    }

    /* Unselect currently selected users.
     * If the splitter's collapsed, the userlist *isn't* visible, but we'll not
     * get told when it becomes visible, so update it even if it's only the
     * splitter visible. */
    if (isVisible("user-list-box") || isVisible("main-splitter"))
    {
        /* Remove currently selected items before this tree gets rerooted,
         * because it seems to remember the selections for eternity if not. */
        if (userList.view && userList.view.selection)
            userList.view.selection.select(-1);

        if (obj.TYPE == "IRCChannel")
        {
            client.rdf.setTreeRoot("user-list", obj.getGraphResource());
            reSortUserlist(userList);
            // Restore any selections previously made
            if (("userlistSelection" in obj) && obj.userlistSelection)
                setSelectedNicknames(userList, obj.userlistSelection);
        }
        else
        {
            var rdf = client.rdf;
            rdf.setTreeRoot("user-list", rdf.resNullChan);
            rdf.Assert (rdf.resNullChan, rdf.resChanUser, rdf.resNullUser,
                        true);
            setTimeout(clearList, 100);
        }
    }

    client.currentObject = obj;
    tb = dispatch("create-tab-for-view", { view: obj });
    if (tb)
    {
        tb.selected = true;
        tb.setAttribute ("state", "current");
    }

    var vk = Number(tb.getAttribute("viewKey"));
    delete client.activityList[vk];
    client.deck.selectedIndex = vk;

    // Style userlist and the like:
    updateAppMotif(obj.prefs["motif.current"]);

    updateTitle();
    updateProgress();
    updateSecurityIcon();

    scrollDown(obj.frame, false);

    // Input area should have the same direction as the output area
    if (("frame" in client.currentObject) &&
        client.currentObject.frame &&
        ("contentDocument" in client.currentObject.frame) &&
        client.currentObject.frame.contentDocument &&
        ("body" in client.currentObject.frame.contentDocument) &&
        client.currentObject.frame.contentDocument.body)
    {
        var contentArea = client.currentObject.frame.contentDocument.body;
        client.input.setAttribute("dir", contentArea.getAttribute("dir"));
    }
    client.input.focus();
}

function checkScroll(frame)
{
    var window = getContentWindow(frame);
    if (!window || !("document" in window))
        return false;

    return (window.document.height - window.innerHeight -
            window.pageYOffset) < 160;
}

function scrollDown(frame, force)
{
    var window = getContentWindow(frame);
    if (window && (force || checkScroll(frame)))
        window.scrollTo(0, window.document.height);
}

/* valid values for |what| are "superfluous", "activity", and "attention".
 * final value for state is dependant on priority of the current state, and the
 * new state. the priority is: normal < superfluous < activity < attention.
 */
function setTabState(source, what, callback)
{
    if (typeof source != "object")
    {
        if (!ASSERT(source in client.viewsArray,
                    "INVALID SOURCE passed to setTabState"))
            return;
        source = client.viewsArray[source].source;
    }

    callback = callback || false;

    var tb = source.dispatch("create-tab-for-view", { view: source });
    var vk = Number(tb.getAttribute("viewKey"));

    var current = ("currentObject" in client && client.currentObject == source);

    /* We want to play sounds if they're from a non-current view, or we don't
     * have focus at all. Also make sure stalk matches always play sounds.
     * Also make sure we don't play on the 2nd half of the flash (Callback).
     */
    if (!callback && (!window.isFocused || !current || (what == "attention")))
    {
        if (what == "attention")
            playEventSounds(source.TYPE, "stalk");
        else if (what == "activity")
            playEventSounds(source.TYPE, "chat");
        else if (what == "superfluous")
            playEventSounds(source.TYPE, "event");
    }

    // Only change the tab's colour if it's not the active view.
    if (!current)
    {
        var state = tb.getAttribute("state");
        if (state == what)
        {
            /* if the tab state has an equal priority to what we are setting
             * then blink it */
            if (client.prefs["activityFlashDelay"] > 0)
            {
                tb.setAttribute("state", "normal");
                setTimeout(setTabState, client.prefs["activityFlashDelay"],
                           vk, what, true);
            }
        }
        else
        {
            if (state == "normal" || state == "superfluous" ||
               (state == "activity" && what == "attention"))
            {
                /* if the tab state has a lower priority than what we are
                 * setting, change it to the new state */
                tb.setAttribute("state", what);
                /* we only change the activity list if priority has increased */
                if (what == "attention")
                   client.activityList[vk] = "!";
                else if (what == "activity")
                    client.activityList[vk] = "+";
                else
                {
                   /* this is functionally equivalent to "+" for now */
                   client.activityList[vk] = "-";
                }
                updateTitle();
            }
            else
            {
                /* the current state of the tab has a higher priority than the
                 * new state.
                 * blink the new lower state quickly, then back to the old */
                if (client.prefs["activityFlashDelay"] > 0)
                {
                    tb.setAttribute("state", what);
                    setTimeout(setTabState,
                               client.prefs["activityFlashDelay"], vk,
                               state, true);
                }
            }
        }
    }
}

function notifyAttention (source)
{
    if (typeof source != "object")
        source = client.viewsArray[source].source;

    if (client.currentObject != source)
    {
        var tb = dispatch("create-tab-for-view", { view: source });
        var vk = Number(tb.getAttribute("viewKey"));

        tb.setAttribute ("state", "attention");
        client.activityList[vk] = "!";
        updateTitle();
    }

    if (client.prefs["notify.aggressive"])
        window.getAttention();

}

function setDebugMode(mode)
{
    if (mode.indexOf("t") != -1)
        client.debugHook.enabled = true;
    else
        client.debugHook.enabled = false;

    if (mode.indexOf("c") != -1)
        client.dbgContexts = true;
    else
        delete client.dbgContexts;

    if (mode.indexOf("d") != -1)
        client.dbgDispatch = true;
    else
        delete client.dbgDispatch;
}

function setListMode(mode)
{
    var elem = document.getElementById("user-list");
    if (mode)
        elem.setAttribute("mode", mode);
    else
        elem.removeAttribute("mode");
    updateUserList();
}

function updateUserList()
{
    var node, chan;

    node = document.getElementById("user-list");
    if (!node.view)
        return;

    // We'll lose the selection in a bit, if we don't save it if necessary:
    if (("currentObject" in client) && client.currentObject &&
        client.currentObject.TYPE == "IRCChannel")
    {
        chan = client.currentObject;
        chan.userlistSelection = getSelectedNicknames(node, chan);
    }
    reSortUserlist(node);

    // If this is a channel, restore the selection in the userlist.
    if (chan)
        setSelectedNicknames(node, client.currentObject.userlistSelection);
}

function reSortUserlist(node)
{
    const nsIXULSortService = Components.interfaces.nsIXULSortService;
    const isupports_uri = "@mozilla.org/xul/xul-sort-service;1";

    var xulSortService =
        Components.classes[isupports_uri].getService(nsIXULSortService);
    if (!xulSortService)
        return;

    var sortResource;

    if (client.prefs["sortUsersByMode"])
        sortResource = RES_PFX + "sortname";
    else
        sortResource = RES_PFX + "unicodeName";

    try
    {
        if ("sort" in xulSortService)
            xulSortService.sort(node, sortResource, "ascending");
        else
            xulSortService.Sort(node, sortResource, "ascending");
    }
    catch(ex)
    {
        dd("Exception calling xulSortService.sort()");
    }
}

function getFrameForDOMWindow(window)
{
    var frame;
    for (var i = 0; i < client.deck.childNodes.length; i++)
    {
        frame = client.deck.childNodes[i];
        if (getContentWindow(frame) == window)
            return frame;
    }
    return undefined;
}

function replaceColorCodes(msg)
{
    // mIRC codes: underline, bold, Original (reset), colors, reverse colors.
    msg = msg.replace(/(^|[^%])%U/g, "$1\x1f");
    msg = msg.replace(/(^|[^%])%B/g, "$1\x02");
    msg = msg.replace(/(^|[^%])%O/g, "$1\x0f");
    msg = msg.replace(/(^|[^%])%C/g, "$1\x03");
    msg = msg.replace(/(^|[^%])%R/g, "$1\x16");
    // %%[UBOCR] --> %[UBOCR].
    msg = msg.replace(/%(%[UBOCR])/g, "$1");

    return msg;
}

function decodeColorCodes(msg)
{
    // %[UBOCR] --> %%[UBOCR].
    msg = msg.replace(/(%[UBOCR])/g, "%$1");
    // Put %-codes back in place of special character codes.
    msg = msg.replace(/\x1f/g, "%U");
    msg = msg.replace(/\x02/g, "%B");
    msg = msg.replace(/\x0f/g, "%O");
    msg = msg.replace(/\x03/g, "%C");
    msg = msg.replace(/\x16/g, "%R");

    return msg;
}

client.progressListener = new Object();

client.progressListener.QueryInterface =
function qi(iid)
{
    return this;
}

client.progressListener.onStateChange =
function client_statechange (webProgress, request, stateFlags, status)
{
    const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
    const START = nsIWebProgressListener.STATE_START;
    const STOP = nsIWebProgressListener.STATE_STOP;
    const IS_NETWORK = nsIWebProgressListener.STATE_IS_NETWORK;
    const IS_DOCUMENT = nsIWebProgressListener.STATE_IS_DOCUMENT;

    var frame;

    // We only care about the initial start of loading, not the loading of
    // and page sub-components (IS_DOCUMENT, etc.).
    if ((stateFlags & START) && (stateFlags & IS_NETWORK) &&
        (stateFlags & IS_DOCUMENT))
    {
        frame = getFrameForDOMWindow(webProgress.DOMWindow);
        if (!frame)
        {
            dd("can't find frame for window (start)");
            try
            {
                webProgress.removeProgressListener(this);
            }
            catch(ex)
            {
                dd("Exception removing progress listener (start): " + ex);
            }
        }
    }
    // We only want to know when the *network* stops, not the page's
    // individual components (STATE_IS_REQUEST/STATE_IS_DOCUMENT/somesuch).
    else if ((stateFlags & STOP) && (stateFlags & IS_NETWORK))
    {
        frame = getFrameForDOMWindow(webProgress.DOMWindow);
        if (!frame)
        {
            dd("can't find frame for window (stop)");
            try
            {
                webProgress.removeProgressListener(this);
            }
            catch(ex)
            {
                dd("Exception removing progress listener (stop): " + ex);
            }
        }
        else
        {
            var cwin = getContentWindow(frame);
            if (cwin && "initOutputWindow" in cwin)
            {
                cwin.getMsg = getMsg;
                cwin.initOutputWindow(client, frame.source, onMessageViewClick);
                cwin.changeCSS(frame.source.getFontCSS("data"), "cz-fonts");
                scrollDown(frame, true);

                try
                {
                    webProgress.removeProgressListener(this);
                }
                catch(ex)
                {
                    dd("Exception removing progress listener (done): " + ex);
                }
            }
        }
    }
}

client.progressListener.onProgressChange =
function client_progresschange (webProgress, request, currentSelf, totalSelf,
                                currentMax, selfMax)
{
}

client.progressListener.onLocationChange =
function client_locationchange (webProgress, request, uri)
{
}

client.progressListener.onStatusChange =
function client_statuschange (webProgress, request, status, message)
{
}

client.progressListener.onSecurityChange =
function client_securitychange (webProgress, request, state)
{
}

function syncOutputFrame(obj, nesting)
{
    const nsIWebProgress = Components.interfaces.nsIWebProgress;
    const WINDOW = nsIWebProgress.NOTIFY_STATE_WINDOW;
    const NETWORK = nsIWebProgress.NOTIFY_STATE_NETWORK;
    const ALL = nsIWebProgress.NOTIFY_ALL;

    var iframe = obj.frame;

    function tryAgain(nLevel)
    {
        syncOutputFrame(obj, nLevel);
    };

    if (typeof nesting != "number")
        nesting = 0;

    if (nesting > 10)
    {
        dd("Aborting syncOutputFrame, taken too many tries.");
        return;
    }

    try
    {
        if (("contentDocument" in iframe) && ("webProgress" in iframe))
        {
            var url = obj.prefs["outputWindowURL"];
            iframe.addProgressListener(client.progressListener, ALL);
            iframe.loadURI(url);
        }
        else
        {
            setTimeout(tryAgain, 500, nesting + 1);
        }
    }
    catch (ex)
    {
        dd("caught exception showing session view, will try again later.");
        dd(dumpObjectTree(ex) + "\n");
        setTimeout(tryAgain, 500, nesting + 1);
    }
}

function createMessages(source)
{
    playEventSounds(source.TYPE, "start");

    source.messages =
    document.createElementNS ("http://www.w3.org/1999/xhtml",
                              "html:table");

    source.messages.setAttribute ("class", "msg-table");
    source.messages.setAttribute ("view-type", source.TYPE);
    var tbody =
        document.createElementNS ("http://www.w3.org/1999/xhtml",
                                  "html:tbody");
    source.messages.appendChild (tbody);
    source.messageCount = 0;
}

/* gets the toolbutton associated with an object
 * if |create| is present, and true, create if not found */
function getTabForObject (source, create)
{
    var name;

    if (!ASSERT(source, "UNDEFINED passed to getTabForObject"))
        return null;

    if ("viewName" in source)
    {
        name = source.viewName;
    }
    else
    {
        ASSERT(0, "INVALID OBJECT passed to getTabForObject");
        return null;
    }

    var tb, id = "tb[" + name + "]";
    var matches = 1;

    for (var i in client.viewsArray)
    {
        if (client.viewsArray[i].source == source)
        {
            tb = client.viewsArray[i].tb;
            break;
        }
        else
            if (client.viewsArray[i].tb.getAttribute("id") == id)
                id = "tb[" + name + "<" + (++matches) + ">]";
    }

    if (!tb && create) /* not found, create one */
    {
        if (!("messages" in source) || source.messages == null)
            createMessages(source);
        var views = document.getElementById ("views-tbar-inner");
        tb = document.createElement ("tab");
        tb.setAttribute("ondraggesture",
                        "nsDragAndDrop.startDrag(event, tabDNDObserver);");
        tb.setAttribute("href", source.getURL());
        tb.setAttribute("name", source.unicodeName);
        tb.setAttribute("onclick", "onTabClick(event, " + id.quote() + ");");
        // This wouldn't be here if there was a supported CSS property for it.
        tb.setAttribute("crop", "center");
        tb.setAttribute("context", "context:tab");
        tb.setAttribute("tooltip", "xul-tooltip-node");
        tb.setAttribute("class", "tab-bottom view-button");
        tb.setAttribute("id", id);
        tb.setAttribute("state", "normal");

        client.viewsArray.push ({source: source, tb: tb});
        tb.setAttribute ("viewKey", client.viewsArray.length - 1);
        tb.view = source;

        if (matches > 1)
            tb.setAttribute("label", name + "<" + matches + ">");
        else
            tb.setAttribute("label", name);

        views.appendChild(tb);

        var browser = document.createElement ("browser");
        browser.setAttribute("class", "output-container");
        // Only use type="chrome" if the host app supports it properly:
        if (client.hostCompat.typeChromeBrowser)
            browser.setAttribute("type", "chrome");
        else
            browser.setAttribute("type", "content");
        browser.setAttribute("flex", "1");
        browser.setAttribute("tooltip", "html-tooltip-node");
        browser.setAttribute("context", "context:messages");
        //browser.setAttribute ("onload", "scrollDown(true);");
        browser.setAttribute("onclick",
                             "return onMessageViewClick(event)");
        browser.setAttribute("onmousedown",
                             "return onMessageViewMouseDown(event)");
        browser.setAttribute("ondragover",
                             "nsDragAndDrop.dragOver(event, " +
                             "contentDropObserver);");
        browser.setAttribute("ondragdrop",
                             "nsDragAndDrop.drop(event, contentDropObserver);");
        browser.setAttribute("ondraggesture",
                             "nsDragAndDrop.startDrag(event, " +
                             "contentAreaDNDObserver);");
        browser.source = source;
        source.frame = browser;
        ASSERT(client.deck, "no deck?");
        client.deck.appendChild (browser);
        syncOutputFrame (source);
    }

    return tb;

}

var contentDropObserver = new Object();

contentDropObserver.onDragOver =
function tabdnd_dover (aEvent, aFlavour, aDragSession)
{
    if (aEvent.getPreventDefault())
        return;

    if (aDragSession.sourceDocument == aEvent.view.document)
    {
        aDragSession.canDrop = false;
        return;
    }
}

contentDropObserver.onDrop =
function tabdnd_drop (aEvent, aXferData, aDragSession)
{
    var url = transferUtils.retrieveURLFromData(aXferData.data,
                                                aXferData.flavour.contentType);
    if (!url || url.search(client.linkRE) == -1)
        return;

    if (url.search(/\.css$/i) != -1  && confirm (getMsg(MSG_TABDND_DROP, url)))
        dispatch("motif", {"motif": url});
    else if (url.search(/^ircs?:\/\//i) != -1)
        dispatch("goto-url", {"url": url});
}

contentDropObserver.getSupportedFlavours =
function tabdnd_gsf ()
{
    var flavourSet = new FlavourSet();
    flavourSet.appendFlavour("text/x-moz-url");
    flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
    flavourSet.appendFlavour("text/unicode");
    return flavourSet;
}

var tabDNDObserver = new Object();

tabDNDObserver.onDragStart =
function tabdnd_dstart (aEvent, aXferData, aDragAction)
{
    var tb = aEvent.currentTarget;
    var href = tb.getAttribute("href");
    var name = tb.getAttribute("name");

    aXferData.data = new TransferData();
    /* x-moz-url has the format "<url>\n<name>", goodie */
    aXferData.data.addDataForFlavour("text/x-moz-url", href + "\n" + name);
    aXferData.data.addDataForFlavour("text/unicode", href);
    aXferData.data.addDataForFlavour("text/html", "<a href='" + href + "'>" +
                                     name + "</a>");
}

var userlistDNDObserver = new Object();

userlistDNDObserver.onDragStart =
function userlistdnd_dstart(event, transferData, dragAction)
{
    var col = new Object(), row = new Object(), cell = new Object();
    var tree = document.getElementById('user-list');
    tree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, cell);
    // Check whether we're actually on a normal row and cell
    if (!cell.value || (row.value == -1))
        return;
    var user = tree.contentView.getItemAtIndex(row.value).firstChild.firstChild;
    var nickname = user.getAttribute("unicodeName");

    transferData.data = new TransferData();
    transferData.data.addDataForFlavour("text/unicode", nickname);
}

function deleteTab (tb)
{
    if (!ASSERT(tb.hasAttribute("viewKey"),
                "INVALID OBJECT passed to deleteTab (" + tb + ")"))
    {
        return null;
    }

    var i;
    var key = Number(tb.getAttribute("viewKey"));

    /* re-index higher toolbuttons */
    for (i = key + 1; i < client.viewsArray.length; i++)
    {
        client.viewsArray[i].tb.setAttribute ("viewKey", i - 1);
    }
    arrayRemoveAt(client.viewsArray, key);
    var tbinner = document.getElementById("views-tbar-inner");
    tbinner.removeChild(tb);

    return key;
}

function filterOutput(msg, msgtype, dest)
{
    if ("outputFilters" in client)
    {
        for (var f in client.outputFilters)
        {
            if (client.outputFilters[f].enabled)
                msg = client.outputFilters[f].func(msg, msgtype, dest);
        }
    }

    return msg;
}

function updateTimestamps(view)
{
    if (!("messages" in view))
        return;

    view._timestampLast = "";
    var node = view.messages.firstChild.firstChild;
    var nested;
    while (node)
    {
        if(node.className == "msg-nested-tr")
        {
            nested = node.firstChild.firstChild.firstChild.firstChild;
            while (nested)
            {
                updateTimestampFor(view, nested);
                nested = nested.nextSibling;
            }
        }
        else
        {
            updateTimestampFor(view, node);
        }
        node = node.nextSibling;
    }
}

function updateTimestampFor(view, displayRow)
{
    var time = new Date(1 * displayRow.getAttribute("timestamp"));
    var tsCell = displayRow.firstChild;
    if (!tsCell)
        return;

    var fmt;
    if (view.prefs["timestamps"])
        fmt = strftime(view.prefs["timestamps.display"], time);

    while (tsCell.lastChild)
        tsCell.removeChild(tsCell.lastChild);

    if (fmt && (!client.prefs["collapseMsgs"] || (fmt != view._timestampLast)))
        tsCell.appendChild(document.createTextNode(fmt));
    view._timestampLast = fmt;
}

client.updateMenus =
function c_updatemenus(menus)
{
    // Don't bother if the menus aren't even created yet.
    if (!client.initialized)
        return null;

    return this.menuManager.updateMenus(document, menus);
}

client.adoptNode =
function cli_adoptnode(node, doc)
{
    try
    {
        doc.adoptNode(node);
    }
    catch(ex)
    {
        dd(formatException(ex));
        var err = ex.name;
        // TypeError from before adoptNode was added; NOT_IMPL after.
        if ((err == "TypeError") || (err == "NS_ERROR_NOT_IMPLEMENTED"))
            client.adoptNode = cli_adoptnode_noop;
    }
    return node;
}

function cli_adoptnode_noop(node, doc)
{
    return node;
}

client.addNetwork =
function cli_addnet(name, serverList, temporary)
{
    client.networks[name] =
        new CIRCNetwork(name, serverList, client.eventPump, temporary);
}

client.removeNetwork =
function cli_removenet(name)
{
    // Allow network a chance to clean up any mess.
    if (typeof client.networks[name].destroy == "function")
        client.networks[name].destroy();

    delete client.networks[name];
}

client.connectToNetwork =
function cli_connect(networkOrName, requireSecurity)
{
    var network;
    var name;


    if (isinstance(networkOrName, CIRCNetwork))
    {
        network = networkOrName;
    }
    else
    {
        name = networkOrName;

        if (!(name in client.networks))
        {
            display(getMsg(MSG_ERR_UNKNOWN_NETWORK, name), MT_ERROR);
            return null;
        }

        network = client.networks[name];
    }
    name = network.unicodeName;

    if (!("messages" in network))
        network.displayHere(getMsg(MSG_NETWORK_OPENED, name));

    dispatch("set-current-view", { view: network });

    if (network.isConnected())
    {
        network.display(getMsg(MSG_ALREADY_CONNECTED, name));
        return network;
    }

    if (network.state != NET_OFFLINE)
        return network;

    if (network.prefs["nickname"] == DEFAULT_NICK)
        network.prefs["nickname"] = prompt(MSG_ENTER_NICK, DEFAULT_NICK);

    if (!("connecting" in network))
        network.display(getMsg(MSG_NETWORK_CONNECTING, name));

    network.connect(requireSecurity);

    network.updateHeader();
    client.updateHeader();
    updateTitle();

    return network;
}


client.getURL =
function cli_geturl ()
{
    return "irc://";
}

client.load =
function cli_load(url, scope)
{
    if (!("_loader" in client))
    {
        const LOADER_CTRID = "@mozilla.org/moz/jssubscript-loader;1";
        const mozIJSSubScriptLoader =
            Components.interfaces.mozIJSSubScriptLoader;

        var cls;
        if ((cls = Components.classes[LOADER_CTRID]))
            client._loader = cls.getService(mozIJSSubScriptLoader);
    }

    return client._loader.loadSubScript(url, scope);
}

client.sayToCurrentTarget =
function cli_say(msg)
{
    if ("say" in client.currentObject)
    {
        client.currentObject.dispatch("say", {message: msg});
        return;
    }

    switch (client.currentObject.TYPE)
    {
        case "IRCClient":
            dispatch("eval", {expression: msg});
            break;

        default:
            if (msg != "")
                display(MSG_ERR_NO_DEFAULT, MT_ERROR);
            break;
    }
}

CIRCNetwork.prototype.__defineGetter__("prefs", net_getprefs);
function net_getprefs()
{
    if (!("_prefs" in this))
    {
        this._prefManager = getNetworkPrefManager(this);
        this._prefs = this._prefManager.prefs;
    }

    return this._prefs;
}

CIRCNetwork.prototype.__defineGetter__("prefManager", net_getprefmgr);
function net_getprefmgr()
{
    if (!("_prefManager" in this))
    {
        this._prefManager = getNetworkPrefManager(this);
        this._prefs = this._prefManager.prefs;
    }

    return this._prefManager;
}

CIRCServer.prototype.__defineGetter__("prefs", srv_getprefs);
function srv_getprefs()
{
    return this.parent.prefs;
}

CIRCServer.prototype.__defineGetter__("prefManager", srv_getprefmgr);
function srv_getprefmgr()
{
    return this.parent.prefManager;
}

CIRCChannel.prototype.__defineGetter__("prefs", chan_getprefs);
function chan_getprefs()
{
    if (!("_prefs" in this))
    {
        this._prefManager = getChannelPrefManager(this);
        this._prefs = this._prefManager.prefs;
    }

    return this._prefs;
}

CIRCChannel.prototype.__defineGetter__("prefManager", chan_getprefmgr);
function chan_getprefmgr()
{
    if (!("_prefManager" in this))
    {
        this._prefManager = getChannelPrefManager(this);
        this._prefs = this._prefManager.prefs;
    }

    return this._prefManager;
}

CIRCUser.prototype.__defineGetter__("prefs", usr_getprefs);
function usr_getprefs()
{
    if (!("_prefs" in this))
    {
        this._prefManager = getUserPrefManager(this);
        this._prefs = this._prefManager.prefs;
    }

    return this._prefs;
}

CIRCUser.prototype.__defineGetter__("prefManager", usr_getprefmgr);
function usr_getprefmgr()
{
    if (!("_prefManager" in this))
    {
        this._prefManager = getUserPrefManager(this);
        this._prefs = this._prefManager.prefs;
    }

    return this._prefManager;
}

CIRCDCCUser.prototype.__defineGetter__("prefs", dccusr_getprefs);
function dccusr_getprefs()
{
    if (!("_prefs" in this))
    {
        this._prefManager = getDCCUserPrefManager(this);
        this._prefs = this._prefManager.prefs;
    }

    return this._prefs;
}

CIRCDCCUser.prototype.__defineGetter__("prefManager", dccusr_getprefmgr);
function dccusr_getprefmgr()
{
    if (!("_prefManager" in this))
    {
        this._prefManager = getDCCUserPrefManager(this);
        this._prefs = this._prefManager.prefs;
    }

    return this._prefManager;
}

CIRCDCCChat.prototype.__defineGetter__("prefs", dccchat_getprefs);
function dccchat_getprefs()
{
    return this.user.prefs;
}

CIRCDCCChat.prototype.__defineGetter__("prefManager", dccchat_getprefmgr);
function dccchat_getprefmgr()
{
    return this.user.prefManager;
}

CIRCDCCFileTransfer.prototype.__defineGetter__("prefs", dccfile_getprefs);
function dccfile_getprefs()
{
    return this.user.prefs;
}

CIRCDCCFileTransfer.prototype.__defineGetter__("prefManager", dccfile_getprefmgr);
function dccfile_getprefmgr()
{
    return this.user.prefManager;
}

CIRCNetwork.prototype.display =
function net_display (message, msgtype, sourceObj, destObj)
{
    var o = getObjectDetails(client.currentObject);

    if (client.SLOPPY_NETWORKS && client.currentObject != this &&
        o.network == this && o.server && o.server.isConnected)
    {
        client.currentObject.display (message, msgtype, sourceObj, destObj);
    }
    else
    {
        this.displayHere (message, msgtype, sourceObj, destObj);
    }
}

CIRCUser.prototype.display =
function usr_display(message, msgtype, sourceObj, destObj)
{
    if ("messages" in this)
    {
        this.displayHere (message, msgtype, sourceObj, destObj);
    }
    else
    {
        var o = getObjectDetails(client.currentObject);
        if (o.server && o.server.isConnected &&
            o.network == this.parent.parent &&
            client.currentObject.TYPE != "IRCUser")
            client.currentObject.display (message, msgtype, sourceObj, destObj);
        else
            this.parent.parent.displayHere (message, msgtype, sourceObj,
                                            destObj);
    }
}

CIRCDCCChat.prototype.display =
CIRCDCCFileTransfer.prototype.display =
function dcc_display(message, msgtype, sourceObj, destObj)
{
    var o = getObjectDetails(client.currentObject);

    if ("messages" in this)
        this.displayHere(message, msgtype, sourceObj, destObj);
    else
        client.currentObject.display(message, msgtype, sourceObj, destObj);
}

function feedback(e, message, msgtype, sourceObj, destObj)
{
    if ("isInteractive" in e && e.isInteractive)
        display(message, msgtype, sourceObj, destObj);
}

CIRCChannel.prototype.feedback =
CIRCNetwork.prototype.feedback =
CIRCUser.prototype.feedback =
CIRCDCCChat.prototype.feedback =
CIRCDCCFileTransfer.prototype.feedback =
client.feedback =
function this_feedback(e, message, msgtype, sourceObj, destObj)
{
    if ("isInteractive" in e && e.isInteractive)
        this.displayHere(message, msgtype, sourceObj, destObj);
}

function display (message, msgtype, sourceObj, destObj)
{
    client.currentObject.display (message, msgtype, sourceObj, destObj);
}

client.getFontCSS =
CIRCNetwork.prototype.getFontCSS =
CIRCChannel.prototype.getFontCSS =
CIRCUser.prototype.getFontCSS =
CIRCDCCChat.prototype.getFontCSS =
CIRCDCCFileTransfer.prototype.getFontCSS =
function this_getFontCSS(format)
{
    /* Wow, this is cool. We just put together a CSS-rule string based on the
     * font preferences. *This* is what CSS is all about. :)
     * We also provide a "data: URL" format, to simplify other code.
     */
    var css;
    var fs;
    var fn;

    if (this.prefs["font.family"] != "default")
        fn = "font-family: " + this.prefs["font.family"] + ";";
    else
        fn = "font-family: inherit;";
    if (this.prefs["font.size"] != 0)
        fs = "font-size: " + this.prefs["font.size"] + "pt;";
    else
        fs = "font-size: medium;";

    css = "body.chatzilla-body { " + fs + fn + " }";

    if (format == "data")
        return "data:text/css," + encodeURIComponent(css);
    return css;
}

client.display =
client.displayHere =
CIRCNetwork.prototype.displayHere =
CIRCChannel.prototype.display =
CIRCChannel.prototype.displayHere =
CIRCUser.prototype.displayHere =
CIRCDCCChat.prototype.displayHere =
CIRCDCCFileTransfer.prototype.displayHere =
function __display(message, msgtype, sourceObj, destObj)
{
    // We need a message type, assume "INFO".
    if (!msgtype)
        msgtype = MT_INFO;

    var blockLevel = false; /* true if this row should be rendered at block
                             * level, (like, if it has a really long nickname
                             * that might disturb the rest of the layout)     */
    var o = getObjectDetails(this);           /* get the skinny on |this|     */

    // Get the 'me' object, so we can be sure to get the attributes right.
    var me;
    if ("me" in this)
        me = this.me;
    else if (o.server && "me" in o.server)
        me = o.server.me;

    // Let callers get away with "ME!" and we have to substitute here.
    if (sourceObj == "ME!")
        sourceObj = me;
    if (destObj == "ME!")
        destObj = me;

    // Get the TYPE of the source object.
    var fromType = (sourceObj && sourceObj.TYPE) ? sourceObj.TYPE : "unk";
    // Is the source a user?
    var fromUser = (fromType.search(/IRC.*User/) != -1);
    // Get some sort of "name" for the source.
    var fromAttr = "";
    if (sourceObj)
    {
        if ("unicodeName" in sourceObj)
            fromAttr = sourceObj.unicodeName;
        else if ("name" in sourceObj)
            fromAttr = sourceObj.name;
        else
            fromAttr = sourceObj.viewName;
    }
    // Attach "ME!" if appropriate, so motifs can style differently.
    if (sourceObj == me)
        fromAttr = fromAttr + " ME!";

    // Get the dest TYPE too...
    var toType = (destObj) ? destObj.TYPE : "unk";
    // Is the dest a user?
    var toUser = (toType.search(/IRC.*User/) != -1);
    // Get a dest name too...
    var toAttr = "";
    if (destObj)
    {
        if ("unicodeName" in destObj)
            toAttr = destObj.unicodeName;
        else if ("name" in destObj)
            toAttr = destObj.name;
        else
            toAttr = destObj.viewName;
    }
    // Also do "ME!" work for the dest.
    if (destObj && destObj == me)
        toAttr = me.unicodeName + " ME!";

    /* isImportant means to style the messages as important, and flash the
     * window, getAttention means just flash the window. */
    var isImportant = false, getAttention = false, isSuperfluous = false;
    var viewType = this.TYPE;
    var code;
    var time = new Date();

    var timeStamp = strftime(this.prefs["timestamps.log"], time);

    // Statusbar text, and the line that gets saved to the log.
    var statusString;
    var logStringPfx = timeStamp + " ";
    var logStrings = new Array();

    if (fromUser)
    {
        statusString = getMsg(MSG_FMT_STATUS,
                              [timeStamp,
                               sourceObj.unicodeName + "!" +
                               sourceObj.name + "@" + sourceObj.host]);
    }
    else
    {
        var name;
        if (sourceObj)
            name = sourceObj.viewName;
        else
            name = this.viewName;

        statusString = getMsg(MSG_FMT_STATUS,
                              [timeStamp, name]);
    }

    // The table row, and it's attributes.
    var msgRow = document.createElementNS("http://www.w3.org/1999/xhtml",
                                          "html:tr");
    msgRow.setAttribute("class", "msg");
    msgRow.setAttribute("msg-type", msgtype);
    msgRow.setAttribute("msg-dest", toAttr);
    msgRow.setAttribute("dest-type", toType);
    msgRow.setAttribute("view-type", viewType);
    msgRow.setAttribute("statusText", statusString);
    msgRow.setAttribute("timestamp", Number(time));
    if (fromAttr)
    {
        if (fromUser)
            msgRow.setAttribute("msg-user", fromAttr);
        else
            msgRow.setAttribute("msg-source", fromAttr);
    }
    if (isImportant)
        msgTimestamp.setAttribute ("important", "true");

    // Timestamp cell.
    var msgRowTimestamp = document.createElementNS("http://www.w3.org/1999/xhtml",
                                                   "html:td");
    msgRowTimestamp.setAttribute("class", "msg-timestamp");

    var canMergeData;
    var msgRowSource, msgRowType, msgRowData;
    if (fromUser && msgtype.match(/^(PRIVMSG|ACTION|NOTICE)$/))
    {
        var nick = sourceObj.unicodeName;
        var decorSt = "";
        var decorEn = "";

        // Set default decorations.
        if (msgtype == "ACTION")
        {
            decorSt = "* ";
        }
        else
        {
            decorSt = "<";
            decorEn = ">";
        }

        var nickURL;
        if ((sourceObj != me) && ("getURL" in sourceObj))
            nickURL = sourceObj.getURL();

        if (sourceObj != me)
        {
            // Not from us...
            if (destObj == me)
            {
                // ...but to us. Messages from someone else to us.

                getAttention = true;
                this.defaultCompletion = "/msg " + nick + " ";

                // If this is a private message, and it's not in a query view,
                // use *nick* instead of <nick>.
                if ((msgtype != "ACTION") && (this.TYPE != "IRCUser"))
                {
                    decorSt = "*";
                    decorEn = "*";
                }
            }
            else
            {
                // ...or to us. Messages from someone else to channel or similar.

                if ((typeof message == "string") && me)
                {
                    isImportant = msgIsImportant(message, nick, o.network);
                    if (isImportant)
                    {
                        this.defaultCompletion = nick +
                            client.prefs["nickCompleteStr"] + " ";
                    }
                }
            }
        }
        else
        {
            // Messages from us, on a channel or network view, to a user
            if (toUser && (this.TYPE != "IRCUser"))
            {
                nick = destObj.unicodeName;
                decorSt = ">";
                decorEn = "<";
            }
        }
        // Log the nickname in the same format as we'll let the user copy.
        logStringPfx += decorSt + nick + decorEn + " ";

        // Mark makes alternate "talkers" show up in different shades.
        //if (!("mark" in this))
        //    this.mark = "odd";

        if (!("lastNickDisplayed" in this) || this.lastNickDisplayed != nick)
        {
            this.lastNickDisplayed = nick;
            this.mark = (("mark" in this) && this.mark == "even") ? "odd" : "even";
        }

        msgRowSource = document.createElementNS("http://www.w3.org/1999/xhtml",
                                             "html:td");
        msgRowSource.setAttribute("class", "msg-user");

        // Make excessive nicks get shunted.
        if (nick && (nick.length > client.MAX_NICK_DISPLAY))
            blockLevel = true;

        if (decorSt)
            msgRowSource.appendChild(newInlineText(decorSt, "chatzilla-decor"));
        if (nickURL)
        {
            var nick_anchor =
                document.createElementNS("http://www.w3.org/1999/xhtml",
                                         "html:a");
            nick_anchor.setAttribute("class", "chatzilla-link");
            nick_anchor.setAttribute("href", nickURL);
            nick_anchor.appendChild(newInlineText(nick));
            msgRowSource.appendChild(nick_anchor);
        }
        else
        {
            msgRowSource.appendChild(newInlineText(nick));
        }
        if (decorEn)
            msgRowSource.appendChild(newInlineText(decorEn, "chatzilla-decor"));
        canMergeData = this.prefs["collapseMsgs"];
    }
    else
    {
        isSuperfluous = true;
        if (!client.debugHook.enabled && msgtype in client.responseCodeMap)
        {
            code = client.responseCodeMap[msgtype];
        }
        else
        {
            if (!client.debugHook.enabled && client.HIDE_CODES)
                code = client.DEFAULT_RESPONSE_CODE;
            else
                code = "[" + msgtype + "]";
        }

        /* Display the message code */
        msgRowType = document.createElementNS("http://www.w3.org/1999/xhtml",
                                           "html:td");
        msgRowType.setAttribute("class", "msg-type");

        msgRowType.appendChild(newInlineText(code));
        logStringPfx += code + " ";
    }

    if (message)
    {
        msgRowData = document.createElementNS("http://www.w3.org/1999/xhtml",
                                           "html:td");
        msgRowData.setAttribute("class", "msg-data");

        var tmpMsgs = message;
        if (typeof message == "string")
        {
            msgRowData.appendChild(stringToMsg(message, this));
        }
        else
        {
            msgRowData.appendChild(message);
            tmpMsgs = tmpMsgs.innerHTML.replace(/<[^<]*>/g, "");
        }
        tmpMsgs = tmpMsgs.split(/\r?\n/);
        for (var l = 0; l < tmpMsgs.length; l++)
            logStrings[l] = logStringPfx + tmpMsgs[l];
    }

    if ("mark" in this)
        msgRow.setAttribute("mark", this.mark);

    if (isImportant)
        msgRow.setAttribute ("important", "true");

    // Timestamps first...
    msgRow.appendChild(msgRowTimestamp);
    // Now do the rest of the row, after block-level stuff.
    if (msgRowSource)
        msgRow.appendChild(msgRowSource);
    else
        msgRow.appendChild(msgRowType);
    if (msgRowData)
        msgRow.appendChild(msgRowData);
    updateTimestampFor(this, msgRow);

    if (blockLevel)
    {
        /* putting a div here crashes mozilla, so fake it with nested tables
         * for now */
        var tr = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:tr");
        tr.setAttribute ("class", "msg-nested-tr");
        var td = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:td");
        td.setAttribute ("class", "msg-nested-td");
        td.setAttribute ("colspan", "3");

        tr.appendChild(td);
        var table = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                              "html:table");
        table.setAttribute ("class", "msg-nested-table");

        td.appendChild (table);
        var tbody =  document.createElementNS ("http://www.w3.org/1999/xhtml",
                                               "html:tbody");

        tbody.appendChild(msgRow);
        table.appendChild(tbody);
        msgRow = tr;
    }

    // Actually add the item.
    addHistory (this, msgRow, canMergeData);

    // Update attention states...
    if (isImportant || getAttention)
    {
        setTabState(this, "attention");
        if (client.prefs["notify.aggressive"])
            window.getAttention();
    }
    else
    {
        if (isSuperfluous)
        {
            setTabState(this, "superfluous");
        }
        else
        {
            setTabState(this, "activity");
        }
    }

    // Copy Important Messages [to network view].
    if (isImportant && client.prefs["copyMessages"] && (o.network != this))
    {
        o.network.displayHere("{" + this.unicodeName + "} " + message, msgtype,
                              sourceObj, destObj);
    }

    // Log file time!
    if (this.prefs["log"])
    {
        if (!this.logFile)
            client.openLogFile(this);

        try
        {
            var LE = client.lineEnd;
            for (var l = 0; l < logStrings.length; l++)
                this.logFile.write(fromUnicode(logStrings[l] + LE, "utf-8"));
        }
        catch (ex)
        {
            // Stop logging before showing any messages!
            this.prefs["log"] = false;
            dd("Log file write error: " + formatException(ex));
            this.displayHere(getMsg(MSG_LOGFILE_WRITE_ERROR, getLogPath(this)),
                             "ERROR");
        }
    }
}

function addHistory (source, obj, mergeData)
{
    if (!("messages" in source) || (source.messages == null))
        createMessages(source);

    var tbody = source.messages.firstChild;
    var appendTo = tbody;

    var needScroll = false;

    if (mergeData)
    {
        var inobj = obj;
        // This gives us the non-nested row when there is nesting.
        if (inobj.className == "msg-nested-tr")
            inobj = inobj.firstChild.firstChild.firstChild.firstChild;

        var thisUserCol = inobj.firstChild;
        while (thisUserCol && !thisUserCol.className.match(/^(msg-user|msg-type)$/))
            thisUserCol = thisUserCol.nextSibling;

        var thisMessageCol = inobj.firstChild;
        while (thisMessageCol && !(thisMessageCol.className == "msg-data"))
            thisMessageCol = thisMessageCol.nextSibling;

        var ci = findPreviousColumnInfo(source.messages);
        var nickColumns = ci.nickColumns;
        var rowExtents = ci.extents;
        var nickColumnCount = nickColumns.length;

        var lastRowSpan, sameNick, sameDest, haveSameType, needSameType;
        var isAction, collapseActions;
        if (nickColumnCount == 0) // No message to collapse to.
        {
            sameNick = sameDest = needSameType = haveSameType = false;
            lastRowSpan = 0;
        }
        else // 1 or more messages, check for doubles
        {
            var lastRow = nickColumns[nickColumnCount - 1].parentNode;
            // What was the span last time?
            lastRowSpan = Number(nickColumns[0].getAttribute("rowspan"));
            // Are we the same user as last time?
            sameNick = (lastRow.getAttribute("msg-user") ==
                        inobj.getAttribute("msg-user"));
            // Do we have the same destination as last time?
            sameDest = (lastRow.getAttribute("msg-dest") ==
                        inobj.getAttribute("msg-dest"));
            // Is this message the same type as the last one?
            haveSameType = (lastRow.getAttribute("msg-type") ==
                            inobj.getAttribute("msg-type"));
            // Is either of the messages an action? We may not want to collapse
            // depending on the collapseActions pref
            isAction = ((inobj.getAttribute("msg-type") == "ACTION") ||
                        (lastRow.getAttribute("msg-type") == "ACTION"));
            // Do we collapse actions?
            collapseActions = source.prefs["collapseActions"];

            // Does the motif collapse everything, regardless of type?
            // NOTE: the collapseActions pref can override this for actions
            needSameType = !(("motifSettings" in source) &&
                             source.motifSettings &&
                             ("collapsemore" in source.motifSettings));
        }

        if (sameNick && sameDest && (haveSameType || !needSameType) &&
            (!isAction || collapseActions))
        {
            obj = inobj;
            if (ci.nested)
                appendTo = source.messages.firstChild.lastChild.firstChild.firstChild.firstChild;

            if (obj.getAttribute("important"))
            {
                nickColumns[nickColumnCount - 1].setAttribute("important",
                                                              true);
            }

            // Remove nickname column from new row.
            obj.removeChild(thisUserCol);

            // Expand previous grouping's nickname cell(s) to fill-in the gap.
            for (var i = 0; i < nickColumns.length; ++i)
                nickColumns[i].setAttribute("rowspan", rowExtents.length + 1);
        }
    }

    if ("frame" in source)
        needScroll = checkScroll(source.frame);
    if (obj)
        appendTo.appendChild(client.adoptNode(obj, appendTo.ownerDocument));

    if (source.MAX_MESSAGES)
    {
        if (typeof source.messageCount != "number")
            source.messageCount = 1;
        else
            source.messageCount++;

        if (source.messageCount > source.MAX_MESSAGES)
        {
            // Get the top of row 2, and subtract the top of row 1.
            var height = tbody.firstChild.nextSibling.firstChild.offsetTop -
                         tbody.firstChild.firstChild.offsetTop;
            var window = getContentWindow(source.frame);
            var x = window.pageXOffset;
            var y = window.pageYOffset;
            tbody.removeChild (tbody.firstChild);
            --source.messageCount;
            while (tbody.firstChild && tbody.firstChild.childNodes[1] &&
                   tbody.firstChild.childNodes[1].getAttribute("class") ==
                   "msg-data")
            {
                --source.messageCount;
                tbody.removeChild (tbody.firstChild);
            }
            if (!checkScroll(source.frame) && (y > height))
                window.scrollTo(x, y - height);
        }
    }

    if (needScroll)
    {
        scrollDown(source.frame, true);
        setTimeout(scrollDown, 500, source.frame, false);
        setTimeout(scrollDown, 1000, source.frame, false);
        setTimeout(scrollDown, 2000, source.frame, false);
    }
}

function findPreviousColumnInfo(table)
{
    // All the rows in the grouping (for merged rows).
    var extents = new Array();
    // Get the last row in the table.
    var tr = table.firstChild.lastChild;
    // Bail if there's no rows.
    if (!tr)
        return {extents: [], nickColumns: [], nested: false};
    // Get message type.
    if (tr.className == "msg-nested-tr")
    {
        var rv = findPreviousColumnInfo(tr.firstChild.firstChild);
        rv.nested = true;
        return rv;
    }
    // Now get the read one...
    var className = (tr && tr.childNodes[1]) ? tr.childNodes[1].getAttribute("class") : "";
    // Keep going up rows until you find the first in a group.
    // This will go up until it hits the top of a multiline/merged block.
    while (tr && tr.childNodes[1] && className.search(/msg-user|msg-type/) == -1)
    {
        extents.push(tr);
        tr = tr.previousSibling;
        if (tr && tr.childNodes[1])
            className = tr.childNodes[1].getAttribute("class");
    }

    // If we ran out of rows, or it's not a talking line, we're outta here.
    if (!tr || className != "msg-user")
        return {extents: [], nickColumns: [], nested: false};

    extents.push(tr);

    // Time to collect the nick data...
    var nickCol = tr.firstChild;
    // All the cells that contain nickname info.
    var nickCols = new Array();
    while (nickCol)
    {
        // Just collect nickname column cells.
        if (nickCol.getAttribute("class") == "msg-user")
            nickCols.push(nickCol);
        nickCol = nickCol.nextSibling;
    }

    // And we're done.
    return {extents: extents, nickColumns: nickCols, nested: false};
}

function getLogPath(obj)
{
    // If we're logging, return the currently-used URL.
    if (obj.logFile)
        return getURLSpecFromFile(obj.logFile.path);
    // If not, return the ideal URL.
    return getURLSpecFromFile(obj.prefs["logFileName"]);
}

client.getConnectionCount =
function cli_gccount ()
{
    var count = 0;

    for (var n in client.networks)
    {
        if (client.networks[n].isConnected())
            ++count;
    }

    return count;
}

client.quit =
function cli_quit (reason)
{
    var net, netReason;
    for (var n in client.networks)
    {
        net = client.networks[n];
        if (net.isConnected())
        {
            netReason = (reason ? reason : net.prefs["defaultQuitMsg"]);
            netReason = (netReason ? netReason : client.userAgent);
            net.quit(netReason);
        }
    }
}

client.wantToQuit =
function cli_wantToQuit(reason, deliberate)
{

    var close = true;
    if (client.prefs["warnOnClose"] && !deliberate)
    {
        const buttons = [MSG_QUIT_ANYWAY, MSG_DONT_QUIT];
        var checkState = { value: true };
        var rv = confirmEx(MSG_CONFIRM_QUIT, buttons, 0, MSG_WARN_ON_EXIT,
                           checkState);
        close = (rv == 0);
        client.prefs["warnOnClose"] = checkState.value;
    }

    if (close)
    {
        client.userClose = true;
        display(MSG_CLOSING);
        client.quit(reason);
    }
}

/* gets a tab-complete match for the line of text specified by |line|.
 * wordStart is the position within |line| that starts the word being matched,
 * wordEnd marks the end position.  |cursorPos| marks the position of the caret
 * in the textbox.
 */
client.performTabMatch =
function gettabmatch_usr (line, wordStart, wordEnd, word, cursorPos)
{
    if (wordStart != 0 || line[0] != client.COMMAND_CHAR)
        return null;

    var matches = client.commandManager.listNames(word.substr(1), CMD_CONSOLE);
    if (matches.length == 1 && wordEnd == line.length)
    {
        matches[0] = client.COMMAND_CHAR + matches[0] + " ";
    }
    else
    {
        for (var i in matches)
            matches[i] = client.COMMAND_CHAR + matches[i];
    }

    return matches;
}

client.openLogFile =
function cli_startlog (view)
{
    function getNextLogFileDate()
    {
        var d = new Date();
        d.setMilliseconds(0);
        d.setSeconds(0);
        d.setMinutes(0);
        switch (view.smallestLogInterval)
        {
            case "h":
                return d.setHours(d.getHours() + 1);
            case "d":
                d.setHours(0);
                return d.setDate(d.getDate() + 1);
            case "m":
                d.setHours(0);
                d.setDate(1);
                return d.setMonth(d.getMonth() + 1);
            case "y":
                d.setHours(0);
                d.setDate(1);
                d.setMonth(0);
                return d.setFullYear(d.getFullYear() + 1);
        }
        //XXXhack: This should work...
        return Infinity;
    };

    const NORMAL_FILE_TYPE = Components.interfaces.nsIFile.NORMAL_FILE_TYPE;

    try
    {
        var file = new LocalFile(view.prefs["logFileName"]);
        if (!file.localFile.exists())
        {
            // futils.umask may be 0022. Result is 0644.
            file.localFile.create(NORMAL_FILE_TYPE, 0666 & ~futils.umask);
        }
        view.logFile = fopen(file.localFile, ">>");
        // If we're here, it's safe to say when we should re-open:
        view.nextLogFileDate = getNextLogFileDate();
    }
    catch (ex)
    {
        view.prefs["log"] = false;
        dd("Log file open error: " + formatException(ex));
        view.displayHere(getMsg(MSG_LOGFILE_ERROR, getLogPath(view)), MT_ERROR);
        return;
    }

    if (!("logFileWrapping" in view) || !view.logFileWrapping)
        view.displayHere(getMsg(MSG_LOGFILE_OPENED, getLogPath(view)));
    view.logFileWrapping = false;
}

client.closeLogFile =
function cli_stoplog(view, wrapping)
{
    if ("frame" in view && !wrapping)
        view.displayHere(getMsg(MSG_LOGFILE_CLOSING, getLogPath(view)));

    view.logFileWrapping = Boolean(wrapping);

    if (view.logFile)
    {
        view.logFile.close();
        view.logFile = null;
    }
}

function checkLogFiles()
{
    // For every view that has a logfile, check if we need a different file
    // based on the current date and the logfile preference. We close the
    // current logfile, and display will open the new one based on the pref
    // when it's needed.

    var d = new Date();
    for (var n in client.networks)
    {
        var net = client.networks[n];
        if (net.logFile && (d > net.nextLogFileDate))
            client.closeLogFile(net, true);
        if (("primServ" in net) && net.primServ && ("channels" in net.primServ))
        {
            for (var c in net.primServ.channels)
            {
                var chan = net.primServ.channels[c];
                if (chan.logFile && (d > chan.nextLogFileDate))
                    client.closeLogFile(chan, true);
            }
        }
        if ("users" in net)
        {
            for (var u in net.users)
            {
                var user = net.users[u];
                if (user.logFile && (d > user.nextLogFileDate))
                    client.closeLogFile(user, true);
            }
        }
    }

    for (var dc in client.dcc.chats)
    {
        var dccChat = client.dcc.chats[dc];
        if (dccChat.logFile && (d > dccChat.nextLogFileDate))
            client.closeLogFile(dccChat, true);
    }
    for (var df in client.dcc.files)
    {
        var dccFile = client.dcc.files[df];
        if (dccFile.logFile && (d > dccFile.nextLogFileDate))
            client.closeLogFile(dccFile, true);
    }

    // Don't forget about the client tab:
    if (client.logFile && (d > client.nextLogFileDate))
        client.closeLogFile(client, true);

    // We use the same line again to make sure we keep a constant offset
    // from the full hour, in case the timers go crazy at some point.
    setTimeout("checkLogFiles()", 3602000 - (Number(new Date()) % 3600000));
}

CIRCChannel.prototype.getLCFunction =
CIRCNetwork.prototype.getLCFunction =
CIRCUser.prototype.getLCFunction    =
CIRCDCCChat.prototype.getLCFunction =
CIRCDCCFileTransfer.prototype.getLCFunction =
function getlcfn()
{
    var details = getObjectDetails(this);
    var lcFn;

    if (details.server)
    {
        lcFn = function(text)
            {
                return details.server.toLowerCase(text);
            }
    }

    return lcFn;
}

CIRCChannel.prototype.performTabMatch =
CIRCNetwork.prototype.performTabMatch =
CIRCUser.prototype.performTabMatch    =
CIRCDCCChat.prototype.performTabMatch =
CIRCDCCFileTransfer.prototype.performTabMatch =
function gettabmatch_other (line, wordStart, wordEnd, word, cursorpos, lcFn)
{
    if (wordStart == 0 && line[0] == client.COMMAND_CHAR)
    {
        return client.performTabMatch(line, wordStart, wordEnd, word,
                                      cursorpos);
    }

    var matchList = new Array();
    var users;
    var channels;
    var userIndex = -1;

    var details = getObjectDetails(this);

    if (details.channel && word == details.channel.unicodeName[0])
    {
        /* When we have #<tab>, we just want the current channel,
           if possible. */
        matchList.push(details.channel.unicodeName);
    }
    else
    {
        /* Ok, not #<tab> or no current channel, so get the full list. */

        if (details.channel)
            users = details.channel.users;

        if (details.server)
        {
            channels = details.server.channels;
            for (var c in channels)
                matchList.push(channels[c].unicodeName);
            if (!users)
                users = details.server.users;
        }

        if (users)
        {
            userIndex = matchList.length;
            for (var n in users)
                matchList.push(users[n].unicodeName);
        }
    }

    var matches = matchEntry(word, matchList, lcFn);

    var list = new Array();
    for (var i = 0; i < matches.length; i++)
        list.push(matchList[matches[i]]);

    if (list.length == 1)
    {
        if (users && (userIndex >= 0) && (matches[0] >= userIndex))
        {
            if (wordStart == 0)
                list[0] += client.prefs["nickCompleteStr"];
        }

        if (wordEnd == line.length)
        {
            /* add a space if the word is at the end of the line. */
            list[0] += " ";
        }
    }

    return list;
}

CIRCChannel.prototype.getGraphResource =
function my_graphres ()
{
    if (!("rdfRes" in this))
    {
        var id = RES_PFX + "CHANNEL:" + this.parent.parent.unicodeName + ":" +
            escape(this.unicodeName);
        this.rdfRes = client.rdf.GetResource(id);
    }

    return this.rdfRes;
}

CIRCUser.prototype.getGraphResource =
function usr_graphres()
{
    if (!ASSERT(this.TYPE == "IRCChanUser",
                "cuser.getGraphResource called on wrong object"))
    {
        return null;
    }

    var rdf = client.rdf;

    if (!("rdfRes" in this))
    {
        if (!("nextResID" in CIRCUser))
            CIRCUser.nextResID = 0;

        this.rdfRes = rdf.GetResource(RES_PFX + "CUSER:" +
                                      this.parent.parent.parent.unicodeName + ":" +
                                      this.parent.unicodeName + ":" +
                                      CIRCUser.nextResID++);

            //dd ("created cuser resource " + this.rdfRes.Value);

        rdf.Assert (this.rdfRes, rdf.resNick, rdf.GetLiteral(this.unicodeName));
        rdf.Assert (this.rdfRes, rdf.resUniName, rdf.GetLiteral(this.unicodeName));
        rdf.Assert (this.rdfRes, rdf.resUser, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resHost, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resSortName, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resFounder, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resAdmin, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resOp, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resHalfOp, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resVoice, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resAway, rdf.litUnk);
        this.updateGraphResource();
    }

    return this.rdfRes;
}

CIRCUser.prototype.updateGraphResource =
function usr_updres()
{
    if (!ASSERT(this.TYPE == "IRCChanUser",
                "cuser.updateGraphResource called on wrong object"))
    {
        return;
    }

    if (!("rdfRes" in this))
    {
        this.getGraphResource();
        return;
    }

    var rdf = client.rdf;

    rdf.Change (this.rdfRes, rdf.resUniName, rdf.GetLiteral(this.unicodeName));
    if (this.name)
        rdf.Change (this.rdfRes, rdf.resUser, rdf.GetLiteral(this.name));
    else
        rdf.Change (this.rdfRes, rdf.resUser, rdf.litUnk);
    if (this.host)
        rdf.Change (this.rdfRes, rdf.resHost, rdf.GetLiteral(this.host));
    else
        rdf.Change (this.rdfRes, rdf.resHost, rdf.litUnk);

    // Check for the highest mode the user has.
    const userModes = this.parent.parent.userModes;
    var modeLevel = 0;
    var mode;
    for (var i = 0; i < this.modes.length; i++)
    {
        for (var j = 0; j < userModes.length; j++)
        {
            if (userModes[j].mode == this.modes[i])
            {
                if (userModes.length - j > modeLevel)
                {
                    modeLevel = userModes.length - j;
                    mode = userModes[j];
                }
                break;
            }
        }
    }

    // Counts numerically down from 9.
    var sortname = (9 - modeLevel) + "-" + this.unicodeName;

    // We want to show mode symbols, but only for modes we don't 'style'.
    var displayname = this.unicodeName;
    if (mode && !mode.mode.match(/^[qaohv]$/))
        displayname = mode.symbol + " " + displayname;

    rdf.Change(this.rdfRes, rdf.resNick, rdf.GetLiteral(displayname));
    rdf.Change(this.rdfRes, rdf.resSortName, rdf.GetLiteral(sortname));
    rdf.Change(this.rdfRes, rdf.resFounder,
               this.isFounder ? rdf.litTrue : rdf.litFalse);
    rdf.Change(this.rdfRes, rdf.resAdmin,
               this.isAdmin ? rdf.litTrue : rdf.litFalse);
    rdf.Change(this.rdfRes, rdf.resOp,
               this.isOp ? rdf.litTrue : rdf.litFalse);
    rdf.Change(this.rdfRes, rdf.resHalfOp,
               this.isHalfOp ? rdf.litTrue : rdf.litFalse);
    rdf.Change(this.rdfRes, rdf.resVoice,
               this.isVoice ? rdf.litTrue : rdf.litFalse);
    rdf.Change(this.rdfRes, rdf.resAway,
               this.isAway ? rdf.litTrue : rdf.litFalse);
}
