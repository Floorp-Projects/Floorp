/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is JSIRC Test Client #3
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. Copyright (C) 1999
 * New Dimenstions Consulting, Inc. All Rights Reserved.
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

function onLoad()
{

    initHost(client);
    initStatic();
    mainStep();
    
}

function onUnload()
{

    client.quit ("re-load");
    
}

/* toolbaritem click */
function onTBIClick (id)
{
    var tbi = document.getElementById (id);
    var view = client.viewsArray[tbi.getAttribute("viewKey")];

    setCurrentObject (view.source);
    
}

function onInputKeyUp (e)
{
    
    switch (e.which)
    {        
        case 13: /* CR */
            e.line = e.target.value;
            onInputCompleteLine (e);
            break;

        case 38: /* up */
            break;

        case 40: /* down */
            break;
            
    }

}

function onInputCompleteLine(e)
{

    if (e.target.getAttribute ("expanded") != "YES")
    {
        e.line = e.line.replace (/\n/g, "");
        
        if (e.line[0] == client.COMMAND_CHAR)
        {
            var ary = e.line.substr(1, e.line.length).match (/(\S+)? ?(.*)/);
            var command = ary[1];
            
            if (command[0].search (/[\[\{\(]/) == 0) /* request to expand */
            {
                e.target.setAttribute("expanded", "YES");
                switch (command[0])
                {
                    case "[":
                        e.target.setAttribute("collapseChar", "]");
                        break;

                    case "{":
                        e.target.setAttribute("collapseChar", "}");
                        break;
                        
                    case "(":
                        e.target.setAttribute("collapseChar", ")");
                        break;        
                }
                e.target.style.height = client.EXPAND_HEIGHT;
            }
            else /* normal command */
            {
                var ev = new CEvent ("client", "input-command", client,
                                     "onInputCommand");
                ev.command = command;
                ev.inputData =  ary[2] ? stringTrim(ary[2]) : "";

                ev.target = client.currentObject;

                switch (client.currentObject.TYPE) /* set up event props */
                {
                    case "IRCChannel":
                        ev.channel = ev.target;
                        ev.server = ev.channel.parent;
                        ev.network = ev.server.parent;
                        break;

                    case "IRCUser":
                        ev.user = ev.target;
                        ev.server = ev.user.parent;
                        ev.network = ev.server.parent;
                        break;

                    case "IRCChanUser":
                        ev.user = ev.target;
                        ev.channel = ev.user.parent;
                        ev.server = ev.channel.parent;
                        ev.network = ev.server.parent;
                        break;

                    case "IRCNetwork":
                        ev.network = client.currentObject;
                        if (ev.network.isConnected())
                            ev.server = ev.network.primServ;
                        break;

                    case "IRCClient":
                        if (client.lastNetwork)
                        {
                            ev.network = client.lastNetwork;
                            if (ev.network.isConnected())
                                ev.server = ev.network.primServ;
                        }
                        break;

                    default:
                        /* no setup for unknown object */
                        
                }

                client.eventPump.addEvent (ev);
            }
        }
        else /* plain text */
        {
            client.sayToCurrentTarget (e.line);
            e.target.value = "";            
        }
    }
    else /* input-box is expanded */
    {
        var lines = e.target.value.split("\n");
        for (i in lines)
            if (lines[i] == "")
                arrayRemoveAt (lines, i);
        var lastLine = lines[lines.length - 1];

        if (lastLine.replace(/s*$/,"") ==
            e.target.getAttribute ("collapseChar"))
        {
            dd ("collapsing...");
            
            e.target.setAttribute("expanded", "NO");
            e.target.style.height = client.COLLAPSE_HEIGHT;
            e.target.value = "";
            for (var i = 1; i < lines.length - 1; i++)
                client.sayToCurrentTarget (lines[i]);
        }
    }
    
}

client.onInputCommand = 
function cli_icommand (e)
{
    var ary = client.commands.list (e.command);
    
    switch (ary.length)
    {        
        case 0:
            client.currentObject.display ("Unknown command '" + e.command +
                                          "'.", "ERROR");
            break;
            
        case 1:
            if (typeof client[ary[0].func] == "undefined")        
                client.currentObject.display ("Sorry, '" + e.command +
                                              "' has not been implemented.", 
                                              "ERROR");
            else
            {
                e.commandEntry = ary[0];
                if (!client[ary[0].func](e))
                    client.currentObject.display (ary[0].name + " " +
                                                  ary[0].usage, "USAGE");
            }
            break;
            
        default:
            client.currentObject.display ("Ambiguous command: '" + e.command +
                                          "'", "ERROR");
            var str = "";
            for (var i in ary)
                str += str ? ", " + ary[i].name : ary[i].name;
            client.currentObject.display (ary.length + " commands match: " +
                                          str, "ERROR");
    }

}

client.onInputHelp =
function cli_ihelp (e)
{
    var ary = client.commands.list (e.inputData);
 
    if (ary.length == 0)
    {
        client.currentObject.display ("No such command, '" + e.inputData +
                                      "'.", "ERROR");
        return false;
    }

    var saveDir = client.PRINT_DIRECTION;
    client.PRINT_DIRECTION = 1;
    
    for (var i in ary)
    {        
        client.currentObject.display (ary[i].name + " " + ary[i].usage,
                                      "USAGE");
        client.currentObject.display (ary[i].help, "HELP");
        client.currentObject.display ("", "HELP");
    }

    client.PRINT_DIRECTION = saveDir;
    
    return true;
    
}
            
client.onInputNetwork =
function clie_inetwork (e)
{
    if (!e.inputData)
        return false;

    var net = client.networks[e.inputData];
    
    if (net)
    {
        client.lastNetwork = net;
        setCurrentObject (net);    
    }
    else
    {        
        client.currentObject.display ("Unknown network '" + e.inputData + "'",
                                      "ERROR");
        return false;
    }
    
    return true;
    
}

client.onInputAttach =
function cli_iattach (e)
{
    var net;

    if (!e.inputData)
    {
        if (client.lastNetwork)
        {        
            client.currentObject.display ("No network specified network, " +
                                          "Using '" + client.lastNetwork.name +
                                          "'", "NOTICE");
            net = client.lastNetwork;
        }
        else
        {
            client.currentObject.display ("No network specified, and no " +
                                          "default network is in place.",
                                          "ERROR");
            return false;
        }
    }
    else
    {
        net = client.networks[e.inputData];
        if (!net)
        {
            client.display ("Unknown network '" + e.inputData + "'", "ERROR");
            return false;
        }
        client.lastNetwork = net;
    }

    net.connect();
    net.display ("Connecting...", "INFO");
    setCurrentObject(net);
    return true;
    
}
    
client.onInputMe =
function cli_ime (e)
{
    if (e.channel)
    {
        e.inputData = filterOutput (e.inputData, "ACTION", "!ME");
        e.channel.act (e.inputData);
    }
    return true;
}

client.onInputNick =
function cli_inick (e)
{

    if (!e.inputData)
        return false;
    
    if (e.server) 
        e.server.sendData ('NICK ' + e.inputData + '\n');
    else
        CIRCNetwork.prototype.INITIAL_NICK = e.inputData;

    return true;
    
    
}

client.onInputName =
function cli_iname (e)
{

    if (!e.inputData)
        return false;
    
    CIRCNetwork.prototype.INITIAL_NAME = e.inputData;

    return true;
    
}

client.onInputDesc =
function cli_idesc (e)
{

    if (!e.inputData)
        return false;
    
    CIRCNetwork.prototype.INITIAL_DESC = e.inputData;
    
    return true;
    
}

client.onInputQuote =
function cli_iquote (e)
{

    client.primNet.primServ.sendData (e.inputData + "\n");
    
    return true;
    
}

client.onInputEval =
function cli_ieval (e)
{
    if (!e.inputData)
        return false;
    
    if (e.inputData.indexOf ("\n") != -1)
        e.inputData = "\n" + e.inputData + "\n";
    
    try
    {
        rv = String(eval (e.inputData));
        if (rv.indexOf ("\n") == -1)
            client.display ("{" + e.inputData + "} " + rv, "EVAL");
        else
            client.display ("{" + e.inputData + "}\n" + rv, "EVAL");
    }
    catch (ex)
    {
        client.display (String(ex), "ERROR");
    }
    
    return true;
    
}
    
client.onInputJoin =
function cli_ijoin (e)
{
    if (!e.network || !e.network.isConnected())
    {
        if (!e.network)
            client.currentObject.display ("No network selected.", "ERROR");
        else
            client.currentObject.display ("Network '" + e.network.name +
                                          " is not connected.", "ERROR");        
        return false;
    }
    
    var name = e.inputData.match(/\S+/);
    if (!name)
        return false;

    if ((name[0] != "#") && (name[0] != "&"))
        name = "#" + name;

    e.channel = e.server.addChannel (String(name));
    e.channel.join();
    e.channel.display ("Joining...", "INFO");
    setCurrentObject(e.channel);
    
    return true;
    
}

client.onInputLeave =
function cli_ipart (e)
{
    if (!e.channel)
    {            
        client.currentObject.display ("Part can only be used from channels.",
                                      "ERROR");
        return false;
    }

    e.channel.part();

    return true;
    
}

client.onInputZoom =
function cli_izoom (e)
{
    if (!e.inputData)
        return false;
    
    if (!e.channel)
    {
        client.currentObject.display ("Zoom can only be used from channels.",
                                     "ERROR");
        return false;
    }
    
    var nick = e.inputData.toLowerCase();
    var cuser = e.channel.users[nick];
    
    if (!cuser)
    {
        client.currentObject.display ("User '" + e.inputData + "' not found.");
        return false;
    }
    
    setCurrentObject(cuser);

    return true;
    
}    

client.onInputTopic =
function cli_itopic (e)
{
    if (!e.channel)
    {
        client.currentObject.display ("Topic can only be used from channels.",
                                      "ERROR");
        return false;
    }
    
    if (!e.inputData)
    {
        if (e.channel.topic)
        {
            client.currentObject.display (e.channel.topic, "TOPIC");
            client.currentObject.display ("Set by " + e.channel.topicBy +
                                          " on " + e.channel.topicDate + ".",
                                          "TOPIC");
        }
        else
            client.currentObject.display ("No topic.", "TOPIC");
    }
    else
    {
        if (!e.channel.setTopic(e.inputData))
            client.currentObject.display ("Could not set topic.", "ERROR");
    }
    
}
            
CIRCNetwork.prototype.onNotice = 
CIRCNetwork.prototype.on001 = /* Welcome! */
CIRCNetwork.prototype.on002 = /* your host is */
CIRCNetwork.prototype.on003 = /* server born-on date */
CIRCNetwork.prototype.on004 = /* server id */
CIRCNetwork.prototype.on251 = /* users */
CIRCNetwork.prototype.on252 = /* opers online (in params[2]) */
CIRCNetwork.prototype.on254 = /* channels found (in params[2]) */
CIRCNetwork.prototype.on255 = /* link info */
CIRCNetwork.prototype.on265 = /* local user details */
CIRCNetwork.prototype.on266 = /* global user details */
CIRCNetwork.prototype.on375 = /* start of MOTD */
CIRCNetwork.prototype.on372 = /* MOTD line */
CIRCNetwork.prototype.on376 = /* end of MOTD */
function my_showtonet (e)
{
    var p = (e.params[2]) ? e.params[2] + " " : "";

    switch (e.code)
    {        
        case 372:
        case 375:
        case 376:
            if (this.IGNORE_MOTD)
                return;
            /* no break */

        default:
            this.display (p + e.meat, e.code.toUpperCase());
            break;
    }
    
}

CIRCChannel.prototype.onPrivmsg =
function my_cprivmsg (e)
{
    
    e.user.display (e.meat, "PRIVMSG");
    
    if (e.meat.indexOf (client.prefix) == 0)
    {
        try
        {
            var v = eval(e.meat.substring (client.prefix.length,
                                           e.meat.length));
        }
        catch (ex)
        {
            this.say (e.user.nick + ": " + String(ex));
            return false;
        }
        
        if (typeof (v) != "undefined")
        {						
            if (v != null)                
                v = String(v);
            else
                v = "null";
            
            var rsp = e.user.nick + ", your result is,";
            
            if (v.indexOf ("\n") != -1)
                rsp += "\n";
            else
                rsp += " ";
            
            this.say (rsp + v);
        }
    }

    return true;
    
}

/* end of names */
CIRCChannel.prototype.on366 =
function my_366 (e)
{

    if (!this.list)
        this.list = new CListBox();
    else
        this.list.clear();

    var ary = new Array();    

    for (var u in this.users)
        ary.push (this.users[u].nick);
    
    ary.sort();
    
    for (var u in ary)
        this.list.add (this.users[ary[u]].getDecoratedNick());
    
}    
    
CIRCChannel.prototype.onNotice =
function my_notice (e)
{

    e.user.display (e.meat, "NOTICE", e.user.nick);
    
}

CIRCChannel.prototype.onCTCPAction =
function my_caction (e)
{

    e.user.display (e.CTCPData, "ACTION");

}

CIRCChannel.prototype.onJoin =
function my_cjoin (e)
{

    if (userIsMe (e.user))
        this.display ("YOU have joined " + e.channel.name, "JOIN", "!ME");
    else
        this.display(e.user.properNick + " (" + e.user.name + "@" +
                     e.user.host + ") has joined " + e.channel.name,
                     "JOIN", e.user.nick);

    var ary = new Array();    

    for (var u in this.users)
        ary.push (this.users[u].nick);
    
    ary.sort();

    for (u in ary)
        if (ary[u] == e.user.nick)
            break;
    
    if (u < ary.length - 1)
    {
        this.list.prepend (e.user.getDecoratedNick(),
                           e.channel.users[ary[u + 1]].getDecoratedNick());
    }
    else
        this.list.add (e.user.getDecoratedNick());
    
}

CIRCChannel.prototype.onPart =
function my_cpart (e)
{

    if (userIsMe (e.user))
        this.display ("YOU have left " + e.channel.name, "PART", "!ME");
    else
        this.display (e.user.properNick + " has left " + e.channel.name,
                      "PART");
    this.list.remove (e.user.getDecoratedNick());
    
}

CIRCChannel.prototype.onKick =
function my_ckick (e)
{

    if (userIsMe (e.lamer))
        this.display ("YOU have been booted from " + e.channel.name +
                      " by " + e.user.properNick + " (" + e.reason + ")",
                      "KICK", e.user.nick);
    else
    {
        var enforcerProper, enforcerNick;
        if (userIsMe (e.user))
        {
            enforcerProper = "YOU";
            enforcerNick = "!ME";
        }
        else
        {
            enforcerProper = e.user.properNick;
            enforcerNick = e.user.nick;
        }
        
        this.display (e.lamer.properNick + " was booted from " +
                      e.channel.name + " by " + enforcerProper + " (" +
                      e.reason + ")", "KICK", enforcerNick);
    }
    
    this.list.listContainer.removeChild (e.user.getDecoratedNick());

}

CIRCChannel.prototype.onChanMode =
function my_cmode (e)
{

    this.display (e.meat + " by " +
                  e.user.nick, "MODE");

    for (var u in e.usersAffected)
        e.usersAffected[u].updateDecoratedNick();
    
}

    

CIRCChannel.prototype.onNick =
function my_cnick (e)
{

    if (userIsMe (e.user))
        this.display ("YOU are now known as " + e.user.properNick, "NICK",
                      "!ME");
    else
        this.display (e.oldNick + " is now known as " + e.user.properNick,
                      "NICK");

    e.user.updateDecoratedNick();
    
}


CIRCChannel.prototype.onQuit =
function my_cquit (e)
{

    if (userIsMe(e.user)) /* I dont think this can happen */
        this.display ("YOU have left " + e.server.parent.name +
                      " (" + e.reason + ")", "QUIT", "!ME");
    else
        this.display (e.user.properNick + " has left " + e.server.parent.name +
                      " (" + e.reason + ")", "QUIT");

    this.list.remove (e.user.getDecoratedNick());
    
}

CIRCUser.prototype.onPrivmsg =
function my_cprivmsg (e)
{
    
    this.display (e.meat, "PRIVMSG");
    
}

CIRCUser.prototype.onNotice =
function my_notice (e)
{

    this.display (e.meat, "NOTICE");
    
}
