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
 */

if (DEBUG)
    dd = function (m) { dump ("-*- chatzilla: " + m + "\n"); }
else
    dd = function (){};

var client = new Object();

client.defaultNick = "IRCMonkey";

client.IMAGEDIR = "chrome://chatzilla/skin/images/";
client.CSSDIR = "chrome://chatzilla/skin/";

client.COMMAND_CHAR = "/";
client.STEP_TIMEOUT = 500;
//client.UPDATE_DELAY = 500;
client.EXPAND_HEIGHT = "200px";
client.COLLAPSE_HEIGHT = "25px";
client.MAX_MESSAGES = 200;
client.MAX_HISTORY = 50;
/* longest nick to show in display before abbreviating */
client.MAX_NICK_DISPLAY = 14;
/* longest word to show in display before abbreviating, currently
 * only for urls and hostnames onJoin */
client.MAX_WORD_DISPLAY = 35;
client.TYPE = "IRCClient";
client.PRINT_DIRECTION = 1; /*1 => new messages at bottom, -1 => at top */
client.ADDRESSED_NICK_SEP = ", ";

client.name = "*client*";
client.viewsArray = new Array();
client.activityList = new Object();
client.uiState = new Object(); /* state of ui elements (visible/collapsed) */
client.inputHistory = new Array();
client.lastHistoryReferenced = -1;
client.incompleteLine = "";
client.isPermanent = true;

client.lastTabUp = new Date();
client.DOUBLETAB_TIME = 500;

client.stalkingVictims = new Array();

CIRCNetwork.prototype.INITIAL_NICK = client.defaultNick;
CIRCNetwork.prototype.INITIAL_NAME = "chatzilla";
CIRCNetwork.prototype.INITIAL_DESC = "New Now Know How";
CIRCNetwork.prototype.INITIAL_CHANNEL = "";
CIRCNetwork.prototype.MAX_MESSAGES = 100;
CIRCNetwork.prototype.IGNORE_MOTD = true;

CIRCServer.prototype.READ_TIMEOUT = 0;
CIRCServer.prototype.VERSION_RPLY = "ChatZilla, running under " + 
    navigator.userAgent;

CIRCUser.prototype.MAX_MESSAGES = 100;

CIRCChannel.prototype.MAX_MESSAGES = 200;

CIRCChanUser.prototype.MAX_MESSAGES = 100;


function initStatic()
{
    var obj;
    
    obj = document.getElementById("input");
    obj.addEventListener("keyup", onInputKeyUp, false);

        //obj = document.getElementById("tb[*client*]");
    /*
    client.quickList = new CListBox(document.getElementById("quickList"));
    client.quickList.selectedItemCallback = quicklistCallback;
    */

    var saveDir = client.PRINT_DIRECTION;
    client.PRINT_DIRECTION = 1;
    client.display ("Welcome to ChatZilla...\n" +
                    "Use /attach <network-name> connect to a network.\n" +
                    "Where <network-name> is one of [" +
                    keys (client.networks) + "]\n" +
                    "More help is available with /help [<command-name>]",
                    "HELLO");

    setCurrentObject (client);

    client.onInputCommands();
    
    client.PRINT_DIRECTION = saveDir;

    if (!jsenv.HAS_XPCOM)
        client.display ("ChatZilla was not given access to the XPConnect " +
                        "service.  You will not be able to connect to an " +
                        "irc server.  A workaround may be to set the " +
                        "'security.checkxpconnect' pref to false in " +
                        "all.js, or your user prefs.",
                        "ERROR");
    window.onkeypress = onWindowKeyPress;

    setMenuCheck ("menu-dmessages", 
                  client.eventPump.getHook ("event-tracer").enabled);
    setMenuCheck ("menu-munger", client.munger.enabled);
    setMenuCheck ("menu-viewicons", client.ICONS_IN_TOOLBAR);
    client.uiState["toolbar"] =
        setMenuCheck ("menu-view-toolbar", isVisible("views-tbox"));
    client.uiState["info"] =
        setMenuCheck ("menu-view-info", isVisible("user-list"));
    client.uiState["status"] =
        setMenuCheck ("menu-view-status", isVisible("status-bar-tbox"));

    onSortCol ("usercol-nick");

    if (document.location.search)
        gotoIRCURL (document.location.search.substr(1));
    
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
        dd ("** Bogus id '" + id + "' passed to isVisible() **");
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
        new CIRCNetwork ("efnet", [{name: "irc.mcs.net", port: 6667},
                                   {name: "irc.magic.ca", port: 6667},
                                   {name: "irc.freei.net", port: 6667},
                                   {name: "irc.cs.cmu.edu",   port: 6667}],
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
    
    obj.primNet = obj.networks["efnet"];

    if (DEBUG)
        /* hook all events EXCEPT server.poll and *.event-end types
         * (the 4th param inverts the match) */
        obj.eventPump.addHook ([{type: "poll", set: /^(server|dcc-chat)$/},
                               {type: "event-end"}], event_tracer,
                               "event-tracer", true /* negate */,
                               false /* disable */);

    obj.munger = new CMunger();
    obj.munger.enabled = true;
    obj.munger.addRule ("you-talking-to-me?", matchMyNick, "");
    //    obj.munger.addRule ("im-stalking-you", matchMyNick, "" );
    obj.munger.addRule
        ("link", /((\w+)\:\/\/[^\<\>\(\)\'\"\s]*|www\.[^\<\>\(\)\'\"\s]+\.[^\<\>\(\)\'\"\s]*)/,
         insertLink);
    obj.munger.addRule
        ("face",
         /((^|\s)[\<\>]?[\;\=\:\8]\~?[\-\^\v]?[\)\|\(pP\<\>oO0\[\]\/\\](\s|$))/,
         insertSmiley);
    obj.munger.addRule ("rheet", /(rhee+t\!*)/i, "rheet");
    obj.munger.addRule ("bold", /(\*[^\*]*\*)/, "bold");
    obj.munger.addRule ("italic", /[^sS](\/[^\/]*\/)/, "italic");
    obj.munger.addRule ("teletype", /(\|[^|]*\|)/, "teletype");
    obj.munger.addRule ("underline1", /^(\_[^\_]*\_)/, "underline");
    obj.munger.addRule ("underline2", /\W(\_[^\_]*\_)/, "underline");
         //obj.munger.addRule ("strikethrough", /(\-.*\-)/, "strikethrough");
    obj.munger.addRule ("smallcap", /(\#[^\#]*\#)/, "smallcap");
    obj.munger.addRule ("word-hyphenator",
                        new RegExp ("(\\S{" + client.MAX_WORD_DISPLAY + ",})"),
                        insertHyphenatedWord);

    obj.rdf = new RDFHelper();
    
    obj.rdf.initTree("user-list");
    obj.rdf.setTreeRoot("user-list", obj.rdf.resNullChan);
    
}

function matchMyNick (text, containerTag, eventDetails)
{
    if (eventDetails && eventDetails.server)
    {        
        var sv = "(" + eventDetails.server.me.nick + ")";
        if ( client.stalkingVictims.length > 0 ) {
            sv += "|(" + client.stalkingVictims.join( ")|(" ) + ")";
        }

        var re = new RegExp("(^|[\\W\\s])" + sv + 
                            "([\\W\\s]|$)", "i");
        if (text.search(re) != -1 || 
            eventDetails.orig.lastNickDisplayed.search(re) != -1)
        {
            containerTag.setAttribute ("directedToMe", "true");
            notifyAttention(eventDetails.orig);
        }
    }

    return false;
    
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
    anchor.setAttribute ("target", "_content");
    if (matchText.length >= client.MAX_WORD_DISPLAY)
        matchText = hyphenateWord (matchText, client.MAX_WORD_DISPLAY);
    anchor.appendChild (document.createTextNode (matchText));
    containerTag.appendChild (anchor);
    
}

function insertSmiley (emoticon, containerTag)
{
    var src = "";

    if (emoticon.search (/\;[\-\^\v]?[\)\>\]]/) != -1)
        src = "face-wink.gif";
    else if (emoticon.search (/[\=\:\8][\-\^\v]?[\)\>\]]/) != -1)
        src = "face-smile.gif";
    else if (emoticon.search (/[\=\:\8][\-\^\v]?[\/\\]/) != -1)
        src = "face-screw.gif";
    else if (emoticon.search (/[\=\:\8]\~[\-\^\v]?\(/) != -1)
        src = "face-cry.gif";
    else if (emoticon.search (/[\=\:\8][\-\^\v]?[\(\<\[]/) != -1)
        src = "face-frown.gif";
    else if (emoticon.search (/\<?[\=\:\8][\-\^\v]?[0oO]/) != -1)
        src = "face-surprise.gif";
    else if (emoticon.search (/[\=\:\8][\-\^\v]?[pP]/) != -1)
        src = "face-tongue.gif";
    else if (emoticon.search (/\>?[\=\:\8][\-\^\v]?[\(\|]/) != -1)
        src = "face-angry.gif";

    containerTag.appendChild (document.createTextNode (emoticon));

    if (src)
    {
        var img = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                            "html:img");
        img.setAttribute ("src", client.IMAGEDIR + src);
        containerTag.appendChild (img);
    }
    
}

function insertHyphenatedWord (longWord, containerTag)
{
    containerTag.appendChild
        (document.createTextNode (hyphenateWord(longWord,
                                                client.MAX_WORD_DISPLAY)));
}

/* timer-based mainloop */
function mainStep()
{

    if (!client.output)
        setClientOutput(frames[0].document);
    else
        client.eventPump.stepEvents();
    setTimeout ("mainStep()", client.STEP_TIMEOUT);
    
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

function setOutputStyle (styleSheet)
{
    var oc = top.frames[0].document;

    oc.close();
    oc.open();
    oc.write ("<html><head>");
    
    if (client.USER_CSS_PRE)
        oc.write("<LINK REL=StyleSheet " +
                 "HREF='" + client.USER_CSS_PRE + "' " +
                 "TYPE='text/css' MEDIA='screen'>");

    oc.write("<LINK REL=StyleSheet " +
             "HREF='" + client.CSSDIR + styleSheet + "' " +
             "TYPE='text/css' MEDIA='screen'>");

    if (client.USER_CSS_POST)
        oc.write("<LINK REL=StyleSheet " +
                 "HREF='" + client.USER_CSS_POST + "' " +
                 "TYPE='text/css' MEDIA='screen'>");

    oc.write ("</head>" +
              "<body><div id='output' class='output-window'></div></body>" +
              "</html>");

    oc.close();

    client.output = oc.getElementById ("output");
    
}

function setClientOutput(doc) 
{
    client.output = doc.getElementById("output");
    if (client.output)
    {
        dd ("Got output element.");
        /* continue processing now: */
        initStatic();
        if (client.STARTUP_NETWORK)
            client.onInputAttach ({inputData: client.STARTUP_NETWORK});
    }
    else
        dd ("ARG!  Couldn't get output element, try again later.");
    
}

testURLs =
    ["irc:", "irc://", "irc:///", "irc:///help", "irc:///help,needkey",
    "irc://irc.foo.org", "irc://foo:6666",
    "irc://foo", "irc://irc.foo.org/", "irc://foo:6666/", "irc://foo/",
    "irc://irc.foo.org/,needpass", "irc://foo/,isserver",
    "irc://moznet/,isserver", "irc://moznet/",
    "irc://foo/chatzilla", "irc://irc.foo.org/?msg=hello%20there",
    "irc://irc.foo.org/?msg=hello%20there&ignorethis",
    "irc://irc.foo.org/%23mozilla,needkey?msg=hello%20there&ignorethis",
    "invalids",
    "irc://irc.foo.org/,isnick"];

function doURLTest()
{
    for (var u in testURLs)
    {
        dd ("-*- testing url \"" + testURLs[u] + "\"");
        var o = parseIRCURL(testURLs[u]);
        if (!o)
            dd ("-!- PARSE FAILED!");
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
    rv.host = client.STARTUP_NETWORK;
    rv.target = "";
    rv.port = 6667;
    rv.msg = "";
    rv.needpass = false;
    rv.needkey = false;
    rv.isnick = false;
    rv.isserver = false;
    
    if (url.search(/^(irc:|irc:\/\/)$/i) != -1)
        return rv;

    /* split url into <host>/<everything-else> pieces */
    var ary = url.match (/^irc:\/\/([^\/\s]+)?(\/.*)?\s*$/i);
    if (!ary)
    {
        dd ("chatzilla: parseIRCURL: initial split failed");
        return null;
    }
    var host = ary[1];
    var rest = ary[2];

    /* split <host> into server (or network) / port */
    ary = host.match (/^([^\s\:]+)?(\:\d+)?$/);
    if (!ary)
    {
        dd ("chatzilla: parseIRCURL: host/port split failed");
        return null;
    }
    
    if (ary[2])
    {
        if (!ary[1])
        {
            dd ("chatzilla: parseIRCURL: port with no host");
            return null;
        }
        specifiedHost = rv.host = ary[1].toLowerCase();
        rv.isserver = true;
        rv.port = parseInt(ary[2].substr(1));
    }
    else if (ary[1])
    {
        if (client.networks[ary[1]])
            specifiedHost = rv.host = ary[1].toLowerCase();
        else
        {
            specifiedHost = rv.host = ary[1].toLowerCase();
            rv.isserver = true;
        }
    }

    if (rest)
    {
        ary = rest.match (/^\/([^\,\?\s]*)?(,[^\?]*)?(\?.*)?$/);
        if (!ary)
        {
            dd ("chatzilla: parseIRCURL: rest split failed '" + rest + "'");
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
                dd ("chatzilla: parseIRCURL: isnick w/o target");
                    /* isnick w/o a target is bogus */
                return null;
            }
        
            rv.isserver =
                (params.search (/,\s*isserver\s*,|,\s*isserver\s*$/) != -1);
            if (rv.isserver && !specifiedHost)
            {
                dd ("chatzilla: parseIRCURL: isserver w/o host");
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
        window.alert ("Invalid IRC URL '" + url + "'");
        return;
    }

    var net;
    var pass = "";
    
    if (url.needpass)
        pass = window.prompt ("Enter a password for the url " + url.spec);
    
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
            dd ("-*- chatzilla: gotoIRCURL: not already connected to " +
                "server " + url.host + " trying to connect...");
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
            dd ("-*- chatzilla: gotoIRCURL: not already connected to " +
                "network " + url.host + " trying to connect...");
            client.onInputAttach ({inputData: url.host + " " + pass});
            if (!net.pendingURLs)
                net.pendingURLs = new Array();
            net.pendingURLs.push (url);            
            return;
        }
    }
    
    /* already connected, do whatever comes next in the url */
    dd ("-*- chatzilla: gotoIRCURL: connected, time to finish parsing '" +
        url + "'");
    if (url.target)
    {
        var key = "";
        if (url.needkey)
            key = window.prompt ("Enter key for url " + url.spec);
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
                    msg = filterOutput (url.msg, "ACTION", "!ME");
                else
                    msg = filterOutput (url.msg, "PRIVMSG", "!ME");
                ev.channel.say (msg);
            }
        }
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

    document.getElementById ("net-name").firstChild.data = net;
    document.getElementById ("server-name").firstChild.data = serv;
    document.getElementById ("server-nick").firstChild.data = nick;
    document.getElementById ("server-lag").firstChild.data = lag;
    document.getElementById ("last-ping").firstChild.data = ping;

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
    
    document.getElementById ("channel-name").firstChild.data = chan;
    document.getElementById ("channel-limit").firstChild.data = l;
    document.getElementById ("channel-key").firstChild.data = k;
    document.getElementById ("channel-mode").firstChild.data = mode;
    document.getElementById ("channel-users").firstChild.data = users;
    document.getElementById ("channel-topic").firstChild.data = topic;
    document.getElementById ("channel-topicby").firstChild.data = topicBy;

}

function updateTitle (obj)
{
    if ((obj && obj != client.currentObject) || !client.currentObject)
        return;

    var tstring = "";
    var o = new Object();
    
    getObjectDetails (client.currentObject, o);

    var net = o.network ? o.network.name : "";

    switch (client.currentObject.TYPE)
    {
        case "IRCServer":
        case "IRCNetwork":
            var serv = "", nick = "";
            if (o.server)
            {
                serv  = o.server.connection.host;
                if (o.server.me)
                    nick = o.server.me.properNick;
            }
            
            if (nick) /* user might be disconnected, nick would be undefined */
                tstring += "user '" + nick + "' ";
            
            if (net)
                if (serv)
                    tstring += "attached to '" + net + "' via " + serv;
                else
                    tstring += "attaching to '" + net + "'";
            
            if (tstring)
                tstring = "ChatZilla: " + tstring;
            else
                tstring = "ChatZilla!!";
            break;
            
        case "IRCChannel":
            var chan = "(none)", mode = "", topic = "";

            chan = o.channel.name;
            mode = o.channel.mode.getModeStr();
            if (client.uiState["toolbar"])
                topic = o.channel.topic ? " " + o.channel.topic :
                    " --no topic--";

            if (!mode)
                mode = "no mode";
            tstring = "ChatZilla: " + chan + " (" + mode + ") " + topic;
            break;

        case "IRCUser":
            tstring = "ChatZilla: Conversation with " +
                client.currentObject.properNick;
            break;

        default:
            tstring = "ChatZilla!";
            break;
    }

    if (!client.uiState["toolbar"])
    {
        var actl = new Array();
        for (var i in client.activityList)
            actl.push ((client.activityList[i] == "!") ? (Number(i) + 1) + "!" : 
                       (Number(i) + 1));
        if (actl.length > 0)
            tstring += " --  Activity [" + actl.join (", ") + "]";
    }

    document.title = tstring;

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

function setCurrentObject (obj)
{
    if (!obj.messages)
    {
        dd ("** INVALID OBJECT passed to setCurrentObject **");
        return false;
    }

    if (client.currentObject == obj)
        return true;
        
    var tb, userList;

    if (client.currentObject)
        tb = getTBForObject(client.currentObject);
    
    if (tb)
        tb.setAttribute ("state", "normal");

    if (client.output.firstChild)
        client.output.removeChild (client.output.firstChild);
    client.output.appendChild (obj.messages);

    /* Unselect currenrly selected users. */
    userList = document.getElementById("user-list");
    if (userList) 
        /* Remove curently selection items before this tree gets rerooted, because it seems to 
        remember the selections for eternity if not. */
        userList.clearItemSelection ();    
    else
        dd ("setCurrentObject: could not find element with ID='user-list'");        

    if (obj.TYPE == "IRCChannel")
        client.rdf.setTreeRoot ("user-list", obj.getGraphResource());
    else
        client.rdf.setTreeRoot ("user-list", client.rdf.resNullChan);

    client.currentObject = obj;
    tb = getTBForObject(obj);
    if (tb)
        tb.setAttribute ("state", "current");

    var vk = Number(tb.getAttribute("viewKey"));
    delete client.activityList[vk];

    updateNetwork();
    updateChannel();
    updateTitle ();

    if (client.PRINT_DIRECTION == 1)
        window.frames[0].scrollTo(0, window.frames[0].document.height);
    
}

function addHistory (source, obj)
{
    if (!source.messages)
    {
        source.messages =
            document.createElementNS ("http://www.w3.org/1999/xhtml",
                                      "html:table");
        source.messages.setAttribute ("class", "chat-view");
        source.messages.setAttribute ("cellpadding", "0");
        source.messages.setAttribute ("cellspacing", "0");
        source.messages.setAttribute ("type", source.TYPE);
        source.messages.setAttribute ("width", "100%");
        
        switch (source.TYPE)
        {
            case "IRCChanUser":
            case "IRCUser":
                source.messages.setAttribute ("nick", source.nick);
                break;

            case "IRCNetwork":
            case "IRCChannel":
            case "IRCClient":
                source.messages.setAttribute ("name", source.name);
                break;

            default:
                dd ("** 'source' has INVALID TYPE in addHistory **");
                break;
        }       
    }

    var needScroll = false;
    var w = window.frames[0];
    
    if (client.PRINT_DIRECTION == 1)
    {
        if ((w.document.height - (w.innerHeight + w.pageYOffset)) <
            (w.innerHeight / 3))
            needScroll = true;
        source.messages.appendChild (obj);
    }
    else
        source.messages.insertBefore (obj, source.messages.firstChild);
    
    if (source.MAX_MESSAGES)
    {
        if (typeof source.messageCount != "number")
            source.messageCount = 1;
        else
            source.messageCount++;

        if (source.messageCount > source.MAX_MESSAGES)
            if (client.PRINT_DIRECTION == 1)
                source.messages.removeChild (source.messages.firstChild);
            else
                source.messages.removeChild (source.messages.lastChild);
    }

    if (needScroll && client.currentObject == source)
        w.scrollTo (w.pageXOffset, w.document.height);
    
}

function notifyActivity (source)
{
    if (typeof source != "object")
        source = client.viewsArray[source].source;
    
    var tb = getTBForObject (source, true);
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
    
    var tb = getTBForObject (source, true);
    var vk = Number(tb.getAttribute("viewKey"));

    if (client.currentObject != source)
    {
        tb.setAttribute ("state", "attention");
        client.activityList[vk] = "!";
        updateTitle();
    }

    if (client.FLASH_WINDOW)
        window.GetAttention();
    
}

/* gets the toolbutton associated with an object
 * if |create| is present, and true, create if not found */
function getTBForObject (source, create)
{
    var name;

    if (!source)
    {
        dd ("** UNDEFINED  passed to getTBForObject **");
        dd (getStackTrace());
        return;
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
            dd ("** INVALID OBJECT passed to getTBForObject **");
            return;
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
        tb = document.createElement ("menubutton");
        tb.setAttribute ("onclick", "onTBIClick('" + id + "');");
        
        //tb.addEventListener("command", onTBIClickTempHandler, false);
        
        var aclass = (client.ICONS_IN_TOOLBAR) ?
            "activity-button-image" : "activity-button-text";
        
        tb.setAttribute ("class", "menubutton " + aclass);
        tb.setAttribute ("id", id);
        tb.setAttribute ("state", "normal");

        client.viewsArray.push ({source: source, tb: tb});
        tb.setAttribute ("viewKey", client.viewsArray.length - 1);
        if (matches > 1)
            tb.setAttribute ("value", name + "<" + matches + ">");
        else
            tb.setAttribute ("value", name);

        views.appendChild (tb);
    }

    return tb;
    
}

/*
 * This is used since setAttribute is funked up right now.
 */
function onTBIClickTempHandler (e)
{ 
  
    dd ("onTBIClickTempHandler called");
    
    var id = "tb[" + e.target.getAttribute("value") + "]";

    var tb = document.getElementById (id);
    var view = client.viewsArray[tb.getAttribute("viewKey")];
   
    setCurrentObject (view.source);

}

function deleteToolbutton (tb)
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
            }
            arrayRemoveAt(client.viewsArray, key);
            var tbinner = document.getElementById("views-tbar-inner");
            tbinner.removeChild(tb);
        }
        else
        {
            window.alert ("Current view cannot be deleted.");
            return -1;
        }
            
    }
    else
        dd  ("*** INVALID OBJECT passed to deleteToolButton (" + tb + ") " +
             "no viewKey attribute. (" + key + ")");

    return key;

}

function filterOutput (msg, msgtype)
{

    for (var f in client.outputFilters)
        if (client.outputFilters[f].enabled)
            msg = client.outputFilters[f].func(msg, msgtype);

    client.currentObject.display (msg, msgtype, "!ME");
    
    return msg;
    
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
            client.currentObject.say (msg);
            break;

        case "IRCClient":
            client.onInputEval ({inputData: msg});
            break;

        default:
            client.display ("No default action for objects of type '" +
                            client.currentObject.TYPE + "'", "ERROR");
            break;
    }

}

client.display =
function cli_display (message, msgtype)
{
    var ary = message.split ("\n");

    var msgRow = newInlineText (
        {network : "{LOCAL}", msgtype: msgtype}, "msg", "html:tr");

    var msg = newInlineText (
        {data: "[" + msgtype + "]", network: "{LOCAL}", msgtype: msgtype},
        "msg-type", "html:td");

    msgRow.appendChild(msg);

    var msgData = newInlineText (
        {network: "{LOCAL}", msgtype: msgtype},
        "msg-data", "html:td");
    
    for (var l in ary)
    {
        msgData.appendChild(newInlineText (ary[l]));
        msgData.appendChild
            (document.createElementNS ("http://www.w3.org/1999/xhtml",
                                       "html:br"));
    }

    msgRow.appendChild (msgData);

    addHistory (this, msgRow);
    
    notifyActivity (this);

}

client.quit =
function cli_quit (reason)
{
    
    for (var n in client.networks)
        if (client.networks[n].primServ)
            client.networks[n].quit (reason);
    
}

CIRCNetwork.prototype.display =
function net_display (message, msgtype)
{
    var ary = message.split ("\n");

    var msgRow = newInlineText (
        {network: this.name, msgtype: msgtype},
        "msg", "html:tr");
    
    var msg = newInlineText (
        {data: "[" + msgtype + "]", network: this.name, msgtype: msgtype},
        "msg-type", "html:td");

    msgRow.appendChild(msg);
        
    var msgData = newInlineText (
        {network: this.name, msgtype: msgtype}, "msg-data", "html:td");
    
    for (var l in ary)
    {
        msgData.appendChild(newInlineText(ary[l]));
        msgData.appendChild
            (document.createElementNS ("http://www.w3.org/1999/xhtml",
                                       "html:br"));
    }

    msgRow.appendChild (msgData);
    
    addHistory (this, msgRow);
    notifyActivity (this);

}

CIRCUser.prototype.display =
function user_display(message, msgtype, sourceNick)
{
    var ary = message.split ("\n");

    if (this.TYPE == "IRCUser")
    {
        var msgRow = newInlineText (
            {network: this.parent.parent.name, user: this.nick,
             msgtype: msgtype},
            "msg", "html:tr");
    
        var nickText;
        var realNick = (!sourceNick || sourceNick != "!ME") ? this.properNick :
            this.parent.me.properNick;

        var displayNick = hyphenateWord (realNick, client.MAX_NICK_DISPLAY);
        
        switch (msgtype)
        {                
                
            case "ACTION":
                nickText = newInlineText ("*" + displayNick + "* ",
                                          "msg-user", "html:td");
                break;
                
            case "NOTICE":
                nickText = newInlineText ("[" + displayNick + "] ",
                                          "msg-user", "html:td");
                break;

            case "PRIVMSG":
                nickText = newInlineText (/*"<" +*/ displayNick /*+ ">"*/,
                                          "msg-user", "html:td");
                break;
                
        }

        if (nickText)
        {
            this.mark = (typeof this.mark != "undefined") ? this.mark :
                false;
        
            if ((this.lastNickDisplayed) &&
                (realNick != this.lastNickDisplayed))
            {
                this.mark = !this.mark ;
                this.lastNickDisplayed = realNick;
            }
            else
                this.lastNickDisplayed = realNick;          
        
            nickText.setAttribute ("mark", (this.mark) ? "even" : "odd");
            nickText.setAttribute ("network", this.parent.parent.name);
            nickText.setAttribute ("user", this.nick);
            nickText.setAttribute ("msgtype", msgtype);
            msgRow.appendChild (nickText);   
        }
        else
        {   
            var msg = newInlineText (
                {data: "[" + msgtype + "]", network: this.parent.parent.name,
                 user: this.nick, msgtype: msgtype},
                 "msg-type", "html:td");
            
            msgRow.appendChild (msg);
        }
        
        var msgData = newInlineText (
            {network: this.parent.parent.name, msgtype: msgtype},
             "msg-data", "html:td");

        msgData.setAttribute ("mark", (this.mark) ? "even" : "odd");
        msgData.setAttribute ("network", this.parent.parent.name);
        msgData.setAttribute ("channel", this.name);
        msgData.setAttribute ("user", nick);
        msgData.setAttribute ("msgtype", msgtype);

        for (var l in ary)
        {
            if (msgtype.search (/PRIVMSG|ACTION/) != -1)
                client.munger.munge(ary[l], msgData, getObjectDetails (this));
            else
                msgData.appendChild(newInlineText (ary[l]));
            msgData.appendChild
                (document.createElementNS ("http://www.w3.org/1999/xhtml",
                                           "html:br"));
        }

        msgRow.appendChild (msgData);
        
        addHistory (this, msgRow);
        notifyActivity (this);

    }
    else
        this.parent.display (message, msgtype, this.nick);

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

CIRCChannel.prototype.display =
function chan_display (message, msgtype, nick)
{
    var ary = message.split ("\n");
    var nickText;

    var msgRow = newInlineText (
        {network: this.parent.parent.name , channel: this.name, user: nick,
         msgtype: msgtype},
        "msg", "html:tr");
    
    if (nick)
    {
        var realNick;
        
        if (this.users[nick])
            realNick = this.users[nick].properNick;
        else if (nick == "!ME")
            realNick = this.parent.me.properNick;
        else    
            realNick = nick + "?";
        
        var displayNick = hyphenateWord (realNick, client.MAX_NICK_DISPLAY);

        switch (msgtype)
        {                
                
            case "ACTION":
                nickText = newInlineText ("*" + displayNick + "* ",
                                          "msg-user", "html:td");
                break;
                
            case "NOTICE":
                nickText = newInlineText ("[" + displayNick + "] ",
                                          "msg-user", "html:td");
                break;

            case "PRIVMSG":
                nickText = newInlineText (/*"<" + */ displayNick /*+ "> "*/,
                                          "msg-user", "html:td");
                break;
                
        }
    }

    if (nickText)
    {
        if (typeof this.mark == "undefined")
            this.mark = "even";
        
        if ((this.lastNickDisplayed) &&
            (nick != this.lastNickDisplayed))
        {
            this.mark = (this.mark == "odd") ? "even" : "odd";
            this.lastNickDisplayed = nick;
        }
        else
            this.lastNickDisplayed = nick;                

        nickText.setAttribute ("mark", this.mark);
        nickText.setAttribute ("network", this.parent.parent.name);
        nickText.setAttribute ("channel", this.name);
        nickText.setAttribute ("user", nick);
        nickText.setAttribute ("msgtype", msgtype);
        msgRow.appendChild (nickText);   
    }
    else
    {   
        var msg = newInlineText (
            {data: "[" + msgtype + "]", network: this.parent.parent.name,
                       channel: this.name, user: nick, msgtype: msgtype},
            "msg-type", "html:td");
        
        msgRow.appendChild (msg);
    }

    var msgData = newInlineText (
            {network: this.parent.parent.name, channel: this.name,
             user: nick, msgtype: msgtype},
            "msg-data", "html:td");
    
    msgData.setAttribute ("mark", this.mark);
    msgData.setAttribute ("network", this.parent.parent.name);
    msgData.setAttribute ("channel", this.name);
    msgData.setAttribute ("user", nick);
    msgData.setAttribute ("msgtype", msgtype);
    
    for (var l in ary)
    {
        if (msgtype.search (/PRIVMSG|ACTION|TOPIC/) != -1)
            client.munger.munge(ary[l], msgData, getObjectDetails (this));
        else
            msgData.appendChild(newInlineText (ary[l]));
        msgData.appendChild
            (document.createElementNS ("http://www.w3.org/1999/xhtml",
                                       "html:br"));
    }

    msgRow.appendChild (msgData);
    
    addHistory (this, msgRow);
    notifyActivity (this);

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
