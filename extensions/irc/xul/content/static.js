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

const __cz_version   = "0.9.64";
const __cz_condition = "green";

var warn;
var ASSERT;
var TEST;

if (DEBUG)
{
    _dd_pfx = "cz: ";
    warn = function (msg) { dumpln ("** WARNING " + msg + " **"); }
    TEST = ASSERT = function (expr, msg) {
                 if (!expr) {
                     dump ("** ASSERTION FAILED: " + msg + " **\n" + 
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
client.STEP_TIMEOUT = 100;
client.MAX_MESSAGES = 200;
client.MAX_HISTORY = 50;
/* longest nick to show in display before forcing the message to a block level
 * element */
client.MAX_NICK_DISPLAY = 14;
/* longest word to show in display before abbreviating */
client.MAX_WORD_DISPLAY = 20;
client.PRINT_DIRECTION = 1; /*1 => new messages at bottom, -1 => at top */

client.MAX_MSG_PER_ROW = 3; /* default number of messages to collapse into a
                             * single row, max. */
client.INITIAL_COLSPAN = 5; /* MAX_MSG_PER_ROW cannot grow to greater than 
                             * one half INITIAL_COLSPAN + 1. */
client.NOTIFY_TIMEOUT = 5 * 60 * 1000; /* update notify list every 5 minutes */

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


client.viewsArray = new Array();
client.activityList = new Object();
client.inputHistory = new Array();
client.lastHistoryReferenced = -1;
client.incompleteLine = "";
client.lastTabUp = new Date();

CIRCNetwork.prototype.INITIAL_CHANNEL = "";
CIRCNetwork.prototype.MAX_MESSAGES = 100;
CIRCNetwork.prototype.IGNORE_MOTD = false;

CIRCServer.prototype.READ_TIMEOUT = 0;
CIRCUser.prototype.MAX_MESSAGES = 200;

CIRCChannel.prototype.MAX_MESSAGES = 300;

CIRCChanUser.prototype.MAX_MESSAGES = 200;

function init()
{
    if (("initialized" in client) && client.initialized)
        return;
    
    client.initialized = false;

    client.networks = new Object();
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

    initRDF();
    initMessages();
    initCommands();
    initPrefs();
    initMunger();
    initNetworks();
    initMenus();
    initStatic();
    initHandlers();
    
    // Create DCC handler.
    client.dcc = new CIRCDCC(client);

    // start logging.  nothing should call display() before this point.
    if (client.prefs["log"])
       client.openLogFile(client);
 
    client.display(MSG_WELCOME, "HELLO");
    setCurrentObject (client);
   
    importFromFrame("updateHeader");
    importFromFrame("setHeaderState");
    importFromFrame("changeCSS");

    processStartupScripts();

    client.commandManager.installKeys(document);
    createMenus();

    client.initialized = true;

    dispatch("networks");
    dispatch("commands");
    
    processStartupURLs();
}

function initStatic()
{
    CIRCServer.prototype.VERSION_RPLY =
        getMsg(MSG_VERSION_REPLY, [__cz_version, navigator.userAgent]);
    
    client.mainWindow = window;
    
    try
    {
        var io = Components.classes['@mozilla.org/network/io-service;1'];
        client.iosvc = io.getService(Components.interfaces.nsIIOService);
    }
    catch (ex)
    {
        throw { summary: "IO service failed to initalize", exception: ex };
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
        throw { summary: "Sound failed to initalize", exception: ex };
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
        throw { summary: "Global History failed to initalize", exception: ex };
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
        throw { summary: "Locale-correct date formatting failed to initalize", exception: ex };
    }
    
    multilineInputMode(client.prefs["multiline"]);
    if (client.prefs["showModeSymbols"])
        setListMode("symbol");
    else
        setListMode("graphic");
    setDebugMode(client.prefs["debugMode"]);
    
    var ary = navigator.userAgent.match (/(rv:[^;)\s]+).*?Gecko\/(\d+)/);
    if (ary)
    {
        client.userAgent = "ChatZilla " + __cz_version + " [Mozilla " + 
            ary[1] + "/" + ary[2] + "]";
    }
    else
    {
        client.userAgent = "ChatZilla " + __cz_version + " [" + 
            navigator.userAgent + "]";
    }

    client.statusBar = new Object();
    
    client.statusBar["server-nick"] = document.getElementById ("server-nick");

    client.statusElement = document.getElementById ("status-text");
    client.defaultStatus = MSG_DEFAULT_STATUS;

    client.lineEnd = "\n";
    if (navigator.platform.search(/win/i) >= 0)
        client.lineEnd = "\r\n";
    
    client.logFile = null;
    setInterval("onNotifyTimeout()", client.NOTIFY_TIMEOUT);
    
    client.defaultCompletion = client.COMMAND_CHAR + "help ";

    client.deck = document.getElementById('output-deck');
}

function initNetworks()
{
    client.addNetwork("moznet",[{name: "irc.mozilla.org", port: 6667}]);
    client.addNetwork("hybridnet", [{name: "irc.ssc.net", port: 6667}]);
    client.addNetwork("slashnet", [{name: "irc.slashnet.org", port:6667}]);
    client.addNetwork("dalnet", [{name: "irc.dal.net", port:6667}]);
    client.addNetwork("undernet", [{name: "irc.undernet.org", port:6667}]);
    client.addNetwork("webbnet", [{name: "irc.webbnet.info", port:6667}]);
    client.addNetwork("quakenet", [{name: "irc.quakenet.org", port:6667}]);
    client.addNetwork("freenode", [{name: "irc.freenode.net", port:6667}]);
    client.addNetwork("efnet",
                      [{name: "irc.mcs.net", port: 6667},
                       {name: "irc.prison.net", port: 6667},
                       {name: "irc.freei.net", port: 6667},
                       {name: "irc.magic.ca", port: 6667}]);
}

function importFromFrame(method)
{
    client.__defineGetter__(method, import_wrapper);
    CIRCNetwork.prototype.__defineGetter__(method, import_wrapper);
    CIRCChannel.prototype.__defineGetter__(method, import_wrapper);
    CIRCUser.prototype.__defineGetter__(method, import_wrapper);
    CIRCDCCChat.prototype.__defineGetter__(method, import_wrapper);

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
        if (url.search(/^irc:\/?\/?\/?$/i) == -1)
        {
            /* if the url is not irc: irc:/ or irc://, then go to it. */
            gotoIRCURL(url);
            wentSomewhere = true;
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
        var tb = getTabForObject(client);
        deleteTab(tb);
        client.deck.removeChild(client.frame);
        client.deck.selectedIndex = 0;
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

function insertLink (matchText, containerTag)
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

    if (!("network" in eventData) || matchText.search(bogusChannels) != -1)
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

function insertBugzillaLink (matchText, containerTag)
{
    var number = matchText.match (/(\d+)/)[1];
    
    var anchor = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:a");
    anchor.setAttribute ("href", client.prefs["bugURL"].replace("%s", number));
    anchor.setAttribute ("class", "chatzilla-link");
    anchor.setAttribute ("target", "_content");
    insertHyphenatedWord (matchText, anchor);
    containerTag.appendChild (anchor);
    
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

function insertEar (matchText, containerTag)
{
    var img = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                        "html:img");
    img.setAttribute ("src", client.IMAGEDIR + "face-ear.gif");
    img.setAttribute ("title", matchText);
    containerTag.appendChild (img);
    
}

function insertSmiley (emoticon, containerTag)
{
    var type = "error";

    if (emoticon.search (/\;[-^v]?[\)>\]]/) != -1)
        type = "face-wink";
    else if (emoticon.search (/[=:;][-^v]?[\)>\]]/) != -1)
        type = "face-smile";
    else if (emoticon.search (/[=:;][-^v]?[\/\\]/) != -1)
        type = "face-screw";
    else if (emoticon.search (/[=:;]\~[-^v]?\(/) != -1)
        type = "face-cry";
    else if (emoticon.search (/[=:;][-^v]?[\(<\[]/) != -1)
        type = "face-frown";
    else if (emoticon.search (/\<?[=:;][-^v]?[0oO]/) != -1)
        type = "face-surprise";
    else if (emoticon.search (/[=:;][-^v]?[pP]/) != -1)
        type = "face-tongue";
    else if (emoticon.search (/\>?[=:;][\-\^\v]?[\(\|]/) != -1)
        type = "face-angry";

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
    if (!client.enableColors)
        return;

    var ary = colorInfo.match (/.(\d{1,2}|)(,(\d{1,2})|)/);

    var fgColor = ary[1];
    if (fgColor > 16)
        fgColor &= 16;

    switch (fgColor.length)
    {
        case 0:
            delete data.currFgColor;
            delete data.currBgColor;
            return;

        case 1:
            data.currFgColor = "0" + fgColor;
            break;

        case 2:
            data.currFgColor = fgColor;
            break;
    }

    if (fgColor == 1)
        delete data.currFgColor;
    if (arrayHasElementAt(ary, 3))
    {
        var bgColor = ary[3];
        if (bgColor > 16)
            bgColor &= 16;

        if (bgColor.length == 1)
            data.currBgColor = "0" + bgColor;
        else
            data.currBgColor = bgColor;

        if (bgColor == 0)
            delete data.currBgColor;
    }

    data.hasColorInfo = true;
}

function mircToggleBold (colorInfo, containerTag, data)
{
    if (!client.enableColors)
        return;

    if ("isBold" in data)
        delete data.isBold;
    else
        data.isBold = true;
    data.hasColorInfo = true;
}

function mircToggleUnder (colorInfo, containerTag, data)
{
    if (!client.enableColors)
        return;

    if ("isUnderline" in data)
        delete data.isUnderline;
    else
        data.isUnderline = true; 
    data.hasColorInfo = true;
}

function mircResetColor (text, containerTag, data)
{
    if (!client.enableColors || !("hasColorInfo" in data))
        return;

    delete data.currFgColor;
    delete data.currBgColor;
    delete data.isBold;
    delete data.isUnder;
    delete data.hasColorInfo;
}

function mircReverseColor (text, containerTag, data)
{
    if (!client.enableColors)
        return;

    var tempColor = ("currFgColor" in data ? data.currFgColor : "01");

    if ("currBgColor" in data)
        data.currFgColor = data.currBgColor;
    else
        data.currFgColor = "00";
    data.currBgColor = tempColor;
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

    ary.push(network.primServ.me.nick.replace(/[^\w\d]/g, escapeChar));
    
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
    return getObjectDetails(client.currentObject, cx);
}

function getMessagesContext(cx, element)
{
    cx = getObjectDetails(client.currentObject, cx);
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
                
                if (!cx.network || !(nickname in cx.network.users))
                    break;
                
                if (cx.channel)
                    cx.user = cx.channel.getUser(nickname);
                else
                    cx.user = cx.network.getUser(nickname);
                
                if (cx.user)
                {
                    cx.nickname = cx.user.properNick;
                    cx.canonNick = cx.user.nick;
                }
                break;
        }
        
        element = element.parentNode;
    }
    
    return cx;
}

function getTabContext(cx, element)
{
    if (!element)
        element = document.popupNode;

    while (element)
    {
        if (element.localName == "tab")
            return getObjectDetails(element.view);
        element = element.parentNode;
    }

    return cx;
}

function getUserlistContext(cx) 
{
    cx = getObjectDetails(client.currentObject, cx);
    if (!cx.channel)
        return cx;
    
    cx.userList = new Array();
    cx.nicknameList = new Array();
    
    var tree = document.getElementById("user-list");
    var rangeCount = tree.view.selection.getRangeCount();
    
    for (var i = 0; i < rangeCount; ++i)
    {
        var start = {}, end = {};
        tree.view.selection.getRangeAt(i, start, end);
        
        // If they == -1, we've got no selection, so bail.
        if ((start.value == -1) && (end.value == -1))
            return cx;
        
        for (var k = start.value; k <= end.value; ++k)
        {
            var item = tree.contentView.getItemAtIndex(k);
            var cell = item.firstChild.firstChild;
            var user = cx.channel.getUser(cell.getAttribute("label"));
            if (user)
            {
                cx.userList.push(user);
                cx.nicknameList.push(user.nick);
                if (i == 0 && k == start.value)
                {
                    cx.user = user;
                    cx.nickname = user.properNick;
                }
            }
        }
    }
    
    return cx;
}

function getFontContext(cx)
{
    cx = getObjectDetails(client.currentObject, cx);
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
     */
    var plainMsg = client.munger.munge(msg, null, {});
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
    
    setCurrentObject (client.viewsArray[destKey].source);
}

// Plays the sound for a particular event on a type of object.
function playEventSounds(type, event)
{
    if (!client.sound)
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
    if (!client.sound)
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
    if (!client.sound || !file)
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
    client.eventPump.stepEvents();
    setTimeout ("mainStep()", client.STEP_TIMEOUT);
}

function openQueryTab(server, nick)
{
    var user = server.addUser(nick);
    client.globalHistory.addPage(user.getURL());
    if (!("messages" in user))
    {
        var value = "";
        var same = true;
        for (var c in server.channels)
        {
            var chan = server.channels[c];
            if (!(user.nick in chan.users))
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
        
        user.displayHere (getMsg(MSG_QUERY_OPENED, user.properNick));
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
            rv.userName = obj.properNick;
            rv.server = rv.user.parent;
            rv.network = rv.server.parent;
            break;

        case "IRCChanUser":
            rv.viewType = MSG_USER;
            rv.user = obj;
            rv.userName = obj.properNick;
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
            
        default:
            /* no setup for unknown object */
            break;
    }

    if (rv.network)
        rv.networkName = rv.network.name;

    return rv;
    
}

function findDynamicRule (selector)
{
    var rules = frames[0].document.styleSheets[1].cssRules;
    
    if (selector instanceof RegExp)
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
        var dispatcher = top.document.commandDispatcher;
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
        var dispatcher = top.document.commandDispatcher;
        var controller = dispatcher.getControllerForCommand(command);
        if (controller && controller.isCommandEnabled(command))
            controller.doCommand(command);
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
    rv.host = null;
    rv.target = "";
    rv.port = 6667;
    rv.msg = "";
    rv.pass = null;
    rv.key = null;
    rv.charset = null;
    rv.needpass = false;
    rv.needkey = false;
    rv.isnick = false;
    rv.isserver = false;
    
    if (url.search(/^(irc:\/?\/?)$/i) != -1)
        return rv;

    /* split url into <host>/<everything-else> pieces */
    var ary = url.match (/^irc:\/\/([^\/\s]+)?(\/.*)?\s*$/i);
    if (!ary || !ary[1])
    {
        dd ("parseIRCURL: initial split failed");
        return null;
    }
    var host = ary[1];
    var rest = arrayHasElementAt(ary, 2) ? ary[2] : "";

    /* split <host> into server (or network) / port */
    ary = host.match (/^([^\s\:]+)?(\:\d+)?$/);
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
        ary = rest.match (/^\/([^\,\?\s\/]*)?\/?(,[^\?]*)?(\?.*)?$/);
        if (!ary)
        {
            dd ("parseIRCURL: rest split failed ``" + rest + "''");
            return null;
        }
        
        rv.target = arrayHasElementAt(ary, 1) ? 
            ecmaUnescape(ary[1]).replace("\n", "\\n") : "";
        var i = rv.target.indexOf(" ");
        if (i != -1)
            rv.target = rv.target.substr(0, i);
        var params = arrayHasElementAt(ary, 2) ? ary[2].toLowerCase() : "";
        var query = arrayHasElementAt(ary, 3) ? ary[3] : "";

        if (params)
        {
            rv.isnick =
                (params.search (/,\s*isnick\s*,|,\s*isnick\s*$/) != -1);
            if (rv.isnick && !rv.target)
            {
                dd ("parseIRCURL: isnick w/o target");
                /* isnick w/o a target is bogus */
                return null;
            }
        
            if (!rv.isserver)
            {
                rv.isserver =
                    (params.search (/,\s*isserver\s*,|,\s*isserver\s*$/) != -1);
            }
            
            if (rv.isserver && !specifiedHost)
            {
                dd ("parseIRCURL: isserver w/o host");
                    /* isserver w/o a host is bogus */
                return null;
            }
                
            rv.needpass =
                (params.search (/,\s*needpass\s*,|,\s*needpass\s*$/) != -1);

            rv.needkey =
                (params.search (/,\s*needkey\s*,|,\s*needkey\s*$/) != -1);

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
    var pass = "";
    
    if (url.needpass)
    {
        if (url.pass)
            pass = url.pass;
        else
            pass = window.promptPassword(getMsg(MSG_URL_PASSWORD, url.spec));
    }
    
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
            network = dispatch("server", {hostname: url.host, port: url.port,
                                          password: pass});
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
            client.connectToNetwork(network);
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

            var charset;
            if (url.charset)
                charset = url.charset;
                    
            targetObject = network.dispatch("join",
                                            { channelName: url.target, key: key,
                                              charset: charset });
            if (!targetObject)
                return;
        }

        if (url.msg)
        {
            var msg;
            if (url.msg.indexOf("\01ACTION") == 0)
            {
                msg = filterOutput(url.msg, "ACTION", "ME!");
                targetObject.display(msg, "ACTION", "ME!",
                                     client.currentObject);
            }
            else
            {
                msg = filterOutput(url.msg, "PRIVMSG", "ME!");
                targetObject.display(msg, "PRIVMSG", "ME!",
                                     client.currentObject);
            }
            targetObject.say(msg);
            setCurrentObject(targetObject);
        }
    }
    else
    {
        if (!network.messages)
            network.displayHere (getMsg(MSG_NETWORK_OPENED, network.name));
        setCurrentObject(network);
    }
}

function setTopicText (text)
{
    var topic = client.statusBar["channel-topic"];
    var span = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                         "html:span");
    
    span.appendChild(stringToMsg(text, client.currentObject));
    topic.removeChild(topic.firstChild);
    topic.appendChild(span);
}
    
function updateNetwork()
{
    var o = getObjectDetails (client.currentObject);

    var lag = MSG_UNKNOWN;
    var nick = "";
    if (o.server)
    {
        if (o.server.me)
            nick = o.server.me.properNick;
        lag = (o.server.lag != -1) ? o.server.lag : MSG_UNKNOWN;
    }
    client.statusBar["header-url"].setAttribute("value", 
                                                 client.currentObject.getURL());
    client.statusBar["header-url"].setAttribute("href", 
                                                 client.currentObject.getURL());
    client.statusBar["header-url"].setAttribute("name", 
                                                 client.currentObject.name);
}

function updateTitle (obj)
{
    if (!(("currentObject" in client) && client.currentObject) || 
        (obj && obj != client.currentObject))
        return;

    var tstring;
    var o = getObjectDetails(client.currentObject);
    var net = o.network ? o.network.name : "";
    var nick = "";
    
    switch (client.currentObject.TYPE)
    {
        case "IRCNetwork":
            var serv = "", port = "";
            if (client.currentObject.isConnected())
            {
                serv = o.server.hostname;
                port = o.server.port;
                if (o.server.me)
                    nick = o.server.me.properNick;
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
                nick = o.parent.me.properNick;
                if (o.parent.me.nick in client.currentObject.users)
                {
                    var cuser = client.currentObject.users[o.parent.me.nick];
                    if (cuser.isOp)
                        nick = "@" + nick;
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
            nick = client.currentObject.properNick;
            var source = "";
            if (client.currentObject.name)
            {
                source = "<" + client.currentObject.name + "@" +
                    client.currentObject.host +">";
            }
            tstring = getMsg(MSG_TITLE_USER, [nick, source]);
            nick = "me" in o.parent ? o.parent.me.properNick : MSG_TITLE_NONICK;
            break;

        default:
            tstring = MSG_TITLE_UNKNOWN;
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
    client.statusBar["server-nick"].setAttribute("value", nick);
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
        client.input = singleInput;
    }

    client.input.focus();
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

    if ("currentObject" in client && client.currentObject)
    {
        tb = getTabForObject(client.currentObject);
    }
    if (tb)
    {
        tb.selected = false;
        tb.setAttribute ("state", "normal");
    }
    
    /* Unselect currently selected users. */
    userList = document.getElementById("user-list");
    /* If the splitter's collapsed, the userlist *isn't* visible, but we'll not
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
            updateUserList();
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
    tb = getTabForObject(obj, true);
    if (tb)
    {
        tb.selected = true;
        tb.setAttribute ("state", "current");
    }
    
    var vk = Number(tb.getAttribute("viewKey"));
    delete client.activityList[vk];
    client.deck.selectedIndex = vk;

    updateTitle();

    if (client.PRINT_DIRECTION == 1)
        scrollDown(obj.frame, false);    

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
    
    var tb = getTabForObject (source, true);
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
        var tb = getTabForObject (source, true);
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
    var node;
    var sortDirection;
    
    node = document.getElementById("user-list");

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
        sortResource = RES_PFX + "nick";

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

    var frame;
    if (stateFlags & START)
    {
        frame = getFrameForDOMWindow(webProgress.DOMWindow);
        if (!frame)
        {
            dd("can't find frame for window")
            webProgress.removeProgressListener(this);
            return;
        }
    }
    else if (stateFlags == 786448)
    {
        frame = getFrameForDOMWindow(webProgress.DOMWindow);
        if (!frame)
        {
            dd("can't find frame for window");
            webProgress.removeProgressListener(this);
            return;
        }

        var cwin = getContentWindow(frame);
        if (cwin && "initOutputWindow" in cwin)
        {
            cwin.getMsg = getMsg;
            cwin.initOutputWindow(client, frame.source, onMessageViewClick);
            cwin.changeCSS(frame.source.getTimestampCSS("data"), 
                           "cz-timestamp-format");
            cwin.changeCSS(frame.source.getFontCSS("data"), 
                           "cz-fonts");
            scrollDown(frame, true);
            webProgress.removeProgressListener(this);
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

function syncOutputFrame(obj)
{
    const nsIWebProgress = Components.interfaces.nsIWebProgress;
    const ALL = nsIWebProgress.NOTIFY_ALL;
    const DOCUMENT = nsIWebProgress.NOTIFY_STATE_DOCUMENT;
    const WINDOW = nsIWebProgress.NOTIFY_STATE_WINDOW;
 
    var iframe = obj.frame;
    
    function tryAgain ()
    {
        syncOutputFrame(obj);
    };
 
    try
    {
        if ("contentDocument" in iframe && "webProgress" in iframe)
        {
            var url = obj.prefs["outputWindowURL"];
            iframe.addProgressListener(client.progressListener, WINDOW);
            iframe.loadURI(url);
        }
        else
        {
            setTimeout(tryAgain, 500);
        }
    }
    catch (ex)
    {
        dd("caught exception showing session view, will try again later.");
        dd(dumpObjectTree(ex) + "\n");
        setTimeout (tryAgain, 500);
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
    
    if ("displayName" in source)
    {
        name = source.displayName;
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
        tb.setAttribute ("ondraggesture",
                         "nsDragAndDrop.startDrag(event, tabDNDObserver);");
        tb.setAttribute ("href", source.getURL());
        tb.setAttribute ("name", source.name);
        tb.setAttribute ("onclick", "onTabClick(event, " + id.quote() + ");");
        tb.setAttribute ("crop", "right");
        tb.setAttribute ("context", "context:tab");
        tb.setAttribute ("tooltip", "xul-tooltip-mode");
        
        tb.setAttribute ("class", "tab-bottom view-button");
        tb.setAttribute ("id", id);
        tb.setAttribute ("state", "normal");

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
        browser.setAttribute("type", "content");
        browser.setAttribute("flex", "1");
        browser.setAttribute("tooltip", "html-tooltip-node");
        browser.setAttribute("context", "context:messages");
        //browser.setAttribute ("onload", "scrollDown(true);");
        browser.setAttribute("onclick",
                             "return onMessageViewClick(event)");
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
    else if (url.search(/^irc:\/\//i) != -1)
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

function filterOutput (msg, msgtype)
{
    if ("outputFilters" in client)
    {
        for (var f in client.outputFilters)
        {
            if (client.outputFilters[f].enabled)
                msg = client.outputFilters[f].func(msg, msgtype);
        }
    }

    return msg;
}

client.addNetwork =
function cli_addnet(name, serverList)
{
    client.networks[name] =
        new CIRCNetwork (name, serverList, client.eventPump);
}

client.connectToNetwork =
function cli_connect(networkOrName)
{
    var network;
    var name;
    
    
    if (networkOrName instanceof CIRCNetwork)
    {
        network = networkOrName;
        name = network.name;
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
    
    if (!("messages" in network))
        network.displayHere(getMsg(MSG_NETWORK_OPENED, name));

    setCurrentObject(network);

    if (network.isConnected())
    {
        network.display(getMsg(MSG_ALREADY_CONNECTED, name));
        return network;
    }

    if (network.connecting)
        return network;
    
    if (network.prefs["nickname"] == DEFAULT_NICK)
        network.prefs["nickname"] = prompt(MSG_ENTER_NICK, DEFAULT_NICK);
    
    if (!("connecting" in network))
        network.display(getMsg(MSG_NETWORK_CONNECTING, name));

    network.connecting = true;
    network.connect();

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
            client._loader = cls.createInstance(mozIJSSubScriptLoader);
    }

    return client._loader.loadSubScript(url, scope);
}
    
client.sayToCurrentTarget =
function cli_say(msg)
{
    if ("say" in client.currentObject)
    {
        msg = filterOutput (msg, "PRIVMSG");
        display(msg, "PRIVMSG", "ME!", client.currentObject);
        client.currentObject.say(msg);
        
        return;
    }
    
    switch (client.currentObject.TYPE)
    {
        case "IRCClient":
            dispatch("eval", {expression: msg});
            break;

        default:
            if (msg != "")
            {
                display(getMsg(MSG_ERR_NO_DEFAULT, client.currentObject.TYPE),
                        MT_ERROR);
            }
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

client.getTimestampCSS =
CIRCNetwork.prototype.getTimestampCSS =
CIRCChannel.prototype.getTimestampCSS =
CIRCUser.prototype.getTimestampCSS =
CIRCDCCChat.prototype.getTimestampCSS =
function this_getTimestampCSS(format)
{
    /* Wow, this is cool. We just put together a CSS-rule string based on the
     * "timestampFormat" preferences. *This* is what CSS is all about. :)
     * We also provide a "data: URL" format, to simplify other code.
     */
    var css;
    
    if (this.prefs["timestamps"])
    {
        /* Hack. To get around a Mozilla bug, we must force the display back 
         * to a displayed value.
         */
        css = ".msg-timestamp { display: table-cell; } " +
              ".msg-timestamp:before { content: '" + 
              this.prefs["timestampFormat"] + "'; }";
        
        var letters = new Array('y', 'm', 'd', 'h', 'n', 's');
        for (var i = 0; i < letters.length; i++)
        {
            css = css.replace("%" + letters[i], "' attr(time-" + 
                              letters[i] + ") '");
        }
    }
    else
    {
        /* Completely remove the <td>s if they're off, neatens display. */
        css = ".msg-timestamp { display: none; }";
    }
    
    if (format == "data")
        return "data:text/css," + encodeURIComponent(css);
    return css;
}

client.getFontCSS =
CIRCNetwork.prototype.getFontCSS =
CIRCChannel.prototype.getFontCSS =
CIRCUser.prototype.getFontCSS =
CIRCDCCChat.prototype.getFontCSS =
function this_getFontCSS(format)
{
    /* See this_getTimestampCSS. */
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
    // We like some control on the number of digits.
    function formatTimeNumber (num, digits)
    {
        var rv = num.toString();
        while (rv.length < digits)
            rv = "0" + rv;
        return rv;
    };
    
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
    var fromAttr = (sourceObj) ? sourceObj.displayName : "";
    // Attach "ME!" if appropriate, so motifs can style differently.
    if (sourceObj == me)
        fromAttr = fromAttr + " ME!";
    
    // Get the dest TYPE too...
    var toType = (destObj) ? destObj.TYPE : "unk";
    // Is the dest a user?
    var toUser = (toType.search(/IRC.*User/) != -1);    
    // Get a dest name too...
    var toAttr = (destObj) ? destObj.displayName : "";
    // Also do "ME!" work for the dest.
    if (destObj && destObj == me)
        toAttr = me.nick + " ME!";
    
    /* isImportant means to style the messages as important, and flash the
     * window, getAttention means just flash the window. */
    var isImportant = false, getAttention = false, isSuperfluous = false;
    var viewType = this.TYPE;
    var code;
    
    var d = new Date();
    var dateInfo = { y: formatTimeNumber(d.getFullYear(), 4),
                     m: formatTimeNumber(d.getMonth() + 1, 2),
                     d: formatTimeNumber(d.getDate(), 2),
                     h: formatTimeNumber(d.getHours(), 2),
                     n: formatTimeNumber(d.getMinutes(), 2),
                     s: formatTimeNumber(d.getSeconds(), 2)
                   };
    
    // Statusbar text, and the line that gets saved to the log.
    var statusString;
    var logString;
    
    var dtf = client.dtFormatter;
    var timeStamp = dtf.FormatDateTime("", dtf.dateFormatShort, 
                                       dtf.timeFormatNoSeconds, d.getFullYear(), 
                                       d.getMonth() + 1, d.getDate(), 
                                       d.getHours(), d.getMinutes(), 
                                       d.getSeconds()
                                      );
    logString = "[" + timeStamp + "] ";
    
    if (fromUser)
    {
        statusString = getMsg(MSG_FMT_STATUS, 
                              [timeStamp,
                               sourceObj.nick + "!" + 
                               sourceObj.name + "@" + sourceObj.host]);
    }
    else
    {
        var name;
        if (sourceObj)
            name = sourceObj.displayName;
        else
            name = this.displayName;
        
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
    if (fromAttr)
    {
        if (fromUser)
            msgRow.setAttribute("msg-user", sourceObj.nick);
        else
            msgRow.setAttribute("msg-source", fromAttr);
    }
    if (isImportant)
        msgTimestamp.setAttribute ("important", "true");
    
    // Timestamp cell.
    var msgRowTimestamp = document.createElementNS("http://www.w3.org/1999/xhtml",
                                                   "html:td");
    msgRowTimestamp.setAttribute("class", "msg-timestamp");
    for (var key in dateInfo)
        msgRowTimestamp.setAttribute("time-" + key, dateInfo[key]);
    
    var canMergeData;
    var msgRowSource, msgRowType, msgRowData;
    if (fromUser && msgtype.match(/^(PRIVMSG|ACTION|NOTICE)$/))
    {
        var nick = sourceObj.displayName;
        
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
                
                if (msgtype == "ACTION")
                {
                    logString += "* " + nick + " ";
                }
                else
                {
                    // If this private message is not in a query view, use 
                    // *nick* instead of <nick>.
                    if (this.TYPE == "IRCUser")
                        logString += "<" + nick + "> ";
                    else
                        logString += "*" + nick + "* ";
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
                if (msgtype == "ACTION")
                    logString += "* " + nick + " ";
                else
                    logString += "<" + nick + "> ";
            }
        }
        else
        {
            // Messages from us to somewhere...
            
            if (toUser)
            {
                // From us to a user.
                
                if (this.TYPE == "IRCUser")
                {
                    if (msgtype == "ACTION")
                        logString += "* " + nick + " ";
                    else
                        logString += "<" + nick + "> ";
                }
                else
                {
                    nick = destObj.displayName;
                    logString += ">" + nick + "< ";
                }
            }
            else
            {
                if (msgtype == "ACTION")
                    logString += "*" + nick + " ";
                else
                    logString += "<" + nick + "> ";
            }
        }
        
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
        logString += code + " ";
    }
    
    if (message)
    {
        msgRowData = document.createElementNS("http://www.w3.org/1999/xhtml",
                                           "html:td");
        msgRowData.setAttribute("class", "msg-data");
        
        if (typeof message == "string")
        {
            msgRowData.appendChild(stringToMsg(message, this));
            logString += message;
        }
        else
        {
            msgRowData.appendChild(message);
            logString += message.innerHTML.replace(/<[^<]*>/g, "");
        }
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
        o.network.displayHere("{" + this.name + "} " + message, msgtype,
                              sourceObj, destObj);
    }
    
    // Log file time!
    if (this.prefs["log"])
    {
        if (!this.logFile)
            client.openLogFile(this);
        
        try
        {
            this.logFile.write(fromUnicode(logString + client.lineEnd, "utf-8"));
        }
        catch (ex)
        {
            this.displayHere(getMsg(MSG_LOGFILE_WRITE_ERROR, this.logFile.path),
                             "ERROR");
            try
            {
                // Close log file, and stop logging.
                this.logFile.close();
                this.prefs["log"] = false;
            }
            catch(ex)
            {
                // can't do much here.
            }
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
        
        // Are we the same user as last time?
        var sameNick = (nickColumnCount > 0 &&
                        nickColumns[nickColumnCount - 1].parentNode.
                        getAttribute("msg-user") ==
                        thisUserCol.parentNode.getAttribute("msg-user"));
        
        // What was the span last time?
        var lastRowSpan = (nickColumnCount > 0) ?
            Number(nickColumns[0].getAttribute("rowspan")) : 0;
        
        if (sameNick)
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
        appendTo.appendChild(obj);
    
    if (source.MAX_MESSAGES)
    {
        if (typeof source.messageCount != "number")
            source.messageCount = 1;
        else
            source.messageCount++;
        
        if (source.messageCount > source.MAX_MESSAGES)
        {
            if (client.PRINT_DIRECTION == 1)
            {
                var height = tbody.firstChild.scrollHeight;
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
            else
            {
                tbody.removeChild (tbody.lastChild);
                --source.messageCount;
                while (tbody.lastChild && tbody.lastChild.childNodes[1] &&
                       tbody.lastChild.childNodes[1].getAttribute("class") ==
                       "msg-data")
                {
                    --source.messageCount;
                    tbody.removeChild (tbody.lastChild);
                }
            }
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
    for (var n in client.networks)
    {
        if (client.networks[n].isConnected())
            client.networks[n].quit(reason);
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
    try
    {
        view.logFile = fopen(view.prefs["logFileName"], ">>");
    }
    catch (ex)
    {
        view.displayHere(getMsg(MSG_LOGFILE_ERROR, view.logFile), MT_ERROR);
        view.logFile = null;
        view.prefs["log"] = false;
        return;
    }

    view.displayHere(getMsg(MSG_LOGFILE_OPENED,
                            getURLSpecFromFile(view.logFile.path)));
}

client.closeLogFile =
function cli_stoplog (view)
{
    view.displayHere(MSG_LOGFILE_CLOSING);

    if (view.logFile)
    {
        view.logFile.close();
        view.logFile = null;
    }
}

CIRCChannel.prototype.performTabMatch =
CIRCNetwork.prototype.performTabMatch =
CIRCUser.prototype.performTabMatch    =
CIRCDCCChat.prototype.performTabMatch =
function gettabmatch_other (line, wordStart, wordEnd, word, cursorpos)
{
    if (wordStart == 0 && line[0] == client.COMMAND_CHAR)
    {
        return client.performTabMatch (line, wordStart, wordEnd, word,
                                       cursorpos);
    }
    
    var matchList = new Array();
    var users;
    var channels;

    var details = getObjectDetails(this);

    if (details.channel && word == details.channel.name[0])
    {
        /* When we have #<tab>, we just want the current channel, 
           if possible. */
        matchList.push (details.channel.unicodeName);
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
                matchList.push (channels[c].name);
            if (!users)
                users = details.server.users;
        }

        if (users)
        {
            for (var n in users)
                matchList.push (users[n].nick);
        }
    }
    
    var matches = matchEntry (word, matchList);

    if (matches.length == 1)
    {
        if (users && matches[0] in users)
        {
            matches[0] = users[matches[0]].properNick;
            if (wordStart == 0)
                matches[0] += client.prefs["nickCompleteStr"];
        }
        else if (channels && matches[0] in channels)
        {
            matches[0] = channels[matches[0]].unicodeName;
        }

        if (wordEnd == line.length)
        {
            /* add a space if the word is at the end of the line. */
            matches[0] += " ";
        }
    }

    return matches;
}

CIRCChannel.prototype.getGraphResource =
function my_graphres ()
{
    if (!("rdfRes" in this))
    {
        var id = RES_PFX + "CHANNEL:" + this.parent.parent.name + ":" + 
            escape(this.name);
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
        
        this.rdfRes = rdf.GetResource (RES_PFX + "CUSER:" + 
                                       this.parent.parent.parent.name + ":" +
                                       escape(this.parent.name) + ":" +
                                       CIRCUser.nextResID++);

            //dd ("created cuser resource " + this.rdfRes.Value);
        
        rdf.Assert (this.rdfRes, rdf.resNick, rdf.GetLiteral(this.properNick));
        rdf.Assert (this.rdfRes, rdf.resUser, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resHost, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resSortName, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resOp, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resHalfOp, rdf.litUnk);
        rdf.Assert (this.rdfRes, rdf.resVoice, rdf.litUnk);
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
    
    rdf.Change (this.rdfRes, rdf.resNick, rdf.GetLiteral(this.properNick));
    if (this.name)
        rdf.Change (this.rdfRes, rdf.resUser, rdf.GetLiteral(this.name));
    else
        rdf.Change (this.rdfRes, rdf.resUser, rdf.litUnk);
    if (this.host)
        rdf.Change (this.rdfRes, rdf.resHost, rdf.GetLiteral(this.host));
    else
        rdf.Change (this.rdfRes, rdf.resHost, rdf.litUnk);

    var sortname;

    if (this.isOp)
        sortname = "a-";
    else if (this.isHalfOp)
        sortname = "b-";
    else if (this.isVoice)
        sortname = "c-";
    else
        sortname = "d-";
    
    sortname += this.nick;
    rdf.Change (this.rdfRes, rdf.resSortName, rdf.GetLiteral(sortname));
    rdf.Change (this.rdfRes, rdf.resOp, 
                this.isOp ? rdf.litTrue : rdf.litFalse);
    rdf.Change (this.rdfRes, rdf.resHalfOp, 
                this.isHalfOp ? rdf.litTrue : rdf.litFalse);
    rdf.Change (this.rdfRes, rdf.resVoice,
                this.isVoice ? rdf.litTrue : rdf.litFalse);
}
