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
 *   Peter Van der Beken, peter.vanderbeken@pandora.be, necko-only version
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

const NS_ERROR_MODULE_NETWORK = 2152398848;

const NS_ERROR_UNKNOWN_HOST = NS_ERROR_MODULE_NETWORK + 30;
const NS_ERROR_CONNECTION_REFUSED = NS_ERROR_MODULE_NETWORK + 13;
const NS_ERROR_NET_TIMEOUT = NS_ERROR_MODULE_NETWORK + 14;
const NS_ERROR_NET_RESET = NS_ERROR_MODULE_NETWORK + 20;

const NS_NET_STATUS_RESOLVING_HOST = NS_ERROR_MODULE_NETWORK + 3;
const NS_NET_STATUS_CONNECTED_TO = NS_ERROR_MODULE_NETWORK + 4;
const NS_NET_STATUS_SENDING_TO = NS_ERROR_MODULE_NETWORK + 5;
const NS_NET_STATUS_RECEIVING_FROM = NS_ERROR_MODULE_NETWORK + 6;
const NS_NET_STATUS_CONNECTING_TO = NS_ERROR_MODULE_NETWORK + 7;

const nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;

const nsIBinaryInputStream = Components.interfaces.nsIBinaryInputStream;
const nsIBinaryOutputStream = Components.interfaces.nsIBinaryOutputStream;

function toSInputStream(stream, binary)
{
    var sstream;

    if (binary)
    {
        sstream = Components.classes["@mozilla.org/binaryinputstream;1"];
        sstream = sstream.createInstance(nsIBinaryInputStream);
        sstream.setInputStream(stream);
    }
    else
    {
        sstream = Components.classes["@mozilla.org/scriptableinputstream;1"];
        sstream = sstream.createInstance(nsIScriptableInputStream);
        sstream.init(stream);
    }

    return sstream;
}

function toSOutputStream(stream, binary)
{
    var sstream;

    if (binary)
    {
        sstream = Components.classes["@mozilla.org/binaryoutputstream;1"];
        sstream = sstream.createInstance(Components.interfaces.nsIBinaryOutputStream);
        sstream.setOutputStream(stream);
    }
    else
    {
        sstream = stream;
    }

    return sstream;
}

function CBSConnection (binary)
{
    var sockServiceClass =
        Components.classesByID["{c07e81e0-ef12-11d2-92b6-00105a1b0d64}"];

    if (!sockServiceClass)
        throw ("Couldn't get socket service class.");

    var sockService = sockServiceClass.getService();
    if (!sockService)
        throw ("Couldn't get socket service.");

    this._sockService = sockService.QueryInterface
        (Components.interfaces.nsISocketTransportService);

    this.wrappedJSObject = this;
    this.binaryMode = binary || false;

    //if (!ASSERT(!this.binaryMode || jsenv.HAS_WORKING_BINARY_STREAMS,
    //            "Unable to use binary streams in this build."))
    //    return null;
}

CBSConnection.prototype.connect =
function bc_connect(host, port, bind, tcp_flag, isSecure, observer)
{
    if (typeof tcp_flag == "undefined")
        tcp_flag = false;

    this.host = host.toLowerCase();
    this.port = port;
    this.bind = bind;
    this.tcp_flag = tcp_flag;

    // Lets get a transportInfo for this
    var cls =
        Components.classes["@mozilla.org/network/protocol-proxy-service;1"];
    var pps = cls.getService(Components.interfaces.nsIProtocolProxyService);

    if (!pps)
        throw ("Couldn't get protocol proxy service");

    var ios = Components.classes["@mozilla.org/network/io-service;1"].
      getService(Components.interfaces.nsIIOService);
    var spec = "irc://" + host + ':' + port;
    var uri = ios.newURI(spec,null,null);
    var info = pps.examineForProxy(uri);

    if (jsenv.HAS_STREAM_PROVIDER)
    {
        if (isSecure)
        {
            this._transport = this._sockService.
                              createTransportOfType("ssl", host, port, info,
                                                    0, 0);
        }
        else
        {
            this._transport = this._sockService.
                              createTransport(host, port, info, 0, 0);
        }
        if (!this._transport)
            throw ("Error creating transport.");

        if (jsenv.HAS_NSPR_EVENTQ)
        {   /* we've got an event queue, so start up an async write */
            this._streamProvider = new StreamProvider (observer);
            this._write_req =
                this._transport.asyncWrite (this._streamProvider, this,
                                            0, -1, 0);
        }
        else
        {
            /* no nspr event queues in this environment, we can't use async
             * calls, so set up the streams. */
            this._outputStream = this._transport.openOutputStream(0, -1, 0);
            if (!this._outputStream)
                throw "Error getting output stream.";
            this._sOutputStream = toSOutputStream(this._outputStream,
                                                  this.binaryMode);

            this._inputStream = this._transport.openInputStream(0, -1, 0);
            if (!this._inputStream)
                throw "Error getting input stream.";
            this._sInputStream = toSInputStream(this._inputStream,
                                                this.binaryMode);
        }
    }
    else
    {
        /* use new necko interfaces */
        if (isSecure)
        {
            this._transport = this._sockService.
                              createTransport(["ssl"], 1, host, port, info);
        }
        else
        {
            this._transport = this._sockService.
                              createTransport(null, 0, host, port, info);
        }
        if (!this._transport)
            throw ("Error creating transport.");

        /* if we don't have an event queue, then all i/o must be blocking */
        var openFlags;
        if (jsenv.HAS_NSPR_EVENTQ)
            openFlags = 0;
        else
            openFlags = Components.interfaces.nsITransport.OPEN_BLOCKING;

        /* no limit on the output stream buffer */
        this._outputStream =
            this._transport.openOutputStream(openFlags, 4096, -1);
        if (!this._outputStream)
            throw "Error getting output stream.";
        this._sOutputStream = toSOutputStream(this._outputStream,
                                              this.binaryMode);

        this._inputStream = this._transport.openInputStream(openFlags, 0, 0);
        if (!this._inputStream)
            throw "Error getting input stream.";
        this._sInputStream = toSInputStream(this._inputStream,
                                            this.binaryMode);
    }

    this.connectDate = new Date();
    this.isConnected = true;

    return this.isConnected;

}

CBSConnection.prototype.listen =
function bc_listen(port, observer)
{
    var serverSockClass =
        Components.classes["@mozilla.org/network/server-socket;1"];

    if (!serverSockClass)
        throw ("Couldn't get server socket class.");

    var serverSock = serverSockClass.createInstance();
    if (!serverSock)
        throw ("Couldn't get server socket.");

    this._serverSock = serverSock.QueryInterface
        (Components.interfaces.nsIServerSocket);

    this._serverSock.init(port, false, -1);

    this._serverSockListener = new SocketListener(this, observer);

    this._serverSock.asyncListen(this._serverSockListener);

    this.port = this._serverSock.port;

    return true;
}

CBSConnection.prototype.accept =
function bc_accept(transport, observer)
{
    this._transport = transport;
    this.host = this._transport.host.toLowerCase();
    this.port = this._transport.port;
    //this.bind = bind;
    //this.tcp_flag = tcp_flag;

    if (jsenv.HAS_STREAM_PROVIDER)
    {
        if (jsenv.HAS_NSPR_EVENTQ)
        {   /* we've got an event queue, so start up an async write */
            this._streamProvider = new StreamProvider (observer);
            this._write_req =
                this._transport.asyncWrite (this._streamProvider, this,
                                            0, -1, 0);
        }
        else
        {
            /* no nspr event queues in this environment, we can't use async
             * calls, so set up the streams. */
            this._outputStream = this._transport.openOutputStream(0, -1, 0);
            if (!this._outputStream)
                throw "Error getting output stream.";
            this._sOutputStream = toSOutputStream(this._outputStream,
                                                  this.binaryMode);

            //this._scriptableInputStream =
            this._inputStream = this._transport.openInputStream(0, -1, 0);
            if (!this._inputStream)
                throw "Error getting input stream.";
            this._sInputStream = toSInputStream(this._inputStream,
                                                this.binaryMode);
        }
    }
    else
    {
        /* if we don't have an event queue, then all i/o must be blocking */
        var openFlags;
        if (jsenv.HAS_NSPR_EVENTQ)
            openFlags = 0;
        else
            openFlags = Components.interfaces.nsITransport.OPEN_BLOCKING;

        /* no limit on the output stream buffer */
        this._outputStream =
            this._transport.openOutputStream(openFlags, 4096, -1);
        if (!this._outputStream)
            throw "Error getting output stream.";
        this._sOutputStream = toSOutputStream(this._outputStream,
                                              this.binaryMode);

        this._inputStream = this._transport.openInputStream(openFlags, 0, 0);
        if (!this._inputStream)
            throw "Error getting input stream.";
        this._sInputStream = toSInputStream(this._inputStream,
                                            this.binaryMode);
    }

    this.connectDate = new Date();
    this.isConnected = true;

    // Clean up listening socket.
    this._serverSock.close();

    return this.isConnected;
}

CBSConnection.prototype.close =
function bc_close()
{
    if ("_serverSock" in this && this._serverSock)
        this._serverSock.close();
}

CBSConnection.prototype.disconnect =
function bc_disconnect()
{
    if ("_inputStream" in this && this._inputStream)
        this._inputStream.close();
    if ("_outputStream" in this && this._outputStream)
        this._outputStream.close();
    this.isConnected = false;
    /*
    this._streamProvider.close();
    if (this._streamProvider.isBlocked)
      this._write_req.resume();
    */
}

CBSConnection.prototype.sendData =
function bc_senddata(str)
{
    if (!this.isConnected)
        throw "Not Connected.";

    if (jsenv.HAS_NSPR_EVENTQ && jsenv.HAS_STREAM_PROVIDER)
        this.asyncWrite (str);
    else
        this.sendDataNow (str);
}

CBSConnection.prototype.readData =
function bc_readdata(timeout, count)
{
    if (!this.isConnected)
        throw "Not Connected.";

    var rv;

    try
    {
        if (this.binaryMode)
            rv = this._sInputStream.readBytes(count);
        else
            rv = this._sInputStream.read(count);
    }
    catch (ex)
    {
        dd ("*** Caught " + ex + " while reading.");
        this.disconnect();
        throw (ex);
    }

    return rv;
}

CBSConnection.prototype.startAsyncRead =
function bc_saread (observer)
{
    if (jsenv.HAS_STREAM_PROVIDER)
    {
        this._transport.asyncRead (new StreamListener (observer),
                                   this, 0, -1, 0);
    }
    else
    {
        var cls = Components.classes["@mozilla.org/network/input-stream-pump;1"];
        var pump = cls.createInstance(Components.interfaces.nsIInputStreamPump);
        pump.init(this._inputStream, -1, -1, 0, 0, false);
        pump.asyncRead(new StreamListener(observer), this);
    }
}

CBSConnection.prototype.asyncWrite =
function bc_awrite (str)
{
    this._streamProvider.pendingData += str;
    if (this._streamProvider.isBlocked)
    {
        this._write_req.resume();
        this._streamProvider.isBlocked = false;
    }
}

CBSConnection.prototype.hasPendingWrite =
function bc_haspwrite ()
{
    if (jsenv.HAS_STREAM_PROVIDER)
        return (this._streamProvider.pendingData != "");
    else
        return false; /* data already pushed to necko */
}

CBSConnection.prototype.sendDataNow =
function bc_senddatanow(str)
{
    var rv = false;

    try
    {
        if (this.binaryMode)
            this._sOutputStream.writeBytes(str, str.length);
        else
            this._sOutputStream.write(str, str.length);
        rv = true;
    }
    catch (ex)
    {
        dd ("*** Caught " + ex + " while sending.");
        this.disconnect();
        throw (ex);
    }

    return rv;
}

function _notimpl ()
{
    throw "Not Implemented.";
}

if (!jsenv.HAS_NSPR_EVENTQ)
{
    CBSConnection.prototype.startAsyncRead = _notimpl;
    CBSConnection.prototype.asyncWrite = _notimpl;
}
else if (jsenv.HAS_STREAM_PROVIDER)
{
    CBSConnection.prototype.sendDataNow = _notimpl;
}
else
{
    CBSConnection.prototype.asyncWrite = _notimpl;
}

delete _notimpl;

function StreamProvider(observer)
{
    this._observer = observer;
}

StreamProvider.prototype.pendingData = "";
StreamProvider.prototype.isBlocked = true;

StreamProvider.prototype.close =
function sp_close ()
{
    this.isClosed = true;
}

StreamProvider.prototype.onDataWritable =
function sp_datawrite (request, ctxt, ostream, offset, count)
{
    //dd ("StreamProvider.prototype.onDataWritable");

    if ("isClosed" in this && this.isClosed)
        throw Components.results.NS_BASE_STREAM_CLOSED;

    if (!this.pendingData)
    {
        this.isBlocked = true;

        /* this is here to support pre-XPCDOM builds (0.9.0 era), which
         * don't have this result code mapped. */
        if (!Components.results.NS_BASE_STREAM_WOULD_BLOCK)
            throw 2152136711;

        throw Components.results.NS_BASE_STREAM_WOULD_BLOCK;
    }

    var len = ostream.write (this.pendingData, this.pendingData.length);
    this.pendingData = this.pendingData.substr (len);
}

StreamProvider.prototype.onStartRequest =
function sp_startreq (request, ctxt)
{
    //dd ("StreamProvider::onStartRequest: " + request + ", " + ctxt);
}


StreamProvider.prototype.onStopRequest =
function sp_stopreq (request, ctxt, status)
{
    //dd ("StreamProvider::onStopRequest: " + request + ", " + ctxt + ", " +
    //    status);
    if (this._observer)
        this._observer.onStreamClose(status);
}

function StreamListener(observer)
{
    this._observer = observer;
}

StreamListener.prototype.onStartRequest =
function sl_startreq (request, ctxt)
{
    //dd ("StreamListener::onStartRequest: " + request + ", " + ctxt);
}

StreamListener.prototype.onStopRequest =
function sl_stopreq (request, ctxt, status)
{
    //dd ("StreamListener::onStopRequest: " + request + ", " + ctxt + ", " +
    //status);
    if (this._observer)
        this._observer.onStreamClose(status);
}

StreamListener.prototype.onDataAvailable =
function sl_dataavail (request, ctxt, inStr, sourceOffset, count)
{
    ctxt = ctxt.wrappedJSObject;
    if (!ctxt)
    {
        dd ("*** Can't get wrappedJSObject from ctxt in " +
            "StreamListener.onDataAvailable ***");
        return;
    }

    if (this._observer)
        this._observer.onStreamDataAvailable(request, inStr, sourceOffset,
                                             count);
}

function SocketListener(connection, observer)
{
    this._connection = connection;
    this._observer = observer;
}

SocketListener.prototype.onSocketAccepted =
function sl_onSocketAccepted(socket, transport)
{
    this._observer.onSocketAccepted(socket, transport);
}
SocketListener.prototype.onStopListening =
function sl_onStopListening(socket, status)
{
    delete this._connection._serverSockListener;
    delete this._connection._serverSock;
}
