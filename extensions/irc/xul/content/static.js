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
 *  Robert Ginda, rginda@ndcico.com, original author
 *  Chiaki Koufugata chiaki@mozilla.gr.jp UI i18n 
 */

if (DEBUG)
    dd = function (m) { dump ("-*- chatzilla: " + m + "\n"); }
else
    dd = function (){};

var client = new Object();

client.defaultNick = getMsg( "defaultNick" );

client.version = "0.8.4";

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
client.ADDRESSED_NICK_SEP = ": ";

client.NOTIFY_TIMEOUT = 5 * 60 * 100; /* update notify list every 5 minutes */

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
client.responseCodeMap["376"]  = "---"; /* end of MOTD */
client.responseCodeMap["318"]  = "---"; /* end of WHOIS */
client.responseCodeMap["366"]  = "---"; /* end of NAMES */

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
    scrollDown();
}

function initStatic()
{
    var obj;

    const nsISound = Components.interfaces.nsISound;
    client.sound =
        Components.classes["@mozilla.org/sound;1"].createInstance(nsISound);
    
    var ary = navigator.userAgent.match (/;\s*([^;\s]+\s*)\).*\/(\d+)/);
    if (ary)
        client.userAgent = "ChatZilla " + client.version + " [Mozilla " + 
            ary[1] + "/" + ary[2] + "]";
    else
        client.userAgent = "ChatZilla " + client.version + "[" + 
            navigator.userAgent + "]";

    obj = document.getElementById("input");
    obj.addEventListener("keyup", onInputKeyUp, false);
    obj = document.getElementById("multiline-input");
    obj.addEventListener("keyup", onMultilineInputKeyUp, false);

    window.onkeypress = onWindowKeyPress;

    setMenuCheck ("menu-dmessages", 
                  client.eventPump.getHook ("event-tracer").enabled);
    setMenuCheck ("menu-munger-global", !client.munger.enabled);

    setupMungerMenu(client.munger);

    client.uiState["tabstrip"] =
        setMenuCheck ("menu-view-tabstrip", isVisible("view-tabs"));
    client.uiState["info"] =
        setMenuCheck ("menu-view-info", isVisible("user-list-box"));
    client.uiState["status"] =
        setMenuCheck ("menu-view-status", isVisible("status-bar-tbar"));

    client.statusBar = new Object();
    
    client.statusBar["net-name"] =
        document.getElementById ("net-name");
    client.statusBar["server-name"] =
        document.getElementById ("server-name");
    client.statusBar["server-nick"] =
        document.getElementById ("server-nick");
    client.statusBar["server-lag"] =
        document.getElementById ("server-lag");
    client.statusBar["last-ping"] =
        document.getElementById ("last-ping");
    client.statusBar["channel-name"] =
        document.getElementById ("channel-name");
    client.statusBar["channel-limit"] =
        document.getElementById ("channel-limit");
    client.statusBar["channel-key"] =
        document.getElementById ("channel-key");
    client.statusBar["channel-mode"] =
        document.getElementById ("channel-mode");
    client.statusBar["channel-users"] =
        document.getElementById ("channel-users");
    client.statusBar["channel-topic"] =
        document.getElementById ("channel-topic");
    client.statusBar["channel-topicby"] =
        document.getElementById ("channel-topicby");

    onSortCol ("usercol-nick");

    client.display (getMsg("welcome"), "HELLO");
    setCurrentObject (client);

    client.onInputNetworks();
    client.onInputCommands();

    ary = client.INITIAL_VICTIMS.split(";");
    for (i in ary)
    {
        ary[i] = stringTrim(ary[i]);
        if (ary[i])
            client.stalkingVictims.push (ary[i]);
    }

    var m = document.getElementById ("menu-settings-autosave");
    m.setAttribute ("checked", String(client.SAVE_SETTINGS));
     
    var wentSomewhere = false;
    
    if (window.arguments && window.arguments[0] && window.arguments[0].url)
    {
        url = window.arguments[0].url;
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
        var ary = client.INITIAL_URLS.split(";");
        for (var i in ary)
            if ((ary[i] = stringTrim(ary[i])))
                gotoIRCURL (ary[i]);
    }

    setInterval ("onNotifyTimeout()", client.NOTIFY_TIMEOUT);
    
}

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

    client.commands = new CCommandManager();
    addCommands (obj.commands);
    
    obj.networks = new Object();
    obj.eventPump = new CEventPump (200);
    
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
    obj.networks["opennet"] =
        new CIRCNetwork ("opennet",         
                         [{name:"irc.openprojects.net", port:6667},
                          {name: "eu.opirc.nu", port:6667},
                          {name: "au.opirc.nu", port:6667},
                          {name: "us.opirc.nu", port:6667}],
                         obj.eventPump);

    obj.primNet = obj.networks["efnet"];

    if (DEBUG)
        /* hook all events EXCEPT server.poll and *.event-end types
         * (the 4th param inverts the match) */
        obj.eventPump.addHook ([{type: "poll", set: /^(server|dcc-chat)$/},
                               {type: "event-end"}], event_tracer,
                               "event-tracer", true /* negate */,
                               false /* disable */);
    obj.linkRE = /((\w+):\/\/[^<>\[\]()\'\"\s]+|www(\.[^.<>\[\]()\'\"\s]+){2,})/;

    obj.munger = new CMunger();
    obj.munger.enabled = true;
    obj.munger.addRule ("mailto", /((mailto:)?[^<>\[\]()\'\"\s]+@[^.<>\[\]()\'\"\s]+\.[^<>\[\]()\'\"\s]+)/i,
                        insertMailToLink);
    obj.munger.addRule ("link", obj.linkRE, insertLink);
    obj.munger.addRule ("channel-link", /[@+]?(#[^<>\[\]()\'\"\s]+[^:,.<>\[\]()\'\"\s])/i,
                        insertChannelLink);
    obj.munger.addRule ("face",
         /((^|\s)[\<\>]?[\;\=\:]\~?[\-\^\v]?[\)\|\(pP\<\>oO0\[\]\/\\](\s|$))/,
         insertSmiley);
    obj.munger.addRule ("ear", /(?:\s|^)(\(\*)(?:\s|$)/, insertEar, false);
    obj.munger.addRule ("rheet", /(?:\s|^)(rhee+t\!*)(?:\s|$)/i, insertRheet);
    obj.munger.addRule ("bold", /(?:\s|^)(\*[^*,.()]*\*)(?:[\s.,]|$)/, 
                        "chatzilla-bold");
    obj.munger.addRule ("italic", /(?:\s|^)(\/[^\/,.()]*\/)(?:[\s.,]|$)/,
                        "chatzilla-italic");
    /* allow () chars inside |code()| blocks */
    obj.munger.addRule ("teletype", /(?:\s|^)(\|[^|,.]*\|)(?:[\s.,]|$)/,
                        "chatzilla-teletype");
    obj.munger.addRule ("underline", /(?:\s|^)(\_[^_,.()]*\_)(?:[\s.,]|$)/,
                        "chatzilla-underline");
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
    
    if (matchText.indexOf ("://") == -1)
        href = "http://" + matchText;
    else
        href = matchText;
    
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
    if (!eventData.network || 
        matchText.search
            (/^#(include|error|define|if|ifdef|else|elsif|endif)$/i) != -1)

    {
        containerTag.appendChild (document.createTextNode(matchText));
        return;
    }
    
    var anchor = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:a");
    anchor.setAttribute ("href", "irc://" + eventData.network.name + "/" +
                         matchText);
    anchor.setAttribute ("class", "chatzilla-link");
    //anchor.setAttribute ("target", "_content");
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

function insertHyphenatedWord (longWord, containerTag)
{
    var wordParts = splitLongWord (longWord, client.MAX_WORD_DISPLAY);
    for (var i = 0; i < wordParts.length; ++i)
    {
        containerTag.appendChild (document.createTextNode (wordParts[i]));
        if (i != wordParts.length)
        {
            var img = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                                "html:img");
            img.setAttribute ("style", "border: none; width: 0px; height: 0px;");
            containerTag.appendChild (img);
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
        return true;

    return false;    
}

function fillInHTMLTooltip(tipElement)
{
    const XULNS =
        "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    const XLinkNS = "http://www.w3.org/1999/xlink";
    const Node = { ELEMENT_NODE : 1 }; // XXX Components.interfaces.Node;
    
    var retVal = false;
    var tipNode = document.getElementById("HTML_TOOLTIP_tooltipBox");
    try {
        while (tipNode.hasChildNodes())
            tipNode.removeChild(tipNode.firstChild);
        var titleText = "";
        var XLinkTitleText = "";
        while (!titleText && !XLinkTitleText && tipElement) {
            if (tipElement.nodeType == Node.ELEMENT_NODE) {
                titleText = tipElement.getAttribute("title");
                XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
            }
            tipElement = tipElement.parentNode;
        }
        var texts = [titleText, XLinkTitleText];
        for (var i = 0; i < texts.length; ++i) {
            var t = texts[i];
            if (t.search(/\S/) >= 0) {
                var tipLineElem =
                    tipNode.ownerDocument.createElementNS(XULNS, "text");
                tipLineElem.setAttribute("value", t);
                tipNode.appendChild(tipLineElem);
                retVal = true;
            }
        }
    } catch (e) {
    }
    return retVal;
}

/* timer-based mainloop */
function mainStep()
{
    if (!frames[0].initialized)
    {  /* voodoo required for skin switching.  When the user changes a skin,
        * the output document is reloaded. this document *cannot* tell us
        * it has been reloaded, because the type="content" attribute used
        * on the iframe (to allow selection) also keeps the iframe from getting
        * to the chrome above it.  Instead, we poll the document looking for
        * the "initialized" variable. If it's not there, we reset the current
        * object, and set initialized in the document. */
        setClientOutput(frames[0].document);        
        if (client.output)
        {
            var o = client.currentObject;
            client.currentObject = null;
            setCurrentObject (o);
            frames[0].initialized = true;
        }
    }

    client.eventPump.stepEvents();
    setTimeout ("mainStep()", client.STEP_TIMEOUT);
    
}

function getMsg (msgName)
{
    var restCount = arguments.length - 1;

    if (!client.bundle)
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
    rv.parent = obj.parent;
    
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
            if (rv.network.isConnected())
                rv.server = rv.network.primServ;
            break;

        case "IRCClient":
            if (obj.lastNetwork)
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
    client.output = frames[0].document.getElementById("output");
    //createHighlightMenu();
}

function createHighlightMenu()
{
    function processStyleRules(rules)
    {
        for (var i = 0; i < rules.length; ++i)
        {
            var rule = rules.item(i);
            if (rule.selectorText)
            {
                ary = rule.selectorText.
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
            else if (rule.styleSheet)
            {
                processStyleRules(rule.styleSheet.rules);
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
    
    //processStyleRules(frames[0].document.styleSheets[0].cssRules);
}

function setupMungerMenu(munger)
{
    var menu = document.getElementById("menu-munger");
    for (var entry in munger.entries)
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
    var rest = ary[2];

    /* split <host> into server (or network) / port */
    ary = host.match (/^([^\s\:]+)?(\:\d+)?$/);
    if (!ary)
    {
        dd ("parseIRCURL: host/port split failed");
        return null;
    }
    
    if (ary[2])
    {
        if (!ary[1])
        {
            dd ("parseIRCURL: port with no host");
            return null;
        }
        specifiedHost = rv.host = ary[1].toLowerCase();
        rv.isserver = true;
        rv.port = parseInt(ary[2].substr(1));
    }
    else if (ary[1])
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
        
        rv.target = (ary[1]) ? 
            unescape(ary[1]).toLowerCase().replace("\n", "\\n"): "";
        var params = (ary[2]) ? ary[2].toLowerCase() : "";
        var query = (ary[3]) ? ary[3] : "";

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
                    if (ary[i])
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
        window.alert (getMsg("gotoIRCURLMsg",url));
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
            if (!net.pendingURLs)
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
            client.onInputAttach ({inputData: url.host + " " + pass});
            if (!net.pendingURLs)
                net.pendingURLs = new Array();
            net.pendingURLs.push (url);            
            return;
        }
    }
    
    /* already connected, do whatever comes next in the url */
    //dd ("gotoIRCURL: connected, time to finish parsing ``" + url + "''");
    if (url.target)
    {
        var key = "";
        if (url.needkey)
            key = window.prompt (getMsg("gotoIRCURLMsg3",url.spec));

        if (url.isnick)
        {
                /* eek, do nick stuff here */
        }
        else
        {
            var ev = {inputData: url.target + " " + key,
                      network: net, server: net.primServ}
            client.onInputJoin (ev);
            if (url.msg)
            {
                var msg;
                if (url.msg.indexOf("\01ACTION") == 0)
                {
                    msg = filterOutput (url.msg, "ACTION", "ME!");
                    ev.channel.display (msg, "ACTION", "ME!",
                                        client.currentObject);
                }
                else
                {
                    msg = filterOutput (url.msg, "PRIVMSG", "ME!");
                    ev.channel.display (msg, "PRIVMSG", "ME!",
                                        client.currentObject);
                }
                ev.channel.say (msg);
                setCurrentObject (ev.channel);
            }
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

function updateNetwork(obj)
{
    var o = new Object();
    getObjectDetails (client.currentObject, o);

    if (obj && obj != o.network)
        return;
    
    var net = o.network ? o.network.name : "(none)";
    var serv = "(none)", nick = "(unknown)", lag = "(unknown)",
        ping = "(never)";
    if (o.server)
    {
        serv  = o.server.connection.host;
        if (o.server.me)
            nick = o.server.me.properNick;
        lag = (o.server.lag != -1) ? o.server.lag : "(unknown)";
        
        if (o.server.lastPing)
        {
            var mins = String(o.server.lastPing.getMinutes());
            if (mins.length == 1)
                mins = "0" + mins;
            ping = o.server.lastPing.getHours() + ":" + mins;
        }
        else
            ping = "(never)";
    }

    client.statusBar["net-name"].setAttribute("value", net);
    client.statusBar["server-name"].setAttribute("value", serv);
    client.statusBar["server-nick"].setAttribute("value", nick);
    client.statusBar["server-lag"].setAttribute("value", lag);
    client.statusBar["last-ping"].setAttribute("value", ping);

}

function updateChannel (obj)
{
    if (obj && obj != client.currentObject)
        return;
    
    var o = new Object();
    getObjectDetails (client.currentObject, o);

    var chan = "(none)", l = "(none)", k = "(none)", mode = "(none)",
        users = 0, topicBy = "(nobody)", topic = "(unknown)";

    if (o.channel)
    {
        chan = o.channel.name;
        l = (o.channel.mode.limit != -1) ? o.channel.mode.limit : "(none)";
        k = o.channel.mode.key ? o.channel.mode.key : "(none)";
        mode = o.channel.mode.getModeStr();
        if (!mode)
            mode = "(none)";
        users = o.channel.getUsersLength();
        topic = o.channel.topic ? o.channel.topic : "(none)";
        topicBy = o.channel.topicBy ? o.channel.topicBy : "(nobody)";
    }
    
    client.statusBar["channel-name"].setAttribute("value", chan);
    client.statusBar["channel-limit"].setAttribute("value", l);
    client.statusBar["channel-key"].setAttribute("value", k);
    client.statusBar["channel-mode"].setAttribute("value", mode);
    client.statusBar["channel-users"].setAttribute("value", users);
    client.statusBar["channel-topic"].setAttribute("value", topic);
    client.statusBar["channel-topicby"].setAttribute("value", topicBy);

}

function updateTitle (obj)
{
    if ((obj && obj != client.currentObject) || !client.currentObject)
        return;

    var tstring;
    var o = getObjectDetails (client.currentObject);
    var net = o.network ? o.network.name : "";

    switch (client.currentObject.TYPE)
    {
        case "IRCNetwork":
            var serv = "", port = "", nick = "";
            if (o.server)
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
            var nick = o.parent.me ? o.parent.me.properNick : 
                                     getMsg ("updateTitleNoNick");
            chan = o.channel.name;
            mode = o.channel.mode.getModeStr();
            if (!mode)
                mode = getMsg("updateTitleNoMode");
            topic = o.channel.topic ? o.channel.topic : 
                                      getMsg("updateTitleNoTopic");

            tstring = getMsg("updateTitleChannel", [nick, chan, mode, topic]);
            break;

        case "IRCUser":
            tstring = getMsg("updateTitleUser", client.currentObject.properNick);
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
                              [tstring, actl.join (", ")]);
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

function setCurrentObject (obj)
{
    if (!obj.messages)
    {
        dd ("** INVALID OBJECT passed to setCurrentObject **");
        return;
    }

    if (client.currentObject == obj)
        return;
        
    var tb, userList;

    if (client.currentObject)
    {
        tb = getTabForObject(client.currentObject);
    }
    if (tb)
    {
        tb.setAttribute ("selected", "false");
        tb.setAttribute ("state", "normal");
    }
    
    if (client.output.firstChild)
        client.output.removeChild (client.output.firstChild);
    client.output.appendChild (obj.messages);

    /* Unselect currently selected users. */
    userList = document.getElementById("user-list");
    if (userList) 
        /* Remove curently selection items before this tree gets rerooted,
         * because it seems to remember the selections for eternity if not. */
        userList.clearItemSelection ();    
    else
        dd ("setCurrentObject: could not find element with ID='user-list'");

    if (obj.TYPE == "IRCChannel")
        client.rdf.setTreeRoot ("user-list", obj.getGraphResource());
    else
        client.rdf.setTreeRoot ("user-list", client.rdf.resNullChan);

    client.currentObject = obj;
    tb = getTabForObject(obj);
    if (tb)
    {
        tb.setAttribute ("selected", "true");
        tb.setAttribute ("state", "current");
    }
    
    var vk = Number(tb.getAttribute("viewKey"));
    delete client.activityList[vk];

    updateNetwork();
    updateChannel();
    updateTitle ();

    if (client.PRINT_DIRECTION == 1)
        scrollDown();

    focusInput();
}

function scrollDown ()
{
    window.frames[0].scrollTo(0, window.frames[0].document.height);
}

function addHistory (source, obj)
{
    var tbody;
    
    if (!source.messages)
    {
        source.messages =
            document.createElementNS ("http://www.w3.org/1999/xhtml",
                                      "html:table");

        source.messages.setAttribute ("class", "msg-table");
        source.messages.setAttribute ("view-type", source.TYPE);
        if (0) /*XXX*/ {
            tbody = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                              "html:tbody");
            source.messages.appendChild (tbody);
        } else {
            tbody = source.messages;
        }
    }
    else
        if (0) /*XXX*/ {        
            tbody = source.messages.firstChild;
        } else {
            tbody = source.messages;
        }

    var needScroll = false;
    var w = window.frames[0];
    
    if (client.PRINT_DIRECTION == 1)
    {
        if ((w.document.height - (w.innerHeight + w.pageYOffset)) <
            (w.innerHeight / 3))
            needScroll = true;
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
                tbody.removeChild (tbody.firstChild);
            else
                tbody.removeChild (tbody.lastChild);
    }

    if (needScroll && client.currentObject == source)
        w.scrollTo (w.pageXOffset, w.document.height);
    
}

function notifyActivity (source)
{
    if (typeof source != "object")
        source = client.viewsArray[source].source;
    
    var tb = getTabForObject (source, true);
    var vk = Number(tb.getAttribute("viewKey"));
    
    if (client.currentObject != source)
    {
        if (tb.getAttribute ("state") == "normal")
        {       
            tb.setAttribute ("state", "activity");
            if (!client.activityList[vk])
            {
                client.activityList[vk] = "+";
                updateTitle();
            }
        }
        else if (tb.getAttribute("state") == "activity")
            /* if act light is already lit, blink it real quick */
        {
            tb.setAttribute ("state", "normal");
            setTimeout ("notifyActivity(" + vk + ");", 200);
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
        window.GetAttention();
    
}

/* gets the toolbutton associated with an object
 * if |create| is present, and true, create if not found */
function getTabForObject (source, create)
{
    var name;

    if (!source)
    {
        dd ("** UNDEFINED  passed to getTabForObject **");
        dd (getStackTrace());
        return null;
    }
    
    create = (typeof create != "undefined") ? Boolean(create) : false;

    switch (source.TYPE)
    {
        case "IRCChanUser":
        case "IRCUser":
            name = source.nick;
            break;
            
        case "IRCNetwork":
        case "IRCChannel":
        case "IRCClient":
            name = source.name;
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
        var views = document.getElementById ("views-tbar-inner");
        tb = document.createElement ("tab");
        tb.setAttribute ("ondraggesture",
                         "nsDragAndDrop.startDrag(event, tabDNDObserver);");
        tb.setAttribute ("href", source.getURL());
        tb.setAttribute ("name", source.name);
        tb.setAttribute ("onclick", "onTabClick('" + id + "');");
        tb.setAttribute ("crop", "right");
        
        tb.setAttribute ("class", "tab-bottom view-button");
        tb.setAttribute ("id", id);
        tb.setAttribute ("state", "normal");

        var spacer = document.createElement ("box");
        spacer.setAttribute ("id", id + "-spacer");
        spacer.setAttribute ("class", "tabs-bottom view-button-spacer");
        spacer.setAttribute ("index", client.viewsArray.length);
        views.appendChild (spacer);

        client.viewsArray.push ({source: source, tb: tb, spacer: spacer});
        tb.setAttribute ("viewKey", client.viewsArray.length - 1);
        if (matches > 1)
            tb.setAttribute("label", name + "<" + matches + ">");
        else
            tb.setAttribute("label", name);

        views.appendChild (tb);        
    }

    return tb;
    
}

function retrieveURLFromData (aData, flavour)
{
    switch (flavour) 
    {
        case "text/unicode":
            if (aData.search(client.linkRE) != -1)
                return aData;
            else
                return null;

        case "text/x-moz-url":
            var data = aData.toString();
            var separator = data.indexOf("\n");
            if (separator != -1)
                data = data.substr(0, separator);
            return data;

        case "application/x-moz-file":
            return aData.URL;
    }

    return null;                                                   
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
    var url = retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);
    if (!url)
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
        if (!client.viewsArray[key].source.isPermanent)
        {
            /* re-index higher toolbuttons */
            for (i = key + 1; i < client.viewsArray.length; i++)
            {
                client.viewsArray[i].tb.setAttribute ("viewKey", i - 1);
                client.viewsArray[i].spacer.setAttribute ("index", i - 1);
            }
            spacer = client.viewsArray[key].spacer;
            arrayRemoveAt(client.viewsArray, key);
            var tbinner = document.getElementById("views-tbar-inner");
            tbinner.removeChild(tb);
            tbinner.removeChild(spacer);
        }
        else
        {
            window.alert (getMsg("deleteTabMsg"));
            return -1;
        }
            
    }
    else
        dd  ("*** INVALID OBJECT passed to deleteTab (" + tb + ") " +
             "no viewKey attribute. (" + key + ")");

    return key;

}

function filterOutput (msg, msgtype)
{

    for (var f in client.outputFilters)
        if (client.outputFilters[f].enabled)
            msg = client.outputFilters[f].func(msg, msgtype);

    return msg;
    
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
            client.currentObject.say (msg);
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
    if (this.messages)
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
CIRCNetwork.prototype.displayHere =
CIRCChannel.prototype.display =
CIRCUser.prototype.displayHere =
function __display(message, msgtype, sourceObj, destObj)
{            
    var d = new Date();
    var mins = d.getMinutes();
    if (mins < 10)
        mins = "0" + mins;
    var dateString = getMsg("cli_dateString", [d.getMonth() + 1, d.getDate(),
                                               d.getHours(), mins]);
 
    function setAttribs (obj, c, attrs)
    {
        for (var a in attrs)
            obj.setAttribute (a, attrs[a]);
        obj.setAttribute ("class", c);
        obj.setAttribute ("msg-type", msgtype);
        obj.setAttribute ("msg-user", fromAttr);
        obj.setAttribute ("msg-dest", toAttr);
        obj.setAttribute ("dest-type", toType);
        obj.setAttribute ("view-type", viewType);
    }

    var blockLevel = false; /* true if this row should be rendered at block
                             * level, (like, if it has a really long nickname
                             * that might disturb the rest of the layout)     */
    var o = getObjectDetails (this);          /* get the skinny on |this|     */
    var me = (o.server) ? o.server.me : null; /* get the object representing
                                               * the user
                                               */
    if (sourceObj == "ME!") sourceObj = me;   /* if the caller to passes "ME!"*/
    if (destObj == "ME!") destObj = me;       /* substitute the actual object */

    var fromType = (sourceObj && sourceObj.TYPE) ? sourceObj.TYPE : "unk";
    var fromAttr;
    
    if      (sourceObj && sourceObj == me)       fromAttr = "ME!";
    else if (fromType.search(/IRC.*User/) != -1) fromAttr = sourceObj.nick;
    else if (typeof sourceObj == "object")       fromAttr = sourceObj.name;
        
    var toType = (destObj) ? destObj.TYPE : "unk";
    var toAttr;
    
    if (destObj == me)
        toAttr = "ME!";
    else if (toType == "IRCUser")
        toAttr = destObj.nick;
    else if (typeof destObj == "object")
        toAttr = destObj.name;

    /* isImportant means to style the messages as important, and flash the
     * window, getAttention means just flash the window. */
    var isImportant = false, getAttention = false;
    var viewType = this.TYPE;
    var code;
    var msgRow = document.createElementNS("http://www.w3.org/1999/xhtml",
                                          "html:tr");
    setAttribs(msgRow, "msg");

    //dd ("fromType is " + fromType + ", fromAttr is " + fromAttr);
    
    if (fromType.search(/IRC.*User/) != -1 &&
        msgtype.search(/PRIVMSG|ACTION|NOTICE/) != -1)
    {
        /* do nick things here */
        var nick;
        
        if (sourceObj != me)
        {
            if (toType == "IRCUser") /* msg from user to me */
            {
                getAttention = true;
                nick = sourceObj.properNick;
            }
            else /* msg from user to channel */
            {
                if (typeof (message == "string") && me)
                    isImportant = msgIsImportant (message, nick, me.nick);
                nick = sourceObj.properNick;
            }
        }
        else if (toType == "IRCUser") /* msg from me to user */
        {
            nick = (this.TYPE == "IRCUser") ? sourceObj.properNick :
                destObj.properNick;
        }
        else /* msg from me to channel */
        {
            nick = sourceObj.properNick;
        }
        
        if (typeof this.mark == "undefined") 
            this.mark = "odd";
        
        if (this.lastNickDisplayed != nick)
        {
            this.lastNickDisplayed = nick;
            this.mark = (this.mark == "even") ? "odd" : "even";
        }

        var msgSource = document.createElementNS("http://www.w3.org/1999/xhtml",
                                                 "html:td");
        setAttribs (msgSource, "msg-user", {title: dateString});
        if (isImportant)
            msgSource.setAttribute ("important", "true");
        if (nick.length > client.MAX_NICK_DISPLAY)
            blockLevel = true;
        msgSource.appendChild (newInlineText (nick));
        msgRow.appendChild (msgSource);

    }
    else if ((client.debugMode && (code = "[" + msgtype + "]")) ||
             (code = client.responseCodeMap[msgtype]) != "none")
    {
        /* Display the message code */
        var msgType = document.createElementNS("http://www.w3.org/1999/xhtml",
                                               "html:td");
        setAttribs (msgType, "msg-type", {title: dateString});

        if (!code)
            if (client.HIDE_CODES)
                code = client.DEFAULT_RESPONSE_CODE;
            else
                code = "[" + msgtype + "]";
        msgType.appendChild (newInlineText (code));
        msgRow.appendChild (msgType);
    }
             
    if (message)
    {
        var msgData = document.createElementNS("http://www.w3.org/1999/xhtml",
                                               "html:td");
        setAttribs (msgData, "msg-data");
        if (isImportant)
            msgData.setAttribute ("important", "true");

        if (this.mark)
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
        if (0) /*XXX*/ {
            var tbody =  document.createElementNS ("http://www.w3.org/1999/xhtml",
                                                   "html:tbody");
        } else {
            tbody = table;
        }
        
        tbody.appendChild (msgRow);
        //XXXtable.appendChild (tbody);
        msgRow = tr;
    }

    addHistory (this, msgRow);
    if (isImportant || getAttention)
        notifyAttention(this);
    else
        notifyActivity (this);
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
        if (client.networks[n].primServ)
            client.networks[n].quit (reason);
    
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

    /* Grab a reference to the tree element with ID = user-list . See chatzilla.xul */
    var tree = document.getElementById("user-list");
    var cell; /* reference to each selected cell of the tree object */
    var rv_ary = new Array; /* return value arrray for CIRCChanUser objects */

    for (var i = 0; i < tree.selectedItems.length; i++)
    {
        /* First, set the reference to the XUL element. */
        cell = tree.selectedItems[i].firstChild.childNodes[2].firstChild;

        /* Now, create an instance of CIRCChaneUser by passing the text
       *  of the cell to the getUser function of this CIRCChannel instance.
       */
        rv_ary[i] = this.getUser( cell.getAttribute("value") );
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
    if (!this.rdfRes)
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
    
    if (!this.rdfRes)
    {
        if (!CIRCUser.nextResID)
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

    if (!this.rdfRes)
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
