/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is JSIRC Test Client #3.
 *
 * The Initial Developer of the Original Code is
 * New Dimensions Consulting, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, rginda@ndcico.com, original author
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

var client = new Object();

client.COMMAND_CHAR = "/";
client.STEP_TIMEOUT = 500;
client.UPDATE_DELAY = 500;
client.EXPAND_HEIGHT = "200px";
client.COLLAPSE_HEIGHT = "25px";
client.MAX_MESSAGES = 200;
client.MAX_HISTORY = 50;
client.TYPE = "IRCClient";
client.OP1_IMG = "g_green_on.gif"; /* user is op image */
client.OP0_IMG = "g_green.gif"; /* user isnt op image */
client.V1_IMG = "g_grey_on.gif"; /* user is voice image */
client.V0_IMG = "g_grey.gif"; /* user isnt voice image */
client.ACT_IMG = "green-on.gif"; /* view has activity image */
client.HEY_YOU_IMG = "green-blink-1.gif"; /* view has activity image */
client.NACT_IMG = "green-off.gif"; /* view has no activity image */
client.CUR_IMG = "yellow-on.gif"; /* view is currently displayed */
client.PRINT_DIRECTION = 1; /*1 => new messages at bottom, -1 => at top */
                
client.name = "*client*";
client.viewsArray = new Array();
client.lastListType = "chan-users";
client.inputHistory = new Array();
client.lastHistoryReferenced = -1;
client.incompleteLine = "";
client.isPermanent = true;

CIRCNetwork.prototype.INITIAL_NICK = "IRCMonkey";
CIRCNetwork.prototype.INITIAL_NAME = "chatzilla";
CIRCNetwork.prototype.INITIAL_DESC = "New Now Know How";
CIRCNetwork.prototype.INITIAL_CHANNEL = "";
CIRCNetwork.prototype.MAX_MESSAGES = 100;
CIRCNetwork.prototype.IGNORE_MOTD = false;

CIRCServer.prototype.READ_TIMEOUT = 0;
CIRCServer.prototype.VERSION_RPLY += "\nChatZilla";
if (jsenv.HAS_DOCUMENT)
{
    CIRCServer.prototype.VERSION_RPLY += ", running under " +
        navigator.userAgent;
}

CIRCUser.prototype.MAX_MESSAGES = 100;

CIRCChannel.prototype.MAX_MESSAGES = 200;

CIRCChanUser.prototype.MAX_MESSAGES = 100;


function initStatic()
{
    var obj;
    
    obj = document.getElementById("input");
    obj.onkeyup = onInputKeyUp;
    
    obj = document.getElementById("tb[*client*]");
    
    client.quickList = new CListBox(document.getElementById("quickList"));

    var saveDir = client.PRINT_DIRECTION;
    client.PRINT_DIRECTION = 1;
    client.display ("Welcome to ChatZilla...\n" +
                    "Use /attach <network-name> connect to a network.\n" +
                    "Where <network-name> is one of [" +
                    keys (client.networks) + "]\n" +
                    "More help is available with /help [<command-name>]",
                    "HELLO");
    client.PRINT_DIRECTION = saveDir;
    
    setCurrentObject (client);

    if (!jsenv.HAS_XPCOM)
        client.display ("ChatZilla was not given access to the XPConnect " +
                        "service.  You will not be able to connect to an " +
                        "irc server.  A workaround may be to set the " +
                        "'security.checkxpconnect' pref to false in " +
                        "all.js, or your user prefs.",
                        "ERROR");
    window.onkeypress = onWindowKeyPress;
    
}

function initHost(obj)
{

    client.commands = new CCommandManager();
    addCommands (obj.commands);
    
    obj.networks = new Object();
    obj.eventPump = new CEventPump (10);
    
    obj.networks["efnet"] =
	new CIRCNetwork ("efnet", [{name: "irc.magic.ca", port: 6667},
                                   {name: "irc.freei.net", port: 6667},
                                   {name: "irc.cs.cmu.edu",   port: 6667}],
                         obj.eventPump);
    obj.networks["moznet"] =
	new CIRCNetwork ("moznet", [{name: "irc.mozilla.org", port: 6667}],
                         obj.eventPump);
    obj.networks["hybridnet"] =
        new CIRCNetwork ("hybridnet", [{name: "irc.ssc.net", port: 6667}],
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
    obj.munger.addRule
        ("link", /((http|mailto|ftp)\:\/\/[^\)\s]*|www\.\S+\.\S[^\)\s]*)/,
         insertLink);
    obj.munger.addRule
        ("face",
         /((^|\s)[\<\>]?[\;\=\:\8]\~?[\-\^\v]?[\)\|\(pP\<\>oO0\[\]\/\\](\s|$))/,
         insertSmiley);
    obj.munger.addRule ("rheet", /(rhee+t\!*)/i, "rheet");
    obj.munger.addRule ("bold", /(\*.*\*)/, "bold");
    obj.munger.addRule ("italic", /[^sS](\/.*\/)/, "italic");
    obj.munger.addRule ("teletype", /(\|.*\|)/, "teletype");
    obj.munger.addRule ("underline", /(\_.*\_)/, "underline");
    //obj.munger.addRule ("strikethrough", /(\-.*\-)/, "strikethrough");
    obj.munger.addRule ("smallcap", /(\#.*\#)/, "smallcap");

}

function matchMyNick (text, containerTag, eventDetails)
{
    if (eventDetails && eventDetails.server)
    {
        if ((stringTrim(text.toLowerCase()).indexOf
            (eventDetails.server.me.nick) == 0) &&
            text[eventDetails.server.me.nick.length + 1].match(/[\W\s]/))
        {
            containerTag.setAttribute ("directedToMe", "true");
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
    
    var anchor = document.createElement ("html:a");
    anchor.setAttribute ("href", href);
    anchor.setAttribute ("target", "other_window");
    anchor.appendChild (document.createTextNode (matchText));
    containerTag.appendChild (anchor);
    
}

function insertSmiley (emoticon, containerTag)
{
    dd ("arguments: " + emoticon + ", " + containerTag);

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
        var img = document.createElement ("html:img");
        img.setAttribute ("src", src);
        containerTag.appendChild (img);
    }
    
}

/* timer-based mainloop */
function mainStep()
{

    client.eventPump.stepEvents();
    setTimeout ("mainStep()", client.STEP_TIMEOUT);
    
}

function getObjectDetails (obj, rv)
{
    if (!rv)
        rv = new Object();
    
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

function setOutputStyle (style)
{
    var oc = top.frames[0].document;

    oc.close();
    oc.open();
    oc.write ("<html><head>" +
              "<LINK REL=StyleSheet " +
              "HREF='resource:///irc/tests/test3-output-" + style + ".css' " +
              "TYPE='text/css' MEDIA='screen'></head> " +
              "<body><div id='output' class='output-window'></div></body>" +
              "</html>");
    client.output = oc.getElementById ("output");
    
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
            var mins = o.server.lastPing.getMinutes();
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

function newInlineText (data, className, tagName)
{
    if (typeof tagName == "undefined")
        tagName = "html:span";
    
    var a = document.createElement (tagName);
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
        
    var tb;

    if (client.currentObject)
        tb = getTBForObject(client.currentObject);
    
    if (tb)
        tb.setAttribute ("src", client.NACT_IMG);

    if (client.output.firstChild)
        client.output.removeChild (client.output.firstChild);
    client.output.appendChild (obj.messages);

    var quickList = document.getElementById ("quickList");
    if (!obj.list)
        obj.list = new CListBox();

    if (quickList.firstChild)
        quickList.removeChild (quickList.firstChild);
    quickList.appendChild (obj.list.listContainer);
        
    client.currentObject = obj;
    tb = getTBForObject(obj);
    if (tb)
        tb.setAttribute ("src", client.CUR_IMG);

    updateNetwork();
    updateChannel();

    if (client.PRINT_DIRECTION == 1)
        window.frames[0].scrollTo(0, 100000);
    
}

function addHistory (source, obj)
{
    if (!source.messages)
    {
        source.messages = document.createElement ("html:table");
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

    if (client.PRINT_DIRECTION == 1)
    {
        source.messages.appendChild (obj);
        window.frames[0].scrollTo(0, 100000);
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
    
}

function notifyActivity (source)
{
    
    if (typeof source != "object")
        source = client.viewsArray[source].source;
    
    var tb = getTBForObject (source, true);
    
    if (client.currentObject != source)
        if (tb.getAttribute ("src") == client.NACT_IMG)
            tb.setAttribute ("src", client.ACT_IMG);
        else /* if act light is already lit, blink it real quick */
        {
            tb.setAttribute ("src", client.NACT_IMG);
            setTimeout ("notifyActivity(" +
                        Number(tb.getAttribute("viewKey")) + ");", 200);
        }
    
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
        if (client.viewsArray[i].source == source)
            tb = client.viewsArray[i].tb;
        else
            if (client.viewsArray[i].tb.id == id)
                id = "tb[" + name + "<" + (++matches) + ">]";

    if (!tb && create) /* not found, create one */
    {
        var views = document.getElementById ("views-tbar");
        var tbi = document.createElement ("toolbaritem");
        tbi.setAttribute ("onclick", "onTBIClick('" + id + "')");
    
        tb = document.createElement ("button");
        tb.setAttribute ("class", "activity-button");
        tb.setAttribute ("id", id);
        client.viewsArray.push ({source: source, tb: tb});
        tb.setAttribute ("viewKey", client.viewsArray.length - 1);
        if (matches > 1)
            tb.setAttribute ("value", name + "<" + matches + ">");
        else
            tb.setAttribute ("value", name);
        tbi.appendChild (tb);
        views.appendChild (tbi);
    }

    return tb;
    
}

function deleteToolbutton (tb)
{
    var i, key = Number(tb.getAttribute("viewKey"));
    
    if (!isNaN(key))
    {
        if (!client.viewsArray[key].source.isPermanent)
        {
            /* re-index higher toolbuttons */
            for (i = key + 1; i < client.viewsArray.length; i--)
            {
                dd ("re-indexing tb " + i);
                tb.setAttribute ("viewKey", Number(key) - 1);
            }

            arrayRemoveAt(client.viewsArray, key);
            document.getElementById("views-tbar").removeChild(tb.parentNode);
        }
        else
        {
            window.alert ("Current view cannot be deleted.");
            return false;
        }
            
    }
    else
        dd  ("*** INVALID OBJECT passed to deleteToolButton (" + tb + ") " +
             "no viewKey attribute. (" + key + ")");

    return true;

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
        msgData.appendChild (document.createElement ("html:br"));
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
        msgData.appendChild (document.createElement ("html:br"));
    }

    msgRow.appendChild (msgData);
    
    addHistory (this, msgRow);
    notifyActivity (this);

}

CIRCUser.prototype.getDecoratedNick =
function usr_decoratednick()
{

    if (!this.decoNick)
    {
        var pfx;
        var el = newInlineText();
        
        if (this.TYPE == "IRCChanUser")
        {
            var img = document.createElement ("html:img", "option-graphic");
            img.setAttribute ("src", this.isOp ? client.OP1_IMG :
                              client.OP0_IMG);
            el.appendChild (img);
            
            img = document.createElement ("html:img", "option-graphic");
            img.setAttribute ("src", this.isVoice ? client.V1_IMG :
                              client.V0_IMG);
            el.appendChild (img);
        }
        
        el.appendChild (newInlineText (this.properNick, "option-text"));

        this.decoNick = el;
    }
    
    return this.decoNick;

}

CIRCUser.prototype.updateDecoratedNick =
function usr_updnick()
{
    var decoNick = this.getDecoratedNick();

    if (!decoNick)
        return;
    
    if (this.TYPE == "IRCChanUser")
    {
        var obj = decoNick.firstChild;
        obj.setAttribute ("src", this.isOp ? client.OP1_IMG :
                          client.OP0_IMG);

        obj = obj.nextSibling;
        obj.setAttribute ("src", this.isVoice ? client.V1_IMG :
                          client.V0_IMG);
        
        obj = obj.nextSibling;
        obj.firstChild.data = this.properNick;
    }
    
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
        
        switch (msgtype)
        {                
                
            case "ACTION":
                nickText = newInlineText ("*" + realNick + "* ",
                                          "msg-user", "html:td");
                break;
                
            case "NOTICE":
                nickText = newInlineText ("[" + realNick + "] ",
                                          "msg-user", "html:td");
                break;

            case "PRIVMSG":
                nickText = newInlineText (/*"<" +*/ realNick /*+ ">"*/,
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
            msgData.appendChild (document.createElement ("html:br"));
        }

        msgRow.appendChild (msgData);
        
        addHistory (this, msgRow);
        notifyActivity (this);

    }
    else
    {
        this.parent.display (message, msgtype, this.nick);
    }

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
        
        switch (msgtype)
        {                
                
            case "ACTION":
                nickText = newInlineText ("*" + realNick + "* ",
                                          "msg-user", "html:td");
                break;
                
            case "NOTICE":
                nickText = newInlineText ("[" + realNick + "] ",
                                          "msg-user", "html:td");
                break;

            case "PRIVMSG":
                nickText = newInlineText (/*"<" + */ realNick /*+ "> "*/,
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
        msgData.appendChild (document.createElement ("html:br"));
    }

    msgRow.appendChild (msgData);
    
    addHistory (this, msgRow);
    notifyActivity (this);

}
