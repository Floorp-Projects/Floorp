/*
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
 * The Original Code is JSIRC Test Client #1
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

var client = new Object();

client.COMMAND_CHAR = "/";
client.STEP_TIMEOUT = 500;
client.UPDATE_DELAY = 500;

client.INITIAL_ALIASES =
[
 {name: "connect", value: "e.network.connect();"},
 {name: "quit",
  value: "client.quit(e.inputData ? e.inputData : 'no reason');"},
 {name: "nick",
  value: "if (e.server && e.inputData) " +
  "e.server.sendData ('NICK ' + e.inputData + '\\n');"},
 {name: "network", value: "setCurrentNetwork(e.inputData); onListChannels();"},
 {name: "channel", value: "setCurrentChannel(e.inputData); onListUsers();"},
 {name: "me", value: "if (e.channel) e.channel.act (e.inputData);"}
];

 
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

CIRCUser.prototype.getDecoratedNick = function usr_decoratednick()
{
    var pfx;
    
    if (typeof this.isOp != "undefined")
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
            pfx = "[*??]";
        else
            pfx = "[??] ";
    
    return pfx + this.nick;

}

CIRCNetwork.prototype.getDecoratedName = function usr_decoratedname()
{
    var pfx = "[";

    pfx += this.isConnected() ? "online" : "offline";
    pfx += "] ";

    return pfx + this.name;
    
}

function initStatic()
{
    
    CIRCNetwork.prototype.INITIAL_NICK = "test1";
    CIRCNetwork.prototype.INITIAL_NAME = "test1";
    CIRCNetwork.prototype.INITIAL_DESC = "New Now Know How";
    CIRCNetwork.prototype.INITIAL_CHANNEL = "";
    CIRCServer.prototype.READ_TIMEOUT = 0;

    var list = document.getElementById("lstQuickList");
    list.onclick = onListClick;
    
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

function listFill (list, source, prop, callFlag /* optional */)
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

function display (line)
{
    var output = document.getElementById ("output");

    output.value = line + "\n" + output.value;
    
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

    var quickList = document.getElementById("lstQuickList");

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
