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
 * The Original Code is JSIRC Test Client #2
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

CIRCNetwork.prototype.INITIAL_NICK = "IRCMonkey";
CIRCNetwork.prototype.INITIAL_NAME = "chatzilla";
CIRCNetwork.prototype.INITIAL_DESC = "New Now Know How";
CIRCNetwork.prototype.INITIAL_CHANNEL = "";
CIRCServer.prototype.READ_TIMEOUT = 0;

var client = new Object();

client.COMMAND_CHAR = "/";
client.STEP_TIMEOUT = 500;
client.UPDATE_DELAY = 500;
client.TYPE = "IRCClient";
client.PRINT_DIRECTION = -1; /*1 => new messages at bottom, -1 => at top */

client.INITIAL_ALIASES =
[
 {name: "connect", value: "e.network.connect();"},
 {name: "quit",
  value: "client.quit(e.inputData ? e.inputData : 'no reason');"},
 {name: "network", value: "setCurrentNetwork(e.inputData); onListChannels();"},
 {name: "channel", value: "setCurrentChannel(e.inputData); onListUsers();"},
];

client.name = "*client*";
client.actList = new Object();
client.activeDisplay = client;


client.addAlias = function cli_addalias (name, value)
{
    var funcName = "onInput" + name[0].toUpperCase() +
        name.substr (1, name.length).toLowerCase();

    try
    {
        
        client[funcName] = eval ("f = function alias_" + name + " (e) { " +
                                 value + "}");
        
    }
    catch (ex)
    {
        display ("Error adding alias (" + name + "): "  + ex);
    }
    
}

client.removeAlias = function cli_remalias (name)
{

    delete client[funcName];
    
}

client.quit = function cli_quit (reason)
{
    
    for (var n in client.networks)
        if (client.networks[n].primServ)
            client.networks[n].quit (reason);
    
}

client.display = function cli_display (message, msgtype)
{
    var ary = message.split ("\n");
    
    var msgContainer = newInlineText ("", "msg");
    msgContainer.setAttribute ("network", "{LOCAL}");
    msgContainer.setAttribute ("msgtype", msgtype);
    
    var msg = newInlineText ("[" + msgtype + "]", "msg-type");
    msg.setAttribute ("network", "{LOCAL}");
    msg.setAttribute ("msgtype", msgtype);
    msgContainer.appendChild(msg);

    for (var l in ary)
    {
        var msg = newInlineText (ary[l], "msg-data");
        msg.setAttribute ("network", "{LOCAL}");
        msg.setAttribute ("msgtype", msgtype);
        msgContainer.appendChild(msg);
        msgContainer.appendChild (document.createElement ("br"));
    }

    addHistory (this, msgContainer);        
    notifyActivity (this);

}

CIRCNetwork.prototype.getDecoratedName = function usr_decoratedname()
{
    var pfx = "[";

    pfx += this.isConnected() ? "online" : "offline";
    pfx += "] ";

    return pfx + this.name;
    
}

CIRCNetwork.prototype.display = function net_display (message, msgtype)
{
    var ary = message.split ("\n");

    var msgContainer = newInlineText ("", "msg");
    msgContainer.setAttribute ("network", this.name);
    msgContainer.setAttribute ("msgtype", msgtype);

    var msg = newInlineText ("[" + msgtype + "]", "msg-type");
    msg.setAttribute ("network", this.name);
    msg.setAttribute ("msgtype", msgtype);
    msgContainer.appendChild(msg);
        
    for (var l in ary)
    {
        var msg = newInlineText (ary[l], "msg-data");
        msg.setAttribute ("network", this.name);
        msg.setAttribute ("msgtype", msgtype);
        msgContainer.appendChild(msg);
        msgContainer.appendChild (document.createElement ("br"));
    }

    addHistory (this, msgContainer);
    notifyActivity (this);
            
}

CIRCUser.prototype.getDecoratedNick = function usr_decoratednick()
{
    var pfx;
    
    if (this.TYPE == "IRCChanUser")
    {
        pfx = "[";
        if (this.__proto__ == this.parent.parent.me)
            pfx += "*";
        pfx += this.isOp ? "O" : "X";
        pfx += this.isVoice ? "V" : "X";
        pfx += "] ";
    }
    else
        if (this == this.parent.me)
            pfx = "[*]";
        else
            pfx = "";
    
    return pfx + this.nick;

}

CIRCUser.prototype.display = function user_display(message, msgtype)
{
    var ary = message.split ("\n");

    dd ("user.display");

    var msgContainer = newInlineText ("", "msg");
    msgContainer.setAttribute ("network", this.parent.parent.name);
    msgContainer.setAttribute ("user", this.nick);
    msgContainer.setAttribute ("msgtype", msgtype);

    var msg = newInlineText ("[" + msgtype + "]", "msg-type");
    msg.setAttribute ("network", this.parent.parent.name);
    msg.setAttribute ("user", this.nick);
    msg.setAttribute ("msgtype", msgtype);
    msgContainer.appendChild(msg);
    
    if (this.TYPE == "IRCUser")
    {
        for (var l in ary)
        {
            var msg = newInlineText (ary[l], "msg-data");
            msg.setAttribute ("network", this.parent.parent.name);
            msg.setAttribute ("user", this.nick);
            msg.setAttribute ("msgtype", msgtype);
            msgContainer.appendChild(msg);
            msgContainer.appendChild (document.createElement ("br"));
            addHistory (this, msgContainer);
            notifyActivity (this);
        }
    }
    else
    {
        this.parent.display (message, msgtype, this.nick);
    }
    
}

CIRCChannel.prototype.display = function chan_display (message, msgtype, nick)
{
    var ary = message.split ("\n");

    dd ("channel.display");
    
    var msgContainer = newInlineText ("", "msg");
    msgContainer.setAttribute ("network", this.parent.parent.name);
    msgContainer.setAttribute ("channel", this.name);
    msgContainer.setAttribute ("user", nick);
    msgContainer.setAttribute ("msgtype", msgtype);

    var msg = newInlineText ("[" + msgtype + "]", "msg-type");
    msg.setAttribute ("network", this.parent.parent.name);
    msg.setAttribute ("channel", this.name);
    msg.setAttribute ("user", nick);
    msg.setAttribute ("msgtype", msgtype);
    msgContainer.appendChild (msg);
    
    for (var l in ary)
    {
        var msg = newInlineText (ary[l], "msg-data");
        msg.setAttribute ("network", this.parent.parent.name);
        msg.setAttribute ("channel", this.name);
        msg.setAttribute ("user", nick);
        msg.setAttribute ("msgtype", msgtype);
        
        if (nick)
        {
            var nickText,
                realNick = (nick == "!ME") ? this.parent.me.nick : nick;
            
            switch (msgtype)
            {
                
                case "PRIVMSG":
                    nickText = newInlineText ("<" + realNick + "> ",
                                              "msg-user");
                    break;
                    
                case "ACTION":
                    nickText = newInlineText ("*" + realNick + "* ",
                                              "msg-user");
                    break;
                    
                case "NOTICE":
                    nickText = newInlineText ("[" + realNick + "] ",
                                              "msg-user");
                    break;
                    
            }

            if (nickText)
            {
                nickText.setAttribute ("network", this.parent.parent.name);
                nickText.setAttribute ("channel", this.name);
                nickText.setAttribute ("user", nick);
                nickText.setAttribute ("msgtype", msgtype);
                msgContainer.appendChild (nickText);
            }
            
        }

        msgContainer.appendChild (msg);
        msgContainer.appendChild (document.createElement ("br"));        
    }

    addHistory (this, msgContainer);        
    notifyActivity (this);

}

function initStatic()
{
    
    var list = document.getElementById("QuickList");
    list.onclick = onListClick;

    var list = document.getElementById("ActList");
    list.onclick = onActListClick;
    
}

/*
 * One time initilization stuff
 */     
function init(obj)
{
    
    obj.networks = new Object();
    obj.eventPump = new CEventPump (10);
    
    obj.networks["efnet"] =
	new CIRCNetwork ("efnet", [{name: "irc.primenet.com", port: 6667},
                                  {name: "irc.cs.cmu.edu",   port: 6667}],
                         obj.eventPump);
    obj.networks["linuxnet"] =
	new CIRCNetwork ("linuxnet", [{name: "irc.mozilla.org", port: 6667}],
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
                               "event-tracer", true /* negate */);
    
}

function listClear (list)
{

    while (list.options.length > 0)
	list.options[0] = null;

}

function __cruft__listFill (list, source, prop, callFlag /* optional */)
{
    var ary = new Array();
    
    listClear (list);

    for (var o in source)
        if (!callFlag)
            ary.push (source[o][prop]);
        else
            ary.push (source[o][prop]());

    ary.sort();
    
    for (var i = 0; i < ary.length; i++)
        list.add (new Option(ary[i]), null);
    
}

function listFill (list, source, prop, callFlag /* optional */)
{
    var ary = new Array();

    if (!list || !source)
    {
        dd ("** BAD PARAMS passes to listFill **");
        return;
    }
    
    for (var o in source)
        if (!callFlag)
            ary.push (source[o][prop]);
        else
            ary.push (source[o][prop]());

    ary.sort();
    
    for (var i = 0; i < ary.length; i++)
    {
        if (list.options[i])
            list.options[i] = null;
        list.options[i] = new Option(ary[i]);
    }

    dd ("deleting...");
    while (list.options[ary.length])
        list.options[ary.length] = null;
    dd ("deleting done.");
    
}

function listGetSelectedIndex (list)
{
    var sels = new Array();

    for (o in list.options)
        if (list.options[o].selected)
            sels[sels.length] = o;

    if (sels.length == 1)
        return sels[0];
    else
        return sels;
    
}

function newInlineText (data, className)
{
    var a = document.createElement ("a");

    if (data)
        a.appendChild (document.createTextNode (data));
    if (typeof className != "undefined")
        a.setAttribute ("class", className);

    return a;
    
}

function addHistory (source, obj)
{

    if (!source.history)
    {
        source.history = document.createElement ("span");
        source.history.setAttribute ("class", "chat-view");
        source.history.setAttribute ("type", source.TYPE);
        
        switch (source.TYPE)
        {
            case "IRCChanUser":
            case "IRCUser":
                source.history.setAttribute ("nick", source.nick);
                break;

            case "IRCNetwork":
            case "IRCChannel":
            case "IRCClient":
                source.history.setAttribute ("name", source.name);
                break;

            default:
                dd ("** 'source' has INVALID TYPE in addHistory **");
                break;
        }       
    }

    if (client.PRINT_DIRECTION == 1)
        source.history.appendChild (obj);
    else
        source.history.insertBefore (obj, source.history.firstChild);    
    
}

function displayHistory (source)
{
    var output = document.getElementById ("output");

    output.removeChild (output.firstChild);
    output.appendChild (source.history);
    client.activeDisplay = source;

    var name;
    
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
    }
    
    for (var o in client.actList)
        if (o == name)
            client.actList[o].value = o;

    listFill (document.getElementById ("ActList"), client.actList, "value");
    
}

function notifyActivity (source)
{
    
    if (client.activeDisplay == source)
    {
        client.actList[source.name] = {value: source.name, source: source};
        listFill (document.getElementById ("ActList"), client.actList,
                  "value");
    }
    else
    {
        var name;
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
        }

        client.actList[name] = {value: "[ACT] " + name, source: source};
        
        listFill (document.getElementById ("ActList"), client.actList,
                  "value");
    }
    
}

function mainstep()
{

    client.eventPump.stepEvents();
    setTimeout ("mainstep()", client.STEP_TIMEOUT);
    
}

/*
 * Hook used to trace events.
 */
function event_tracer (e)
{
    var name="", data="";

    switch (e.set)
    {
        case "server":
            name = e.destObject.connection.host;
            if (e.type == "rawdata")
                data = "'" + e.data + "'";
            break;

        case "channel":
            name = e.destObject.name;
            break;
            
        case "user":
            name = e.destObject.nick;
            break;

        case "httpdoc":
            name = e.destObject.server + e.destObject.path;
            if (e.destObject.state != "complete")
                data = "state: '" + e.destObject.state + "', recieved " +
                    e.destObject.data.length;
            else
                dd ("document done:\n" + dumpObjectTree (this));
            break;

        case "dcc-chat":
            name = e.destObject.host + ":" + e.destObject.port;
            if (e.type == "rawdata")
                data = "'" + e.data + "'";
            break;

        case "client":
            if (e.type == "do-connect")
                data = "attempt: " + e.attempt + "/" +
                    e.destObject.MAX_CONNECT_ATTEMPTS;
            break;

        default:
            break;
    }

    if (name)
        name = "[" + name + "]";

    str = "Level " + e.level + ": '" + e.type + "', " +
        e.set + name + "." + e.destMethod;
	if (data)
	  str += "\ndata   : " + data;

    dd (str);

    return true;

}

function updateList ()
{

    if (!client.updateTimeout)
        client.updateTimeout = setTimeout ("updateListNow()",
                                           client.UPDATE_DELAY);    
}

function updateListNow()
{

    var quickList = document.getElementById("QuickList");

    if (client.updateTimeout)
    {
        clearTimeout (client.updateTimeout);
        delete client.updateTimeout;
    }

    switch (client.lastListType)
    {

        case "networks":
            listFill (quickList, client.networks, "getDecoratedName",
                      true);
            break;

        case "users":
            listFill (quickList, client.network.primServ.users,
                      "getDecoratedNick", true);
            break;

        case "channels":
            listFill (quickList, client.network.primServ.channels, "name",
                      false);
            break;

        case "chan-users":
            listFill (quickList, client.channel.users, "getDecoratedNick",
                      true);
            break;

        default:
            break;
            
    }
    
}

function updateStatus()
{
    var status = document.getElementById ("status");
    
    var netName = (client.network) ? client.network.name : "[no-network]";
    var chanName = (client.channel) ? client.channel.name : "[no-channel]";
    
    status.value = netName + " " + chanName;
    
}

function setCurrentNetwork (o)
{
    var oldnetwork = client.network;
    
    switch (typeof o)
    {
        case "object":
            client.network = o;
            break;

        case "string":
            var v = client.networks[o];
            if (v)
                client.network = v;
            break;

        default:
            dd ("setCurrentNetwork: typeof o (" + typeof o + ") confused me.");
            return false;
    }

    if (client.network != oldnetwork)
    {
        client.channel = (void 0);
        client.user = (void 0);
        client.cuser = (void 0);
    }

    var props = document.getElementById ("txtProperties");
    props.value = "network: " + client.network.name + "\n" +
        dumpObjectTree (client.network, 1) +
        "\n============\nclient object:\n" + dumpObjectTree (client, 1);
    updateStatus();

}

function setCurrentUser (o)
{
    switch (typeof o)
    {
        case "object":
            client.user = o;
            break;

        case "string":
            if (!client.network.primServ)
                return false;
            
            var v = client.network.primServ.users[o];
            if (v)
                client.user = v;
            break;

        default:
            return false;
    }

    var props = document.getElementById ("txtProperties");
    props.value = "network user: " + client.user.nick + "\n" +
        dumpObjectTree (client.user, 1) +
        "\n============\nclient object:\n" + dumpObjectTree (client, 1);
    updateStatus();
    
}

function setCurrentChannel (o)
{
    var oldchannel = client.channel;
    
    switch (typeof o)
    {
        case "object":
            client.channel = o;
            break;

        case "string":
            if (!client.network.primServ)
                return false;
            
            var v = client.network.primServ.channels[o];
            if (v)
                client.channel = v;
            break;

        default:
            return false;
    }

    if (client.channel != oldchannel)
        client.cuser = (void 0);
    
    var props = document.getElementById ("txtProperties");
    props.value = "channel: " + client.channel.name + "\n" +
        dumpObjectTree (client.channel, 1) +
        "\n============\nclient object:\n" + dumpObjectTree (client, 1);
    updateStatus();
    
}
function setCurrentCUser (o)
{
    switch (typeof o)
    {
        case "object":
            client.cuser = o;
            break;

        case "string":
            if (!client.channel)
                return false;
            
            var v = client.channel.users[o];
            if (v)
                client.cuser = v;
            break;

        default:
            return false;
    }

    setCurrentUser (o.__proto__);
    var props = document.getElementById ("txtProperties");
    props.value = "channel user: " + client.cuser.nick + "\n" +
        dumpObjectTree (client.cuser, 1) +
        "\n============\nclient object:\n" + dumpObjectTree (client, 1);
    updateStatus();
    
}
