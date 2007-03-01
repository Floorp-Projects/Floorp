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
const JSIRC_ERR_CANCELLED = "JSIRCE:C";
const JSIRC_ERR_NO_SECURE = "JSIRCE:NO_SECURE";
const JSIRC_ERR_OFFLINE   = "JSIRCE:OFFLINE";

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

var i = 1;

const NET_OFFLINE       = i++; // Initial, disconected.
const NET_WAITING       = i++; // Waiting before trying.
const NET_CONNECTING    = i++; // Trying a connect...
const NET_CANCELLING    = i++; // Cancelling connect.
const NET_ONLINE        = i++; // Connected ok.
const NET_DISCONNECTING = i++; // Disconnecting.

delete i;

/*
 * irc network
 */
function CIRCNetwork (name, serverList, eventPump, temporary)
{
    this.unicodeName = name;
    this.viewName = name;
    this.canonicalName = name;
    this.encodedName = name;
    this.servers = new Object();
    this.serverList = new Array();
    this.ignoreList = new Object();
    this.ignoreMaskCache = new Object();
    this.state = NET_OFFLINE;
    this.temporary = Boolean(temporary);

    for (var i = 0; i < serverList.length; ++i)
    {
        var server = serverList[i];
        var password = ("password" in server) ? server.password : null;
        var isSecure = ("isSecure" in server) ? server.isSecure : false;
        this.serverList.push(new CIRCServer(this, server.name, server.port, isSecure,
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
CIRCNetwork.prototype.getReconnectDelayMs = function() { return 15000; }
CIRCNetwork.prototype.stayingPower = false;

// "http" = use HTTP proxy, "none" = none, anything else = auto.
CIRCNetwork.prototype.PROXY_TYPE_OVERRIDE = "";

CIRCNetwork.prototype.TYPE = "IRCNetwork";

CIRCNetwork.prototype.getURL =
function net_geturl(target)
{
    if (this.temporary)
        return this.serverList[0].getURL(target);

    if (!target)
        target = "";

    /* Determine whether to use the irc:// or ircs:// scheme */
    if ("primServ" in this && this.primServ.isConnected)
    {
        if (this.primServ.isSecure)
            return "ircs://" + ecmaEscape(this.unicodeName) + "/" + target;
    }
    else
    {
        /* No current connections, so let's see if every server has SSL */
        var isSecure = true;
        for (var s in this.serverList)
        {
            if (!this.serverList[s].isSecure)
            {
                isSecure = false;
                break;
            }
        }
        if (isSecure)
            return "ircs://" + ecmaEscape(this.unicodeName) + "/" + target;
    }
    return "irc://" + ecmaEscape(this.unicodeName) + "/" + target;
}

CIRCNetwork.prototype.getUser =
function net_getuser (nick)
{
    if ("primServ" in this && this.primServ)
        return this.primServ.getUser(nick);

    return null;
}

CIRCNetwork.prototype.addServer =
function net_addsrv(host, port, isSecure, password)
{
    this.serverList.push(new CIRCServer(this, host, port, isSecure, password));
}

// Checks if a network has a secure server in its list.
CIRCNetwork.prototype.hasSecureServer =
function net_hasSecure()
{
    for (var i = 0; i < this.serverList.length; i++)
    {
        if (this.serverList[i].isSecure)
            return true;
    }

    return false;
}

CIRCNetwork.prototype.clearServerList =
function net_clearserverlist()
{
    /* Note: we don't have to worry about being connected, since primServ
     * keeps the currently connected server alive if we still need it.
     */
    this.servers = new Object();
    this.serverList = new Array();
}

/** Trigger an onDoConnect event after a delay. */
CIRCNetwork.prototype.delayedConnect =
function net_delayedConnect(eventProperties)
{
    function reconnectFn(network, eventProperties)
    {
        network.immediateConnect(eventProperties);
    };

    if ((-1 != this.MAX_CONNECT_ATTEMPTS) &&
        (this.connectAttempt >= this.MAX_CONNECT_ATTEMPTS))
    {
        this.state = NET_OFFLINE;

        var ev = new CEvent("network", "error", this, "onError");
        ev.debug = "Connection attempts exhausted, giving up.";
        ev.errorCode = JSIRC_ERR_EXHAUSTED;
        this.eventPump.addEvent(ev);

        return;
    }

    this.state = NET_WAITING;
    this.reconnectTimer = setTimeout(reconnectFn,
                                     this.getReconnectDelayMs(),
                                     this,
                                     eventProperties);
}

/**
 * Immediately trigger an onDoConnect event. Use delayedConnect for automatic
 * repeat attempts, instead, to throttle the attempts to a reasonable pace.
 */
CIRCNetwork.prototype.immediateConnect =
function net_immediateConnect(eventProperties)
{
    var ev = new CEvent("network", "do-connect", this, "onDoConnect");

    if (typeof eventProperties != "undefined")
        for (var key in eventProperties)
            ev[key] = eventProperties[key];

    this.eventPump.addEvent(ev);
}

CIRCNetwork.prototype.connect =
function net_connect(requireSecurity)
{
    if ("primServ" in this && this.primServ.isConnected)
        return true;

    // We need to test for secure servers in the network object here,
    // because without them all connection attempts will fail anyway.
    if (requireSecurity && !this.hasSecureServer())
    {
        // No secure server, cope.
        ev = new CEvent ("network", "error", this, "onError");
        ev.server = this;
        ev.debug = "No connection attempted: no secure servers in list";
        ev.errorCode = JSIRC_ERR_NO_SECURE;
        this.eventPump.addEvent(ev);

        return false;
    }

    this.state = NET_CONNECTING;
    this.connectAttempt = 0;            // actual connection attempts
    this.connectCandidate = 0;          // incl. requireSecurity non-attempts
    this.nextHost = 0;
    this.requireSecurity = requireSecurity || false;
    this.immediateConnect({"password": null});
    return true;
}

CIRCNetwork.prototype.quit =
function net_quit (reason)
{
    if (this.isConnected())
        this.primServ.logout(reason);
}

CIRCNetwork.prototype.cancel =
function net_cancel()
{
    // We're online, pull the plug on the current connection, or...
    if (this.state == NET_ONLINE)
    {
        this.quit();
    }
    // We're waiting for the 001, too late to throw a reconnect, or...
    else if (this.state == NET_CONNECTING)
    {
        this.state = NET_CANCELLING;
        this.primServ.connection.disconnect();
        // Throw the necessary error events:
        ev = new CEvent ("network", "error", this, "onError");
        ev.server = this;
        ev.debug = "Connect sequence was cancelled.";
        ev.errorCode = JSIRC_ERR_CANCELLED;
        this.eventPump.addEvent(ev);
    }
    // We're waiting for onDoConnect, so try a reconnect (which will fail us)
    else if (this.state == NET_WAITING)
    {
        this.state = NET_CANCELLING;
        // onDoConnect will throw the error events for us, as it will fail
        this.immediateConnect();
    }
    else
    {
        dd("Network cancel in odd state: " + this.state);
    }
}

/*
 * Handles a request to connect to a primary server.
 */
CIRCNetwork.prototype.onDoConnect =
function net_doconnect(e)
{
    const NS_ERROR_OFFLINE = 0x804b0010;
    var c;

    // Clear the timer, if there is one.
    if ("reconnectTimer" in this)
    {
        clearTimeout(this.reconnectTimer);
        delete this.reconnectTimer;
    }

    var ev;

    if (this.state == NET_CANCELLING)
    {
        if ("primServ" in this && this.primServ.connection)
            this.primServ.connection.disconnect();
        else
            this.state = NET_OFFLINE;

        ev = new CEvent ("network", "error", this, "onError");
        ev.server = this;
        ev.debug = "Connect sequence was cancelled.";
        ev.errorCode = JSIRC_ERR_CANCELLED;
        this.eventPump.addEvent(ev);

        return false;
    }

    if ("primServ" in this && this.primServ.isConnected)
        return true;

    this.connectAttempt++;
    this.connectCandidate++;

    this.state = NET_CONNECTING; /* connection is considered "made" when server
                                  * sends a 001 message (see server.on001) */

    var host = this.nextHost++;
    if (host >= this.serverList.length)
    {
        this.nextHost = 1;
        host = 0;
    }

    if (this.serverList[host].isSecure || !this.requireSecurity)
    {
        ev = new CEvent ("network", "startconnect", this, "onStartConnect");
        ev.debug = "Connecting to " + this.serverList[host].unicodeName + ":" +
                   this.serverList[host].port + ", attempt " + this.connectAttempt +
                   " of " + this.MAX_CONNECT_ATTEMPTS + "...";
        ev.host = this.serverList[host].hostname;
        ev.port = this.serverList[host].port;
        ev.server = this.serverList[host];
        ev.connectAttempt = this.connectAttempt;
        ev.reconnectDelayMs = this.getReconnectDelayMs();
        this.eventPump.addEvent (ev);

        try
        {
            if (!this.serverList[host].connect(null))
            {
                /* connect failed, try again  */
                this.delayedConnect();
            }
        }
        catch(ex)
        {
            this.state = NET_OFFLINE;

            ev = new CEvent ("network", "error", this, "onError");
            ev.server = this;
            ev.debug = "Exception opening socket: " + ex;
            ev.errorCode = JSIRC_ERR_NO_SOCKET;
            if ((typeof ex == "object") && (ex.result == NS_ERROR_OFFLINE))
                ev.errorCode = JSIRC_ERR_OFFLINE;
            this.eventPump.addEvent(ev);
        }
    }
    else
    {
        /* Server doesn't use SSL as requested, try next one.
         * In the meantime, correct the connection attempt counter  */
        this.connectAttempt--;
        this.immediateConnect();
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
function CIRCServer (parent, hostname, port, isSecure, password)
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

    s.unicodeName = serverName;
    s.viewName = serverName;
    s.canonicalName = serverName;
    s.encodedName = serverName;
    s.hostname = hostname;
    s.port = port;
    s.parent = parent;
    s.isSecure = isSecure;
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
    s.maxLineLength = 400;

    parent.servers[s.canonicalName] = s;
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
CIRCServer.prototype.OS_RPLY = "Unknown";
CIRCServer.prototype.HOST_RPLY = "Unknown";
CIRCServer.prototype.DEFAULT_REASON = "no reason";
/* true means on352 code doesn't collect hostmask, username, etc. */
CIRCServer.prototype.LIGHTWEIGHT_WHO = false;
/* -1 == never, 0 == prune onQuit, >0 == prune when >X ms old */
CIRCServer.prototype.PRUNE_OLD_USERS = -1;

CIRCServer.prototype.TYPE = "IRCServer";

// Define functions to set modes so they're easily readable.
// name is the name used on the CIRCChanMode object
// getValue is a function returning the value the canonicalmode should be set to
// given a certain modifier and appropriate data.
CIRCServer.prototype.canonicalChanModes = {
    i: {
           name: "invite",
           getValue: function (modifier) { return (modifier == "+"); }
       },
    m: {
           name: "moderated",
           getValue: function (modifier) { return (modifier == "+"); }
       },
    n: {
           name: "publicMessages",
           getValue: function (modifier) { return (modifier == "-"); }
       },
    t: {
           name: "publicTopic",
           getValue: function (modifier) { return (modifier == "-"); }
       },
    s: {
           name: "secret",
           getValue: function (modifier) { return (modifier == "+"); }
       },
    p: {
           name: "pvt",
           getValue: function (modifier) { return (modifier == "+"); }
       },
    k: {
           name: "key",
           getValue: function (modifier, data)
                     {
                         if (modifier == "+")
                             return data;
                         else
                             return "";
                     }
       },
    l: {
           name: "limit",
           getValue: function (modifier, data)
                     {
                         // limit is special - we return -1 if there is no limit.
                         if (modifier == "-")
                             return -1;
                         else
                             return data;
                     }
       }
};

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

    var url = (this.isSecure ? "ircs://" : "irc://") + this.hostname;
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
function chan_getuser(nick)
{
    var tnick = this.toLowerCase(nick);

    if (tnick in this.users)
        return this.users[tnick];

    tnick = this.toLowerCase(fromUnicode(nick, this));

    if (tnick in this.users)
        return this.users[tnick];

    return null;
}

CIRCServer.prototype.getChannel =
function chan_getchannel(name)
{
    var tname = this.toLowerCase(name);

    if (tname in this.channels)
        return this.channels[tname];

    tname = this.toLowerCase(fromUnicode(name, this));

    if (tname in this.channels)
        return this.channels[tname];

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

    var config = { isSecure: this.isSecure };
    if (this.parent.PROXY_TYPE_OVERRIDE)
        config.proxy = this.parent.PROXY_TYPE_OVERRIDE;

    if (this.connection.connect(this.hostname, this.port, config))
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
            this.parent.eventPump.addEvent(new CEvent("server", "poll", this,
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
    this.parent.eventPump.routeEvent(ev);
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

    this.me = new CIRCUser(this, nick, null, name);
    if (this.password)
       this.sendData("PASS " + this.password + "\n");
    this.sendData("NICK " + this.me.encodedName + "\n");
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
    return new CIRCChannel(this, unicodeName, fromUnicode(unicodeName, charset));
}

CIRCServer.prototype.addUser =
function serv_addusr(unicodeName, name, host)
{
    return new CIRCUser(this, unicodeName, null, name, host);
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
    {
        if (lines[i])
        {
            while (lines[i].length > this.maxLineLength)
            {
                var extraLine = lines[i].substr(0, this.maxLineLength - 5);
                var pos = extraLine.lastIndexOf(" ");

                if ((pos >= 0) && (pos >= this.maxLineLength - 15))
                {
                    // Smart-split.
                    extraLine = lines[i].substr(0, pos) + "...";
                    lines[i] = "..." + lines[i].substr(extraLine.length - 2);
                }
                else
                {
                    // Dump-split.
                    extraLine = lines[i].substr(0, this.maxLineLength);
                    lines[i] = lines[i].substr(extraLine.length);
                }
                if (!this.messageTo(code, target, extraLine, ctcpCode))
                    return false;
            }
        }
    }
    // Check this again, since we may have actually sent stuff in the loop above.
    if (this.MAX_LINES_PER_SEND &&
        this.sendsThisRound > this.MAX_LINES_PER_SEND)
        return false;

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

CIRCServer.prototype.changeNick =
function serv_changenick(newNick)
{
    this.sendData("NICK " + fromUnicode(newNick, this) + "\n");
}

CIRCServer.prototype.updateLagTimer =
function serv_uptimer()
{
    this.connection.sendData("PING :LAGTIMER\n");
    this.lastPing = this.lastPingSent = new Date();
}

CIRCServer.prototype.userhost =
function serv_userhost(target)
{
    this.sendData("USERHOST " + fromUnicode(target, this) + "\n");
}

CIRCServer.prototype.userip =
function serv_userip(target)
{
    this.sendData("USERIP " + fromUnicode(target, this) + "\n");
}

CIRCServer.prototype.who =
function serv_who(target)
{
    this.sendData("WHO " + fromUnicode(target, this) + "\n");
}

/**
 * Abstracts the whois command.
 *
 * @param target        intended user(s).
 */
CIRCServer.prototype.whois =
function serv_whois (target)
{
    this.sendData("WHOIS " + fromUnicode(target, this) + "\n");
}

CIRCServer.prototype.whowas =
function serv_whowas(target, limit)
{
    if (typeof limit == "undefined")
        limit = 1;
    else if (limit == 0)
        limit = "";

    this.sendData("WHOWAS " + fromUnicode(target, this) + " " + limit + "\n");
}

CIRCServer.prototype.onDisconnect =
function serv_disconnect(e)
{
    function stateChangeFn(network, state) {
        network.state = state;
    };

    function delayedConnectFn(network) {
        network.delayedConnect();
    };

    if ((this.parent.state == NET_CONNECTING) ||
        /* fell off while connecting, try again */
        (this.parent.primServ == this) && (this.parent.state == NET_ONLINE) &&
        (!("quitting" in this) && this.parent.stayingPower))
    { /* fell off primary server, reconnect to any host in the serverList */
        setTimeout(delayedConnectFn, 0, this.parent);
    }
    else
    {
        setTimeout(stateChangeFn, 0, this.parent, NET_OFFLINE);
    }

    e.server = this;
    e.set = "network";
    e.destObject = this.parent;

    e.quitting = this.quitting;

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
    if (!this.isConnected || (this.parent.state == NET_CANCELLING))
    {
        dd ("Can't send to disconnected socket");
        this.flushSendQueue();
        return false;
    }

    var d = new Date();

    this.sendsThisRound = 0;

    // Wheee, some sanity checking! (there's been at least one case of lastSend
    // ending up in the *future* at this point, which kinda busts things)
    if (this.lastSend > d)
        this.lastSend = 0;

    if (((d - this.lastSend) >= this.MS_BETWEEN_SENDS) &&
        this.sendQueue.length > 0)
    {
        var s = this.sendQueue.pop();

        if (s)
        {
            try
            {
                this.connection.sendData(s);
            }
            catch(ex)
            {
                dd("Exception in queued send: " + ex);
                this.flushSendQueue();

                var ev = new CEvent("server", "disconnect",
                                    this, "onDisconnect");
                ev.server = this;
                ev.reason = "error";
                ev.exception = ex;
                this.parent.eventPump.addEvent(ev);

                return false;
            }
            this.lastSend = d;
        }

    }
    else
    {
        this.parent.eventPump.addEvent(new CEvent("event-pump", "yield",
                                                  null, ""));
    }

    if (this.sendQueue.length > 0)
        this.parent.eventPump.addEvent(new CEvent("server", "senddata",
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
        if (this.parent.state != NET_CANCELLING)
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
        this.parent.eventPump.routeEvent(ev);
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
        {
            if (ev.data.match(/^(?::[^ ]+ )?(?:32[123]|352|315) /i))
                this.parent.eventPump.addBulkEvent(ev);
            else
                this.parent.eventPump.addEvent(ev);
        }
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
    var ary;
    var l = e.data;

    if (l.length == 0)
    {
        dd ("empty line on onRawData?");
        return false;
    }

    if (l[0] == ":")
    {
        // Must split only on a REAL space here, not just any old whitespace.
        ary = l.match(/:([^ ]+) (.*)/);
        e.source = ary[1];
        l = ary[2];
        ary = e.source.match(/([^ ]+)!([^ ]+)@(.*)/);
        if (ary)
        {
            e.user = new CIRCUser(this, null, ary[1], ary[2], ary[3]);
        }
        else
        {
            ary = e.source.match(/([^ ]+)@(.*)/);
            if (ary)
            {
                e.user = new CIRCUser(this, null, ary[1], null, ary[2]);
            }
            else
            {
                ary = e.source.match(/([^ ]+)!(.*)/);
                if (ary)
                    e.user = new CIRCUser(this, null, ary[1], ary[2], null);
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
    e.channel = new CIRCChannel(this, null, e.params[1]);
    e.channel.topicBy = e.user.unicodeName;
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
    this.parent.connectCandidate = 0;
    this.parent.state = NET_ONLINE;
    // nextHost is incremented after picking a server. Push it back here.
    this.parent.nextHost--;

    /* servers won't send a nick change notification if user was forced
     * to change nick while logging in (eg. nick already in use.)  We need
     * to verify here that what the server thinks our name is, matches what
     * we think it is.  If not, the server wins.
     */
    if (e.params[1] != e.server.me.encodedName)
    {
        renameProperty(e.server.users, e.server.me.canonicalName,
                       this.toLowerCase(e.params[1]));
        e.server.me.changeNick(toUnicode(e.params[1], this));
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
    // Default to support of v/+ and o/@ only.
    this.userModes = [
                       { mode: 'o', symbol: '@' },
                       { mode: 'v', symbol: '+' }
                     ];
    // Assume the server supports no extra interesting commands.
    this.servCmds = {};

    if (this.parent.INITIAL_UMODE)
    {
        e.server.sendData("MODE " + e.server.me.encodedName + " :" +
                          this.parent.INITIAL_UMODE + "\n");
    }

    this.parent.users = this.users;
    e.destObject = this.parent;
    e.set = "network";
}

/* server features */
CIRCServer.prototype.on005 =
function serv_005 (e)
{
    var oldCaseMapping = this.supports["casemapping"];
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
    // Update all users and channels if the casemapping changed.
    if (this.supports["casemapping"] != oldCaseMapping)
    {
        var newName, encodedName;
        for (var user in this.users)
        {
            newName = this.toLowerCase(this.users[user].encodedName);
            renameProperty(this.users, user, newName);
            this.users[newName].canonicalName = newName;
        }
        for (var channel in this.channels)
        {
            newName = this.toLowerCase(this.channels[channel].encodedName);
            renameProperty(this.channels, this.channels[channel].canonicalName,
                           newName);
            this.channels[channel].canonicalName = newName;
            for (user in this.channels[channel].users)
            {
                encodedName = this.channels[channel].users[user].encodedName;
                newName = this.toLowerCase(encodedName);
                renameProperty(this.channels[channel].users, user, newName);
            }
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

    if ("cmds" in this.supports)
    {
        // Map this.supports.cmds [comma-list] into this.servCmds [props].
        var cmdlist = this.supports.cmds.split(/,/);
        for (var i = 0; i < cmdlist.length; i++)
            this.servCmds[cmdlist[i].toLowerCase()] = true;
    }

    this.supports.rpl_isupport = true;

    e.destObject = this.parent;
    e.set = "network";

    return true;
}

/* users */
CIRCServer.prototype.on251 =
function serv_251(e)
{
    // 251 is the first message we get after 005, so it's now safe to do
    // things that might depend upon server features.

    if (("namesx" in this.supports) && this.supports.namesx)
        this.sendData("PROTOCTL NAMESX\n");

    if (this.parent.INITIAL_CHANNEL)
    {
        this.parent.primChan = this.addChannel(this.parent.INITIAL_CHANNEL);
        this.parent.primChan.join();
    }

    e.destObject = this.parent;
    e.set = "network";
}

/* user away message */
CIRCServer.prototype.on301 =
function serv_301(e)
{
    e.user = new CIRCUser(this, null, e.params[2]);
    e.user.awayMessage = e.decodeParam(3, e.user);
    e.destObject = this.parent;
    e.set = "network";
}

/* whois name */
CIRCServer.prototype.on311 =
function serv_311 (e)
{
    e.user = new CIRCUser(this, null, e.params[2], e.params[3], e.params[4]);
    e.user.desc = e.decodeParam(6, e.user);
    e.destObject = this.parent;
    e.set = "network";

    this.pendingWhoisLines = e.user;
}

/* whois server */
CIRCServer.prototype.on312 =
function serv_312 (e)
{
    e.user = new CIRCUser(this, null, e.params[2]);
    e.user.connectionHost = e.params[3];

    e.destObject = this.parent;
    e.set = "network";
}

/* whois idle time */
CIRCServer.prototype.on317 =
function serv_317 (e)
{
    e.user = new CIRCUser(this, null, e.params[2]);
    e.user.idleSeconds = e.params[3];

    e.destObject = this.parent;
    e.set = "network";
}

/* whois channel list */
CIRCServer.prototype.on319 =
function serv_319(e)
{
    e.user = new CIRCUser(this, null, e.params[2]);

    e.destObject = this.parent;
    e.set = "network";
}

/* end of whois */
CIRCServer.prototype.on318 =
function serv_318(e)
{
    e.user = new CIRCUser(this, null, e.params[2]);

    if ("pendingWhoisLines" in this)
        delete this.pendingWhoisLines;

    e.destObject = this.parent;
    e.set = "network";
}

/* TOPIC reply - no topic set */
CIRCServer.prototype.on331 =
function serv_331 (e)
{
    e.channel = new CIRCChannel(this, null, e.params[2]);
    e.channel.topic = "";
    e.destObject = e.channel;
    e.set = "channel";

    return true;
}

/* TOPIC reply - topic set */
CIRCServer.prototype.on332 =
function serv_332 (e)
{
    e.channel = new CIRCChannel(this, null, e.params[2]);
    e.channel.topic = toUnicode(e.params[3], e.channel);
    e.destObject = e.channel;
    e.set = "channel";

    return true;
}

/* topic information */
CIRCServer.prototype.on333 =
function serv_333 (e)
{
    e.channel = new CIRCChannel(this, null, e.params[2]);
    e.channel.topicBy = toUnicode(e.params[3], this);
    e.channel.topicDate = new Date(Number(e.params[4]) * 1000);
    e.destObject = e.channel;
    e.set = "channel";

    return true;
}

/* who reply */
CIRCServer.prototype.on352 =
function serv_352 (e)
{
    e.userHasChanges = false;
    if (this.LIGHTWEIGHT_WHO)
    {
        e.user = new CIRCUser(this, null, e.params[6]);
    }
    else
    {
        e.user = new CIRCUser(this, null, e.params[6], e.params[3], e.params[4]);
        e.user.connectionHost = e.params[5];
        if (8 in e.params)
        {
            var ary = e.params[8].match(/(?:(\d+)\s)?(.*)/);
            e.user.hops = ary[1];
            var desc = fromUnicode(ary[2], e.user);
            if (e.user.desc != desc)
            {
                e.userHasChanges = true;
                e.user.desc = desc;
            }
        }
    }
    var away = (e.params[7][0] == "G");
    if (e.user.isAway != away)
    {
        e.userHasChanges = true;
        e.user.isAway = away;
    }

    e.destObject = this.parent;
    e.set = "network";

    return true;
}

/* end of who */
CIRCServer.prototype.on315 =
function serv_315 (e)
{
    e.user = new CIRCUser(this, null, e.params[2]);
    e.destObject = this.parent;
    e.set = "network";

    return true;
}

/* names reply */
CIRCServer.prototype.on353 =
function serv_353 (e)
{
    e.channel = new CIRCChannel(this, null, e.params[3]);
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

        var modes = new Array();
        do
        {
            var found = false;
            for (var m in mList)
            {
                if (nick[0] == mList[m].symbol)
                {
                    nick = nick.substr(1);
                    modes.push(mList[m].mode);
                    found = true;
                    break;
                }
            }
        } while (found && ("namesx" in this.supports) && this.supports.namesx);

        new CIRCChanUser(e.channel, null, nick, modes);
    }

    return true;
}

/* end of names */
CIRCServer.prototype.on366 =
function serv_366 (e)
{
    e.channel = new CIRCChannel(this, null, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    e.channel.usersStable = true;

    return true;
}

/* channel time stamp? */
CIRCServer.prototype.on329 =
function serv_329 (e)
{
    e.channel = new CIRCChannel(this, null, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    e.channel.timeStamp = new Date (Number(e.params[3]) * 1000);

    return true;
}

/* channel mode reply */
CIRCServer.prototype.on324 =
function serv_324 (e)
{
    e.channel = new CIRCChannel(this, null, e.params[2]);
    e.destObject = this;
    e.type = "chanmode";
    e.destMethod = "onChanMode";

    return true;
}

/* channel ban entry */
CIRCServer.prototype.on367 =
function serv_367(e)
{
    e.channel = new CIRCChannel(this, null, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    e.ban = e.params[3];
    e.user = new CIRCUser(this, null, e.params[4]);
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
    e.channel = new CIRCChannel(this, null, e.params[2]);
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
    e.channel = new CIRCChannel(this, null, e.params[2]);
    e.destObject = e.channel;
    e.set = "channel";
    e.except = e.params[3];
    e.user = new CIRCUser(this, null, e.params[4]);
    e.exceptTime = new Date (Number(e.params[5]) * 1000);

    if (typeof e.channel.excepts[e.except] == "undefined")
    {
        e.channel.excepts[e.except] = {host: e.except, user: e.user,
                                       time: e.exceptTime };
    }

    return true;
}

/* channel except list end */
CIRCServer.prototype.on349 =
function serv_349(e)
{
    e.channel = new CIRCChannel(this, null, e.params[2]);
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
    e.channel = new CIRCChannel(this, null, e.params[2]);
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
        e.channel = new CIRCChannel(this, null, e.params[1]);
        if ("user" in e && e.user)
            e.user = new CIRCChanUser(e.channel, e.user.unicodeName);
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
    e.user = new CIRCUser(this, null, e.params[1])
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
    var umList = this.userModes;
    var cmList = this.channelModes;
    var modeMap = this.canonicalChanModes;
    var canonicalModeValue;

    for (var i = 0; i < mode_str.length ; i++)
    {
        /* Take care of modifier first. */
        if ((mode_str[i] == '+') || (mode_str[i] == '-'))
        {
            modifier = mode_str[i];
            continue;
        }

        var done = false;
        for (var m in umList)
        {
            if ((mode_str[i] == umList[m].mode) && (modifier != ""))
            {
                nick = e.params[BASE_PARAM + params_eaten];
                user = new CIRCChanUser(e.channel, null, nick,
                                        [ modifier + umList[m].mode ]);
                params_eaten++;
                e.usersAffected.push (user);
                done = true;
                break;
            }
        }
        if (done)
            continue;

        // Update legacy canonical modes if necessary.
        if (mode_str[i] in modeMap)
        {
            // Get the data in case we need it, but don't increment the counter.
            var datacounter = BASE_PARAM + params_eaten;
            var data = (datacounter in e.params) ? e.params[datacounter] : null;
            canonicalModeValue = modeMap[mode_str[i]].getValue(modifier, data);
            e.channel.mode[modeMap[mode_str[i]].name] = canonicalModeValue;
        }

        if (arrayContains(cmList.a, mode_str[i]))
        {
            var data = e.params[BASE_PARAM + params_eaten++];
            if (modifier == "+")
            {
                e.channel.mode.modeA[data] = true;
            }
            else
            {
                if (data in e.channel.mode.modeA)
                {
                    delete e.channel.mode.modeA[data];
                }
                else
                {
                    dd("** Trying to remove channel mode '" + mode_str[i] +
                       "'/'" + data + "' which does not exist in list.");
                }
            }
        }
        else if (arrayContains(cmList.b, mode_str[i]))
        {
            var data = e.params[BASE_PARAM + params_eaten++];
            if (modifier == "+")
            {
                e.channel.mode.modeB[mode_str[i]] = data;
            }
            else
            {
                // Save 'null' even though we have some data.
                e.channel.mode.modeB[mode_str[i]] = null;
            }
        }
        else if (arrayContains(cmList.c, mode_str[i]))
        {
            if (modifier == "+")
            {
                var data = e.params[BASE_PARAM + params_eaten++];
                e.channel.mode.modeC[mode_str[i]] = data;
            }
            else
            {
                e.channel.mode.modeC[mode_str[i]] = null;
            }
        }
        else if (arrayContains(cmList.d, mode_str[i]))
        {
            e.channel.mode.modeD[mode_str[i]] = (modifier == "+");
        }
        else
        {
            dd("** UNKNOWN mode symbol '" + mode_str[i] + "' in ChanMode event **");
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
    var oldKey = e.user.canonicalName;
    var ev;

    renameProperty (this.users, oldKey, newKey);
    e.oldNick = e.user.unicodeName;
    e.user.changeNick(toUnicode(newNick, this));

    for (var c in this.channels)
    {
        if (this.channels[c].active &&
            ((oldKey in this.channels[c].users) || e.user == this.me))
        {
            var cuser = this.channels[c].users[oldKey];
            renameProperty (this.channels[c].users, oldKey, newKey);
            ev = new CEvent ("channel", "nick", this.channels[c], "onNick");
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
            e.user.canonicalName in e.server.channels[c].users)
        {
            var ev = new CEvent ("channel", "quit", e.server.channels[c],
                                 "onQuit");
            ev.user = e.server.channels[c].users[e.user.canonicalName];
            ev.channel = e.server.channels[c];
            ev.server = ev.channel.parent;
            ev.reason = reason;
            this.parent.eventPump.addEvent(ev);
            delete e.server.channels[c].users[e.user.canonicalName];
        }
    }

    this.users[e.user.canonicalName].lastQuitMessage = reason;
    this.users[e.user.canonicalName].lastQuitDate = new Date();

    // 0 == prune onQuit.
    if (this.PRUNE_OLD_USERS == 0)
        delete this.users[e.user.canonicalName];

    e.reason = reason;
    e.destObject = e.user;
    e.set = "user";

    return true;
}

CIRCServer.prototype.onPart =
function serv_part (e)
{
    e.channel = new CIRCChannel(this, null, e.params[1]);
    e.reason = (e.params.length > 2) ? e.decodeParam(2, e.channel) : "";
    e.user = new CIRCChanUser(e.channel, e.user.unicodeName);
    if (userIsMe(e.user))
    {
        e.channel.active = false;
        e.channel.joined = false;
    }
    e.channel.removeUser(e.user.encodedName);
    e.destObject = e.channel;
    e.set = "channel";

    return true;
}

CIRCServer.prototype.onKick =
function serv_kick (e)
{
    e.channel = new CIRCChannel(this, null, e.params[1]);
    e.lamer = new CIRCChanUser(e.channel, null, e.params[2]);
    delete e.channel.users[e.lamer.canonicalName];
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
    e.channel = new CIRCChannel(this, null, e.params[1]);
    e.user = new CIRCChanUser(e.channel, e.user.unicodeName);

    if (userIsMe(e.user))
    {
        var delayFn1 = function(t) {
            if (!e.channel.active)
                return;

            // Give us the channel mode!
            e.server.sendData("MODE " + e.channel.encodedName + "\n");
        };
        // Between 1s - 3s.
        setTimeout(delayFn1, 1000 + 2000 * Math.random(), this);

        var delayFn2 = function(t) {
            if (!e.channel.active)
                return;

            // Get a full list of bans and exceptions, if supported.
            if (arrayContains(t.channelModes.a, "b"))
            {
                e.server.sendData("MODE " + e.channel.encodedName + " +b\n");
                e.channel.pendingBanList = true;
            }
            if (arrayContains(t.channelModes.a, "e"))
            {
                e.server.sendData("MODE " + e.channel.encodedName + " +e\n");
                e.channel.pendingExceptList = true;
            }
        };
        // Between 10s - 20s.
        setTimeout(delayFn2, 10000 + 10000 * Math.random(), this);

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

CIRCServer.prototype.onInvite =
function serv_invite(e)
{
    e.channel = new CIRCChannel(this, null, e.params[2]);

    e.destObject = this.parent;
    e.set = "network";
}

CIRCServer.prototype.onNotice =
function serv_notice (e)
{
    var targetName = e.params[1];

    if (this.userModes)
    {
        // Strip off one (and only one) user mode prefix.
        for (var i = 0; i < this.userModes.length; i++)
        {
            if (targetName[0] == this.userModes[i].symbol)
            {
                targetName = targetName.substr(1);
                break;
            }
        }
    }

    if (arrayIndexOf(this.channelTypes, targetName[0]) != -1)
    {
        e.channel = new CIRCChannel(this, null, targetName);
        if ("user" in e)
            e.user = new CIRCChanUser(e.channel, e.user.unicodeName);
        e.replyTo = e.channel;
        e.set = "channel";
    }
    else if (!("user" in e))
    {
        e.set = "network";
        e.destObject = this.parent;
        return true;
    }
    else
    {
        e.set = "user";
        e.replyTo = e.user; /* send replies to the user who sent the message */
    }

    if (e.params[2].search (/\x01.*\x01/i) != -1)
    {
        e.type = "ctcp-reply";
        e.destMethod = "onCTCPReply";
        e.set = "server";
        e.destObject = this;
    }
    else
    {
        e.msg = e.decodeParam(2, e.replyTo);
        e.destObject = e.replyTo;
    }

    return true;
}

CIRCServer.prototype.onPrivmsg =
function serv_privmsg (e)
{
    var targetName = e.params[1];

    if (this.userModes)
    {
        // Strip off one (and only one) user mode prefix.
        for (var i = 0; i < this.userModes.length; i++)
        {
            if (targetName[0] == this.userModes[i].symbol)
            {
                targetName = targetName.substr(1);
                break;
            }
        }
    }

    /* setting replyTo provides a standard place to find the target for     */
    /* replies associated with this event.                                  */
    if (arrayIndexOf(this.channelTypes, targetName[0]) != -1)
    {
        e.channel = new CIRCChannel(this, null, targetName);
        e.user = new CIRCChanUser(e.channel, e.user.unicodeName);
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
    {
        e.destObject = e.replyTo;
        e.msg = e.decodeParam(2, e.replyTo);
    }

    return true;
}

CIRCServer.prototype.onCTCPReply =
function serv_ctcpr (e)
{
    var ary = e.params[2].match (/^\x01([^ ]+) ?(.*)\x01$/i);

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
    var ary = e.params[2].match (/^\x01([^ ]+) ?(.*)\x01$/i);

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

    if (e.CTCPData)
    {
        var cmdName = "onCTCP" + e.CTCPData[0].toUpperCase() +
                      e.CTCPData.substr (1, e.CTCPData.length).toLowerCase();
        var helpName = cmdName.replace(/^onCTCP/, "CTCPHelp");

        // Check we support the command.
        if (cmdName in this)
        {
            // Do we have help for it?
            if (helpName in this)
            {
                var msg;
                if (typeof this[helpName] == "function")
                    msg = this[helpName]();
                else
                    msg = this[helpName];

                e.user.ctcp("CLIENTINFO", msg, "NOTICE");
            }
            else
            {
                e.user.ctcp("CLIENTINFO",
                            getMsg(MSG_ERR_NO_CTCP_HELP, e.CTCPData), "NOTICE");
            }
        }
        else
        {
            e.user.ctcp("CLIENTINFO",
                        getMsg(MSG_ERR_NO_CTCP_CMD, e.CTCPData), "NOTICE");
        }
        return true;
    }

    for (var fname in this)
    {
        var ary = fname.match(/^onCTCP(.+)/);
        if (ary && ary[1].search(/^Reply/) == -1)
            clientinfo.push (ary[1].toUpperCase());
    }

    e.user.ctcp("CLIENTINFO", clientinfo.join(" "), "NOTICE");

    return true;
}

CIRCServer.prototype.onCTCPAction =
function serv_cact (e)
{
    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";
}

CIRCServer.prototype.onCTCPFinger =
function serv_cfinger (e)
{
    e.user.ctcp("FINGER", this.parent.INITIAL_DESC, "NOTICE");
    return true;
}

CIRCServer.prototype.onCTCPTime =
function serv_cping (e)
{
    e.user.ctcp("TIME", new Date(), "NOTICE");

    return true;
}

CIRCServer.prototype.onCTCPVersion =
function serv_cver (e)
{
    var lines = e.server.VERSION_RPLY.split ("\n");

    for (var i in lines)
        e.user.ctcp("VERSION", lines[i], "NOTICE");

    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";

    return true;
}

CIRCServer.prototype.onCTCPSource =
function serv_csrc (e)
{
    e.user.ctcp("SOURCE", this.SOURCE_RPLY, "NOTICE");

    return true;
}

CIRCServer.prototype.onCTCPOs =
function serv_os(e)
{
    e.user.ctcp("OS", this.OS_RPLY, "NOTICE");

    return true;
}

CIRCServer.prototype.onCTCPHost =
function serv_host(e)
{
    e.user.ctcp("HOST", this.HOST_RPLY, "NOTICE");

    return true;
}

CIRCServer.prototype.onCTCPPing =
function serv_cping (e)
{
    /* non-queued send */
    this.connection.sendData("NOTICE " + e.user.encodedName + " :\01PING " +
                             e.CTCPData + "\01\n");
    e.destObject = e.replyTo;
    e.set = (e.replyTo == e.user) ? "user" : "channel";

    return true;
}

CIRCServer.prototype.onCTCPDcc =
function serv_dcc (e)
{
    var ary = e.CTCPData.match (/([^ ]+)? ?(.*)/);

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
    var ary = e.DCCData.match(/([^ ]+) (\d+) (\d+) (\d+)/);

    /* Just for mIRC: filenames with spaces may be enclosed in double-quotes.
     * (though by default it replaces spaces with underscores, but we might as
     * well cope). */
    if ((ary[1][0] == '"') || (ary[1][ary[1].length - 1] == '"'))
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

function CIRCChannel(parent, unicodeName, encodedName)
{
    // Both unicodeName and encodedName are optional, but at least one must be
    // present.

    if (!encodedName && !unicodeName)
        throw "Hey! Come on, I need either an encoded or a Unicode name.";
    if (!encodedName)
        encodedName = fromUnicode(unicodeName, parent);

    var canonicalName = parent.toLowerCase(encodedName);
    if (canonicalName in parent.channels)
        return parent.channels[canonicalName];

    this.parent = parent;
    this.encodedName = encodedName;
    this.canonicalName = canonicalName;
    this.unicodeName = unicodeName || toUnicode(encodedName, this);
    this.viewName = this.unicodeName;

    this.users = new Object();
    this.bans = new Object();
    this.excepts = new Object();
    this.mode = new CIRCChanMode(this);
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

    this.parent.channels[this.canonicalName] = this;
    if ("onInit" in this)
        this.onInit();

    return this;
}

CIRCChannel.prototype.TYPE = "IRCChannel";
CIRCChannel.prototype.topic = "";

CIRCChannel.prototype.getURL =
function chan_geturl ()
{
    var target = this.encodedName;

    if ((target[0] == "#") &&
        arrayIndexOf(this.parent.channelTypes, target[1]) == -1)
    {
        /* First character is "#" (which we're allowed to ommit), and the
         * following character is NOT a valid prefix, so it's safe to remove.
         */
        target = ecmaEscape(target.substr(1));
    }
    else
    {
        target = ecmaEscape(target);
    }

    target = target.replace("/", "%2f");

    return this.parent.parent.getURL(target);
}

CIRCChannel.prototype.rehome =
function chan_rehome(newParent)
{
    delete this.parent.channels[this.canonicalName];
    this.parent = newParent;
    this.parent.channels[this.canonicalName] = this;
}

CIRCChannel.prototype.addUser =
function chan_adduser (unicodeName, modes)
{
    return new CIRCChanUser(this, unicodeName, null, modes);
}

CIRCChannel.prototype.getUser =
function chan_getuser(nick)
{
    // Try assuming it's an encodedName first.
    nick = this.parent.toLowerCase(nick);
    if (nick in this.users)
        return this.users[nick];

    // Ok, failed, so try assuming it's a unicodeName.
    nick = this.parent.toLowerCase(fromUnicode(nick, this.parent));
    if (nick in this.users)
        return this.users[nick];

    return null;
}

CIRCChannel.prototype.removeUser =
function chan_removeuser(nick)
{
    // Try assuming it's an encodedName first.
    nick = this.parent.toLowerCase(nick);
    if (nick in this.users)
        delete this.users[nick]; // see ya

    // Ok, failed, so try assuming it's a unicodeName.
    nick = this.parent.toLowerCase(fromUnicode(nick, this.parent));
    if (nick in this.users)
        delete this.users[nick];
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
    return this.active && this.users[this.parent.me.canonicalName].isOp;
}

CIRCChannel.prototype.iAmHalfOp =
function chan_amhalfop()
{
    return this.active && this.users[this.parent.me.canonicalName].isHalfOp;
}

CIRCChannel.prototype.iAmVoice =
function chan_amvoice()
{
    return this.active && this.users[this.parent.me.canonicalName].isVoice;
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
function chan_part (reason)
{
    if (!reason)
        reason = "";
    this.parent.sendData ("PART " + this.encodedName + " :" +
                          fromUnicode(reason, this) + "\n");
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

CIRCChannel.prototype.findUsers = 
function chan_findUsers(mask)
{
    var ary = [];
    var unchecked = 0;
    mask = getHostmaskParts(mask);
    for (var nick in this.users)
    {
        var user = this.users[nick];
        if (!user.host || !user.name)
            unchecked++;
        else if (hostmaskMatches(user, mask))
            ary.push(user);
    }
    return { users: ary, unchecked: unchecked };
}

/*
 * channel mode
 */
function CIRCChanMode (parent)
{
    this.parent = parent;

    this.modeA = new Object();
    this.modeB = new Object();
    this.modeC = new Object();
    this.modeD = new Object();

    this.invite = false;
    this.moderated = false;
    this.publicMessages = true;
    this.publicTopic = true;
    this.secret = false;
    this.pvt = false;
    this.key = "";
    this.limit = -1;
}

CIRCChanMode.prototype.TYPE = "IRCChanMode";

CIRCChanMode.prototype.getModeStr =
function chan_modestr (f)
{
    var str = "";
    var modeCparams = "";

    /* modeA are 'list' ones, and so should not be shown.
     * modeB are 'param' ones, like +k key, so we wont show them either.
     * modeC are 'on-param' ones, like +l limit, which we will show.
     * modeD are 'boolean' ones, which we will definitely show.
     */

    // Add modeD:
    for (var m in this.modeD)
    {
        if (this.modeD[m])
            str += m;
    }

    // Add modeC, save parameters for adding all the way at the end:
    for (var m in this.modeC)
    {
        if (this.modeC[m])
        {
            str += m;
            modeCparams += " " + this.modeC[m];
        }
    }

    // Add parameters:
    if (str)
        str = "+" + str + modeCparams;

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

function CIRCUser(parent, unicodeName, encodedName, name, host)
{
    // Both unicodeName and encodedName are optional, but at least one must be
    // present.

    if (!encodedName && !unicodeName)
        throw "Hey! Come on, I need either an encoded or a Unicode name.";
    if (!encodedName)
        encodedName = fromUnicode(unicodeName, parent);

    var canonicalName = parent.toLowerCase(encodedName);
    if (canonicalName in parent.users)
    {
        var existingUser = parent.users[canonicalName];
        if (name)
            existingUser.name = name;
        if (host)
            existingUser.host = host;
        return existingUser;
    }

    this.parent = parent;
    this.encodedName = encodedName;
    this.canonicalName = canonicalName;
    this.unicodeName = unicodeName || toUnicode(encodedName, this.parent);
    this.viewName = this.unicodeName;

    this.name = name;
    this.host = host;
    this.desc = "";
    this.connectionHost = null;
    this.isAway = false;
    this.modestr = this.parent.parent.INITIAL_UMODE;

    this.parent.users[this.canonicalName] = this;
    if ("onInit" in this)
        this.onInit();

    return this;
}

CIRCUser.prototype.TYPE = "IRCUser";

CIRCUser.prototype.getURL =
function usr_geturl ()
{
    var target = ecmaEscape(this.encodedName);
    target = target.replace("/", "%2f");
    //target += "?charset=" + this.prefs["charset"];

    return this.parent.parent.getURL(target) + ",isnick";
}

CIRCUser.prototype.rehome =
function usr_rehome(newParent)
{
    delete this.parent.users[this.canonicalName];
    this.parent = newParent;
    this.parent.users[this.canonicalName] = this;
}

CIRCUser.prototype.changeNick =
function usr_changenick(unicodeName)
{
    this.unicodeName = unicodeName;
    this.viewName = this.unicodeName;
    this.encodedName = fromUnicode(this.unicodeName, this.parent);
    this.canonicalName = this.parent.toLowerCase(this.encodedName);
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

CIRCUser.prototype.getBanMask =
function usr_banmask()
{
    if (!this.host)
        return this.unicodeName + "!*@*";

    var hostmask = this.host;
    if (!/^\d+\.\d+\.\d+\.\d+$/.test(hostmask))
        hostmask = hostmask.replace(/^[^.]+/, "*");
    else
        hostmask = hostmask.replace(/[^.]+$/, "*");
    return "*!" + this.name + "@" + hostmask;
}

CIRCUser.prototype.say =
function usr_say (msg)
{
    this.parent.sayTo(this.encodedName, fromUnicode(msg, this));
}

CIRCUser.prototype.notice =
function usr_notice (msg)
{
    this.parent.noticeTo(this.encodedName, fromUnicode(msg, this));
}

CIRCUser.prototype.act =
function usr_act (msg)
{
    this.parent.actTo(this.encodedName, fromUnicode(msg, this));
}

CIRCUser.prototype.ctcp =
function usr_ctcp (code, msg, type)
{
    msg = msg || "";
    type = type || "PRIVMSG";

    this.parent.ctcpTo(this.encodedName, fromUnicode(code, this),
                       fromUnicode(msg, this), type);
}

CIRCUser.prototype.whois =
function usr_whois ()
{
    this.parent.whois(this.unicodeName);
}

/*
 * channel user
 */
function CIRCChanUser(parent, unicodeName, encodedName, modes)
{
    // Both unicodeName and encodedName are optional, but at least one must be
    // present.

    if (!encodedName && !unicodeName)
        throw "Hey! Come on, I need either an encoded or a Unicode name.";
    else if (encodedName && !unicodeName)
        unicodeName = toUnicode(encodedName, parent);
    else if (!encodedName && unicodeName)
        encodedName = fromUnicode(unicodeName, parent);

    // We should have both unicode and encoded names by now.

    var canonicalName = parent.parent.toLowerCase(encodedName);

    if (canonicalName in parent.users)
    {
        var existingUser = parent.users[canonicalName];
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
        existingUser.isFounder = (arrayContains(existingUser.modes, "q")) ?
            true : false;
        existingUser.isAdmin = (arrayContains(existingUser.modes, "a")) ?
            true : false;
        existingUser.isOp = (arrayContains(existingUser.modes, "o")) ?
            true : false;
        existingUser.isHalfOp = (arrayContains(existingUser.modes, "h")) ?
            true : false;
        existingUser.isVoice = (arrayContains(existingUser.modes, "v")) ?
            true : false;

        return existingUser;
    }

    var protoUser = new CIRCUser(parent.parent, unicodeName, encodedName);

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
    this.isFounder = (arrayContains(this.modes, "q")) ? true : false;
    this.isAdmin = (arrayContains(this.modes, "a")) ? true : false;
    this.isOp = (arrayContains(this.modes, "o")) ? true : false;
    this.isHalfOp = (arrayContains(this.modes, "h")) ? true : false;
    this.isVoice = (arrayContains(this.modes, "v")) ? true : false;

    parent.users[this.canonicalName] = this;

    return this;
}

function cusr_geturl ()
{
    return this.parent.parent.getURL(ecmaEscape(this.unicodeName)) + ",isnick";
}

function cusr_setop (f)
{
    var server = this.parent.parent;
    var me = server.me;

    var modifier = (f) ? " +o " : " -o ";
    server.sendData("MODE " + this.parent.encodedName + modifier + this.encodedName + "\n");

    return true;
}

function cusr_sethalfop (f)
{
    var server = this.parent.parent;
    var me = server.me;

    var modifier = (f) ? " +h " : " -h ";
    server.sendData("MODE " + this.parent.encodedName + modifier + this.encodedName + "\n");

    return true;
}

function cusr_setvoice (f)
{
    var server = this.parent.parent;
    var me = server.me;

    var modifier = (f) ? " +v " : " -v ";
    server.sendData("MODE " + this.parent.encodedName + modifier + this.encodedName + "\n");

    return true;
}

function cusr_kick (reason)
{
    var server = this.parent.parent;
    var me = server.me;

    reason = typeof reason == "string" ? reason : "";

    server.sendData("KICK " + this.parent.encodedName + " " + this.encodedName + " :" +
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
    modifier += fromUnicode(this.getBanMask(), server) + " ";

    server.sendData("MODE " + this.parent.encodedName + modifier + "\n");

    return true;
}

function cusr_kban (reason)
{
    var server = this.parent.parent;
    var me = server.me;

    if (!this.host)
        return false;

    reason = (typeof reason != "undefined") ? reason : this.encodedName;
    var modifier = " -o+b " + this.encodedName + " " +
                   fromUnicode(this.getBanMask(), server) + " ";

    server.sendData("MODE " + this.parent.encodedName + modifier + "\n" +
                    "KICK " + this.parent.encodedName + " " +
                    this.encodedName + " :" + reason + "\n");

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
