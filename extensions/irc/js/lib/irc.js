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
 * The Original Code is JSIRC Library.
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

const JSIRC_ERR_NO_SOCKET = "JSIRCE:NS";
const JSIRC_ERR_EXHAUSTED = "JSIRCE:E";

function userIsMe (user)
{
    
    switch (user.TYPE)
    {
        case "IRCUser":
            return (user == user.parent.me);
            break;
            
        case "IRCChanUser":
            return (user.__proto__ == user.parent.parent.me);
            break;
            
        default:
            return false;
            
    }

    return false;
}

/*
 * Attached to event objects in onRawData
 */
function decodeParam(number, charsetOrObject)
{
    if (!charsetOrObject)
        charsetOrObject = this.currentObject;
    
    var rv = toUnicode(this.params[number], charsetOrObject);
    
    return rv;
}

/*
 * irc network
 */
function CIRCNetwork (name, serverList, eventPump)
{
    this.name = name;
    this.displayName = name;
    this.servers = new Object();
    this.serverList = new Array();
    this.ignoreList = new Object();
    this.ignoreMaskCache = new Object();
    this.connecting = false;
    
    for (var i = 0; i < serverList.length; ++i)
    {
        var server = serverList[i];
        var password = ("password" in server) ? server.password : null;
        this.serverList.push(new CIRCServer(this, server.name, server.port,
                                            password));
    }
        
    this.eventPump = eventPump;
    if ("onInit" in this)
        this.onInit();
}

/** Clients should override this stuff themselves **/
CIRCNetwork.prototype.INITIAL_NICK = "js-irc";
CIRCNetwork.prototype.INITIAL_NAME = "INITIAL_NAME";
CIRCNetwork.prototype.INITIAL_DESC = "INITIAL_DESC";
/* set INITIAL_CHANNEL to "" if you don't want a primary channel */
CIRCNetwork.prototype.INITIAL_CHANNEL = "#jsbot"; 
CIRCNetwork.prototype.INITIAL_UMODE = "+iw";

CIRCNetwork.prototype.MAX_CONNECT_ATTEMPTS = 5;
CIRCNetwork.prototype.stayingPower = false; 

CIRCNetwork.prototype.TYPE = "IRCNetwork";

CIRCNetwork.prototype.getURL =
function net_geturl(target)
{
    if (this.serverList.length == 1 &&
        this.serverList[0].name == this.name &&
        this.serverList[0].port != 6667)
    {
        return this.serverList[0].getURL(target);
    }
    
    if (!target)
        target = "";
    
    return "irc://" + ecmaEscape(this.name) + "/" + target;
}

CIRCNetwork.prototype.getUser =
function net_getuser (nick) 
{
    if ("primServ" in this && this.primServ)
        return this.primServ.getUser(nick);
    
    return null;
}

CIRCNetwork.prototype.addServer =
function net_addsrv(host, port, password)
{
    this.serverList.push(new CIRCServer(this, host, port, password));
}

CIRCNetwork.prototype.connect =
function net_connect()
{
    if ("primServ" in this && this.primServ.isConnected)
        return;

    this.connectAttempt = 0;
    this.nextHost = 0;
    var ev = new CEvent("network", "do-connect", this, "onDoConnect");
    this.eventPump.addEvent(ev);
}

CIRCNetwork.prototype.quit =
function net_quit (reason)
{
    if (this.isConnected())
        this.primServ.logout(reason);
}

/*
 * Handles a request to connect to a primary server.
 */
CIRCNetwork.prototype.onDoConnect =
function net_doconnect(e)
{
    var c;
    
    if ("primServ" in this && this.primServ.isConnected)
        return true;

    var ev;
    
    if (this.connectAttempt++ >= this.MAX_CONNECT_ATTEMPTS)
    {
        ev = new CEvent ("network", "error", this, "onError");
        ev.server = this;
        ev.debug = "Connection attempts exhausted, giving up.";
        ev.errorCode = JSIRC_ERR_EXHAUSTED;
        this.eventPump.addEvent (ev);        
        return false;
    }

    this.connecting = true; /* connection is considered "made" when serve
                             * sends a 001 message (see server.on001) */

    var host = this.nextHost++;
    if (host >= this.serverList.length)
    {
        this.nextHost = 1;
        host = 0;
    }

    ev = new CEvent ("network", "startconnect", this, "onStartConnect");
    ev.debug = "Connecting to " + this.serverList[host].name + ":" +    
               this.serverList[host].port + ", attempt " + this.connectAttempt +
               " of " + this.MAX_CONNECT_ATTEMPTS + "...";
    ev.host = this.serverList[host].hostname;
    ev.port = this.serverList[host].port;
    ev.connectAttempt = this.connectAttempt;
    this.eventPump.addEvent (ev);

    var connected = false;    

    if (!this.serverList[host].connect())
    {
        /* connect failed, try again  */
        ev = new CEvent ("network", "do-connect", this, "onDoConnect");
        this.eventPump.addEvent (ev);
    }

    return true;
}

CIRCNetwork.prototype.isConnected = 
function net_connected (e)
{
    return ("primServ" in this && this.primServ.isConnected);   
}

CIRCNetwork.prototype.ignore =
function net_ignore (hostmask)
{
    var input = getHostmaskParts(hostmask);
    
    if (input.mask in this.ignoreList)
        return false;
    
    this.ignoreList[input.mask] = input;
    this.ignoreMaskCache = new Object();
    return true;
}

CIRCNetwork.prototype.unignore =
function net_ignore (hostmask)
{
    var input = getHostmaskParts(hostmask);
    
    if (!(input.mask in this.ignoreList))
        return false;
    
    delete this.ignoreList[input.mask];
    this.ignoreMaskCache = new Object();
    return true;
}

/*
 * irc server
 */ 
function CIRCServer (parent, hostname, port, password)
{
    var serverName = hostname + ":" + port;

    var s;
    if (serverName in parent.servers)
    {
        s = parent.servers[serverName];
    }
    else
    {
        s = this;
        s.channels = new Object();
        s.users = new Object();
    }
    
    s.name = serverName;
    s.hostname = hostname;
    s.port = port;
    s.parent = parent;
    s.password = password;
    s.connection = null;
    s.isConnected = false;
    s.sendQueue = new Array();
    s.lastSend = new Date("1/1/1980");
    s.lastPingSent = null;
    s.lastPing = null;
    s.sendsThisRound = 0;
    s.savedLine = "";
    s.lag = -1;    
    s.usersStable = true;
    s.supports = null;
    s.channelTypes = null;
    s.channelModes = null;
    s.userModes = null;

    parent.servers[serverName] = s;
    if ("onInit" in s)
        s.onInit();
    return s;
}

CIRCServer.prototype.MAX_LINES_PER_SEND = 0; /* unlimited */
CIRCServer.prototype.MS_BETWEEN_SENDS = 1500;
CIRCServer.prototype.READ_TIMEOUT = 100;
CIRCServer.prototype.TOO_MANY_LINES_MSG = "\01ACTION has said too much\01";
CIRCServer.prototype.VERSION_RPLY = "JS-IRC Library v0.01, " +
    "Copyright (C) 1999 Robert Ginda; rginda@ndcico.com";
CIRCServer.prototype.DEFAULT_REASON = "no reason";

CIRCServer.prototype.TYPE = "IRCServer";

CIRCServer.prototype.toLowerCase =
function serv_tolowercase(str)
{
    /* This is an implementation that lower-cases strings according to the 
     * prevailing CASEMAPPING setting for the server. Values for this are:
     *   
     *   o  "ascii": The ASCII characters 97 to 122 (decimal) are defined as
     *      the lower-case characters of ASCII 65 to 90 (decimal).  No other
     *      character equivalency is defined.
     *   o  "strict-rfc1459": The ASCII characters 97 to 125 (decimal) are
     *      defined as the lower-case characters of ASCII 65 to 93 (decimal).
     *      No other character equivalency is defined.
     *   o  "rfc1459": The ASCII characters 97 to 126 (decimal) are defined as
     *      the lower-case characters of ASCII 65 to 94 (decimal).  No other
     *      character equivalency is defined.
     * 
     */
     
     function replaceFunction(chr)
     {
         return String.fromCharCode(chr.charCodeAt(0) + 32);
     }
     
     var mapping = "rfc1459";
     if (this.supports)
         mapping = this.supports.casemapping;
     
     /* NOTE: There are NO breaks in this switch. This is CORRECT.
      * Each mapping listed is a super-set of those below, thus we only
      * transform the extra characters, and then fall through.
      */
     switch (mapping)
     {
         case "rfc1459":
             str = str.replace(/\^/g, replaceFunction);
         case "strict-rfc1459":
             str = str.replace(/[\[\\\]]/g, replaceFunction);
         case "ascii":
             str = str.replace(/[A-Z]/g, replaceFunction);
     }
     return str;
}

CIRCServer.prototype.getURL =
function serv_geturl(target)
{
    if (!target)
        target = "";
    
    var url = "irc://" + this.hostname;
    if (this.port != 6667)
        url += ":" + this.port;
    url += "/" + target;
    if (url.indexOf(".") == -1)
        url += ",isserver";
    if (this.password)
        url += ",needpass";

    return url;
}

CIRCServer.prototype.getUser =
function chan_getuser (nick) 
{
    nick = this.toLowerCase(nick);

    if (nick in this.users)
        return this.users[nick];

    return null;
}

CIRCServer.prototype.connect = 
function serv_connect (password)
{
    try
    {
        this.connection = new CBSConnection();
    }
    catch (ex)
    {
        ev = new CEvent ("server", "error", this, "onError");
        ev.server = this;
        ev.debug = "Couldn't create socket :" + ex;
        ev.errorCode = JSIRC_ERR_NO_SOCKET;
        ev.exception = ex;
        this.parent.eventPump.addEvent (ev);
        return false;
    }

    if (this.connection.connect(this.hostname, this.port, null, true, null))
    {
        var ev = new CEvent("server", "connect", this, "onConnect");

        if (password)
            this.password = password;
                
        ev.server = this;
        this.parent.eventPump.addEvent (ev);
        this.isConnected = true;

        if (jsenv.HAS_NSPR_EVENTQ)
            this.connection.startAsyncRead(this);
        else
            s.parent.eventPump.addEvent(new CEvent ("server", "poll", s,
                                                    "onPoll"));
    }

    return true;
}

/*
 * What to do when the client connects to it's primary server
 */
CIRCServer.prototype.onConnect = 
function serv_onconnect (e)
{
    this.parent.primServ = e.server;
    this.login(this.parent.INITIAL_NICK, this.parent.INITIAL_NAME,
               this.parent.INITIAL_DESC);
    return true;
}

CIRCServer.prototype.onStreamDataAvailable = 
function serv_sda (request, inStream, sourceOffset, count)
{
    var ev = new CEvent ("server", "data-available", this,
                         "onDataAvailable");

    ev.line = this.connection.readData(0, count);
    /* route data-available as we get it.  the data-available handler does
     * not do much, so we can probably get away with this without starving
     * the UI even under heavy input traffic.
     */
    this.parent.eventPump.routeEvent (ev);
}

CIRCServer.prototype.onStreamClose = 
function serv_sockdiscon(status)
{
    var ev = new CEvent ("server", "disconnect", this, "onDisconnect");
    ev.server = this;
    ev.disconnectStatus = status;
    this.parent.eventPump.addEvent (ev);
}

    
CIRCServer.prototype.flushSendQueue =
function serv_flush()
{
    this.sendQueue.length = 0;
    dd("sendQueue flushed.");

    return true;
}

CIRCServer.prototype.login =
function serv_login(nick, name, desc)
{
    nick = nick.replace(" ", "_");
    name = name.replace(" ", "_");

    if (!nick)
        nick = "nick";
    
    if (!name)
        name = nick;
    
    if (!desc)
        desc = nick;
    
    this.me = new CIRCUser(this, nick, name);
    if (this.password)
       this.sendData("PASS " + this.password + "\n");
    this.sendData("NICK " + nick + "\n");
    this.sendData("USER " + name + " * * :" +
                  fromUnicode(desc, this) + "\n");
}

CIRCServer.prototype.logout =
function serv_logout(reason)
{
    if (reason == null || typeof reason == "undefined")
        reason = this.DEFAULT_REASON;

    this.quitting = true;

    this.connection.sendData("QUIT :" + 
                             fromUnicode(reason, this.parent) + "\n");
    this.connection.disconnect();
}

CIRCServer.prototype.addTarget =
function serv_addtarget(name)
{
    if (arrayIndexOf(this.channelTypes, name[0]) != -1) {
        return this.addChannel(name);
    } else {
        return this.addUser(name);
    }
}

CIRCServer.prototype.addChannel =
function serv_addchan(unicodeName, charset)
{
    return new CIRCChannel(this, fromUnicode(unicodeName, charset),
                           unicodeName);
}
    
CIRCServer.prototype.addUser =
function serv_addusr (nick, name, host)
{
    return new CIRCUser(this, nick, name, host);
}

CIRCServer.prototype.getChannelsLength =
function serv_chanlen()
{
    var i = 0;

    for (var p in this.channels)
        i++;

    return i;
}

CIRCServer.prototype.getUsersLength =
function serv_chanlen()
{
    var i = 0;
    
    for (var p in this.users)
        i++;

    return i;
}

CIRCServer.prototype.sendData =
function serv_senddata (msg)
{
    this.queuedSendData (msg);
}

CIRCServer.prototype.queuedSendData =
function serv_senddata (msg)
{
    if (this.sendQueue.length == 0)
        this.parent.eventPump.addEvent (new CEvent ("server", "senddata",
                                                    this, "onSendData"));
    arrayInsertAt (this.sendQueue, 0, new String(msg));
}

/*
 * Takes care not to let more than MAX_LINES_PER_SEND lines out per
 * cycle.  Cycle's are defined as the time between onPoll calls.
 */
CIRCServer.prototype.messageTo =
function serv_messto (code, target, msg, ctcpCode)
{
    var lines = String(msg).split ("\n");
    var sendable = 0, i;
    var pfx = "", sfx = "";

    if (this.MAX_LINES_PER_SEND &&
        this.sendsThisRound > this.MAX_LINES_PER_SEND)
        return false;

    if (ctcpCode)
    {
        pfx = "\01" + ctcpCode;
        sfx = "\01";
    }

    for (i in lines)
        if ((lines[i] != "") || ctcpCode) sendable++;

    for (i in lines)
    {
        if (this.MAX_LINES_PER_SEND && (
            ((this.sendsThisRound == this.MAX_LINES_PER_SEND - 1) &&
             (sendable > this.MAX_LINES_PER_SEND)) ||
            this.sendsThisRound == this.MAX_LINES_PER_SEND))
        {
            this.sendData ("PRIVMSG " + target + " :" +
                           this.TOO_MANY_LINES_MSG + "\n");
            this.sendsThisRound++;
            return true;
        }
            
        if ((lines[i] != "") || ctcpCode)
        {
            var line = code + " " + target + " :" + pfx;
            this.sendsThisRound++;
            if (lines[i] != "")
            {
                if (ctcpCode)
                    line += " ";
                line += lines[i] + sfx;
            }
            else
                line += sfx;
            //dd ("-*- irc sending '" +  line + "'");
            this.sendData(line + "\n");
        }
        
    }

    return true;        
}

CIRCServer.prototype.sayTo = 
function serv_sayto (target, msg)
{
    this.messageTo("PRIVMSG", target, msg);
}

CIRCServer.prototype.noticeTo = 
function serv_noticeto (target, msg)
{
    this.messageTo("NOTICE", target, msg);
}

CIRCServer.prototype.actTo = 
function serv_actto (target, msg)
{
    this.messageTo("PRIVMSG", target, msg, "ACTION");
}

CIRCServer.prototype.ctcpTo = 
function serv_ctcpto (target, code, msg, method)
{
    msg = msg || "";
    method = method || "PRIVMSG";
    
    code = code.toUpperCase();
    if (code == "PING" && !msg)
        msg = Number(new Date());
    this.messageTo(method, target, msg, code);
}

CIRCServer.prototype.updateLagTimer = 
function serv_uptimer() 
{
    this.connection.sendData("PING :LAGTIMER\n");
    this.lastPing = this.lastPingSent = new Date();
}

/**
 * Abstracts the whois command.
 *
 * @param target        intended user(s).
 */
CIRCServer.prototype.whois = 
function serv_whois (target) 
{
    this.sendData ("WHOIS " + target + "\n");
}

CIRCServer.prototype.onDisconnect = 
function serv_disconnect(e)
{
    if ((this.parent.connecting) ||
        /* fell off while connecting, try again */
        (this.parent.primServ == this) &&
        (!("quitting" in this) && this.parent.stayingPower))
    { /* fell off primary server, reconnect to any host in the serverList */
	var ev = new CEvent ("network", "do-connect", this.parent,
                             "onDoConnect");
	this.parent.eventPump.addEvent (ev);
    }

    e.server = this;
    e.set = "network";
    e.destObject = this.parent;

    for (var c in this.channels)
    {
        this.channels[c].users = new Object();
        this.channels[c].active = false;
    }

    this.connection = null;
    this.isConnected = false;
    
    delete this.quitting;
        
    return true;
}

CIRCServer.prototype.onSendData =
function serv_onsenddata (e)
{
    if (!this.isConnected)
    {
        dd ("Can't send to disconnected socket");
        this.flushSendQueue();
        return false;
    }
        
    var d = new Date();

    this.sendsThisRound = 0;

    if (((d - this.lastSend) >= this.MS_BETWEEN_SENDS) &&
        this.sendQueue.length > 0)                            
    {
        var s = this.sendQueue.pop();  
      
        if (s)
        {
            //dd ("queued send: " + s);
            this.connection.sendData (s);
            this.lastSend = d;
        }

    }
    else
        this.parent.eventPump.addEvent (new CEvent ("event-pump", "yield",
                                                    null, ""));

    if (this.sendQueue.length > 0)
        this.parent.eventPump.addEvent (new CEvent ("server", "senddata",
                                                    this, "onSendData"));
    return true;
}

CIRCServer.prototype.onPoll = 
function serv_poll(e)
{
    var lines;
    var ex;
    var ev;
    
    try
    {
        line = this.connection.readData(this.READ_TIMEOUT);
    }
    catch (ex)
    {
        dd ("*** Caught exception " + ex + " reading from server " +
            this.hostname);
        if (jsenv.HAS_RHINO && (ex instanceof java.lang.ThreadDeath))
        {
            dd("### catching a ThreadDeath");
            throw(ex);
        }
        else
        {
            ev = new CEvent ("server", "disconnect", this, "onDisconnect");
            ev.server = this;
            ev.reason = "error";
            ev.exception = ex;
            this.parent.eventPump.addEvent (ev);
            return false;
        }
    }
        
    this.parent.eventPump.addEvent (new CEvent ("server", "poll", this,
                                                "onPoll"));

    if (line)
    {
        ev = new CEvent ("server", "data-available", this, "onDataAvailable");
        ev.line = line;
        this.parent.eventPump.addEvent (ev);
    }

    return true;
}

CIRCServer.prototype.onDataAvailable = 
function serv_ppline(e)
{
    var line = e.line;
    
    if (line == "")
        return false;
    
    var incomplete = (line[line.length - 1] != '\n');
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
        ev.data = lines[i].replace(/\r/g, "");
        if (ev.data)
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
 * params.........array containing the parameters of the message
 * code...........the first parameter (most messages have this)
 *
 * See Section 2.3.1 of RFC 1459 for details on <prefix>, <middle> and
 * <trailing> tokens.
 */
CIRCServer.prototype.onRawData = 
function serv_onRawData(e)
{
    function makeMaskRegExp(text)
    {
        function escapeChars(c)
        {
            if (c == "*")
                return ".*";
            if (c == "?")
                return ".";
            return "\\" + c;
        }
        // Anything that's not alpha-numeric gets escaped.
        // "*" and "?" are 'escaped' to ".*" and ".".
        // Optimisation; * translates as 'match all'.
        return new RegExp("^" + text.replace(/[^\w\d]/g, escapeChars) + "$", "i");
    };
    function hostmaskMatches(user, mask)
    {
        // Need to match .nick, .user, and .host.
        if (!("nickRE" in mask))
        {
            // We cache all the regexp objects, but use null if the term is 
            // just "*", so we can skip having the object *and* the .match
            // later on.
            if (mask.nick == "*")
                mask.nickRE = null;
            else
                mask.nickRE = makeMaskRegExp(mask.nick);
            
            if (mask.user == "*")
                mask.userRE = null;
            else
                mask.userRE = makeMaskRegExp(mask.user);
            
            if (mask.host == "*")
                mask.hostRE = null;
            else
                mask.hostRE = makeMaskRegExp(mask.host);
        }
        if ((!mask.nickRE || user.nick.match(mask.nickRE)) && 
            (!mask.userRE || user.name.match(mask.userRE)) && 
            (!mask.hostRE || user.host.match(mask.hostRE)))
            return true;
        return false;
    };
    
    var ary;
    var l = e.data;

    if (l.length == 0)
    {
        dd ("empty line on onRawData?");
        return false;
    }
    
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
    
    e.ignored = false;
    if (("user" in e) && e.user && ("ignoreList" in this.parent))
    {
        // Assumption: if "ignoreList" is in this.parent, we assume that:
        //   a) it's an array.
        //   b) ignoreMaskCache also exists, and
        //   c) it's an array too.
        
        if (!(e.source in this.parent.ignoreMaskCache))
        {
            for (var m in this.parent.ignoreList)
            {
                if (hostmaskMatches(e.user, this.parent.ignoreList[m]))
                {
                    e.ignored = true;
                    break;
                }
            }
            /* Save this exact source in the cache, with results of tests. */
            this.parent.ignoreMaskCache[e.source] = e.ignored;
        }
        else
        {
            e.ignored = this.parent.ignoreMaskCache[e.source];
        }
    }

    e.server = this;

    var sep = l.indexOf(" :");

    if (sep != -1) /* <trailing> param, if there is one */
    {
        var trail = l.substr (sep + 2, l.length);
        e.params = l.substr(0, sep).split(" ");
        e.params[e.params.length] = trail;
    }
    else
    {
        e.params = l.split(" ");
    }

    e.decodeParam = decodeParam;
    e.code = e.params[0].toUpperCase();
    
    // Ignore all Privmsg and Notice messages here.
    if (e.ignored && ((e.code == "PRIVMSG") || (e.code == "NOTICE")))
        return true;

    e.type = "parseddata";
    e.destObject = this;
    e.destMethod = "onParsedData";
    
    return true;
}

/*
 * onParsedData forwards to next event, based on |e.code|
 */
CIRCServer.prototype.onParsedData = 
function serv_onParsedData(e)
{
    e.type = this.toLowerCase(e.code);
    if (!e.code[0])
    {
        dd (dumpObjectTree (e));
        return false;
    }
    
    e.destMethod = "on" + e.code[0].toUpperCase() +
        e.code.substr (1, e.code.length).toLowerCase();

    if (typeof this[e.destMethod] == "function")
        e.destObject = this;
    else if (typeof this["onUnknown"] == "function")
        e.destMethod = "onUnknown";
    else if (typeof this.parent[e.destMethod] == "function")
    {
        e.set = "network";
        e.destObject = this.parent;
    }
    else
    {
        e.set = "network";
        e.destObject = this.parent;
        e.destMethod = "onUnknown";
    }

    return true;
}

/* User changed topic */
CIRCServer.prototype.onTopic = 
function serv_topic (e)
{
    e.channel = new CIRCChannel(this, e.params[1]);
    e.channel.topicBy = e.user.properNick;
    e.channel.topicDate = new Date();
    e.channel.topic = toUnicode(e.params[2], e.channel);
    e.destObject = e.channel;
    e.set = "channel";

    return true;
}

/* Successful login */
CIRCServer.prototype.on001 =
function serv_001 (e)
{
    this.parent.connectAttempt = 0;
    this.parent.connecting = false;

    /* servers wont send a nick change notification if user was forced
     * to change nick while logging in (eg. nick already in use.)  We need
     * to verify here that what the server thinks our name is, matches what
     * we think it is.  If not, the server wins.
     */
    if (e.params[1] != e.server.me.properNick)
    {
        renameProperty (e.server.users, e.server.me.nick,
                        this.toLowerCase(e.params[1]));
        e.server.me.changeNick(e.params[1]);
    }
    
    /* Set up supports defaults here.
     * This is so that we don't waste /huge/ amounts of RAM for the network's 
     * servers just because we know about them. Until we connect, that is.
     * These defaults are taken from the draft 005 RPL_ISUPPORTS here:
     * http://www.ietf.org/internet-drafts/draft-brocklesby-irc-isupport-02.txt
     */
    this.supports = new Object();
    this.supports.modes = 3;
    this.supports.maxchannels = 10;
    this.supports.nicklen = 9;
    this.supports.casemapping = "rfc1459";
    this.supports.channellen = 200;
    this.supports.chidlen = 5;
    /* Make sure it's possible to tell if we've actually got a 005 message. */
    this.supports.rpl_isupport = false;
    this.channelTypes = [ '#', '&' ];
    /* This next one isn't in the isupport draft, but instead is defaulting to
     * the codes we understand. It should be noted, some servers include the
     * mode characters (o, h, v) in the 'a' list, although the draft spec says
     * they should be treated as type 'b'. Luckly, in practise this doesn't 
     * matter, since both 'a' and 'b' types always take a parameter in the 
     * MODE message, and parsing is not affected. */
    this.channelModes = {
                          a: ['b'],
                          b: ['k'],
                          c: ['l'],
                          d: ['i', 'm', 'n', 'p', 's', 't']
                        };
    this.userModes = [
                       { mode: 'o', symbol: '@' },
                       { mode: 'v', symbol: '+' }
                     ];
    
    if (this.parent.INITIAL_UMODE)
    {
        e.server.sendData("mode " + e.server.me.nick + " :" +
                          this.parent.INITIAL_UMODE + "\n");
    }
    
    if (this.parent.INITIAL_CHANNEL)
    {
        this.parent.primChan = this.addChannel (this.parent.INITIAL_CHANNEL);
        this.parent.primChan.join();
    }

    this.parent.users = this.users;
    e.destObject = this.parent;
    e.set = "network";
}


/* server features */
CIRCServer.prototype.on005 =
function serv_005 (e)
{
    /* Drop params 0 and 1. */
    for (var i = 2; i < e.params.length; i++) {
        var itemStr = e.params[i];
        /* Items may be of the forms:
         *   NAME
         *   -NAME
         *   NAME=value
         * Value may be empty on occasion.
         * No value allowed for -NAME items.
         */
        var item = itemStr.match(/^(-?)([A-Z]+)(=(.*))?$/i);
        if (! item)
            continue;
        
        var name = item[2].toLowerCase();
        if (("3" in item) && item[3])
        {
            // And other items are stored as-is, though numeric items
            // get special treatment to make our life easier later.
            if (("4" in item) && item[4].match(/^\d+$/))
                this.supports[name] = Number(item[4]);
            else
                this.supports[name] = item[4];
        }
        else
        {
            // Boolean-type items stored as 'true'.
            this.supports[name] = !(("1" in item) && item[1] == "-");
        }
    }
    
    // Supported 'special' items:
    //   CHANTYPES (--> channelTypes{}), 
    //   PREFIX (--> userModes[{mode,symbol}]), 
    //   CHANMODES (--> channelModes{a:[], b:[], c:[], d:[]}).
    
    var m;
    if ("chantypes" in this.supports)
    {
        this.channelTypes = [];
        for (m = 0; m < this.supports.chantypes.length; m++)
            this.channelTypes.push( this.supports.chantypes[m] );
    }
    
    if ("prefix" in this.supports)
    {
        var mlist = this.supports.prefix.match(/^\((.*)\)(.*)$/i);
        if ((! mlist) || (mlist[1].length != mlist[2].length))
        {
            dd ("** Malformed PREFIX entry in 005 SUPPORTS message **");
        }
        else
        {
            this.userModes = [];
            for (m = 0; m < mlist[1].length; m++)
                this.userModes.push( { mode: mlist[1][m], 
                                                   symbol: mlist[2][m] } );
        }
    }
    
    if ("chanmodes" in this.supports)
    {
        var cmlist = this.supports.chanmodes.split(/,/);
        if ((!cmlist) || (cmlist.length < 4))
        {
            dd ("** Malformed CHANMODES entry in 005 SUPPORTS message **");
        }
        else
        {
            // 4 types - list, set-unset-param, set-only-param, flag.
            this.channelModes = { 
                                           a: cmlist[0].split(''), 
                                           b: cmlist[1].split(''), 
                                           c: cmlist[2].split(''), 
                                           d: cmlist[3].split('')
                                         };
        }
    }
    
    this.supports.rpl_isupport = true;
    
    e.destObject = this.parent;
    e.set = "network";
    
    return true;
}


/* whois name */
CIRCServer.prototype.on311 =
function serv_311 (e)
{
    e.user = new CIRCUser (this, e.params[2], e.params[3], e.params[4]);
    e.user.desc = e.decodeParam(6, e.user);
    e.destObject = this.parent;
    e.set = "network";
    
    this.pendingWhoisLines = e.user;
}
    
/* whois server */
CIRCServer.prototype.on312 =
function serv_312 (e)
{
    e.user = new CIRCUser (this, e.params[2]);
    e.user.connectionHost = e.params[3];

    e.destObject = this.parent;
    e.set = "network";
}

/* whois idle time */
CIRCServer.prototype.on317 =
function serv_317 (e)
{
    e.user = new CIRCUser (this, e.params[2]);
    e.user.idleSeconds = e.params[3];

    e.destObject = this.parent;
    e.set = "network";
}

/* whois channel list */
CIRCServer.prototype.on319 =
function serv_319(e)
{
    e.user = new CIRCUser(this, e.params[2]);
    
    e.destObject = this.parent;
    e.set = "network";
}

/* end of whois */
CIRCServer.prototype.on318 =
function serv_318(e)
{
    e.user = new CIRCUser(this, e.params[2]);
    
    if ("pendingWhoisLines" in this)
        delete this.pendingWhoisLines;
    
    e.destObject = this.parent;
    e.set = "network";
}

/* TOPIC reply - no topic set */
CIRCServer.prototype.on331 =
function serv_331 (e)
{
    e.channel = new CIRCChannel (this, e.params[2]);
    e.channel.topic = "";
    e.destObject = e.channel;
    e.set = "channel";

    return true;
}

/* TOPIC reply - topic set */
CIRCServer.prototype.on332 =
function serv_332 (e)
{
    e.channel = new CIRCChannel (this, e.params[2]);
    e.channel.topic = toUnicode(e.params[3], e.channel);
    e.destObject = e.channel;
    e.set = "channel";

    return true;
}

/* topic information */
CIRCServer.prototype.on333 = 
function serv_333 (e)
{
    e.channel = new CIRCChannel (this, e.params[2]);
    e.channel.topicBy = e.params[3];
    e.channel.topicDate = new Date(Number(e.params[4]) * 1000);
    e.destObject = e.channel;
    e.set = "channel";

    return true;
}

/* who reply */
CIRCServer.prototype.on352 = 
function serv_352 (e)
{
    e.user = new CIRCUser (this, e.params[6], e.params[3], e.params[4]);
    e.user.connectionHost = e.params[5];
    if (8 in e.params)
    {
        var ary = e.params[8].match(/(?:\d+\s)?(.*)/);
        e.user.desc = fromUnicode(ary[1], e.user);
    }
    
    e.destObject = this.parent;
    e.set = "network";

    return true;
}

/* end of who */
CIRCServer.prototype.on315 = 
function serv_315 (e)
{
    e.user = new CIRCUser (this, e.params[2]);
    e.destObject = this.parent;
    e.set = "network";

    return true;
}

/* name reply */
CIRCServer.prototype.on353 = 
function serv_353 (e)
{
    e.channel = new CIRCChannel (this, e.params[3]);
    if (e.channel.usersStable)
    {        
        e.channel.users = new Object();
        e.channel.usersStable = false;
    }
    
    e.destObject = e.channel;
    e.set = "channel";

    var nicks = e.params[4].split (" ");
    var mList = this.userModes;

    for (var n in nicks)
    {
        var nick = nicks[n];
        if (nick == "")
            break;
        
        var found = false;
        for (var m in mList)
        {
            if (nick[0] == mList[m].symbol)
            {
                e.user = new CIRCChanUser (e.channel,
                                           nick.substr(1, nick.length),
                                           [ mList[m].mode ]);
                found = true;
                break;
            }
        }
        if (!found)
            e.user = new CIRCChanUser (e.channel, nick, [ ]);

    }

    return true;
}

/* end of names */
CIRCServer.prototype.on366 = 
function serv_366 (e)
{
    e.channel = new CIRCChannel (this, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    e.channel.usersStable = true;

    return true;
}    

/* channel time stamp? */
CIRCServer.prototype.on329 = 
function serv_329 (e)
{
    e.channel = new CIRCChannel (this, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    e.channel.timeStamp = new Date (Number(e.params[3]) * 1000);
    
    return true;
}
    
/* channel mode reply */
CIRCServer.prototype.on324 = 
function serv_324 (e)
{
    e.channel = new CIRCChannel (this, e.params[2]);
    e.destObject = this;
    e.type = "chanmode";
    e.destMethod = "onChanMode";

    return true;
}

/* channel ban entry */
CIRCServer.prototype.on367 = 
function serv_367(e)
{
    e.channel = new CIRCChannel (this, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    e.ban = e.params[3];
    e.user = new CIRCUser(this, e.params[4]);
    e.banTime = new Date (Number(e.params[5]) * 1000);
    
    if (typeof e.channel.bans[e.ban] == "undefined")
    {
        e.channel.bans[e.ban] = {host: e.ban, user: e.user, time: e.banTime };
        var ban_evt = new CEvent("channel", "ban", e.channel, "onBan");
        ban_evt.channel = e.channel;
        ban_evt.ban = e.ban;
        ban_evt.source = e.user;
        this.parent.eventPump.addEvent(ban_evt);
    }
    
    return true;
}

/* channel ban list end */
CIRCServer.prototype.on368 = 
function serv_368(e)
{
    e.channel = new CIRCChannel (this, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    
    /* This flag is cleared in a timeout (which occurs right after the current 
     * message has been processed) so that the new event target (the channel)
     * will still have the flag set when it executes.
     */
    if ("pendingBanList" in e.channel)
        setTimeout(function() { delete e.channel.pendingBanList; }, 0);
    
    return true;
}

/* channel except entry */
CIRCServer.prototype.on348 = 
function serv_348(e)
{
    e.channel = new CIRCChannel (this, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    
    return true;
}

/* channel except list end */
CIRCServer.prototype.on349 = 
function serv_349(e)
{
    e.channel = new CIRCChannel (this, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    
    if ("pendingExceptList" in e.channel)
        setTimeout(function (){ delete e.channel.pendingExceptList; }, 0);
    
    return true;
}

/* don't have operator perms */
CIRCServer.prototype.on482 = 
function serv_482(e)
{
    e.channel = new CIRCChannel (this, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    
    /* Some servers (e.g. Hybrid) don't let you get the except list without ops,
     * so we might be waiting for this list forever otherwise.
     */
    if ("pendingExceptList" in e.channel)
        setTimeout(function (){ delete e.channel.pendingExceptList; }, 0);
    
    return true;
}

/* userhost reply */
CIRCServer.prototype.on302 = 
function serv_302(e)
{
    var list = e.params[2].split(/\s+/);
    
    for (var i = 0; i < list.length; i++)
    {
        //  <reply> ::= <nick>['*'] '=' <'+'|'-'><hostname>
        // '*' == IRCop. '+' == here, '-' == away.
        var data = list[i].match(/^(.*)(\*?)=([-+])(.*)@(.*)$/);
        if (data)
            this.addUser(data[1], data[4], data[5]);
    }
    
    e.destObject = this.parent;
    e.set = "network";
    
    return true;
}

/* user changed the mode */
CIRCServer.prototype.onMode = 
function serv_mode (e)
{
    e.destObject = this;
    /* modes are not allowed in +channels -> no need to test that here.. */
    if (arrayIndexOf(this.channelTypes, e.params[1][0]) != -1)
    {
        e.channel = new CIRCChannel (this, e.params[1]);
        if ("user" in e && e.user)
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

CIRCServer.prototype.onUserMode = 
function serv_usermode (e)
{
    e.user = new CIRCUser(this, e.params[1])
    e.user.modestr = e.params[2];
    e.destObject = this.parent;
    e.set = "network";

    // usermode usually happens on connect, after the MOTD, so it's a good
    // place to kick off the lag timer.
    this.updateLagTimer();

    return true;
}

CIRCServer.prototype.onChanMode = 
function serv_chanmode (e)
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

    e.modeStr = mode_str;
    e.usersAffected = new Array();

    var nick;
    var user;
    var mList = this.userModes;
    
    for (var i = 0; i < mode_str.length ; i++)
    {
        /* Take care of modifier first. */
        if ((mode_str[i] == '+') || (mode_str[i] == '-'))
        {
            modifier = mode_str[i];
            continue;
        }
        
        var done = false;
        for (var m in mList)
        {
            if ((mode_str[i] == mList[m].mode) && (modifier != ""))
            {
                nick = e.params[BASE_PARAM + params_eaten];
                user = new CIRCChanUser (e.channel, nick, 
                                         [ modifier + mList[m].mode ]);
                params_eaten++;
                e.usersAffected.push (user);
                done = true;
                break;
            }
        }
        if (done)
            continue;
        
        switch (mode_str[i])
        {
            /* user modes */
            case "b": /* ban */
                var ban = e.params[BASE_PARAM + params_eaten];
                params_eaten++;

                if ((modifier == "+") &&
                    (typeof e.channel.bans[ban] == "undefined"))
                {
                    e.channel.bans[ban] = {host: ban};
                    var ban_evt = new CEvent ("channel", "ban", e.channel,
                                          "onBan");
                    ban_evt.channel = e.channel;
                    ban_evt.ban = ban;
                    ban_evt.source = e.user;
                    this.parent.eventPump.addEvent (e);
                }
                else
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
                else
                    if (modifier == "-")
                        e.channel.mode.limit = -1;
                break;

            case "k": /* key */
                var key = e.params[BASE_PARAM + params_eaten];
                params_eaten++;

                if (modifier == "+")
                    e.channel.mode.key = key;
                else
                    if (modifier == "-")
                        e.channel.mode.key = "";
                break;

            case "m": /* moderated */
                if (modifier == "+")
                    e.channel.mode.moderated = true;
                else
                    if (modifier == "-")
                        e.channel.mode.moderated = false;
                break;

            case "n": /* no outside messages */
                if (modifier == "+")
                    e.channel.mode.publicMessages = false;
                else
                    if (modifier == "-")
                        e.channel.mode.publicMessages = true;
                break;

            case "t": /* topic */
                if (modifier == "+")
                    e.channel.mode.publicTopic = false;
                else
                    if (modifier == "-")
                        e.channel.mode.publicTopic = true;
                break;
                
            case "i": /* invite */
                if (modifier == "+")
                    e.channel.mode.invite = true;
                else
                    if (modifier == "-")
                        e.channel.mode.invite = false;
                break;

            case "s": /* secret */
                if (modifier == "+")
                    e.channel.mode.secret  = true;
                else
                    if (modifier == "-")
                        e.channel.mode.secret = false;
                break;
                
            case "p": /* private */
                if (modifier == "+")
                    e.channel.mode.pvt = true;
                else
                    if (modifier == "-")
                        e.channel.mode.pvt = false;
                break;
                
        }
    }

    e.destObject = e.channel;
    e.set = "channel";
    return true;
}

CIRCServer.prototype.onNick = 
function serv_nick (e)
{
    var newNick = e.params[1]; 
    var newKey = this.toLowerCase(newNick);
    var oldKey = e.user.nick;
    var ev;
    
    renameProperty (this.users, oldKey, newKey);
    e.oldNick = e.user.properNick;
    e.user.changeNick(newNick);
    
    for (var c in this.channels)
    {
        if (this.channels[c].active &&
            ((oldKey in this.channels[c].users) || e.user == this.me))
        {
            var cuser = this.channels[c].users[oldKey];
            renameProperty (this.channels[c].users, oldKey, newKey);
            ev = new CEvent ("channel", "nick", this.channels[c],
                                 "onNick");
            ev.channel = this.channels[c];
            ev.user = cuser;
            ev.server = this;
            ev.oldNick = e.oldNick;
            this.parent.eventPump.addEvent(ev);
        }
        
    }

    if (e.user == this.me)
    {
        /* if it was me, tell the network about the nick change as well */
        ev = new CEvent ("network", "nick", this.parent, "onNick");
        ev.user = e.user;
        ev.server = this;
        ev.oldNick = e.oldNick;
        this.parent.eventPump.addEvent(ev);
    }

    e.destObject = e.user;
    e.set = "user";    

    return true;
}

CIRCServer.prototype.onQuit = 
function serv_quit (e)
{
    var reason = e.decodeParam(1);
    
    for (var c in e.server.channels)
    {
        if (e.server.channels[c].active &&
            e.user.nick in e.server.channels[c].users)
        {
            var ev = new CEvent ("channel", "quit", e.server.channels[c],
                                 "onQuit");
            ev.user = e.server.channels[c].users[e.user.nick];
            ev.channel = e.server.channels[c];
            ev.server = ev.channel.parent;
            ev.reason = reason;
            this.parent.eventPump.addEvent(ev);
            delete e.server.channels[c].users[e.user.nick];
        }
    }

    this.users[e.user.nick].lastQuitMessage = reason;
    this.users[e.user.nick].lastQuitDate = new Date();

    e.reason = reason;
    e.destObject = e.user;
    e.set = "user";

    return true;
}

CIRCServer.prototype.onPart = 
function serv_part (e)
{
    e.channel = new CIRCChannel (this, e.params[1]);
    e.reason = (e.params.length > 2) ? e.decodeParam(2, e.channel) : "";
    e.user = new CIRCChanUser (e.channel, e.user.nick);
    if (userIsMe(e.user))
    {
        e.channel.active = false;
        e.channel.joined = false;
    }
    e.channel.removeUser(e.user.nick);
    e.destObject = e.channel;
    e.set = "channel";

    return true;
}

CIRCServer.prototype.onKick = 
function serv_kick (e)
{
    e.channel = new CIRCChannel (this, e.params[1]);
    e.lamer = new CIRCChanUser (e.channel, e.params[2]);
    delete e.channel.users[e.lamer.nick];
    if (userIsMe(e.lamer))
    {
        e.channel.active = false;
        e.channel.joined = false;
    }
    e.reason = e.decodeParam(3, e.channel);
    e.destObject = e.channel;
    e.set = "channel"; 

    return true;
}

CIRCServer.prototype.onJoin = 
function serv_join(e)
{
    e.channel = new CIRCChannel(this, e.params[1]);
    e.user = new CIRCChanUser(e.channel, e.user.nick);
    
    if (userIsMe(e.user))
    {
        // Give us the channel mode!
        e.server.sendData("MODE " + e.channel.encodedName + "\n");
        
        // Get a full list of bans and exceptions, if supported.
        if (arrayContains(this.channelModes.a, "b"))
        {
            e.server.sendData("MODE " + e.channel.encodedName + " +b\n");
            e.channel.pendingBanList = true;
        }
        if (arrayContains(this.channelModes.a, "e"))
        {
            e.server.sendData("MODE " + e.channel.encodedName + " +e\n");
            e.channel.pendingExceptList = true;
        }
        
        /* Clean up the topic, since servers don't always send RPL_NOTOPIC
         * (no topic set) when joining a channel without a topic. In fact, 
         * the RFC even fails to mention sending a RPL_NOTOPIC after a join!
         */
        e.channel.topic = "";
        e.channel.topicBy = null;
        e.channel.topicDate = null;
        
        // And we're in!
        e.channel.active = true;
        e.channel.joined = true;
    }
    
    e.destObject = e.channel;
    e.set = "channel";
    
    return true;
}

CIRCServer.prototype.onPing = 
function serv_ping (e)
{
    /* non-queued send, so we can calcualte lag */
    this.connection.sendData("PONG :" + e.params[1] + "\n");
    this.updateLagTimer();
    e.destObject = this.parent;
    e.set = "network";

    return true;
}

CIRCServer.prototype.onPong = 
function serv_pong (e)
{
    if (e.params[2] != "LAGTIMER")
        return true;
    
    if (this.lastPingSent)
        this.lag = roundTo ((new Date() - this.lastPingSent) / 1000, 2);

    this.lastPingSent = null;
    
    e.destObject = this.parent;
    e.set = "network";

    return true;
}

CIRCServer.prototype.onNotice = 
function serv_notice (e)
{
    if (!("user" in e))
    {
        e.set = "network";
        e.destObject = this.parent;
        return true;
    }
        
    if (arrayIndexOf(this.channelTypes, e.params[1][0]) != -1)
    {
        e.channel = new CIRCChannel(this, e.params[1]);
        e.user = new CIRCChanUser (e.channel, e.user.nick);
        e.replyTo = e.channel;
        e.set = "channel";
    }
    else if (e.params[2].search (/\x01.*\x01/i) != -1)
    {
        e.type = "ctcp-reply";
        e.destMethod = "onCTCPReply";
        e.set = "server";
        e.destObject = this;
        return true;
    }
    else
    {
        e.set = "user";
        e.replyTo = e.user; /* send replys to the user who sent the message */
    }

    e.destObject = e.replyTo;

    return true;
}

CIRCServer.prototype.onPrivmsg = 
function serv_privmsg (e)
{
    /* setting replyTo provides a standard place to find the target for     */
    /* replys associated with this event.                                   */
    if (arrayIndexOf(this.channelTypes, e.params[1][0]) != -1)
    {
        e.channel = new CIRCChannel(this, e.params[1]);
        e.user = new CIRCChanUser(e.channel, e.user.nick);
        e.replyTo = e.channel;
        e.set = "channel";
    }
    else
    {
        e.set = "user";
        e.replyTo = e.user; /* send replys to the user who sent the message */
    }

    if (e.params[2].search (/\x01.*\x01/i) != -1)
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

CIRCServer.prototype.onCTCPReply = 
function serv_ctcpr (e)
{
    var ary = e.params[2].match (/^\x01(\S+) ?(.*)\x01$/i);

    if (ary == null)
        return false;

    e.CTCPData = ary[2] ? ary[2] : "";

    e.CTCPCode = ary[1].toLowerCase();
    e.type = "ctcp-reply-" + e.CTCPCode;
    e.destMethod = "onCTCPReply" + ary[1][0].toUpperCase() +
        ary[1].substr (1, ary[1].length).toLowerCase();

    if (typeof this[e.destMethod] != "function")
    { /* if there's no place to land the event here, try to forward it */
        e.destObject = this.parent;
        e.set = "network";
        
        if (typeof e.destObject[e.destMethod] != "function")
        { /* if there's no place to forward it, send it to unknownCTCP */
            e.type = "unk-ctcp-reply";
            e.destMethod = "onUnknownCTCPReply";
            if (e.destMethod in this)
            {
                e.set = "server";
                e.destObject = this;
            }
            else
            {
                e.set = "network";
                e.destObject = this.parent;
            }
        }
    }
    else
        e.destObject = this;

    return true;
}

CIRCServer.prototype.onCTCP = 
function serv_ctcp (e)
{
    var ary = e.params[2].match (/^\x01(\S+) ?(.*)\x01$/i);

    if (ary == null)
        return false;

    e.CTCPData = ary[2] ? ary[2] : "";

    e.CTCPCode = ary[1].toLowerCase();
    if (e.CTCPCode.search (/^reply/i) == 0)
    {
        dd ("dropping spoofed reply.");
        return false;
    }
    
    e.CTCPCode = toUnicode(e.CTCPCode, e.replyTo);
    e.CTCPData = toUnicode(e.CTCPData, e.replyTo);
    
    e.type = "ctcp-" + e.CTCPCode;
    e.destMethod = "onCTCP" + ary[1][0].toUpperCase() +
        ary[1].substr (1, ary[1].length).toLowerCase();

    if (typeof this[e.destMethod] != "function")
    { /* if there's no place to land the event here, try to forward it */
        e.destObject = e.replyTo;
        e.set = (e.replyTo == e.user) ? "user" : "channel";
        
        if (typeof e.replyTo[e.destMethod] != "function")
        { /* if there's no place to forward it, send it to unknownCTCP */
            e.type = "unk-ctcp";
            e.destMethod = "onUnknownCTCP";
        }
    }
    else
        e.destObject = this;

    return true;
}

CIRCServer.prototype.onCTCPClientinfo =
function serv_ccinfo (e)
{
    var clientinfo = new Array();

    for (var fname in this)
    {
        var ary = fname.match(/^onCTCP(.*)/);
        if (ary && ary[1].search(/^Reply/) == -1)
            clientinfo.push (ary[1].toUpperCase());
    }

    e.server.ctcpTo (e.user.nick, "CLIENTINFO", clientinfo.join(" "),
                     "NOTICE");

    return true;
}

CIRCServer.prototype.onCTCPAction =
function serv_cact (e)
{
    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";
}

CIRCServer.prototype.onCTCPTime = 
function serv_cping (e)
{
    e.server.ctcpTo (e.user.nick, "TIME", new Date(), "NOTICE");

    return true;
}

CIRCServer.prototype.onCTCPVersion = 
function serv_cver (e)
{
    var lines = e.server.VERSION_RPLY.split ("\n");
    
    for (var i in lines)
        e.user.notice ("\01VERSION " + lines[i] + "\01");
    
    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";

    return true;
}

CIRCServer.prototype.onCTCPPing = 
function serv_cping (e)
{
    /* non-queued send */
    e.server.connection.sendData ("NOTICE " + e.user.nick + " :\01PING " +
                                  e.CTCPData + "\01\n");
    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";

    return true;
}

CIRCServer.prototype.onCTCPDcc = 
function serv_dcc (e)
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

CIRCServer.prototype.onDCCChat = 
function serv_dccchat (e)
{
    var ary = e.DCCData.match (/(chat) (\d+) (\d+)/i);

    if (ary == null)
        return false;

    e.id = ary[2];
    // Checky longword --> dotted IP conversion.
    var host = Number(e.id).toString(16);
    e.host = Number("0x" + host.substr(0, 2)) + "." + 
             Number("0x" + host.substr(2, 2)) + "." + 
             Number("0x" + host.substr(4, 2)) + "." + 
             Number("0x" + host.substr(6, 2));
    e.port = Number(ary[3]);
    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";
    
    return true;
}

CIRCServer.prototype.onDCCSend = 
function serv_dccsend (e)
{
    var ary = e.DCCData.match(/(\S+) (\d+) (\d+) (\d+)/);
    
    /* Just for mIRC: filenames with spaces may be enclosed in double-quotes.
     * (though by default it replaces spaces with underscores, but we might as
     * well cope). */
    if (ary[1][0] == '"')
        ary = e.DCCData.match(/"(.+)" (\d+) (\d+) (\d+)/);
    
    if (ary == null)
        return false;
    
    e.file = ary[1];
    e.id   = ary[2];
    // Cheeky longword --> dotted IP conversion.
    var host = Number(e.id).toString(16);
    e.host = Number("0x" + host.substr(0, 2)) + "." + 
             Number("0x" + host.substr(2, 2)) + "." + 
             Number("0x" + host.substr(4, 2)) + "." + 
             Number("0x" + host.substr(6, 2));
    e.port = Number(ary[3]);
    e.size = Number(ary[4]);
    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";

    return true;
}

/*
 * channel
 */

function CIRCChannel (parent, encodedName, unicodeName)
{
    this.normalizedName = parent.toLowerCase(encodedName);
    this.name = this.normalizedName;

    if (this.normalizedName in parent.channels)
        return parent.channels[this.normalizedName];

    this.parent = parent;
    this.encodedName = encodedName;
    if (unicodeName)
        this.unicodeName = unicodeName;
    else
        this.unicodeName = toUnicode(encodedName, this);
    this.displayName = this.unicodeName;

    this.users = new Object();
    this.bans = new Object();
    this.mode = new CIRCChanMode (this);
    this.usersStable = true;
    /* These next two flags represent a subtle difference in state:
     *   active - in the channel, from the server's point of view.
     *   joined - in the channel, from the user's point of view.
     * e.g. parting the channel clears both, but being disconnected only 
     * clears |active| - the user still wants to be in the channel, even 
     * though they aren't physically able to until we've reconnected.
     */
    this.active = false;
    this.joined = false;
    
    this.parent.channels[this.normalizedName] = this;
    if ("onInit" in this)
        this.onInit();

    return this;
}

CIRCChannel.prototype.TYPE = "IRCChannel";
CIRCChannel.prototype.topic = "";

CIRCChannel.prototype.getURL =
function chan_geturl ()
{
    var target;
    if (this.normalizedName.match(/^#[^!+&]/))
        target = ecmaEscape(this.normalizedName.substr(1));
    else
        target = ecmaEscape(this.normalizedName);

    return this.parent.parent.getURL(target);
}

CIRCChannel.prototype.rehome =
function chan_rehome(newParent)
{
    delete this.parent.channels[this.normalizedName];
    this.parent = newParent;
    this.parent.channels[this.normalizedName] = this;
}

CIRCChannel.prototype.addUser = 
function chan_adduser (nick, modes)
{
    return new CIRCChanUser (this, nick, modes);
}

CIRCChannel.prototype.getUser =
function chan_getuser (nick) 
{
    nick = this.parent.toLowerCase(nick);

    if (nick in this.users)
        return this.users[nick];

    return null;
}

CIRCChannel.prototype.removeUser =
function chan_removeuser (nick)
{
    delete this.users[this.parent.toLowerCase(nick)]; // see ya
}

CIRCChannel.prototype.getUsersLength = 
function chan_userslen (mode)
{
    var i = 0;
    var p;
    this.opCount = 0;
    this.halfopCount = 0;
    this.voiceCount = 0;
    
    if (typeof mode == "undefined")
    {
        for (p in this.users)
        {
            if (this.users[p].isOp)
                this.opCount++;
            if (this.users[p].isHalfOp)
                this.halfopCount++;
            if (this.users[p].isVoice)
                this.voiceCount++;
            i++;
        }
    }
    else
    {
        for (p in this.users)
            if (arrayContains(this.users[p].modes, mode))
                i++;
    }
    
    return i;
}

CIRCChannel.prototype.iAmOp =
function chan_amop()
{
    return this.users[this.parent.me.nick].isOp;
}

CIRCChannel.prototype.iAmHalfOp =
function chan_amhalfop()
{
    return this.users[this.parent.me.nick].isHalfOp;
}

CIRCChannel.prototype.iAmVoice =
function chan_amvoice()
{
    return this.parent.users[this.parent.parent.me.nick].isVoice;
}

CIRCChannel.prototype.setTopic = 
function chan_topic (str)
{
    this.parent.sendData ("TOPIC " + this.encodedName + " :" + 
                          fromUnicode(str, this) + "\n");
}

CIRCChannel.prototype.say = 
function chan_say (msg)
{
    this.parent.sayTo(this.encodedName, fromUnicode(msg, this));
}

CIRCChannel.prototype.act = 
function chan_say (msg)
{
    this.parent.actTo(this.encodedName, fromUnicode(msg, this));
}

CIRCChannel.prototype.notice = 
function chan_notice (msg)
{
    this.parent.noticeTo(this.encodedName, fromUnicode(msg, this));
}

CIRCChannel.prototype.ctcp = 
function chan_ctcpto (code, msg, type)
{
    msg = msg || "";
    type = type || "PRIVMSG";
    
    this.parent.ctcpTo(this.encodedName, fromUnicode(code, this), 
                       fromUnicode(msg, this), type);
}

CIRCChannel.prototype.join = 
function chan_join (key)
{
    if (!key)
        key = "";
    
    this.parent.sendData ("JOIN " + this.encodedName + " " + key + "\n");
    return true;
}

CIRCChannel.prototype.part = 
function chan_part ()
{
    this.parent.sendData ("PART " + this.encodedName + "\n");
    this.users = new Object();
    return true;
}

/**
 * Invites a user to a channel.
 *
 * @param nick  the user name to invite.
 */
CIRCChannel.prototype.invite =
function chan_inviteuser (nick)
{
    this.parent.sendData("INVITE " + nick + " " + this.encodedName + "\n");
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
    this.secret = false;
    this.pvt = false;
}

CIRCChanMode.prototype.TYPE = "IRCChanMode";

CIRCChanMode.prototype.getModeStr = 
function chan_modestr (f)
{
    var str = "";

    if (this.invite)
        str += "i";
    if (this.moderated)
        str += "m";
    if (!this.publicMessages)
        str += "n";
    if (!this.publicTopic)
        str += "t";
    if (this.secret)
        str += "s";
    if (this.pvt)
        str += "p";
    if (this.key)
        str += "k";
    if (this.limit != -1)
        str += "l " + this.limit;
    
    if (str)
        str = "+" + str;

    return str;
}

CIRCChanMode.prototype.setMode = 
function chanm_mode (modestr)
{
    this.parent.parent.sendData ("MODE " + this.parent.encodedName + " " +
                                 modestr + "\n");

    return true;
}

CIRCChanMode.prototype.setLimit = 
function chanm_limit (n)
{
    if ((typeof n == "undefined") || (n <= 0))
    {
        this.parent.parent.sendData("MODE " + this.parent.encodedName +
                                    " -l\n");
    }
    else
    {
        this.parent.parent.sendData("MODE " + this.parent.encodedName + " +l " +
                                    Number(n) + "\n");
    }

    return true;
}

CIRCChanMode.prototype.lock = 
function chanm_lock (k)
{
    this.parent.parent.sendData("MODE " + this.parent.encodedName + " +k " +
                                k + "\n");
    return true;
}

CIRCChanMode.prototype.unlock = 
function chan_unlock (k)
{
    this.parent.parent.sendData("MODE " + this.parent.encodedName + " -k " +
                                k + "\n");
    return true;
}

CIRCChanMode.prototype.setModerated = 
function chan_moderate (f)
{
    var modifier = (f) ? "+" : "-";
    
    this.parent.parent.sendData("MODE " + this.parent.encodedName + " " +
                                modifier + "m\n");
    return true;
}

CIRCChanMode.prototype.setPublicMessages = 
function chan_pmessages (f)
{
    var modifier = (f) ? "-" : "+";
    
    this.parent.parent.sendData("MODE " + this.parent.encodedName + " " +
                                modifier + "n\n");
    return true;
}

CIRCChanMode.prototype.setPublicTopic = 
function chan_ptopic (f)
{
    var modifier = (f) ? "-" : "+";
    
    this.parent.parent.sendData("MODE " + this.parent.encodedName + " " +
                                modifier + "t\n");
    return true;
}

CIRCChanMode.prototype.setInvite = 
function chan_invite (f)
{
    var modifier = (f) ? "+" : "-";
    
    this.parent.parent.sendData("MODE " + this.parent.encodedName + " " +
                                modifier + "i\n");
    return true;
}

CIRCChanMode.prototype.setPvt = 
function chan_pvt (f)
{
    var modifier = (f) ? "+" : "-";
    
    this.parent.parent.sendData("MODE " + this.parent.encodedName + " " +
                                modifier + "p\n");
    return true;
}

CIRCChanMode.prototype.setSecret = 
function chan_secret (f)
{
    var modifier = (f) ? "+" : "-";
    
    this.parent.parent.sendData("MODE " + this.parent.encodedName + " " +
                                modifier + "s\n");
    return true;
}

/*
 * user
 */

function CIRCUser (parent, nick, name, host)
{
    var properNick = nick;
    nick = parent.toLowerCase(nick);
    if (nick in parent.users)
    {
        var existingUser = parent.users[nick];
        if (name)
            existingUser.name = name;
        if (host)
            existingUser.host = host;
        return existingUser;
    }

    this.parent = parent;
    this.nick = nick;
    this.properNick = properNick;
    this.displayName = properNick;
    this.name = name;
    this.host = host;
    this.connectionHost = null;
    this.modestr = this.parent.parent.INITIAL_UMODE;
    
    parent.users[nick] = this;
    if ("onInit" in this)
        this.onInit();

    return this;
}

CIRCUser.prototype.TYPE = "IRCUser";

CIRCUser.prototype.getURL =
function usr_geturl ()
{
    return this.parent.parent.getURL(this.nick) + ",isnick";
}

CIRCUser.prototype.rehome =
function usr_rehome(newParent)
{
    delete this.parent.users[this.nick];
    this.parent = newParent;
    this.parent.users[this.nick] = this;
}

CIRCUser.prototype.changeNick =
function usr_changenick (nick)
{
    this.properNick = nick;
    this.displayName = nick;
    this.nick = this.parent.toLowerCase(nick);
}

CIRCUser.prototype.getHostMask = 
function usr_hostmask (pfx)
{
    pfx = (typeof pfx != "undefined") ? pfx : "*!" + this.name + "@*.";
    var idx = this.host.indexOf(".");
    if (idx == -1)
        return pfx + this.host;
    
    return (pfx + this.host.substr(idx + 1, this.host.length));
}

CIRCUser.prototype.say = 
function usr_say (msg)
{
    this.parent.sayTo(this.nick, fromUnicode(msg, this));
}

CIRCUser.prototype.notice = 
function usr_notice (msg)
{
    this.parent.noticeTo(this.nick, fromUnicode(msg, this));
}

CIRCUser.prototype.act = 
function usr_act (msg)
{
    this.parent.actTo(this.nick, fromUnicode(msg, this));
}

CIRCUser.prototype.ctcp = 
function usr_ctcp (code, msg, type)
{
    msg = msg || "";
    type = type || "PRIVMSG";
    
    this.parent.ctcpTo(this.nick, fromUnicode(code, this), 
                       fromUnicode(msg, this), type);
}

CIRCUser.prototype.whois =
function usr_whois ()
{
    this.parent.whois(this.nick);
}
    
/*
 * channel user
 */
function CIRCChanUser (parent, nick, modes)
{
    var properNick = nick;
    nick = parent.parent.toLowerCase(nick);

    if (nick in parent.users)
    {
        var existingUser = parent.users[nick];
        if (modes)
        {
            // If we start with a single character mode, assume we're replacing
            // the list. (i.e. the list is either all +/- modes, or all normal)
            if ((modes.length >= 1) && (modes[0].search(/^[-+]/) == -1))
            {
                // Modes, but no +/- prefixes, so *replace* mode list.
                existingUser.modes = modes;
            }
            else
            {
                // We have a +/- mode list, so carefully update the mode list.
                for (var m in modes)
                {
                    // This will remove '-' modes, and all other modes will be
                    // added.
                    var mode = modes[m][1];
                    if (modes[m][0] == "-")
                    {
                        if (arrayContains(existingUser.modes, mode))
                        {
                            var i = arrayIndexOf(existingUser.modes, mode);
                            arrayRemoveAt(existingUser.modes, i);
                        }
                    }
                    else
                    {
                        if (!arrayContains(existingUser.modes, mode))
                            existingUser.modes.push(mode);
                    }
                }
            }
        }
        existingUser.isOp = (arrayContains(existingUser.modes, "o")) ?
            true : false;
        existingUser.isHalfOp = (arrayContains(existingUser.modes, "h")) ?
            true : false;
        existingUser.isVoice = (arrayContains(existingUser.modes, "v")) ?
            true : false;

        return existingUser;
    }
        
    var protoUser = new CIRCUser (parent.parent, properNick);
        
    this.__proto__ = protoUser;
    this.getURL = cusr_geturl;
    this.setOp = cusr_setop;
    this.setHalfOp = cusr_sethalfop;
    this.setVoice = cusr_setvoice;
    this.setBan = cusr_setban;
    this.kick = cusr_kick;
    this.kickBan = cusr_kban;
    this.say = cusr_say;
    this.notice = cusr_notice;
    this.act = cusr_act;
    this.whois = cusr_whois;
    this.parent = parent;
    this.TYPE = "IRCChanUser";
    
    this.modes = new Array();
    if (typeof modes != "undefined")
        this.modes = modes;
    this.isOp = (arrayContains(this.modes, "o")) ? true : false;
    this.isHalfOp = (arrayContains(this.modes, "h")) ? true : false;
    this.isVoice = (arrayContains(this.modes, "v")) ? true : false;
    
    parent.users[nick] = this;

    return this;
}

function cusr_geturl ()
{
    return this.parent.parent.getURL(ecmaEscape(this.nick)) + ",isnick";
}

function cusr_setop (f)
{
    var server = this.parent.parent;
    var me = server.me;

    var modifier = (f) ? " +o " : " -o ";
    server.sendData("MODE " + this.parent.name + modifier + this.nick + "\n");

    return true;
}

function cusr_sethalfop (f)
{
    var server = this.parent.parent;
    var me = server.me;

    var modifier = (f) ? " +h " : " -h ";
    server.sendData("MODE " + this.parent.name + modifier + this.nick + "\n");

    return true;
}

function cusr_setvoice (f)
{
    var server = this.parent.parent;
    var me = server.me;

    var modifier = (f) ? " +v " : " -v ";
    server.sendData("MODE " + this.parent.name + modifier + this.nick + "\n");

    return true;
}

function cusr_kick (reason)
{
    var server = this.parent.parent;
    var me = server.me;

    reason = typeof reason == "string" ? reason : "";
    
    server.sendData("KICK " + this.parent.encodedName + " " + this.nick + " :" +
                    fromUnicode(reason, this) + "\n");

    return true;
}

function cusr_setban (f)
{
    var server = this.parent.parent;
    var me = server.me;

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

    if (!this.host)
        return false;

    reason = (typeof reason != "undefined") ? reason : this.nick;
    var modifier = " -o+b " + this.nick + " " + this.getHostMask() + " ";
    
    server.sendData("MODE " + this.parent.encodedName + modifier + "\n" +
                    "KICK " + this.parent.encodedName + " " + 
                    fromUnicode(this.nick, this) + " :" +
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

function cusr_whois ()
{
    this.__proto__.whois ();
}
