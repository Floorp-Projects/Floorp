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

function onLoad()
{

    initHost(client);
    setOutputStyle ("default");
    initStatic();
    readIRCPrefs();
    mainStep();
    
}

function onUnload()
{

    client.quit ("ChatZilla! [" + navigator.userAgent + "]");
    
}

function onNotImplemented()
{

    alert ("'We're accepting patches'");
    
}

/* toolbaritem click */
function onTBIClick (id)
{
    var tbi = document.getElementById (id);
    var view = client.viewsArray[tbi.getAttribute("viewKey")];

    setCurrentObject (view.source);
    
}

function onToggleTraceHook()
{
    var h = client.eventPump.getHook ("event-tracer");
    
    h.enabled = !h.enabled;
    document.getElementById("menu-dmessages").setAttribute ("checked",
                                                            h.enabled);
    
}   

function onDoStyleChange (newStyle)
{

    if (newStyle == "other")
        newStyle = window.prompt ("Enter style name");

    if (newStyle)
    {
        setOutputStyle (newStyle);
        setCurrentObject(client.currentObject);
    }
    
}

function onHideCurrentView()
{
    var tb = getTBForObject(client.currentObject);
    
    if (tb)
        if (deleteToolbutton (tb))
            setCurrentObject (client);
    
}

function onClearCurrentView()
{

    if (client.output.firstChild)
        client.output.removeChild (client.output.firstChild);
    delete client.currentObject.messages;

    client.currentObject.display ("Messages Cleared.", "INFO");

    client.output.appendChild (client.currentObject.messages);
    
}

function onDeleteCurrentView()
{
    var tb = getTBForObject(client.currentObject);
    
    if (tb)
    {
        if (deleteToolbutton (tb))
        {
            delete client.currentObject.messages;
            setCurrentObject (client);
        }
        
    }
    
}

function onToggleMunger()
{
    client.munger.enabled = !client.munger.enabled;

    if (client.munger.enabled)
        alert ("The munger may be broken, see " +
               "http://bugzilla.mozilla.org/show_bug.cgi?id=22048");
    
    document.getElementById("menu-munger").setAttribute ("checked",
                                                         client.munger.enabled);
}

function onInputKeyUp (e)
{
    
    switch (e.keyCode)
    {        
        case 13: /* CR */
            if (e.target.id != "input")
            {
                dd ("** KeyUp event came from the wrong place, faking it.");
                dd ("** e.target (" + e.target + ", '" + e.target.id + "') " +
                    " is of type '" + typeof e.target + "'");
                e = new Object();
                e.target = document.getElementById ("input");
            }
            e.line = e.target.value;
            onInputCompleteLine (e);
            break;

        case 38: /* up */
            if (client.lastHistoryReferenced < client.inputHistory.length - 1)
                e.target.value =
                    client.inputHistory[++client.lastHistoryReferenced];
            break;

        case 40: /* down */
            if (client.lastHistoryReferenced > 0)
                e.target.value =
                    client.inputHistory[--client.lastHistoryReferenced];
            else
            {
                client.lastHistoryReferenced = -1;
                e.target.value = client.incompleteLine;
            }
            
            break;

        default:
            client.incompleteLine = e.target.value;

            
    }

}

function onWindowKeyPress (e)
{
    var code = Number (e.keyCode)
    switch (code)
    {
        case 112: /* F1 */
        case 113: /* ... */
        case 114:
        case 115:
        case 116:
        case 117:
        case 118:
        case 119:
        case 120:
        case 121: /* F10 */
            var idx = code - 112;
            if ((client.viewsArray[idx]) && (client.viewsArray[idx].source))
                setCurrentObject(client.viewsArray[idx].source);
            
            return false;
            break;

        default:
            
    }
            
}

function onInputCompleteLine(e)
{

    if (e.target.getAttribute ("expanded") != "YES")
    {
        e.line = e.line.replace (/\n/g, "");

        if (client.inputHistory[0] != e.line)
            client.inputHistory.unshift (e.line);

        if (client.inputHistory.length > client.MAX_HISTORY)
            client.inputHistory.pop();
        
        client.lastHistoryReferenced = -1;
        client.incompleteLine = "";
        
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
                getObjectDetails (ev.target, ev);
                client.eventPump.addEvent (ev);
                e.target.value = "";            
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
            client.sayToCurrentTarget (lines[i]);
            e.target.value = "";            
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
                client.currentObject.display ("Sorry, '" + ary[0].name +
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
    }

    client.PRINT_DIRECTION = saveDir;
    
    return true;
    
}

client.onInputTestDisplay =
function cli_testdisplay (e)
{

    client.currentObject.display ("Hello World!", "HELLO");
    client.currentObject.display ("Nothing is wrong.", "ERROR");
    client.currentObject.display ("Use not, want not.", "USAGE");
    client.currentObject.display ("Don't Panic", "HELP");
    client.currentObject.display ("NOTICE this!", "NOTICE", "Mozilla");
    client.currentObject.display ("And hear this.", "PRIVMSG", "Mozilla");
    client.currentObject.display ("But dont do this?", "ACTION", "Mozilla");
    client.currentObject.display ("or you'll get KICKed", "KICK", "Mozilla");
    client.currentObject.display ("JOIN in the fun.", "JOIN", "Mozilla");
    client.currentObject.display ("PART when you want.", "PART", "Mozilla");
    client.currentObject.display ("But never QUIT", "QUIT", "Mozilla");

    if (client.currentObject.TYPE == "IRCChannel")
    {
        var mynick = e.server.me.nick;
        client.currentObject.display ("NOTICE this!", "NOTICE", "!ME");
        client.currentObject.display ("And hear this.", "PRIVMSG", "!ME");
        client.currentObject.display ("But dont do this?", "ACTION", "!ME");
        client.currentObject.display ("or you'll get KICKed", "KICK", "!ME");
        client.currentObject.display ("JOIN in the fun.", "JOIN", "!ME");
        client.currentObject.display ("PART when you want.", "PART", "!ME");
        client.currentObject.display ("But never QUIT", "QUIT", "!ME");
    }

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
            client.currentObject.display ("Unknown network '" +
                                          e.inputData + "'", "ERROR");
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
    if (!e.channel)
    {
        client.currentObject.display ("Me can only be used from channels.",
                                      "ERROR");
        return false;
    }

    e.inputData = filterOutput (e.inputData, "ACTION", "!ME");
    e.channel.act (e.inputData);
    
    return true;
}

client.onInputMsg =
function cli_imsg (e)
{

    if (!e.network || !e.network.isConnected())
    {
        client.currentObject.display ("You must be connected to a network " +
                                      "to use msg", "ERROR");
        return false;
    }

    var ary = e.inputData.match (/(\S+)\s+(.*)/);
    if (ary == null)
        return false;

    var usr = e.network.primServ.addUser(ary[1].toLowerCase());

    if (!usr.messages)
        usr.display ("Chat with " + usr.nick + " opened.", "INFO");
    setCurrentObject (usr);
    var msg = filterOutput(ary[2], "PRIVMSG", "!ME");
    usr.say (ary[2]);

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
    if (!e.network || !e.network.isConnected())
    {
        client.currentObject.display ("You must be connected to a network " +
                                      "to use quote.", "ERROR");
        return false;
    }

    e.server.sendData (e.inputData + "\n");
    
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
            client.currentObject.display ("{" + e.inputData + "} " + rv,
                                          "EVAL");
        else
            client.currentObject.display ("{" + e.inputData + "}\n" + rv,
                                          "EVAL");
    }
    catch (ex)
    {
        client.currentObject.display (String(ex), "ERROR");
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

    name = String(name);
    
    if ((name[0] != "#") && (name[0] != "&"))
        name = "#" + name;

    e.channel = e.server.addChannel (name);
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
        client.currentObject.display ("Leave can only be used from channels.",
                                      "ERROR");
        return false;
    }

    e.channel.part();

    return true;
    
}

client.onInputZoom =
function cli_izoom (e)
{
    client.currentObject.display ("**WARNING** Zoom is busted at this time :(",
                                  "WARNING");

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

client.onInputWhoIs =
function cli_whois (e)
{

    if (!e.inputData)
        return false;

    if (!e.network || !e.network.isConnected())
    {
        if (!e.network)
            client.currentObject.display ("No network selected.", "ERROR");
        else
            client.currentObject.display ("Network '" + e.network.name +
                                          " is not connected.", "ERROR");        
        return false;
    }
    
    var nick = e.inputData.match(/\S+/);
    
    e.server.sendData ("whois " + nick + "\n");

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
            client.currentObject.display ("Topic: " + e.channel.topic,
                                          "TOPIC");
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

    return true;
    
}

/* 'private' function, should only be used from inside */
CIRCChannel.prototype._addToUserList =
function my_addtolist (user)
{
    var ary = new Array();
    var u;
    var i;

    for (u in this.users)
        ary.push (this.users[u].nick);
    
    ary.sort();

    for (i = 0; i < ary.length; i++)
        if (user.nick == ary[i])
            break;
    
    if (!this.list)
        this.list = new CListBox();
    
    if (i < ary.length - 1)
    {
        this.list.prepend (user.getDecoratedNick(),
                           this.users[ary[i + 1]].getDecoratedNick());
    }
    else
        this.list.add (user.getDecoratedNick());
    
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
    var str = "";

    switch (e.code)
    {
        case 004:
            str = e.params.slice(1).join (" ");
            break;

        case "NOTICE":
            updateNetwork (e.network);
            /* no break */
            
        case 372:
        case 375:
        case 376:
            if (this.IGNORE_MOTD)
                return;
            /* no break */

        default:
            str = e.meat;
            break;
    }

    this.display (p + str, e.code.toUpperCase());
    
}

CIRCNetwork.prototype.on311 = /* whois name */
CIRCNetwork.prototype.on319 = /* whois channels */
CIRCNetwork.prototype.on312 = /* whois server */
CIRCNetwork.prototype.on317 = /* whois idle time */
CIRCNetwork.prototype.on318 = /* whois end of whois*/
function my_whoisreply (e)
{
    var text = "egads!";
    
    switch (Number(e.code))
    {
        case 311:
            text = e.params[2] + " (" + e.params[3] + "@" + e.params[4] +
                ") is " + e.meat;
            break;
            
        case 319:
            text = e.params[2] + " is a member of " + e.meat;
            break;
            
        case 312:
            text = e.params[2] + " is attached to " + e.params[3]
            break;
            
        case 317:
            text = e.params[2] + " has been idle for " + e.params[3] +
                " seconds (on since " + new Date(Number(e.params[4]) * 100) +
                ")";
            break;
            
        case 318:
            text = "End of whois information for " + e.params[2];
            break;
            
    }

    e.server.parent.display(text, e.code);
    
}

CIRCNetwork.prototype.on433 = /* nickname in use */
function my_433 (e)
{

    e.server.parent.display ("The nickname '" + e.params[2] +
                             "' is already in use.", e.code);
    
}

CIRCNetwork.prototype.onError =
function my_neterror (e)
{

    e.server.parent.display (e.meat, "ERROR");
    
}

CIRCNetwork.prototype.onPing =
function my_netping (e)
{

    updateNetwork (e.network);
    
}

CIRCNetwork.prototype.onPong =
function my_netpong (e)
{

    updateNetwork (e.network);
    
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

CIRCChannel.prototype.onTopic = /* user changed topic */
CIRCChannel.prototype.on332 = /* TOPIC reply */
function my_topic (e)
{

    if (e.code == "TOPIC")
        e.channel.display (e.channel.topicBy + " has changed the topic to '" +
                           e.channel.topic + "'", "TOPIC");
    
    updateChannel (e.channel);
    
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

    this._addToUserList (e.user);
    
    updateChannel (e.channel);
    
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

    updateChannel (e.channel);
    
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

    updateChannel (e.channel);
    
}

CIRCChannel.prototype.onChanMode =
function my_cmode (e)
{

    if (e.user)
        this.display ("Mode " + e.params.slice(1).join(" ") + " by " +
                      e.user.nick, "MODE");

    for (var u in e.usersAffected)
        e.usersAffected[u].updateDecoratedNick();

    updateChannel (e.channel);
    
}

    

CIRCChannel.prototype.onNick =
function my_cnick (e)
{

    if (userIsMe (e.user))
    {
        this.display ("YOU are now known as " + e.user.properNick, "NICK",
                      "!ME");
        updateNetwork();
    }
    else
        this.display (e.oldNick + " is now known as " + e.user.properNick,
                      "NICK");

    this.list.remove (e.user.getDecoratedNick());
    e.user.updateDecoratedNick();
    this._addToUserList(e.user);
    
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

    updateChannel (e.channel);
    
}

CIRCUser.prototype.onPrivmsg =
function my_cprivmsg (e)
{
    
    this.display (e.meat, "PRIVMSG");
    
}

CIRCUser.prototype.onNick =
function my_unick (e)
{

    if (userIsMe(e.user))
        updateNetwork();
    
}

CIRCUser.prototype.onNotice =
function my_notice (e)
{

    this.display (e.meat, "NOTICE");
    
}
