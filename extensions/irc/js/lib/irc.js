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
 * The Original Code is JSIRC Library
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. Copyright (C) 1999
 * New Dimenstions Consulting, Inc. All Rights Reserved.
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 *
 ****
 *
 * depends on utils.js, events.js, and connection.js
 *
 * IRC(RFC 1459) library.
 * Contains the following classes:
 *
 * CIRCNetwork
 * Networtk object.  Takes care of logging into a "primary" server given a
 * list of potential hostnames for an IRC network.  (among other things.)
 *
 * CIRCServer
 * Server object.  Requires an initialized nsIConnection object for
 * communicating with the irc server.
 * Server.sayTo queues outgoing PRIVMSGs for sending to the server.  Using
 * sayTo takes care not to send lines faster than one every 1.5 seconds.
 * Server.connection.sendData sends raw lines over the connection, avoiding the
 * queue.
 *
 * CIRCUser
 * User objects.  Children of server objects.
 *
 * CIRCChannel
 * Channel object.  Children of server objects.
 *
 * CIRCChanMode
 * Channel mode object.  Children of channel objects
 *
 * CIRCChanUser
 * Channel User objects.  Children of channel objects, with __proto__ set
 * to a CIRCUser object (automatically.)
 *
 * 1999-09-15 rginda@ndcico.com           v1.0
 *
 */

/*
 * irc network
 */
function CIRCNetwork (name, serverList, eventPump)
{

    this.name = name;
    this.serverList = serverList;
    this.eventPump = eventPump;

}

/** Clients should override this stuff themselves **/
CIRCNetwork.prototype.INITIAL_NICK = "js-irc";
CIRCNetwork.prototype.INITIAL_NAME = "INITIAL_NAME";
CIRCNetwork.prototype.INITIAL_DESC = "INITIAL_DESC";
CIRCNetwork.prototype.INITIAL_CHANNEL = "#jsbot"; 
/* set INITIAL_CHANNEL to "" if you don't want a primary channel */

CIRCNetwork.prototype.MAX_CONNECT_ATTEMPTS = 5000;
CIRCNetwork.prototype.stayingPower = false; 

CIRCNetwork.prototype.TYPE = "IRCNetwork";

CIRCNetwork.prototype.connect = function net_conenct()
{

    var ev = new CEvent ("network", "do-connect", this, "onDoConnect");
    this.eventPump.addEvent (ev);

}

CIRCNetwork.prototype.quit = CIRCNetwork.prototype.exit = 
function net_quit (reason)
{

    this.stayingPower = false;
    this.primServ.logout (reason);

}

/*
 * Handles a request to connect to a primary server.
 */
CIRCNetwork.prototype.onDoConnect = function net_doconnect(e)
{

    if ((this.primServ) && (this.primServ.connection.isConnected))
        return true;

    var attempt = (typeof e.attempt == "undefined") ? 1 : e.attempt + 1;
    var host = (typeof e.lastHost == "undefined") ? 0 : e.lastHost + 1;

    if (attempt > this.MAX_CONNECT_ATTEMPTS)
        return false;	

    var c = new CBSConnection();

    if (host >= this.serverList.length)
        host = 0;

    if (c.connect (this.serverList[host].name, this.serverList[host].port,
                   (void 0), true))
    {
        var ev = new CEvent ("network", "connect", this, "onConnect");
        ev.server = this.primServ = new CIRCServer (this, c);
        this.eventPump.addEvent (ev);
    }
    else
    { /* connect failed, try again  */
        var ev = new CEvent ("network", "do-connect", this, "onDoConnect");
        ev.lastHost = host;
        ev.attempt = attempt;
        this.eventPump.addEvent (ev);
    }

    return true;

}

/*
 * What to do when the client connects to it's primary server
 */
CIRCNetwork.prototype.onConnect = function net_connect (e)
{

    this.primServ = e.server;
    this.primServ.login (this.INITIAL_NICK, this.INITIAL_NAME,
                         this.INITIAL_DESC);

    if (this.INITIAL_CHANNEL)
    {
        this.primChan = this.primServ.addChannel (this.INITIAL_CHANNEL);
        this.primChan.join();
    }

    e.set = "server";
    e.destObject = this.primServ;

    return true;

}

CIRCNetwork.prototype.isConnected = function net_connected (e)
{

    return (this.primServ && this.primServ.connection.isConnected);
    
}

/*
 * irc server
 */ 
function CIRCServer (parent, connection)
{

    this.parent = parent;
    this.connection = connection;
    this.channels = new Object();
    this.users = new Object();
    this.sendQueue = new Array();
    this.lastSend = new Date();
    this.sendsThisRound = 0;
    this.savedLine = "";
    this.lag = -1;    
    
    this.parent.eventPump.addEvent (new CEvent ("server", "poll", this,
                                                "onPoll"));
    
}

CIRCServer.prototype.MAX_LINES_PER_SEND = 5;
CIRCServer.prototype.MS_BETWEEN_SENDS = 1500;
CIRCServer.prototype.READ_TIMEOUT = 2000;
CIRCServer.prototype.TOO_MANY_LINES_MSG = "\01ACTION has said too much\01";
CIRCServer.prototype.VERSION_RPLY = "JS-IRC Library v0.01, " +
    "Copyright (C) 1999 Robert Ginda; rginda@ndcico.com";
CIRCServer.prototype.DEFAULT_REASON = "no reason";

CIRCServer.prototype.TYPE = "IRCServer";

CIRCServer.prototype.flushSendQueue = function serv_flush()
{

    this.sendQueue.length = 0;
    dd("sendQueue flushed.");

    return true;

}

CIRCServer.prototype.login = function serv_login(nick, name, desc)
{

    this.me = new CIRCUser (this, nick, name);
    this.sendData ("NICK " + nick + "\n");
    this.sendData ("USER " + name + " foo bar :" + desc + "\n");
    
}

CIRCServer.prototype.logout = function serv_logout(reason)
{
    
    if (typeof reason == "undefined") reason = this.DEFAULT_REASON;

    this.connection.sendData ("QUIT :" + reason + "\n");
    this.connection.disconnect();

}

CIRCServer.prototype.addChannel = function serv_addchan (name)
{

    return new CIRCChannel (this, name);
    
}
    
CIRCServer.prototype.addUser = function serv_addusr (nick, name, host)
{

    return new CIRCUser (this, nick, name, host);
    
}
    
CIRCServer.prototype.getChannelsLength = function serv_chanlen()
{
    var i = 0;

    for (var p in this.channels)
        i++;

    return i;
    
}

CIRCServer.prototype.getUsersLength = function serv_chanlen()
{
    var i = 0;

    for (var p in this.users)
        i++;

    return i;
    
}

CIRCServer.prototype.sendData = function serv_senddata (msg)
{
    
    arrayInsertAt (this.sendQueue, 0, msg);
        
}

CIRCServer.prototype.queuedSendData = function serv_senddata (msg)
{
    
    arrayInsertAt (this.sendQueue, 0, msg);
        
}

/*
 * Takes care not to let more than MAX_LINES_PER_SEND lines out per
 * cycle.  Cycle's are defined as the time between onPoll calls.
 */
CIRCServer.prototype.messageTo = function serv_messto (code, target, msg,
                                                       ctcpCode)
{
    var lines = String(msg).split ("\n");
    var sendable = 0, i;
    var pfx = "", sfx = "";

    if (this.sendsThisRound > this.MAX_LINES_PER_SEND)
        return false;

    if (ctcpCode)
    {
        pfx = "\01" + ctcpCode + " ";
        sfx = "\01";
    }

    for (i in lines)
        if (lines[i] != "") sendable++;

    for (i in lines)
    {
        if (((this.sendsThisRound == this.MAX_LINES_PER_SEND - 1) &&
             (sendable > this.MAX_LINES_PER_SEND)) ||
            (this.sendsThisRound == this.MAX_LINES_PER_SEND))
        {
            this.sendData ("PRIVMSG " + target + " :" +
                           this.TOO_MANY_LINES_MSG + "\n");
            this.sendsThisRound++;
            return true;
        }
            
        if (lines[i] != "")
        {
            this.sendsThisRound++;
            this.sendData (code + " " + target + " :" + pfx + lines[i] +
                           sfx + "\n");
        }
        
    }

    return true;
        
}

CIRCServer.prototype.sayTo = function serv_sayto (target, msg)
{

    this.messageTo ("PRIVMSG", target, msg);

}

CIRCServer.prototype.noticeTo = function serv_noticeto (target, msg)
{

    this.messageTo ("NOTICE", target, msg);

}

CIRCServer.prototype.actTo = function serv_actto (target, msg)
{

    this.messageTo ("PRIVMSG", target, msg, "ACTION");

}

CIRCServer.prototype.onDisconnect = function serv_disconnect(e)
{

  if ((this.parent.primServ == this) && (this.parent.stayingPower))
  { /* fell off primary server, reconnect to any host in the serverList */
	var ev = new CEvent ("network", "do-connect", this.parent,
                             "onDoConnect");
	this.parent.eventPump.addEvent (ev);
  }

  return true;

}

CIRCServer.prototype.onPoll = function serv_poll(e)
{
    var lines;
    var ex;
    var d = new Date();

    if (!this.connection.isConnected)
    {
        var ev = new CEvent ("server", "disconnect", this,
                             "onDisconnect");
        ev.reason = "unknown";
        this.parent.eventPump.addEvent (ev);
        return false;
    }

    this.sendsThisRound = 0;

    if (((d - this.lastSend) >= this.MS_BETWEEN_SENDS) &&
        this.sendQueue.length > 0)                            
    {
        var s = this.sendQueue.pop();
        dd ("queued send: " + s);
        this.connection.sendData (s);
        this.lastSend = d;
    }
    
    try
    {
        line = this.connection.readData(this.READ_TIMEOUT);
    }
    catch (ex)
    {
        dd ("** Caught exception " + ex + " reading from server " +
            this.connection.name);
        if (typeof ex != "undefined")
        {
             var ev = new CEvent ("server", "disconnect", this,
                                  "onDisconnect");
             ev.reason = "error";
             ev.exception = ex;
             this.parent.eventPump.addEvent (ev);
             return false;
        }
        else
            line = ""
    }
        
    this.parent.eventPump.addEvent (new CEvent ("server", "poll", this,
                                                "onPoll"));
    if (line == "")
        return false;
    
    var incomplete = (line[line.length] != '\n');
    var lines = line.split("\n");

    if (this.savedLine)
    {
        lines[0] = this.savedLine + lines[0];
        this.savedLine = "";
    }
    
    if (incomplete)
        this.savedLine = lines.pop();
    
    for (i in lines)
    {
        var ev = new CEvent("server", "rawdata", this, "onRawData");
        ev.data = lines[i];
        this.parent.eventPump.addEvent (ev);
    }
    
    return true;
    
}

/*
 * onRawData begins shaping the event by parsing the IRC message at it's
 * simplest level.  After onRawData, the event will have the following
 * properties:
 * name           value
 *
 * set............"server"
 * type..........."parsedata"
 * destMethod....."onParsedData"
 * destObject.....server (this)
 * server.........server (this)
 * connection.....CBSConnection (this.connection)
 * source.........the <prefix> of the message (if it exists)
 * user...........user object initialized with data from the message <prefix>
 * params.........array containing the <middle> parameters of the message
 * code...........the first <middle> parameter (most messages have this)
 * meat...........the <trailing> parameter of the message
 *
 * See Section 2.3.1 of RFC 1459 for details on <prefix>, <middle> and
 * <trailing> tokens.
 */
CIRCServer.prototype.onRawData = function serv_onRawData(e)
{
    var ary;
    var l = e.data;

    if (l[0] == ":")
    {
        ary = l.match (/:(\S+)\s(.*)/);
        e.source = ary[1];
        l = ary[2];
        ary = e.source.match (/(\S+)!(\S+)@(.*)/);
        if (ary)
        {
            e.user = new CIRCUser(this, ary[1], ary[2], ary[3]);
        }
        else
        {
            ary = e.source.match (/(\S+)@(.*)/);
            if (ary)
            {
                e.user = new CIRCUser(this, "", ary[1], ary[2]);
            }
        }
    }

    e.server = this;

    var sep = l.indexOf(":");
    if (sep != -1)
        e.params = l.substr(0, sep).split(" ");
    else
        e.params = l.split(" ");
    e.code = e.params[0].toUpperCase();
    if (e.params[e.params.length - 1] == "")
        e.params.length--;
    e.meat = l.substr (sep + 1, l.length);

    e.type = "parseddata";
    e.destObject = this;
    e.destMethod = "onParsedData";
    
    return true;
    
}

/*
 * onParsedData forwards to next event, based on |e.code|
 */
CIRCServer.prototype.onParsedData = function serv_onParsedData(e)
{

    e.type = e.code.toLowerCase();
    e.destObject = this;
    e.destMethod = "on" + e.code[0].toUpperCase() +
        e.code.substr (1, e.code.length).toLowerCase();

    return true;
    
}

/* User changed topic */
CIRCServer.prototype.onTopic = function serv_topic (e)
{

    e.channel = new CIRCChannel (this, e.params[1]);
    e.channel.topicBy = e.user.nick;
    e.channel.topicDate = new Date();
    e.channel.topic = e.meat;
    e.destObject = e.channel;
    e.set = "channel";

    return true;
    
}

/* TOPIC reply */
CIRCServer.prototype.on332 = function serv_332 (e)
{

    e.channel = new CIRCChannel (this, e.params[2]);
    e.channel.topic = e.meat;
    e.destObject = e.channel;
    e.set = "channel";

    return true;
    
}

/* topic information */
CIRCServer.prototype.on333 = function serv_333 (e)
{

    e.channel = new CIRCChannel (this, e.params[2]);
    e.channel.topicBy = e.params[3];
    e.channel.topicDate = new Date(Number(e.params[4]) * 1000);
    e.destObject = e.channel;
    e.set = "channel";

    return true;

}

/* name reply */
CIRCServer.prototype.on353 = function serv_353 (e)
{
    
    e.channel = new CIRCChannel (this, e.params[3]);
    if (e.channel.usersStable)
        e.channel.users = new Object();
    
    e.destObject = e.channel;
    e.set = "channel";

    var nicks = e.meat.split (" ");

    for (var n in nicks)
    {
        nick = nicks[n];
        if (nick == "")
            break;
        
        switch (nick[0])
        {
            case "@":
                e.user = new CIRCChanUser (e.channel,
                                           nick.substr(1, nick.length),
                                           true, (void 0));
                break;
            
            case "+":
                e.user = new CIRCChanUser (e.channel,
                                           nick.substr(1, nick.length),
                                           (void 0), true);
                break;
            
            default:
                e.user = new CIRCChanUser (e.channel, nick);
                break;
        }

    }

    return true;
    
}

/* end of names */
CIRCServer.prototype.on366 = function serv_366 (e)
{

    e.channel = new CIRCChannel (this, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    e.channel.usersStable = true;

    return true;
    
}    

/* channel time stamp? */
CIRCServer.prototype.on329 = function serv_329 (e)
{

    e.channel = new CIRCChannel (this, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    e.channel.timeStamp = new Date (Number(e.params[3]) * 1000);
    
    return true;
    
}

/* channel mode reply */
CIRCServer.prototype.on324 = function serv_324 (e)
{

    e.channel = new CIRCChannel (this, e.params[2]);
    e.destObject = this;
    e.type = "chanmode";
    e.destMethod = "onChanMode";

    return true;

}

/* user changed the mode */
CIRCServer.prototype.onMode = function serv_mode (e)
{

    e.destObject = this;
    
    if ((e.params[1][0] == "#") || (e.params[1][0] == "&"))
    {
        e.channel = new CIRCChannel (this, e.params[1]);
        if (e.user)
            e.user = new CIRCChanUser (e.channel, e.user.nick);
        e.type = "chanmode";
        e.destMethod = "onChanMode";
    }
    else
    {
        e.type = "usermode";
        e.destMethod = "onUserMode";
    }
    
    return true;
    
}

CIRCServer.prototype.onUserMode = function serv_usermode (e)
{

    return true;
    
}

CIRCServer.prototype.onChanMode = function serv_chanmode (e)
{
    var modifier = "";
    var params_eaten = 0;
    var BASE_PARAM;

    if (e.code.toUpperCase() == "MODE")
        BASE_PARAM = 2;
    else
        if (e.code == "324")
            BASE_PARAM = 3;
        else
        {
            dd ("** INVALID CODE in ChanMode event **");
            return false;
        }

    var mode_str = e.params[BASE_PARAM];
    params_eaten++;

    for (var i = 0; i < mode_str.length ; i++)
    {
        switch (mode_str[i])
        {
            case "+":
            case "-":
                modifier = mode_str[i];
                break;

            /* user modes */
            case "o": /* operator */
                if (modifier == "+")
                {
                    var nick = e.params[BASE_PARAM + params_eaten];
                    var user = new CIRCChanUser (e.channel, nick, true);
                    params_eaten++;
                } else
                    if (modifier == "-")
                    {
                        var nick = e.params[BASE_PARAM + params_eaten];
                        var user = new CIRCChanUser (e.channel, nick, false);
                        params_eaten++;
                    }
                break;
                        
            case "v": /* voice */
                if (modifier == "+")
                {
                    var nick = e.params[BASE_PARAM + params_eaten];
                    var user = new CIRCChanUser (e.channel, nick, (void 0),
                                                 true);
                    params_eaten++;
                } else
                    if (modifier == "-")
                    {
                        var nick = e.params[BASE_PARAM + params_eaten];
                        var user = new CIRCChanUser (e.channel, nick, (void 0),
                                                     false);
                        params_eaten++;
                    }
                break;
                
            case "b": /* ban */
                var ban = e.params[BASE_PARAM + params_eaten];
                params_eaten++;

                if ((modifier == "+") &&
                    (typeof e.channel.bans[ban] == "undefined"))
                {
                    e.channel.bans[ban] = {host: ban};
                    ban_evt = new CEvent ("channel", "ban", e.channel,
                                          "onBan");
                    ban_evt.channel = e.channel;
                    ban_evt.ban = ban;
                    ban_evt.source = e.user;
                    this.parent.eventPump.addEvent (e);
                } else
                    if (modifier == "-")
                        delete e.channel.bans[ban];
                break;

                
            /* channel modes */
            case "l": /* limit */
                if (modifier == "+")
                {
                    var limit = e.params[BASE_PARAM + params_eaten];
                    params_eaten++;
                    e.channel.mode.limit = limit;
                }
                else if (modifier == "-")
                    e.channel.mode.limit = -1;
                break;

            case "k": /* key */
                var key = e.params[BASE_PARAM + params_eaten];
                params_eaten++;

                if (modifier == "+")
                    e.channel.mode.key = key;
                else if (modifier == "-")
                    e.channel.mode.key = "";
                break;

            case "m": /* moderated */
                if (modifier == "+")
                    e.channel.mode.moderated = true;
                else if (modifier == "-")
                    e.channel.mode.moderated = false;
                break;

            case "n": /* no outside messages */
                if (modifier == "+")
                    e.channel.mode.publicMessages = false;
                else if (modifier == "-")
                    e.channel.mode.publicMessages = true;
                break;

            case "t": /* topic */
                if (modifier == "+")
                    e.channel.mode.publicTopic = false;
                else if (modifier == "-")
                    e.channel.mode.publicTopic = true;
                break;
                
            case "i": /* invite */
                if (modifier == "+")
                    e.channel.mode.invite = true;
                else if (modifier == "-")
                    e.channel.mode.invite = false;
                break;

            case "s": /* secret */
                if (modifier == "+")
                    e.channel.mode.secret  = true;
                else if (modifier == "-")
                    e.channel.mode.secret = false;
                break;
                
            case "p": /* private */
                if (modifier == "+")
                    e.channel.mode.pvt = true;
                else if (modifier == "-")
                    e.channel.mode.pvt = false;
                break;
                
        }
    }

    e.destObject = e.channel;
    e.set = "channel";

    return true;

}

CIRCServer.prototype.onNick = function serv_nick (e)
{
    var new_nick = e.meat.toLowerCase();
    
    renameProperty (e.server.users, e.user.nick, new_nick);   

    for (var c in e.server.channels)
    {
        if (typeof e.server.channels[c].users[e.user.nick] != "undefined")
            renameProperty (e.server.channels[c].users, e.user.nick,
                            new_nick);

    }

    e.oldNick = e.user.nick;
    e.user.nick = new_nick;
    e.destObject = e.user;
    e.set = "user";    

    return true;
    
}

CIRCServer.prototype.onQuit = function serv_quit (e)
{

    for (var c in e.server.channels)
        delete e.server.channels[c].users[e.user.nick];

    this.users[e.user.nick].lastQuitMessage = e.meat;
    this.users[e.user.nick].lastQuitDate = new Date;

    e.reason = e.meat;
    e.destObject = e.user;
    e.set = "user";

    return true;

}

CIRCServer.prototype.onPart = function serv_part (e)
{

    e.channel = new CIRCChannel (this, e.params[1]);
    e.user = new CIRCChanUser (e.channel, e.user.nick);
    delete e.channel.users[e.user.nick];
    e.destObject = e.channel;
    e.set = "channel";

    return true;
    
}

CIRCServer.prototype.onKick = function serv_kick (e)
{

    e.channel = new CIRCChannel (this, e.params[1]);
    e.lamer = new CIRCChanUser (e.channel, e.params[2]);
    delete e.channel.users[e.lamer.nick];
    e.reason = e.meat;
    e.destObject = e.channel;
    e.set = "channel"; 

    return true;
    
}

CIRCServer.prototype.onJoin = function serv_join (e)
{

    e.channel = new CIRCChannel (this, e.meat);
    if (e.user == this.me)
        e.server.sendData ("MODE " + e.channel.name + "\n" /* +
                           "BANS " + e.channel.name + "\n" */);
    e.user = new CIRCChanUser (e.channel, e.user.nick);

    e.destObject = e.channel;
    e.set = "channel";

    return true;
    
}

CIRCServer.prototype.onPing = function serv_ping (e)
{

    /* non-queued send, so we can calcualte lag */
    this.connection.sendData ("PONG :" + e.meat + "\n");
    this.connection.sendData ("PING :" + e.meat + "\n");
    this.lastPing = this.lastPingSent = new Date();

    return true;
    
}

CIRCServer.prototype.onPong = function serv_pong (e)
{

    if (this.lastPingSent)
        this.lag = (new Date() - this.lastPingSent) / 1000;

    delete this.lastPingSent;
    
    return true;
    
}

CIRCServer.prototype.onNotice = function serv_notice (e)
{

    if (!e.user)
    {
        e.set = "network";
        e.destObject = this.parent;
        return true;
    }
        
    if ((e.params[1][0] == "#") || (e.params[1][0] == "&"))
    {
        e.channel = new CIRCChannel(this, e.params[1]);
        e.user = new CIRCChanUser (e.channel, e.user.nick);
        e.replyTo = e.channel;
        e.set = "channel";
    }
    else
    {
        e.set = "user";
        e.replyTo = e.user; /* send replys to the user who sent the message */
    }

    e.destObject = e.replyTo;

    return true;
    
}

CIRCServer.prototype.onPrivmsg = function serv_privmsg (e)
{
    
    /* setting replyTo provides a standard place to find the target for     */
    /* replys associated with this event.                                   */
    if ((e.params[1][0] == "#") || (e.params[1][0] == "&"))
    {
        e.channel = new CIRCChannel(this, e.params[1]);
        e.user = new CIRCChanUser (e.channel, e.user.nick);
        e.replyTo = e.channel;
        e.set = "channel";
    }
    else
    {
        e.set = "user";
        e.replyTo = e.user; /* send replys to the user who sent the message */
    }

    if (e.meat.search (/\01.*\01/i) == 0)
    {
        e.type = "ctcp";
        e.destMethod = "onCTCP";
        e.set = "server";
        e.destObject = this;
    }
    else
        e.destObject = e.replyTo;

    return true;
    
}

CIRCServer.prototype.onCTCP = function serv_ctcp (e)
{
    var ary = e.meat.match (/^\01(\S+)? ?(.*)\01$/i);

    if (ary == null)
        return false;

    e.CTCPData = ary[2] ? ary[2] : "";
    
    e.type = "ctcp-" + ary[1].toLowerCase();
    e.destMethod = "onCTCP" + ary[1][0].toUpperCase() +
        ary[1].substr (1, ary[1].length).toLowerCase();

    if (typeof this[e.destMethod] != "function")
    { /* if there's no place to land the event here, try to forward it */
        e.destObject = e.replyTo;
        e.set = (e.replyTo == e.user) ? "user" : "channel";
    }
    else
        e.destObject = this;

    return true;
    
}

CIRCServer.prototype.onCTCPVersion = function serv_cver (e)
{
    var lines = e.server.VERSION_RPLY.split ("\n");
    
    for (var i in lines)
        e.user.notice ("\01VERSION " + lines[i] + "\01");
    
    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";

    return true;
    
}

CIRCServer.prototype.onCTCPPing = function serv_cping (e)
{

    /* non-queued send */
    e.server.connection.sendData ("NOTICE " + e.user.nick + " :\01PING " +
                                  e.CTCPData + "\01\n");
    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";

    return true;
    
}

CIRCServer.prototype.onCTCPDcc = function serv_dcc (e)
{

    var ary = e.CTCPData.match (/(\S+)? ?(.*)/);
    
    e.DCCData = ary[2];
    e.type = "dcc-" + ary[1].toLowerCase();
    e.destMethod = "onDCC" + ary[1][0].toUpperCase() +
        ary[1].substr (1, ary[1].length).toLowerCase();

    if (typeof this[e.destMethod] != "function")
    { /* if there's no place to land the event here, try to forward it */
        e.destObject = e.replyTo;
        e.set = (e.replyTo == e.user) ? "user" : "channel";
    }
    else
        e.destObject = this;

    return true;

}

CIRCServer.prototype.onDCCChat = function serv_dccchat (e)
{
    var ary = e.DCCData.match (/(chat) (\d+) (\d+)/i);

    if (ary == null)
        return false;

    e.id = ary[2];
    e.port = ary[3];
    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";
    
    return true;
    
}

CIRCServer.prototype.onDCCSend = function serv_dccsend (e)
{

    var ary = e.DCCData.match (/(\S+) (\d+) (\d+) (\d+)/);

    if (ary == null)
        return false;
    
    e.file = ary[1];
    e.id   = ary[2];
    e.port = ary[3];
    e.size = ary[4];
    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";

    return true;
    
}

/*
 * channel
 */

function CIRCChannel (parent, name)
{

    name = name.toLowerCase();
    
    var existingChannel = parent.channels[name];

    if (typeof existingChannel == "object")
        return existingChannel;
    
    this.parent = parent;
    this.name = name;
    this.users = new Object();
    this.bans = new Object();
    this.mode = new CIRCChanMode (this);
    
    parent.channels[name] = this;

}

CIRCChannel.prototype.TYPE = "IRCChannel";
    
CIRCChannel.prototype.addUser = function chan_adduser (nick, isOp, isVoice)
{

    return new CIRCChanUser (this, nick, isOp, isVoice);
    
}

CIRCChannel.prototype.getUsersLength = function chan_userslen ()
{
    var i = 0;

    for (var p in this.users)
        i++;

    return i;
    
}

CIRCChannel.prototype.setTopic = function chan_topic (str)
{
    if ((!this.mode.publicTopic) && 
        (!this.parent.me.isOp))
        return false;
    
    str = String(str).split("\n");
    for (var i in str)
        this.parent.sendData ("TOPIC " + this.name + " :" + str[i] + "\n");
    
    return true;

}

CIRCChannel.prototype.say = function chan_say (msg)
{

    this.parent.sayTo (this.name, msg);
    
}

CIRCChannel.prototype.act = function chan_say (msg)
{

    this.parent.actTo (this.name, msg);
    
}

CIRCChannel.prototype.notice = function chan_notice (msg)
{

    this.parent.noticeTo (this.name, msg);
    
}

CIRCChannel.prototype.join = function chan_join ()
{
    
    this.parent.sendData ("JOIN " + this.name + "\n");
    return true;
    
}

CIRCChannel.prototype.part = function chan_part ()
{
    
    this.parent.sendData ("PART " + this.name + "\n");
    return true;
    
}

/*
 * channel mode
 */
function CIRCChanMode (parent)
{

    this.parent = parent;
    this.limit = -1;
    this.key = "";
    this.moderated = false;
    this.publicMessages = true;
    this.publicTopic = true;
    this.invite = false;
    this.pvt = false;
    
}

CIRCChanMode.prototype.TYPE = "IRCChanMode";
    
CIRCChanMode.prototype.setMode = function chanm_mode (modestr)
{
    if (!this.parent.users[this.parent.parent.me.nick].isOp)
        return false;

    this.parent.parent.sendData ("MODE " + this.parent.name + " " +
                                 modestr + "\n");

    return true;
    
}

CIRCChanMode.prototype.setLimit = function chanm_limit (n)
{
    if (!this.parent.users[this.parent.parent.me.nick].isOp)
        return false;

    if ((typeof n == "undefined") || (n <= 0))
        this.parent.parent.sendData ("MODE " + this.parent.name + " -l\n");
    else
        this.parent.parent.sendData ("MODE " + this.parent.name + " +l " +
                                     Number(n) + "\n");

    return true;
    
}

CIRCChanMode.prototype.lock = function chanm_lock (k)
{
    if (!this.parent.users[this.parent.parent.me.nick].isOp)
        return false;
    
    this.parent.parent.sendData ("MODE " + this.parent.name + " +k " +
                                 k + "\n");
    return true;
    
}

CIRCChanMode.prototype.unlock = function chan_unlock (k)
{
    if (!this.parent.users[this.parent.parent.me.nick].isOp)
        return false;
    
    this.parent.parent.sendData ("MODE " + this.parent.name + " -k " +
                                 k + "\n");
    return true;
    
}

CIRCChanMode.prototype.setModerated = function chan_moderate (f)
{
    if (!this.parent.users[this.parent.parent.me.nick].isOp)
        return false;

    var modifier = (f) ? "+" : "-";
    
    this.parent.parent.sendData ("MODE " + this.parent.name + " " +
                                 modifier + "m\n");
    return true;
    
}

CIRCChanMode.prototype.setPublicMessages = function chan_pmessages (f)
{
    if (!this.parent.users[this.parent.parent.me.nick].isOp)
        return false;

    var modifier = (f) ? "-" : "+";
    
    this.parent.parent.sendData ("MODE " + this.parent.name + " " +
                                 modifier + "n\n");
    return true;
    
}

CIRCChanMode.prototype.setPublicTopic = function chan_ptopic (f)
{
    if (!this.parent.users[this.parent.parent.me.nick].isOp)
        return false;

    var modifier = (f) ? "-" : "+";
    
    this.parent.parent.sendData ("MODE " + this.parent.name + " " +
                                 modifier + "t\n");
    return true;
    
}

CIRCChanMode.prototype.setInvite = function chan_invite (f)
{
    if (!this.parent.users[this.parent.parent.me.nick].isOp)
        return false;

    var modifier = (f) ? "+" : "-";
    
    this.parent.parent.sendData ("MODE " + this.parent.name + " " +
                                 modifier + "i\n");
    return true;
    
}

CIRCChanMode.prototype.setPvt = function chan_pvt (f)
{
    if (!this.parent.users[this.parent.parent.me.nick].isOp)
        return false;

    var modifier = (f) ? "+" : "-";
    
    this.parent.parent.sendData ("MODE " + this.parent.name + " " +
                                 modifier + "p\n");
    return true;
    
}

CIRCChanMode.prototype.setSecret = function chan_secret (f)
{
    if (!this.parent.users[this.parent.parent.me.nick].isOp)
        return false;

    var modifier = (f) ? "+" : "-";
    
    this.parent.parent.sendData ("MODE " + this.parent.name + " " +
                                 modifier + "p\n");
    return true;
    
}

/*
 * user
 */

function CIRCUser (parent, nick, name, host)
{
    nick = nick.toLowerCase();
    
    var existingUser = parent.users[nick];

    if (typeof existingUser == "object")
    {
        if (name) existingUser.name = name;
        if (host) existingUser.host = host;
        return existingUser;
    }

    this.parent = parent;
    this.nick = nick;
    this.name = name;
    this.host = host;

    parent.users[nick] = this;

}

CIRCUser.prototype.TYPE = "IRCUser";
    
CIRCUser.prototype.getHostMask = function usr_hostmask (pfx)
{
    pfx = (typeof pfx != "undefined") ? pfx : "*!" + this.name + "@*.";
    var idx = this.host.indexOf(".");
    if (idx == -1)
        return pfx + this.host;
    
    return (pfx + this.host.substr(idx + 1, this.host.length));
    
}

CIRCUser.prototype.say = function usr_say (msg)
{

    this.parent.sayTo (this.nick, msg);
    
}

CIRCUser.prototype.notice = function usr_notice (msg)
{

    this.parent.noticeTo (this.nick, msg);
    
}

CIRCUser.prototype.act = function usr_act (msg)
{

    this.parent.actTo (this.nick, msg);
    
}
    
/*
 * channel user
 */
function CIRCChanUser (parent, nick, isOp, isVoice)
{
    nick = nick.toLowerCase();    

    var existingUser = parent.users[nick];

    if (typeof existingUser != "undefined")
    {
        if (typeof isOp != "undefined") existingUser.isOp = isOp;
        if (typeof isVoice != "undefined") existingUser.isVoice = isVoice;
        return existingUser;
    }
        
    protoUser = new CIRCUser (parent.parent, nick);
        
    this.__proto__ = protoUser;
    this.setOp = cusr_setop;
    this.setVoice = cusr_setvoice;
    this.setBan = cusr_setban;
    this.kick = cusr_kick;
    this.kickBan = cusr_kban;
    this.say = cusr_say;
    this.notice = cusr_notice;
    this.act = cusr_act;
    this.parent = parent;
    this.isOp = (typeof isOp != "undefined") ? isOp : false;
    this.isVoice = (typeof isVoice != "undefined") ? isVoice : false;
    this.TYPE = "IRCChanUser";
    
    parent.users[nick] = this;

}

function cusr_setop (f)
{
    var server = this.parent.parent;
    var me = server.me;

    if (!this.parent.users[me.nick].isOp)
        return false;

    var modifier = (f) ? " +o " : " -o ";
    server.sendData("MODE " + this.parent.name + modifier + this.nick + "\n");

    return true;
    
}

function cusr_setvoice (f)
{
    var server = this.parent.parent;
    var me = server.me;

    if (!this.parent.users[me.nick].isOp)
        return false;
    
    var modifier = (f) ? " +v " : " -v ";
    server.sendData("MODE " + this.parent.name + modifier + this.nick + "\n");

    return true;
    
}

function cusr_kick (reason)
{
    var server = this.parent.parent;
    var me = server.me;

    reason = (typeof reason != "undefined") ? reason : this.nick;
    if (!this.parent.users[me.nick].isOp)
        return false;
    
    server.sendData("KICK " + this.parent.name + " " + this.nick + " :" +
                    reason + "\n");

    return true;
    
}

function cusr_setban (f)
{
    var server = this.parent.parent;
    var me = server.me;

    if (!this.parent.users[me.nick].isOp)
        return false;

    if (!this.host)
        return false;

    var modifier = (f) ? " +b " : " -b ";
    modifier += this.getHostMask() + " ";
    
    server.sendData("MODE " + this.parent.name + modifier + "\n");

    return true;
    
}

function cusr_kban (reason)
{
    var server = this.parent.parent;
    var me = server.me;

    if (!this.parent.users[me.nick].isOp)
        return false;

    if (!this.host)
        return false;

    reason = (typeof reason != "undefined") ? reason : this.nick;
    var modifier = " -o+b " + this.nick + " " + this.getHostMask() + " ";
    
    server.sendData("MODE " + this.parent.name + modifier + "\n" +
                    "KICK " + this.parent.name + " " + this.nick + " :" +
                    reason + "\n");

    return true;
    
}

function cusr_say (msg)
{

    this.__proto__.say (msg);

}

function cusr_notice (msg)
{

    this.__proto__.notice (msg);
    
}

function cusr_act (msg)
{

    this.__proto__.act (msg);
    
}
