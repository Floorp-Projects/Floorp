/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is ChatZilla
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Robert Ginda, rginda@netscape.com, original author
 *  Chiaki Koufugata chiaki@mozilla.gr.jp UI i18n 
 *  Samuel Sieb, samuel@sieb.net, MIRC color codes, munger menu, and various
 */

if (DEBUG)
    dd = function (m) { dump ("-*- chatzilla: " + m + "\n"); }
else
    dd = function (){};

var client = new Object();

const MSG_CSP       = getMsg ("commaSpace", " ");
const MSG_NONE      = getMsg ("none");
const MSG_UNKNOWN   = getMsg ("unknown");

client.defaultNick = getMsg( "defaultNick" );

client.version = "0.8.11";

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
client.PRINT_DIRECTION = 1; /*1 => new messages at bottom, -1 => at top */
client.ADDRESSED_NICK_SEP = ":";

client.MAX_MSG_PER_ROW = 3; /* default number of messages to collapse into a
                             * single row, max. */
client.INITIAL_COLSPAN = 5; /* MAX_MSG_PER_ROW cannot grow to greater than 
                             * one half INITIAL_COLSPAN + 1. */
client.COLLAPSE_ROWS = false;
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
client.DEFAULT_RESPONSE_CODE = "===";


/* XXX maybe move this into css */
client.responseCodeMap = new Object();
client.responseCodeMap["HELLO"]  = getMsg("responseCodeMapHello");
client.responseCodeMap["HELP"]  = getMsg("responseCodeMapHelp");
client.responseCodeMap["USAGE"]  = getMsg("responseCodeMapUsage");
client.responseCodeMap["ERROR"]  = getMsg("responseCodeMapError");
client.responseCodeMap["WARNING"]  = getMsg("responseCodeMapWarning");
client.responseCodeMap["INFO"]  = getMsg("responseCodeMapInfo");
client.responseCodeMap["EVAL-IN"]  = getMsg("responseCodeMapEvalIn");
client.responseCodeMap["EVAL-OUT"]  = getMsg("responseCodeMapEvalOut");
client.responseCodeMap["JOIN"]  = "-->|";
client.responseCodeMap["PART"]  = "<--|";
client.responseCodeMap["QUIT"]  = "|<--";
client.responseCodeMap["NICK"]  = "=-=";
client.responseCodeMap["TOPIC"] = "=-=";
client.responseCodeMap["KICK"]  = "=-=";
client.responseCodeMap["MODE"]  = "=-=";
client.responseCodeMap["END_STATUS"] = "---";
client.responseCodeMap["315"]  = "---"; /* end of WHO */
client.responseCodeMap["318"]  = "---"; /* end of WHOIS */
client.responseCodeMap["366"]  = "---"; /* end of NAMES */
client.responseCodeMap["376"]  = "---"; /* end of MOTD */

client.name = getMsg("clientname");
client.viewsArray = new Array();
client.activityList = new Object();
client.uiState = new Object(); /* state of ui elements (visible/collapsed) */
client.inputHistory = new Array();
client.lastHistoryReferenced = -1;
client.incompleteLine = "";
client.lastTabUp = new Date();
client.stalkingVictims = new Array();

CIRCNetwork.prototype.INITIAL_NICK = client.defaultNick;
CIRCNetwork.prototype.INITIAL_NAME = "chatzilla";
CIRCNetwork.prototype.INITIAL_DESC = getMsg("circnetworkInitialDesc");
CIRCNetwork.prototype.INITIAL_CHANNEL = "";
CIRCNetwork.prototype.MAX_MESSAGES = 100;
CIRCNetwork.prototype.IGNORE_MOTD = false;

CIRCServer.prototype.READ_TIMEOUT = 0;
CIRCServer.prototype.VERSION_RPLY = getMsg("circserverVersionRply",
                                           [client.version,
                                            navigator.userAgent]);
CIRCUser.prototype.MAX_MESSAGES = 200;

CIRCChannel.prototype.MAX_MESSAGES = 300;

CIRCChanUser.prototype.MAX_MESSAGES = 200;

window.onresize =
function ()
{
    for (var i = 0; i < client.deck.childNodes.length; i++)
        scrollDown(client.deck.childNodes[i], true);
}

function ucConvertIncomingMessage (e)
{
    e.meat = toUnicode(e.meat);
    return true;
}

function toUnicode (msg)
{
    if (!("ucConverter" in client))
        return msg;
    
    /* XXX set charset again to force the encoder to reset, see bug 114923 */
    client.ucConverter.charset = client.CHARSET;
    try
    {
        return client.ucConverter.ConvertToUnicode(msg);
    }
    catch (ex)
    {
        dd ("caught exception " + ex + " converting " + msg + " to charset " +
            client.CHARSET);
    }

    return msg;
}

function fromUnicode (msg)
{
    if (!("ucConverter" in client))
        return msg;
    
    /* XXX set charset again to force the encoder to reset, see bug 114923 */
    client.ucConverter.charset = client.CHARSET;
    return client.ucConverter.ConvertFromUnicode(msg);
}

function setCharset (charset)
{
    client.CHARSET = charset;
    
    if (!charset)
    {
        delete client.ucConverter;
        client.eventPump.removeHookByName("uc-hook");
        return true;
    }
    
    var ex;
    
    try
    {
        
        if (!("ucConverter" in client))
        {
            const UC_CTRID = "@mozilla.org/intl/scriptableunicodeconverter";
            const nsIUnicodeConverter = 
                Components.interfaces.nsIScriptableUnicodeConverter;
            client.ucConverter =
                Components.classes[UC_CTRID].getService(nsIUnicodeConverter);
        }
        
        client.ucConverter.charset = charset;
        
        if (!client.eventPump.getHook("uc-hook"))
        {
            client.eventPump.addHook ([{type: "parseddata", set: "server"}],
                                      ucConvertIncomingMessage, "uc-hook");
        }
    }
    catch (ex)
    {
        dd ("Caught exception setting charset to " + charset + "\n" + ex);
        delete client.ucConverter;
        client.CHARSET = "";
        client.eventPump.removeHookByName("uc-hook");
        return false;
    }

    return true;
}

function initStatic()
{
    var obj;

    const nsISound = Components.interfaces.nsISound;
    client.sound =
        Components.classes["@mozilla.org/sound;1"].createInstance(nsISound);

    if (client.CHARSET)
        setCharset(client.CHARSET);
    
    var ary = navigator.userAgent.match (/;\s*([^;\s]+\s*)\).*\/(\d+)/);
    if (ary)
        client.userAgent = "ChatZilla " + client.version + " [Mozilla " + 
            ary[1] + "/" + ary[2] + "]";
    else
        client.userAgent = "ChatZilla " + client.version + " [" + 
            navigator.userAgent + "]";

    obj = document.getElementById("input");
    obj.addEventListener("keypress", onInputKeyPress, false);
    obj = document.getElementById("multiline-input");
    obj.addEventListener("keypress", onMultilineInputKeyPress, false);
    obj = document.getElementById("channel-topicedit");
    obj.addEventListener("keypress", onTopicKeyPress, false);
    obj.active = false;

    window.onkeypress = onWindowKeyPress;

    setMenuCheck ("menu-dmessages",
                  client.eventPump.getHook ("event-tracer").enabled);
    setMenuCheck ("menu-munger-global", !client.munger.enabled);
    setMenuCheck ("menu-colors", client.enableColors);

    setupMungerMenu(client.munger);

    client.uiState["tabstrip"] =
        setMenuCheck ("menu-view-tabstrip", isVisible("view-tabs"));
    client.uiState["info"] =
        setMenuCheck ("menu-view-info", isVisible("user-list-box"));
    client.uiState["header"] =
        setMenuCheck ("menu-view-header", isVisible("header-bar-tbox"));
    client.uiState["status"] =
        setMenuCheck ("menu-view-status", isVisible("status-bar"));

    client.statusBar = new Object();
    
    client.statusBar["header-url"] =
        document.getElementById ("header-url");
    client.statusBar["server-nick"] =
        document.getElementById ("server-nick");
    client.statusBar["channel-mode"] =
        document.getElementById ("channel-mode");
    client.statusBar["channel-users"] =
        document.getElementById ("channel-users");
    client.statusBar["channel-topic"] =
        document.getElementById ("channel-topic");
    client.statusBar["channel-topicedit"] =
        document.getElementById ("channel-topicedit");

    client.statusElement = document.getElementById ("status-text");
    client.defaultStatus = getMsg ("defaultStatus");
    
    onSortCol ("usercol-nick");

    client.display (getMsg("welcome"), "HELLO");
    setCurrentObject (client);

    client.onInputNetworks();
    client.onInputCommands();

    ary = client.INITIAL_VICTIMS.split(/\s*;\s*/);
    for (i in ary)
    {
        if (ary[i])
            client.stalkingVictims.push (ary[i]);
    }

    var m = document.getElementById ("menu-settings-autosave");
    m.setAttribute ("checked", String(client.SAVE_SETTINGS));
     
    setInterval ("onNotifyTimeout()", client.NOTIFY_TIMEOUT);
    
}

function processStartupURLs()
{
    var wentSomewhere = false;

    if ("arguments" in window &&
        0 in window.arguments && typeof window.arguments[0] == "object" &&
        "url" in window.arguments[0])
    {
        var url = window.arguments[0].url;
        if (url.search(/^irc:\/?\/?$/i) == -1)
        {
            /* if the url is not irc: irc:/ or irc://, then go to it. */
            gotoIRCURL (url);
            wentSomewhere = true;
        }
    }    

    if (!wentSomewhere)
    {
        /* if we had nowhere else to go, connect to any default urls */
        var ary = client.INITIAL_URLS.split(/\s*;\s*/).reverse();
        for (var i in ary)
        {
            if (ary[i] && ary[i] != "irc://")
                gotoIRCURL (ary[i]);
        }
    }
    
    if (client.viewsArray.length > 1 && !isStartupURL("irc://"))
    {
        var tb = getTabForObject (client);
        deleteTab(tb);
        client.deck.removeChild(client.frame);
        client.deck.selectedIndex = 0;
    }
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

function setMenuCheck (id, state)
{
    var m = document.getElementById(id);
    m.setAttribute ("checked", String(Boolean(state)));
    return state;
}

function isVisible (id)
{
    var e = document.getElementById(id);

    if (!e)
    {
        dd ("** Bogus id ``" + id + "'' passed to isVisible() **");
        return false;
    }
    
    return (e.getAttribute ("collapsed") != "true");
}

function initHost(obj)
{
    obj.commands = new CCommandManager();
    addCommands (obj.commands);
    
    obj.networks = new Object();
    obj.eventPump = new CEventPump (200);

    obj.defaultCompletion = client.COMMAND_CHAR + "help ";

    obj.networks["efnet"] =
        new CIRCNetwork ("efnet",
                         [{name: "irc.mcs.net", port: 6667},
                          {name: "irc.prison.net", port: 6667},
                          {name: "irc.freei.net", port: 6667},
                          {name: "irc.magic.ca", port: 6667}],
                         obj.eventPump);
    obj.networks["moznet"] =
        new CIRCNetwork ("moznet", [{name: "irc.mozilla.org", port: 6667}],
                         obj.eventPump);
    obj.networks["hybridnet"] =
        new CIRCNetwork ("hybridnet", [{name: "irc.ssc.net", port: 6667}],
                         obj.eventPump);
    obj.networks["slashnet"] =
        new CIRCNetwork ("slashnet", [{name: "irc.slashnet.org", port:6667}],
                         obj.eventPump);
    obj.networks["dalnet"] =
        new CIRCNetwork ("dalnet", [{name: "irc.dal.net", port:6667}],
                         obj.eventPump);
    obj.networks["undernet"] =
        new CIRCNetwork ("undernet", [{name: "irc.undernet.org", port:6667}],
                         obj.eventPump);
    obj.networks["webbnet"] =
        new CIRCNetwork ("webbnet", [{name: "irc.webbnet.org", port:6667}],
                         obj.eventPump);
    obj.networks["quakenet"] =
        new CIRCNetwork ("quakenet", [{name: "irc.quakenet.org", port:6667}],
                         obj.eventPump);
    obj.networks["freenode"] =
        new CIRCNetwork ("freenode",         
                         [{name: "irc.freenode.net", port:6667}],
                         obj.eventPump);

    obj.primNet = obj.networks["efnet"];

    if (DEBUG)
        /* hook all events EXCEPT server.poll and *.event-end types
         * (the 4th param inverts the match) */
        obj.eventPump.addHook ([{type: "poll", set: /^(server|dcc-chat)$/},
                               {type: "event-end"}], event_tracer,
                               "event-tracer", true /* negate */,
                               false /* disable */);

    obj.linkRE = /((\w+):[^<>\[\]()\'\"\s]+|www(\.[^.<>\[\]()\'\"\s]+){2,})/;

    obj.munger = new CMunger();
    obj.munger.enabled = true;
    obj.munger.addRule ("link", obj.linkRE, insertLink);
    obj.munger.addRule ("mailto",
       /(?:\s|\W|^)((mailto:)?[^<>\[\]()\'\"\s]+@[^.<>\[\]()\'\"\s]+\.[^<>\[\]()\'\"\s]+)/i,
                        insertMailToLink);
    obj.munger.addRule ("bugzilla-link", /(?:\s|\W|^)(bug\s+#?\d{3,6})/i,
                        insertBugzillaLink);
    obj.munger.addRule ("channel-link",
                /(?:\s|\W|^)[@+]?(#[^<>\[\](){}\"\s]*[^:,.<>\[\](){}\'\"\s])/i,
                        insertChannelLink);
    
    obj.munger.addRule ("face",
         /((^|\s)[\<\>]?[\;\=\:]\~?[\-\^\v]?[\)\|\(pP\<\>oO0\[\]\/\\](\s|$))/,
         insertSmiley);
    obj.munger.addRule ("ear", /(?:\s|^)(\(\*)(?:\s|$)/, insertEar, false);
    obj.munger.addRule ("rheet", /(?:\s|\W|^)(rhee+t\!*)(?:\s|$)/i, insertRheet);
    obj.munger.addRule ("bold", /(?:\s|^)(\*[^*,.()]*\*)(?:[\s.,]|$)/, 
                        "chatzilla-bold");
    obj.munger.addRule ("italic", /(?:\s|^)(\/[^\/,.()]*\/)(?:[\s.,]|$)/,
                        "chatzilla-italic");
    /* allow () chars inside |code()| blocks */
    obj.munger.addRule ("teletype", /(?:\s|^)(\|[^|,.]*\|)(?:[\s.,]|$)/,
                        "chatzilla-teletype");
    obj.munger.addRule ("underline", /(?:\s|^)(\_[^_,.()]*\_)(?:[\s.,]|$)/,
                        "chatzilla-underline");
    obj.munger.addRule (".mirc-colors", /(\x03((\d{1,2})(,\d{1,2}|)|))/,
                         mircChangeColor);
    obj.munger.addRule (".mirc-bold", /(\x02)/, mircToggleBold);
    obj.munger.addRule (".mirc-underline", /(\x1f)/, mircToggleUnder);
    obj.munger.addRule (".mirc-color-reset", /(\x0f)/, mircResetColor);
    obj.munger.addRule (".mirc-reverse", /(\x16)/, mircReverseColor);
    obj.munger.addRule ("ctrl-char", /([\x01-\x1f])/, showCtrlChar);
    obj.munger.addRule ("word-hyphenator",
                        new RegExp ("(\\S{" + client.MAX_WORD_DISPLAY + ",})"),
                        insertHyphenatedWord);

    obj.rdf = new RDFHelper();
    
    obj.rdf.initTree("user-list");
    obj.rdf.setTreeRoot("user-list", obj.rdf.resNullChan);

    multilineInputMode(false);
}

function insertLink (matchText, containerTag)
{

    var href;
    
    if (matchText.match (/^[a-zA-Z-]+:/))
        href = matchText;
    else
        href = "http://" + matchText;
    
    var anchor = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:a");
    anchor.setAttribute ("href", href);
    anchor.setAttribute ("class", "chatzilla-link");
    anchor.setAttribute ("target", "_content");
    insertHyphenatedWord (matchText, anchor);
    containerTag.appendChild (anchor);
    
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
    var encodedMatchText = fromUnicode(matchText + " ");
    /* bug 114923 */
    encodedMatchText = encodedMatchText.substr(0, encodedMatchText.length - 1);

    if (!("network" in eventData) || 
        matchText.search
            (/^#(include|error|define|if|ifdef|else|elsif|endif|\d+)$/i) != -1)

    {
        containerTag.appendChild (document.createTextNode(matchText));
        return;
    }
    
    var anchor = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:a");
    anchor.setAttribute ("href", "irc://" + escape(eventData.network.name) +
                         "/" + escape (encodedMatchText));
    anchor.setAttribute ("class", "chatzilla-link");
    //anchor.setAttribute ("target", "_content");
    insertHyphenatedWord (matchText, anchor);
    containerTag.appendChild (anchor);
    
}

function insertBugzillaLink (matchText, containerTag)
{

    var number = matchText.match (/(\d+)/)[1];
    
    var anchor = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:a");
    anchor.setAttribute ("href",
                         "http://bugzilla.mozilla.org/show_bug.cgi?id=" + 
                         number);
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
                         "ftp://ftp.mozilla.org/pub/mozilla/libraries/bonus-tracks/rheet.wav");
    anchor.setAttribute ("class", "chatzilla-rheet chatzilla-link");
    //anchor.setAttribute ("target", "_content");
    insertHyphenatedWord (matchText, anchor);
    containerTag.appendChild (anchor);    
}

function insertEar (matchText, containerTag)
{
    if (client.smileyText)
        containerTag.appendChild (document.createTextNode (matchText));

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
     * chatzilla-emote-txt { display: none; } turns off chatzilla-emote-txt:after
     * as well.*/
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
    if (ary.length >= 4)
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

function msgIsImportant (msg, sourceNick, myNick)
{    
    var sv = "(" + myNick + ")";
    if (client.stalkingVictims.length > 0)
        sv += "|(" + client.stalkingVictims.join(")|(") + ")";
    
    var str = "(^|[\\W\\s])" + sv + "([\\W\\s]|$)";
    var re = new RegExp(str, "i");
    if (msg.search(re) != -1 || sourceNick && sourceNick.search(re) != -1)
    {
        playSounds(client.STALK_BEEP);
        return true;
    }

    return false;    
}

function isStartupURL(url)
{
    var ary = client.INITIAL_URLS.split(/\s*;\s*/);
    return arrayContains(ary, url);
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
    
    setCurrentObject (client.viewsArray[destKey].source);
}

function playSounds (list)
{
    var ary = list.split (" ");
    if (ary.length == 0)
        return;
    
    playSound (ary[0]);
    for (var i = 1; i < ary.length; ++i)
        setTimeout (playSound, 250 * i, ary[i]);
}

function playSound (file)
{
    if (!client.sound || !file)
        return;
    
    if (file == "beep")
    {
        client.sound.beep();
    }
    else
    {
        var uri = Components.classes["@mozilla.org/network/standard-url;1"];
        uri = uri.createInstance(Components.interfaces.nsIURI);
        uri.spec = file;
        client.sound.play (uri);
    }
}

function fillInTooltip(tipElement, id)
{
  const XLinkNS = "http://www.w3.org/1999/xlink";

  var retVal = false;

  var titleText = null;
  var XLinkTitleText = null;
  
  while (!titleText && !XLinkTitleText && tipElement) {
    if (tipElement.nodeType == Node.ELEMENT_NODE) {
      titleText = tipElement.getAttribute("title");
      XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
    }
    tipElement = tipElement.parentNode;
  }

  var texts = [titleText, XLinkTitleText];
  var tipNode = document.getElementById(id);

  for (var i = 0; i < texts.length; ++i) {
    var t = texts[i];
    if (t && t.search(/\S/) >= 0) {
      tipNode.setAttribute("label", t);
      retVal = true;
    }
  }

  return retVal;
}

/* timer-based mainloop */
function mainStep()
{
    client.eventPump.stepEvents();
    setTimeout ("mainStep()", client.STEP_TIMEOUT);
}

function getMsg (msgName)
{
    var restCount = arguments.length - 1;

    if (!("bundle" in client))
    {       
        client.bundle = 
            srGetStrBundle("chrome://chatzilla/locale/chatzilla.properties");
    }
    
    try 
    {
        if (restCount == 1 && arguments[1] instanceof Array)
        {
            return client.bundle.formatStringFromName (msgName, arguments[1], 
                                                       arguments[1].length);
        }
        else if (restCount > 0)
        {
            var subPhrases = new Array();
            for (var i = 1; i < arguments.length; ++i)
                subPhrases.push(arguments[i]);
            return client.bundle.formatStringFromName (msgName, subPhrases,
                                                         subPhrases.length);
        }

        return client.bundle.GetStringFromName (msgName);
    }
    catch (ex)
    {
        dd ("caught exception getting value for ``" + msgName + "''\n" + ex +
            "\n" + getStackTrace());
        return msgName;
    }
}

function openQueryTab (server, nick)
{    
    var usr = server.addUser(nick.toLowerCase());
    if (!("messages" in usr))
        usr.displayHere (getMsg("cli_imsgMsg3", usr.properNick), "INFO");
    server.sendData ("WHO " + nick + "\n");
    return usr;
}

function arraySpeak (ary, single, plural)
{
    var rv = "";
    var and = getMsg ("arraySpeakAnd");
    
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

function quicklistCallback (element, ndx, ary) 
{   
    /* Check whether the selected attribute == true */
    if (element.getAttribute("selected") == "true")
    {
        /* extract the nick data from the element */
        /* Hmmm, nice walk ... */
        ary.push(element.childNodes[0].childNodes[2].childNodes[0].nodeValue);
    }    
}

function getObjectDetails (obj, rv)
{
    if (!rv)
        rv = new Object();

    if (!obj || (typeof obj != "object"))
    {
        dd ("** INVALID OBJECT passed to getObjectDetails (" + obj + "). **");
        dd (getStackTrace());
    }

    rv.orig = obj;
    rv.parent = ("parent" in obj) ? obj.parent : null;
    
    switch (obj.TYPE)
    {
        case "IRCChannel":
            rv.channel = obj;
            rv.server = rv.channel.parent;
            rv.network = rv.server.parent;
            break;

        case "IRCUser":
            rv.user = obj;
            rv.server = rv.user.parent;
            rv.network = rv.server.parent;
            break;

        case "IRCChanUser":
            rv.user = obj;
            rv.channel = rv.user.parent;
            rv.server = rv.channel.parent;
            rv.network = rv.server.parent;
            break;

        case "IRCNetwork":
            rv.network = obj;
            if ("primServ" in rv.network)
                rv.server = rv.network.primServ;
            break;

        case "IRCClient":
            if ("lastNetwork" in obj)
            {
                rv.network = obj.lastNetwork;
                if (rv.network.isConnected())
                    rv.server = rv.network.primServ;
            }
            break;
            
        default:
            /* no setup for unknown object */
            break;
    }

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

function setClientOutput(doc) 
{
    client.deck = document.getElementById('output-deck');
    //XXXcreateHighlightMenu();
}

function createHighlightMenu()
{
    /* Look for "special" highlighting rules int he motif.  These special rules
     * are in the format ``.chatzilla-highlight[name="<display-name>"] { ... }''
     * where <display-name> is a textual description to be placed in the
     * Highlight submenu of the message area context menu.  The body of
     * these rules can be applied by the user to different irc messages.  They
     * are special becaus they do not actually match an element in the content
     * model.  The style body is copied into a new rule that matches a pettern
     * determined by the user.
     */
    function processStyleRules(rules)
    {
        for (var i = 0; i < rules.length; ++i)
        {
            var rule = rules.item(i);
            if (rule instanceof CSSStyleRule)
            {
                var ary = rule.selectorText.
                    match(/\.chatzilla-highlight\[name=\"?([^\"]+)\"?\]/i);
                if (ary)
                {
                    menuitem = document.createElement("menuitem");
                    menuitem.setAttribute ("class", "highlight-menu-item");
                    menuitem.setAttribute ("label", ary[1]);
                    menuitem.setAttribute ("oncommand", "onPopupHighlight('" + 
                                           rule.style.cssText + "');");
                    menuitem.setAttribute ("style", rule.style.cssText);
                    menu.appendChild(menuitem);
                }
            }
            else if (rule instanceof CSSImportRule)
            {
                processStyleRules(rule.styleSheet.cssRules);
            }
        }
    }
    
    var menu = document.getElementById("highlightMenu");
    while (menu.firstChild)
        menu.removeChild(menu.firstChild);

    var menuitem = document.createElement("menuitem");
    menuitem.setAttribute ("label", getMsg("noStyle"));
    menuitem.setAttribute ("class", "highlight-menu-item");
    menuitem.setAttribute ("oncommand", "onPopupHighlight('');");
    menu.appendChild(menuitem);
    
    processStyleRules(frames[0].document.styleSheets[0].cssRules);
}

function setupMungerMenu(munger)
{
    var menu = document.getElementById("menu-munger");
    for (var entry in munger.entries)
    {
        if (entry[0] != ".")
        {
            var menuitem = document.createElement("menuitem");
            menuitem.setAttribute ("label", munger.entries[entry].description);
            menuitem.setAttribute ("id", "menu-munger-" + entry);
            menuitem.setAttribute ("type", "checkbox");
            if (munger.entries[entry].enabled)
                menuitem.setAttribute ("checked", "true");
            menuitem.setAttribute ("oncommand", "onToggleMungerEntry('" + 
                                   entry + "');");
            menu.appendChild(menuitem);
        }
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
    rv.needpass = false;
    rv.needkey = false;
    rv.isnick = false;
    rv.isserver = false;
    
    if (url.search(/^(irc:\/?\/?)$/i) != -1)
        return rv;

    rv.host = client.DEFAULT_NETWORK;
    
    /* split url into <host>/<everything-else> pieces */
    var ary = url.match (/^irc:\/\/([^\/\s]+)?(\/.*)?\s*$/i);
    if (!ary)
    {
        dd ("parseIRCURL: initial split failed");
        return null;
    }
    var host = ary[1];
    var rest = (2 in ary) ? ary[2] : "";

    /* split <host> into server (or network) / port */
    ary = host.match (/^([^\s\:]+)?(\:\d+)?$/);
    if (!ary)
    {
        dd ("parseIRCURL: host/port split failed");
        return null;
    }
    
    if (2 in ary)
    {
        if (!(1 in ary))
        {
            dd ("parseIRCURL: port with no host");
            return null;
        }
        specifiedHost = rv.host = ary[1].toLowerCase();
        rv.isserver = true;
        rv.port = parseInt(ary[2].substr(1));
    }
    else if (1 in ary)
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
        
        rv.target = (1 in ary) ? 
            unescape(ary[1]).replace("\n", "\\n") : "";
        var params = (2 in ary) ? ary[2].toLowerCase() : "";
        var query = (3 in ary) ? ary[3] : "";

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
        
            rv.isserver =
                (params.search (/,\s*isserver\s*,|,\s*isserver\s*$/) != -1);
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
            ary = query.match
                (/^\?msg=([^\&]*)$|^\?msg=([^\&]*)\&|\&msg=([^\&]*)\&|\&msg=([^\&]*)$/);
            if (ary)
                for (var i = 1; i < ary.length; i++)
                    if (i in ary)
                    {
                        rv.msg = unescape(ary[i]).replace ("\n", "\\n");
                        break;
                    }
            
        }
    }

    return rv;
    
}

function gotoIRCURL (url)
{
    if (typeof url == "string")
        url = parseIRCURL(url);
    
    if (!url)
    {
        window.alert (getMsg("badIRCURL",url));
        return;
    }

    if (!url.host)
    {
        /* focus the *client* view for irc:, irc:/, and irc:// (the only irc
         * urls that don't have a host.  (irc:/// implies a connect to the
         * default network.)
         */
        onSimulateCommand("/client");
        return;
    }

    var net;
    var pass = "";
    
    if (url.needpass)
        pass = window.prompt (getMsg("gotoIRCURLMsg2",url.spec));
    
    if (url.isserver)
    {
        var alreadyThere = false;
        for (var n in client.networks)
        {
            if ((client.networks[n].isConnected()) &&
                (client.networks[n].primServ.connection.host == url.host) &&
                (client.networks[n].primServ.connection.port == url.port))
            {
                /* already connected to this server/port */
                net = client.networks[n];
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
            client.onInputServer ({inputData: url.host + " " + url.port +
                                                  " " + pass});
            net = client.networks[url.host];
            if (!("pendingURLs" in net))
                net.pendingURLs = new Array();
            net.pendingURLs.push (url);            
            return;
        }
    }
    else
    /* parsed as a network name */
    {
        net = client.networks[url.host];
        if (!net.isConnected())
        {
            /*
            dd ("gotoIRCURL: not already connected to " +
                "network " + url.host + " trying to connect...");
            */
            client.connectToNetwork (url.host, pass);
            if (!("pendingURLs" in net))
                net.pendingURLs = new Array();
            net.pendingURLs.push (url);            
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
            {
                nick = ary[0];
            }    

            ev =  {inputData: nick, network: net, server: net.primServ};
            client.onInputQuery(ev);
            targetObject = ev.user;
        }
        else
        {
            /* url points to a channel */
            var key = "";
            if (url.needkey)
                key = window.prompt (getMsg("gotoIRCURLMsg3", url.spec));
            ev = {inputData: url.target + " " + key,
                  network: net, server: net.primServ};
            client.onInputJoin (ev);
            targetObject = ev.channel;
        }

        if (url.msg)
        {
            var msg;
            if (url.msg.indexOf("\01ACTION") == 0)
            {
                msg = filterOutput (url.msg, "ACTION", "ME!");
                targetObject.display (msg, "ACTION", "ME!",
                                      client.currentObject);
            }
            else
            {
                msg = filterOutput (url.msg, "PRIVMSG", "ME!");
                targetObject.display (msg, "PRIVMSG", "ME!",
                                      client.currentObject);
            }
            targetObject.say (fromUnicode(msg));
            setCurrentObject (targetObject);
        }
    }
    else
    {
        if (!net.messages)
            net.displayHere (getMsg("gotoIRCURLMsg4",net.name),
                             "INFO");
        setCurrentObject (net);
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
    
function updateNetwork(obj)
{
    var o = getObjectDetails (client.currentObject);

    var lag = MSG_UNKNOWN;
    var nick = "";
    if ("server" in o)
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
    client.statusBar["server-nick"].setAttribute("value", nick);
}

function updateChannel (obj)
{
    if (obj && obj != client.currentObject)
        return;
    
    var o = getObjectDetails (client.currentObject);

    var mode = MSG_NONE, users = MSG_NONE, topic = MSG_UNKNOWN;

    if ("channel" in o)
    {
        mode = o.channel.mode.getModeStr();
        if (!mode)
            mode = MSG_NONE;
        users = getMsg("userCountDetails",
                       [o.channel.getUsersLength(), o.channel.opCount,
                        o.channel.voiceCount]);
        topic = o.channel.topic ? o.channel.topic : MSG_NONE;
    }
    
    client.statusBar["channel-mode"].setAttribute("value", mode);
    client.statusBar["channel-users"].setAttribute("value", users);
    var regex = new RegExp ("(\\S{" + client.MAX_WORD_DISPLAY + ",})", "g");
    var ary = topic.match(regex);
    if (ary && ary.length)
    {
        for (var i = 0; i < ary.length; ++i)
        {
            var hyphenated = hyphenateWord(ary[i], client.MAX_WORD_DISPLAY);
            topic = topic.replace(ary[i], hyphenated);
        }
    }        

    client.statusBar["channel-topic"].firstChild.data = topic;

}

function updateTitle (obj)
{
    if (!("currentObject" in client) || (obj && obj != client.currentObject))
        return;

    var tstring;
    var o = getObjectDetails (client.currentObject);
    var net = "network" in o ? o.network.name : "";
    var nick = "";
    
    switch (client.currentObject.TYPE)
    {
        case "IRCNetwork":
            var serv = "", port = "";
            if ("server" in o)
            {
                serv = o.server.connection.host;
                port = o.server.connection.port;
                if (o.server.me)
                    nick = o.server.me.properNick;
                tstring = getMsg("updateTitleNetwork", [nick, net, serv, port]);
            }
            else
            {
                nick = client.currentObject.INITIAL_NICK;
                tstring = getMsg("updateTitleNetwork2", [nick, net]);
            }
            break;
            
        case "IRCChannel":
            var chan = "", mode = "", topic = "";
            nick = "me" in o.parent ? o.parent.me.properNick : 
                getMsg ("updateTitleNoNick");
            chan = o.channel.unicodeName;
            mode = o.channel.mode.getModeStr();
            if (!mode)
                mode = getMsg("updateTitleNoMode");
            topic = o.channel.topic ? o.channel.topic : 
                                      getMsg("updateTitleNoTopic");

            tstring = getMsg("updateTitleChannel", [nick, chan, mode, topic]);
            break;

        case "IRCUser":
            nick = client.currentObject.properNick;
            var source = "";
            if (client.currentObject.name)
            {
                source = client.currentObject.name + "@" +
                    client.currentObject.host;
            }
            tstring = getMsg("updateTitleUser", [nick, source]);
            //client.statusBar["channel-topic"].setAttribute("value", tstring);
            client.statusBar["channel-topic"].firstChild.data = tstring;
            break;

        default:
            tstring = getMsg("updateTitleUnknown");
            break;
    }

    if (!client.uiState["tabstrip"])
    {
        var actl = new Array();
        for (var i in client.activityList)
            actl.push ((client.activityList[i] == "!") ?
                       (Number(i) + 1) + "!" : (Number(i) + 1));
        if (actl.length > 0)
            tstring = getMsg("updateTitleWithActivity",
                              [tstring, actl.join (MSG_CSP)]);
    }

    document.title = tstring;

}

function multilineInputMode (state)
{
    var multiInput = document.getElementById("multiline-input");
    var singleInput = document.getElementById("input");
    var splitter = document.getElementById("input-splitter");
    var iw = document.getElementById("input-widgets");
    var h;

    client._mlMode = state;
    
    if (state)  /* turn on multiline input mode */
    {
        
        h = iw.getAttribute ("lastHeight");
        if (h)
            iw.setAttribute ("height", h); /* restore the slider position */

        singleInput.setAttribute ("collapsed", "true");
        splitter.setAttribute ("collapsed", "false");
        multiInput.setAttribute ("collapsed", "false");
        client.input = multiInput;
    }
    else  /* turn off multiline input mode */
    {
        h = iw.getAttribute ("height");
        iw.setAttribute ("lastHeight", h); /* save the slider position */
        iw.removeAttribute ("height");     /* let the slider drop */
        
        splitter.setAttribute ("collapsed", "true");
        multiInput.setAttribute ("collapsed", "true");
        singleInput.setAttribute ("collapsed", "false");
        client.input = singleInput;
    }

    client.input.focus();
}

function focusInput ()
{
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
            dd ("** INVALID TYPE ('" + typeof data + "') passed to " +
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
    return ("contentWindow" in panel ? panel.contentWindow : undefined);
}

client.__defineGetter__ ("currentFrame", getFrame);

function setCurrentObject (obj)
{
    if (!obj.messages)
    {
        dd ("** INVALID OBJECT passed to setCurrentObject **");
        return;
    }

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
    if (isVisible("user-list-box"))
    {
        /* Remove currently selected items before this tree gets rerooted,
         * because it seems to remember the selections for eternity if not. */
        if (userList.treeBoxObject.selection)
            userList.treeBoxObject.selection.clearSelection ();

        if (obj.TYPE == "IRCChannel")
            client.rdf.setTreeRoot ("user-list", obj.getGraphResource());
        else
            client.rdf.setTreeRoot ("user-list", client.rdf.resNullChan);
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

    updateNetwork();
    updateChannel();
    updateTitle ();

    if (client.PRINT_DIRECTION == 1)
        scrollDown(obj.frame, false);
    
    onTopicEditEnd();

    if (client.currentObject.TYPE == "IRCChannel")
        client.statusBar["channel-topic"].setAttribute ("editable", "true");
    else
        client.statusBar["channel-topic"].removeAttribute ("editable");

    var status = document.getElementById("offline-status");
    if (client.currentObject == client)
    {
        status.setAttribute ("hidden", "true");
    }
    else
    {
        status.removeAttribute ("hidden");
        var details = getObjectDetails(client.currentObject);
        if ("network" in details && details.network.isConnected())
            status.removeAttribute ("offline");    
        else
            status.setAttribute ("offline", "true");
    }
}

function checkScroll(frame)
{
    if (!frame || !("contentWindow" in frame))
        return false;

    var w = frame.contentWindow;
    return ((w.document.height - (w.innerHeight + w.pageYOffset)) < 160);
}

function scrollDown(frame, force)
{
    if (!frame || !("contentWindow" in frame))
        return;

    var w = frame.contentWindow;

    if (force || checkScroll(frame))
        w.scrollTo(0, w.document.height);
}

/* valid values for |what| are "superfluous", "activity", and "attention".
 * final value for state is dependant on priority of the current state, and the
 * new state. the priority is: normal < superfluous < activity < attention.
 */
function setTabState (source, what)
{
    if (typeof source != "object")
        source = client.viewsArray[source].source;
    
    var tb = getTabForObject (source, true);
    var vk = Number(tb.getAttribute("viewKey"));
    
    if ("currentObject" in client && client.currentObject != source)
    {
        var state = tb.getAttribute ("state");
        if (state == what)
        {
            /* if the tab state has an equal priority to what we are setting
             * then blink it */
            tb.setAttribute ("state", "normal");
            setTimeout (setTabState, 200, vk, what);
        }
        else
        {
            if (state == "normal" || state == "superfluous" ||
               (state == "activity" && what == "attention"))
            {
                /* if the tab state has a lower priority than what we are
                 * setting, change it to the new state */
                tb.setAttribute ("state", what);
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
                tb.setAttribute ("state", what);
                setTimeout (setTabState, 200, vk, state);
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

    if (client.FLASH_WINDOW)
        window.getAttention();
    
}

function getFrameForDOMWindow(window)
{
    var frame;
    for (i = 0; i < client.deck.childNodes.length; i++)
    {
        frame = client.deck.childNodes[i];
        if (frame.contentWindow == window)
            return frame;
    }
    return undefined;
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
    const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;    const START = nsIWebProgressListener.STATE_START;
    const STOP = nsIWebProgressListener.STATE_STOP;

    var frame;
    if (stateFlags & START)
    {
        frame = getFrameForDOMWindow(webProgress.DOMWindow);
        if (!frame)
        {
            dd("can't find frame for window")
            webProgress.removeProgressListener (this);
            return;
        }
    }
    else if (stateFlags == 786448)
    {
        frame = getFrameForDOMWindow(webProgress.DOMWindow);
        if (!frame)
        {
            dd("can't find frame for window")
            webProgress.removeProgressListener (this);
            return;
        }
        frame.contentDocument.body.appendChild(frame.source.messages);
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

function syncOutputFrame(iframe)
{
    const nsIWebProgress = Components.interfaces.nsIWebProgress;
    const ALL = nsIWebProgress.NOTIFY_ALL;
    const DOCUMENT = nsIWebProgress.NOTIFY_STATE_DOCUMENT;
    const WINDOW = nsIWebProgress.NOTIFY_STATE_WINDOW;
 
    function tryAgain ()
    {
        syncOutputFrame(iframe);
    };
 
    try
    {
        if ("contentDocument" in iframe && "webProgress" in iframe)
        {
 
            iframe.addProgressListener (client.progressListener, WINDOW);
            iframe.loadURI ("chrome://chatzilla/content/outputwindow.html?" + 
                            client.DEFAULT_STYLE);
        }
        else
        {
            setTimeout (tryAgain, 500);
        }
    }
    catch (ex)
    {
        dd ("caught exception showing session view, will try again later.");
        dd (dumpObjectTree(ex)+"\n");
        setTimeout (tryAgain, 500);
    }
}

function createMessages(source)
{
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

    if (!source)
    {
        dd ("** UNDEFINED passed to getTabForObject **");
        dd (getStackTrace());
        return null;
    }
    
    switch (source.TYPE)
    {
        case "IRCChanUser":
        case "IRCUser":
            name = source.nick;
            break;
            
        case "IRCNetwork":
        case "IRCClient":
            name = source.name;
            break;
        case "IRCChannel":
            name = source.unicodeName;
            break;

        default:
            dd ("** INVALID OBJECT passed to getTabForObject **");
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
        tb.setAttribute ("onclick", "onTabClick(" + id.quote() + ");");
        tb.setAttribute ("crop", "right");
        
        tb.setAttribute ("class", "tab-bottom view-button");
        tb.setAttribute ("id", id);
        tb.setAttribute ("state", "normal");

        client.viewsArray.push ({source: source, tb: tb});
        tb.setAttribute ("viewKey", client.viewsArray.length - 1);
        if (matches > 1)
            tb.setAttribute("label", name + "<" + matches + ">");
        else
            tb.setAttribute("label", name);

        views.appendChild (tb);        

        var browser = document.createElement ("browser");
        browser.setAttribute ("class", "output-container");
        browser.setAttribute ("type", "content");
        browser.setAttribute ("flex", "1");
        browser.setAttribute ("tooltip", "aHTMLTooltip");
        browser.setAttribute ("context", "outputContext");
        //browser.setAttribute ("onload", "scrollDown(true);");
        browser.setAttribute ("onclick", "focusInput()");
        browser.setAttribute ("ondragover", "nsDragAndDrop.dragOver(event, contentDropObserver);");
        browser.setAttribute ("ondragdrop", "nsDragAndDrop.drop(event, contentDropObserver);");
        browser.setAttribute ("ondraggesture", "nsDragAndDrop.startDrag(event, contentAreaDNDObserver);");
        browser.source = source;
        source.frame = browser;
        client.deck.appendChild (browser);
        syncOutputFrame (browser);
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
    var url = transferUtils.retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);
    if (!url || url.search(client.linkRE) == -1)
        return;
    
    if (url.search(/\.css$/i) != -1  && confirm (getMsg("tabdnd_drop", url)))
    {
        onSimulateCommand("/css " + url);
    }
    else if (url.search(/^irc:\/\//i) != -1)
    {
        gotoIRCURL (url);
    }
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
    var i, key = Number(tb.getAttribute("viewKey"));
    
    if (!isNaN(key))
    {
        if ("isPermanent" in client.viewsArray[key].source &&
            client.viewsArray[key].source.isPermanent)
        {
            window.alert (getMsg("deleteTabMsg"));
            return -1;
        }
        else
        {
            /* re-index higher toolbuttons */
            for (i = key + 1; i < client.viewsArray.length; i++)
            {
                client.viewsArray[i].tb.setAttribute ("viewKey", i - 1);
            }
            arrayRemoveAt(client.viewsArray, key);
            var tbinner = document.getElementById("views-tbar-inner");
            tbinner.removeChild(tb);
        }
    }
    else
        dd  ("*** INVALID OBJECT passed to deleteTab (" + tb + ") " +
             "no viewKey attribute. (" + key + ")");

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

client.connectToNetwork =
function cli_connet (netname, pass)
{
    if (!(netname in client.networks))
    {
        display (getMsg("cli_attachNoNet", netname), "ERROR");
        return false;
    }
    
    var netobj = client.networks[netname];

    if (!("messages" in netobj))
        netobj.displayHere (getMsg("cli_attachOpened", netname), "INFO");
    setCurrentObject(netobj);

    if (netobj.isConnected())
    {
        netobj.display (getMsg("cli_attachAlreadyThere", netname), "ERROR");
        return true;
    }
    
    if (CIRCNetwork.prototype.INITIAL_NICK == client.defaultNick)
        CIRCNetwork.prototype.INITIAL_NICK =
            prompt (getMsg("cli_attachGetNick"), client.defaultNick);
    
    if (!("connecting" in netobj))
        netobj.display (getMsg("cli_attachWorking",netobj.name), "INFO");
    netobj.connect(pass);
    return true;
}


client.getURL =
function cli_geturl ()
{
    return "irc://";
}

client.load =
function cli_load(url, obj)
{
    if (!client._loader)
    {
        const LOADER_CTRID = "@mozilla.org/moz/jssubscript-loader;1";
        const mozIJSSubScriptLoader = 
            Components.interfaces.mozIJSSubScriptLoader;

        var cls;
        if ((cls = Components.classes[LOADER_CTRID]))
            client._loader = cls.createInstance (mozIJSSubScriptLoader);
    }
    
    try {
        client.currentObject.display (getMsg("cli_loadLoading", url));
        client._loader.loadSubScript (url, obj);
    }
    catch (ex)
    {
        var msg = getMsg("cli_loadError", ex.lineNumber, ex.fileName, ex);
        client.currentObject.display (msg, "ERROR");        
    }
}

    
client.sayToCurrentTarget =
function cli_say(msg)
{
    switch (client.currentObject.TYPE)
    {
        case "IRCChannel":
        case "IRCUser":
        case "IRCChanUser":
            msg = filterOutput (msg, "PRIVMSG");
            client.currentObject.display (msg, "PRIVMSG", "ME!",
                                          client.currentObject);
            client.currentObject.say (fromUnicode(msg));
            break;

        case "IRCClient":
            client.onInputEval ({inputData: msg});
            break;

        default:
            if (msg != "")
                client.currentObject.display 
                    (getMsg("cli_sayMsg", client.currentObject.TYPE), "ERROR");
            break;
    }

}

CIRCNetwork.prototype.display =
function n_display (message, msgtype, sourceObj, destObj)
{
    var o = getObjectDetails(client.currentObject);

    if (client.SLOPPY_NETWORKS && client.currentObject != client &&
        client.currentObject != this && o.network == this &&
        o.server.connection.isConnected)
        client.currentObject.display (message, msgtype, sourceObj, destObj);
    else
        this.displayHere (message, msgtype, sourceObj, destObj);
}

CIRCUser.prototype.display =
function u_display(message, msgtype, sourceObj, destObj)
{
    if ("messages" in this)
        this.displayHere (message, msgtype, sourceObj, destObj);
    else
    {
        var o = getObjectDetails(client.currentObject);
        if (o.server.connection.isConnected &&
            o.network == this.parent.parent &&
            client.currentObject.TYPE != "IRCUser")
            client.currentObject.display (message, msgtype, sourceObj, destObj);
        else
            this.parent.parent.displayHere (message, msgtype, sourceObj,
                                            destObj);
    }
}

function display (message, msgtype, sourceObj, destObj)
{
    client.currentObject.display (message, msgtype, sourceObj, destObj);
}

client.display =
client.displayHere =
CIRCNetwork.prototype.displayHere =
CIRCChannel.prototype.display =
CIRCChannel.prototype.displayHere =
CIRCUser.prototype.displayHere =
function __display(message, msgtype, sourceObj, destObj)
{            
    var canMergeData = false;
    var canCollapseRow = false;

    function setAttribs (obj, c, attrs)
    {
        if (attrs)
        {
            for (var a in attrs)
                obj.setAttribute (a, attrs[a]);
        }
        obj.setAttribute ("class", c);
        obj.setAttribute ("msg-type", msgtype);
        obj.setAttribute ("msg-dest", toAttr);
        obj.setAttribute ("dest-type", toType);
        obj.setAttribute ("view-type", viewType); 
        if (fromAttr)
            if (fromUser)
                obj.setAttribute ("msg-user", fromAttr);
            else
                obj.setAttribute ("msg-source", fromAttr);
   }

    var blockLevel = false; /* true if this row should be rendered at block
                             * level, (like, if it has a really long nickname
                             * that might disturb the rest of the layout)     */
    var o = getObjectDetails (this);          /* get the skinny on |this|     */
    var me;
    if ("server" in o && "me" in o.server)
    {
        me = o.server.me;    /* get the object representing the user           */
    }
    if (sourceObj == "ME!") sourceObj = me;   /* if the caller to passes "ME!"*/
    if (destObj == "ME!") destObj = me;       /* substitute the actual object */

    var fromType = (sourceObj && sourceObj.TYPE) ? sourceObj.TYPE : "unk";
    var fromUser = (fromType.search(/IRC.*User/) != -1);    
    var fromAttr;
    if      (fromUser && sourceObj == me)  fromAttr = me.nick + " ME!";
    else if (fromUser)                     fromAttr = sourceObj.nick;
    else if (typeof sourceObj == "object") fromAttr = sourceObj.name;

    var toType = (destObj) ? destObj.TYPE : "unk";
    var toAttr;
    
    if (destObj && destObj == me)
        toAttr = me.nick + " ME!";
    else if (toType == "IRCUser")
        toAttr = destObj.nick;
    else if (typeof destObj == "object")
        toAttr = destObj.name;

    /* isImportant means to style the messages as important, and flash the
     * window, getAttention means just flash the window. */
    var isImportant = false, getAttention = false, isSuperfluous = false;
    var viewType = this.TYPE;
    var code;
    var msgRow = document.createElementNS("http://www.w3.org/1999/xhtml",
                                          "html:tr");
    setAttribs(msgRow, "msg");

    //dd ("fromType is " + fromType + ", fromAttr is " + fromAttr);
    var d = new Date();
    var mins = d.getMinutes();
    if (mins < 10)
        mins = "0" + mins;
    var statusString;
    
    if (fromUser)
    {
        statusString =
            getMsg("cli_statusString", [d.getMonth() + 1, d.getDate(),
                                        d.getHours(), mins,
                                        sourceObj.nick + "!" + 
                                        sourceObj.name + "@" + sourceObj.host]);
    }
    else
    {
        var name;
        if (sourceObj)
        {
            name = (sourceObj.TYPE == "CIRCChannel") ?
                sourceObj.unicodeName : sourceObj.name;
        }
        else
        {
            name = (this.TYPE == "CIRCChannel") ?
                this.unicodeName : this.name;
        }

        statusString =
            getMsg("cli_statusString", [d.getMonth() + 1, d.getDate(),
                                        d.getHours(), mins, name]);
    }
    
    if (fromType.search(/IRC.*User/) != -1 &&
        msgtype.search(/PRIVMSG|ACTION|NOTICE/) != -1)
    {
        /* do nick things here */
        var nick;
        var nickURL;
        
        if (sourceObj != me)
        {
            nick = sourceObj.properNick;
            if (!nick)
                nick = sourceObj.name + "@" + sourceObj.host;
            else if ("getURL" in sourceObj)
                nickURL = sourceObj.getURL();
            
            if (toType == "IRCUser") /* msg from user to me */
            {
                getAttention = true;
                this.defaultCompletion = "/msg " + nick + " ";
            }
            else /* msg from user to channel */
            {
                if (typeof (message == "string") && me)
                {
                    isImportant = msgIsImportant (message, nick, me.nick);
                    if (isImportant)
                        this.defaultCompletion = nick +
                            client.ADDRESSED_NICK_SEP + " ";
                }
            }
        }
        else if (toType == "IRCUser") /* msg from me to user */
        {
            if (this.TYPE == "IRCUser")
                nick    = sourceObj.properNick;
            else
                nick    = destObj.properNick;
        }
        else /* msg from me to channel */
        {
            nick = sourceObj.properNick;
        }
        
        if (!("mark" in this))
            this.mark = "odd";
        
        if (!("lastNickDisplayed" in this) ||
            this.lastNickDisplayed != nick)
        {
            this.lastNickDisplayed = nick;
            this.mark = (this.mark == "even") ? "odd" : "even";
        }

        var msgSource = document.createElementNS("http://www.w3.org/1999/xhtml",
                                                 "html:td");
        setAttribs (msgSource, "msg-user", {statusText: statusString});
        if (isImportant)
            msgSource.setAttribute ("important", "true");
        if (nick.length > client.MAX_NICK_DISPLAY)
            blockLevel = true;
        if (nickURL)
        {
            var nick_anchor =
                document.createElementNS("http://www.w3.org/1999/xhtml",
                                         "html:a");
            nick_anchor.setAttribute ("class", "chatzilla-link");
            nick_anchor.setAttribute ("href", nickURL);
            nick_anchor.appendChild (newInlineText (nick));
            msgSource.appendChild (nick_anchor);
        }
        else
        {
            msgSource.appendChild (newInlineText (nick));
        }
        msgRow.appendChild (msgSource);
        canMergeData = client.COLLAPSE_MSGS;
        canCollapseRow = client.COLLAPSE_ROWS;

    }
    else
    {
        isSuperfluous = true;
        if (!client.debugMode && msgtype in client.responseCodeMap)
        {
            code = client.responseCodeMap[msgtype];
        }
        else
        {
            if (!client.debugMode && client.HIDE_CODES)
                code = client.DEFAULT_RESPONSE_CODE;
            else
                code = "[" + msgtype + "]";
        }
        
        /* Display the message code */
        var msgType = document.createElementNS("http://www.w3.org/1999/xhtml",
                                               "html:td");
        setAttribs (msgType, "msg-type", {statusText: statusString});

        msgType.appendChild (newInlineText (code));
        msgRow.appendChild (msgType);
    }
             
    if (message)
    {
        var msgData = document.createElementNS("http://www.w3.org/1999/xhtml",
                                               "html:td");
        setAttribs (msgData, "msg-data", {statusText: statusString, 
                                          colspan: client.INITIAL_COLSPAN});
        if (isImportant)
            msgData.setAttribute ("important", "true");

        if ("mark" in this)
            msgData.setAttribute ("mark", this.mark);
        
        if (typeof message == "string")
        {
            msgData.appendChild (stringToMsg (message, this));
        }
        else
            msgData.appendChild (message);

        msgRow.appendChild (msgData);
    }

    if (isImportant)
        msgRow.setAttribute ("important", "true");

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
        td.setAttribute ("colspan", "2");
        
        tr.appendChild(td);
        var table = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                              "html:table");
        table.setAttribute ("class", "msg-nested-table");
        table.setAttribute ("cellpadding", "0");
        
        td.appendChild (table);
        var tbody =  document.createElementNS ("http://www.w3.org/1999/xhtml",
                                               "html:tbody");
        
        tbody.appendChild (msgRow);
        table.appendChild (tbody);
        msgRow = tr;
        canMergeData = false;
        canCollapseRow = false;
    }

    addHistory (this, msgRow, canMergeData, canCollapseRow);
    if (isImportant || getAttention)
    {
        setTabState(this, "attention");
        if (client.FLASH_WINDOW)
            window.getAttention();
    }
    else
    {
        if (isSuperfluous)
            setTabState(this, "superfluous");
        else
            setTabState(this, "activity");
    }

    if (isImportant && client.COPY_MESSAGES)
    {
        if ("network" in o && o.network != this)
            o.network.displayHere("{" + this.name + "} " + message, msgtype,
                                  sourceObj, destObj);
    }
}

function addHistory (source, obj, mergeData, collapseRow)
{
    if (!("messages" in source) || (source.messages == null))
        createMessages(source);

    var tbody = source.messages.firstChild;

    var needScroll = false;

    if (client.PRINT_DIRECTION == 1)
    {
        if (mergeData || collapseRow)
        {
            var thisUserCol = obj.firstChild;
            var thisMessageCol = thisUserCol.nextSibling;
            var ci = findPreviousColumnInfo(source.messages);
            var nickColumns = ci.nickColumns;
            var rowExtents = ci.extents;
            var nickColumnCount = nickColumns.length;
            var sameNick = (nickColumnCount > 0 &&
                            nickColumns[nickColumnCount - 1].
                            getAttribute("msg-user") ==
                            thisUserCol.getAttribute("msg-user"));
            var lastRowSpan = (nickColumnCount > 0) ?
                Number(nickColumns[0].getAttribute("rowspan")) : 0;
            if (sameNick && mergeData)
            {
                if (obj.getAttribute("important"))
                {
                    nickColumns[nickColumnCount - 1].setAttribute ("important",
                                                                   true);
                }
                /* message is from the same person as last time,
                 * strip the nick first... */
                obj.removeChild(obj.firstChild);
                /* Adjust height of previous cells, maybe. */
                for (i = 0; i < rowExtents.length - 1; ++i)
                {
                    var myLastData = 
                        rowExtents[i].childNodes[nickColumnCount - 1];
                    var myLastRowSpan = (myLastData) ?
                        myLastData.getAttribute("rowspan") : 0;
                    if (myLastData && myLastRowSpan > 1)
                    {
                        myLastData.removeAttribute("rowspan");
                    }
                }
                /* then add one to the colspan for the previous user columns */
                if (!lastRowSpan)
                    lastRowSpan = 1;
                for (var i = 0; i < nickColumns.length; ++i)
                    nickColumns[i].setAttribute ("rowspan", lastRowSpan + 1);
            }
            else if (!sameNick && collapseRow && nickColumnCount > 0 &&
                     nickColumnCount < client.MAX_MSG_PER_ROW)
            {
                /* message is from a different person, but is elegible to
                 * be contained by the previous row. */
                var tr = nickColumns[0].parentNode;
                for (i = 0; i < rowExtents.length; ++i)
                    rowExtents[i].lastChild.removeAttribute("colspan");
                obj.firstChild.setAttribute ("rowspan", lastRowSpan);
                tr.appendChild (obj.firstChild);
                var lastColSpan =
                    Number(rowExtents[0].lastChild.getAttribute("colspan"));
                obj.lastChild.setAttribute ("colspan", lastColSpan - 2);
                obj.lastChild.setAttribute ("rowspan", lastRowSpan);
                tr.appendChild (obj.lastChild);
                obj = null;
            }   
        }
        
        if ("frame" in source)
            needScroll = checkScroll (source.frame);
        if (obj)
            tbody.appendChild (obj);
    }
    else
        tbody.insertBefore (obj, source.messages.firstChild);
    
    if (source.MAX_MESSAGES)
    {
        if (typeof source.messageCount != "number")
            source.messageCount = 1;
        else
            source.messageCount++;

        if (source.messageCount > source.MAX_MESSAGES)
            if (client.PRINT_DIRECTION == 1)
            {
                tbody.removeChild (tbody.firstChild);
                --source.messageCount;
                while (tbody.firstChild &&
                       tbody.firstChild.firstChild.getAttribute("class") ==
                       "msg-data")
                {
                    --source.messageCount;
                    tbody.removeChild (tbody.firstChild);
                }
            }
            else
            {
                tbody.removeChild (tbody.lastChild);
                --source.messageCount;
                while (tbody.lastChild &&
                       tbody.lastChild.firstChild.getAttribute("class") ==
                       "msg-data")
                {
                    --source.messageCount;
                    tbody.removeChild (tbody.lastChild);
                }
            }
    }

    if (needScroll)
    {
        scrollDown(source.frame, true);
        setTimeout (scrollDown, 500, source.frame, false);
        setTimeout (scrollDown, 1000, source.frame, false);
        setTimeout (scrollDown, 2000, source.frame, false);
    }
}

function findPreviousColumnInfo (table)
{
    var extents = new Array();
    var tr = table.firstChild.lastChild;
    var className = tr ? tr.firstChild.getAttribute("class") : "";
    while (tr && className.search(/msg-user|msg-type|msg-nested-td/) == -1)
    {
        extents.push(tr);
        tr = tr.previousSibling;
        if (tr)
            className = tr.firstChild.getAttribute("class");
    }
    
    if (!tr || className != "msg-user")
        return {extents: [], nickColumns: []};
    
    extents.push(tr);
    var nickCol = tr.firstChild;
    var nickCols = new Array();
    while (nickCol)
    {
        if (nickCol.getAttribute("class") == "msg-user")
            nickCols.push (nickCol);
        nickCol = nickCol.nextSibling.nextSibling;
    }

    return {extents: extents, nickColumns: nickCols};
}    

client.getConnectionCount =
function cli_gccount ()
{
    var count = 0;
    
    for (var n in client.networks)
        if (client.networks[n].isConnected())
            ++count;

    return count;
}   

client.quit =
function cli_quit (reason)
{    
    for (var n in client.networks)
        if ("primServ" in client.networks[n])
            client.networks[n].quit (reason);   
}

/* gets a tab-complete match for the line of text specified by |line|.  wordStart
 * is the position within |line| that starts the word being matched, wordEnd
 * marks the end position.  |cursorPos| marks the position of the caret in the
 * textbox.
 */
client.performTabMatch =
function gettabmatch_usr (line, wordStart, wordEnd, word, cursorPos)
{
    if (wordStart != 0 || line[0] != client.COMMAND_CHAR)
        return null;

    var matches = client.commands.listNames(word.substr(1));
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

CIRCChannel.prototype.performTabMatch =
CIRCNetwork.prototype.performTabMatch =
CIRCUser.prototype.performTabMatch    =
function gettabmatch_usr (line, wordStart, wordEnd, word, cursorpos)
{
    if (wordStart == 0 && line[0] == client.COMMAND_CHAR)
        return client.performTabMatch (line, wordStart, wordEnd, word,
                                       cursorpos);
    
    if (!("users" in this))
        return [];
    
    var users = this.users;
    var nicks = new Array();
        
    for (var n in users)
        nicks.push (users[n].nick);
        
    var matches = matchEntry (word, nicks);
    if (matches.length == 1)
    {
        matches[0] = this.users[matches[0]].properNick;
        if (wordStart == 0)
            matches[0] += client.ADDRESSED_NICK_SEP;

        if (wordEnd == line.length)
        {
            /* add a space if the word is at the end of the line. */
            matches[0] += " ";
        }
    }

    return matches;
}

/**
 * Retrieves the selected nicks from the user-list
 * tree object. This grabs the tree element's
 * selected items, extracts the appropriate text
 * for the nick, promotes each nick to a CIRCChanUser
 * instance and returns an array of these objects.
 */
CIRCChannel.prototype.getSelectedUsers =
function my_getselectedusers () 
{
    var tree = document.getElementById("user-list");
    var cell; /* reference to each selected cell of the tree object */
    var rv_ary = new Array; /* return value arrray for CIRCChanUser objects */

    var rangeCount = tree.view.selection.getRangeCount();
    for (var i = 0; i < rangeCount; ++i)
    {
        var start = {}, end = {};
        tree.view.selection.getRangeAt(i, start, end);
        for (var k = start.value; k <= end.value; ++k)
        {
            var item = tree.contentView.getItemAtIndex(k);

            /* First, set the reference to the XUL element. */
            cell = item.firstChild.childNodes[2];
            
            /* Now, create an instance of CIRCChaneUser by passing the text
          *  of the cell to the getUser function of this CIRCChannel instance.
          */
            rv_ary[i] = this.getUser( cell.getAttribute("label") );
        }
    }

    /* 
     *  USAGE NOTE: If the return value is non-null, the caller
     *  can assume the array is valid, and NOT 
     *  need to check the length, and vice versa.
     */

    return rv_ary.length > 0 ? rv_ary : null;

}

CIRCChannel.prototype.getGraphResource =
function my_graphres ()
{
    if (!("rdfRes" in this))
    {
        this.rdfRes = 
            client.rdf.GetResource(RES_PFX + "CHANNEL:" +
                                   this.parent.parent.name +
                                   ":" + this.name);
            //dd ("created channel resource " + this.rdfRes.Value);

    }
    
    return this.rdfRes;
}

CIRCUser.prototype.getGraphResource =
function usr_graphres()
{
    if (this.TYPE != "IRCChanUser")
        dd ("** WARNING: cuser.getGraphResource called on wrong object **");
    
    var rdf = client.rdf;
    
    if (!("rdfRes" in this))
    {
        if (!("nextResID" in CIRCUser))
            CIRCUser.nextResID = 0;
        
        this.rdfRes = rdf.GetResource (RES_PFX + "CUSER:" + 
                                       this.parent.parent.parent.name + ":" +
                                       this.parent.name + ":" +
                                       CIRCUser.nextResID++);

            //dd ("created cuser resource " + this.rdfRes.Value);
        
        rdf.Assert (this.rdfRes, rdf.resNick, rdf.GetLiteral(this.properNick));
        if (this.name)
            rdf.Assert (this.rdfRes, rdf.resUser, rdf.GetLiteral(this.name));
        else
            rdf.Assert (this.rdfRes, rdf.resUser, rdf.litUnk);
        if (this.host)
            rdf.Assert (this.rdfRes, rdf.resHost, rdf.GetLiteral(this.host));
        else
            rdf.Assert (this.rdfRes, rdf.resHost, rdf.litUnk);

        rdf.Assert (this.rdfRes, rdf.resOp, 
                    this.isOp ? rdf.litTrue : rdf.litFalse);
        rdf.Assert (this.rdfRes, rdf.resVoice,
                    this.isVoice ? rdf.litTrue : rdf.litFalse);
    }

    return this.rdfRes;

}

CIRCUser.prototype.updateGraphResource =
function usr_updres()
{
    if (this.TYPE != "IRCChanUser")
        dd ("** WARNING: cuser.updateGraphResource called on wrong object **");

    if (!("rdfRes" in this))
        this.getGraphResource();
    
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
    
    rdf.Change (this.rdfRes, rdf.resOp, 
                this.isOp ? rdf.litTrue : rdf.litFalse);
    rdf.Change (this.rdfRes, rdf.resVoice,
                this.isVoice ? rdf.litTrue : rdf.litFalse);
}
