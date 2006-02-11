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
 * The Original Code is Ident Server for ChatZilla.
 *
 * The Initial Developer of the Original Code is Robert Marshall.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Marshall <rdm@rdmsoft.com> (original author)
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

// RFC1413-ish identification server for ChatZilla
// One Ident Server is used for all networks. When multiple networks are in the
// process of connecting, it won't stop listening until they're all done.

function IdentServer(parent)
{
    this.responses = new Array();
    this.listening = false;
    this.dns = getService("@mozilla.org/network/dns-service;1","nsIDNSService");

    this.parent = parent;
    this.eventPump = parent.eventPump;
}

IdentServer.prototype.start =
function ident_start()
{
    if (this.listening)
        return true;

    if (!this.socket)
        this.socket = new CBSConnection();

    if (!this.socket.listen(113, this))
        return false;

    this.listening = true;
    return true;
}

IdentServer.prototype.stop =
function ident_stop()
{
    if (!this.socket || !this.listening)
        return;

    // No need to destroy socket, listen() will work again.
    this.socket.close();
    this.listening = false;
}

IdentServer.prototype.addNetwork =
function ident_add(net, serv)
{
    var addr, dnsRecord = this.dns.resolve(serv.hostname, 0);

    while (dnsRecord.hasMore())
    {
        addr = dnsRecord.getNextAddrAsString();
        this.responses.push({net: net, ip: addr, port: serv.port,
                             username: net.INITIAL_NAME});
    }

    if (this.responses.length == 0)
        return false;

    // Start the ident server if necessary.
    if (!this.listening)
        return this.start();

    return true;
}

IdentServer.prototype.removeNetwork =
function ident_remove(net)
{
    var newResponses = new Array();

    for (var i = 0; i < this.responses.length; ++i)
    {
        if (this.responses[i].net != net)
            newResponses.push(this.responses[i]);
    }
    this.responses = newResponses;

    // Only stop listening if responses is empty - no networks need us anymore.
    if (this.responses.length == 0)
        this.stop();
}

IdentServer.prototype.onSocketAccepted =
function ident_gotconn(serv, transport)
{
    // Using the listening CBSConnection to accept would stop it listening.
    // A new CBSConnection does exactly what we want.
    var connection = new CBSConnection();
    connection.accept(transport);

    if (jsenv.HAS_NSPR_EVENTQ)
        connection.startAsyncRead(new IdentListener(this, connection));
    else
        throw "IdentServer requires HAS_NSPR_EVENTQ."; // FIXME
}

function IdentListener(server, connection)
{
    this.server = server;
    this.connection = connection;
}

IdentListener.prototype.onStreamDataAvailable =
function ident_listener_sda(request, inStream, sourceOffset, count)
{
    var ev = new CEvent("ident-listener", "data-available", this,
                        "onDataAvailable");
    ev.line = this.connection.readData(0, count);
    this.server.eventPump.routeEvent(ev);
}

IdentListener.prototype.onStreamClose =
function ident_listener_sclose(status)
{
}

IdentListener.prototype.onDataAvailable =
function ident_listener_dataavailable(e)
{
    var incomplete = (e.line.substr(-2) != "\r\n");
    var lines = e.line.split(/\r\n/);

    if (this.savedLine)
    {
        lines[0] = this.savedLine + lines[0];
        this.savedLine = "";
    }

    if (incomplete)
        this.savedLine = lines.pop()

    for (var i in lines)
    {
        var ev = new CEvent("ident-listener", "rawdata", this, "onRawData");
        ev.line = lines[i];
        this.server.eventPump.routeEvent(ev);
    }
}

IdentListener.prototype.onRawData =
function ident_listener_rawdata(e)
{
    var ports = e.line.match(/(\d+) *, *(\d+)/);
    // <port-on-server> , <port-on-client>
    // (where "server" is the ident server)

    if (!ports)
    {
        this.disconnect(); // same meaning as "ERROR : UNKNOWN-ERROR"
        return;
    }

    e.type = "parsedrequest";
    e.destObject = this;
    e.destMethod = "onParsedRequest";
    e.localPort = ports[1];
    e.remotePort = ports[2];
}

IdentListener.prototype.onParsedRequest =
function ident_listener_request(e)
{
    function response(str)
    {
        return e.localPort + " , " + e.remotePort + " : " + str + "\r\n";
    };

    function validPort(p)
    {
        return (p >= 1) && (p <= 65535);
    };

    if (!validPort(e.localPort) || !validPort(e.remotePort))
    {
        this.connection.sendData(response("ERROR : INVALID-PORT"));
        this.disconnect();
        return;
    }

    var found, responses = this.server.responses;
    for (var i = 0; i < responses.length; ++i)
    {
        if ((e.remotePort == responses[i].port) &&
            (this.connection._transport.host == responses[i].ip))
        {
            // charset defaults to US-ASCII
            // anything except an OS username should use OTHER
            // however, ircu sucks, so we can't do that.
            this.connection.sendData(response("USERID : CHATZILLA :" + 
                                              responses[i].username));
            found = true;
            break;
        }
    }

    if (!found)
        this.connection.sendData(response("ERROR : NO-USER"));

    // Spec gives us a choice: drop the connection, or listen for more queries.
    // Since IRC servers will only ever want one name, we disconnect.
    this.disconnect();
}
