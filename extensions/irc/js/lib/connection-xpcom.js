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
const NS_ERROR_UNKNOWN_PROXY_HOST = NS_ERROR_MODULE_NETWORK + 42;
const NS_ERROR_PROXY_CONNECTION_REFUSED = NS_ERROR_MODULE_NETWORK + 72;

// Offline error constants:
const NS_ERROR_BINDING_ABORTED = NS_ERROR_MODULE_NETWORK + 2;
const NS_ERROR_ABORT = 0x80004004;

const NS_NET_STATUS_RESOLVING_HOST = NS_ERROR_MODULE_NETWORK + 3;
const NS_NET_STATUS_CONNECTED_TO = NS_ERROR_MODULE_NETWORK + 4;
const NS_NET_STATUS_SENDING_TO = NS_ERROR_MODULE_NETWORK + 5;
const NS_NET_STATUS_RECEIVING_FROM = NS_ERROR_MODULE_NETWORK + 6;
const NS_NET_STATUS_CONNECTING_TO = NS_ERROR_MODULE_NETWORK + 7;

// Security Constants.
const STATE_IS_BROKEN = 1;
const STATE_IS_SECURE = 2;
const STATE_IS_INSECURE = 3;

const STATE_SECURE_LOW = 1;
const STATE_SECURE_HIGH = 2;

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
    /* Since 2003-01-17 18:14, Mozilla has had this contract ID for the STS.
     * Prior to that it didn't have one, so we also include the CID for the
     * STS back then - DO NOT UPDATE THE ID if it changes in Mozilla.
     */
    const sockClassByName =
        Components.classes["@mozilla.org/network/socket-transport-service;1"];
    const sockClassByID =
        Components.classesByID["{c07e81e0-ef12-11d2-92b6-00105a1b0d64}"];

    var sockServiceClass = (sockClassByName || sockClassByID);

    if (!sockServiceClass)
        throw ("Couldn't get socket service class.");

    var sockService = sockServiceClass.getService();
    if (!sockService)
        throw ("Couldn't get socket service.");

    this._sockService = sockService.QueryInterface
        (Components.interfaces.nsISocketTransportService);

    /* Note: as part of the mess from bug 315288 and bug 316178, ChatZilla now
     *       uses the *binary* stream interfaces for all network
     *       communications.
     *
     *       However, these interfaces do not exist prior to 1999-11-05. To
     *       make matters worse, an incompatible change to the "readBytes"
     *       method of this interface was made on 2003-03-13; luckly, this
     *       change also added a "readByteArray" method, which we will check
     *       for below, to determin if we can use the binary streams.
     */

    // We want to check for working binary streams only the first time.
    if (CBSConnection.prototype.workingBinaryStreams == -1)
    {
        CBSConnection.prototype.workingBinaryStreams = false;

        if (typeof nsIBinaryInputStream != "undefined")
        {
            var isCls = Components.classes["@mozilla.org/binaryinputstream;1"];
            var inputStream = isCls.createInstance(nsIBinaryInputStream);
            if ("readByteArray" in inputStream)
                CBSConnection.prototype.workingBinaryStreams = true;
        }
    }

    this.wrappedJSObject = this;
    if (typeof binary != "undefined")
        this.binaryMode = binary;
    else
        this.binaryMode = this.workingBinaryStreams;

    if (!ASSERT(!this.binaryMode || this.workingBinaryStreams,
                "Unable to use binary streams in this build."))
    {
        throw ("Unable to use binary streams in this build.");
    }
}

CBSConnection.prototype.workingBinaryStreams = -1;

CBSConnection.prototype.connect =
function bc_connect(host, port, config, observer)
{
    this.host = host.toLowerCase();
    this.port = port;

    if (typeof config != "object")
        config = {};

    // Lets get a transportInfo for this
    var pps = getService("@mozilla.org/network/protocol-proxy-service;1",
                         "nsIProtocolProxyService");
    if (!pps)
        throw ("Couldn't get protocol proxy service");

    var ios = getService("@mozilla.org/network/io-service;1", "nsIIOService");

    function getProxyFor(uri)
    {
        uri = ios.newURI(uri, null, null);
        // As of 2005-03-25, 'examineForProxy' was replaced by 'resolve'.
        if ("resolve" in pps)
            return pps.resolve(uri, 0);
        if ("examineForProxy" in pps)
            return pps.examineForProxy(uri);
        return null;
    };

    var proxyInfo = null;
    var usingHTTPCONNECT = false;
    if ("proxy" in config)
    {
        /* Force Necko to supply the HTTP proxy info if desired. For none,
         * force no proxy. Other values will get default treatment.
         */
        if (config.proxy == "http")
            proxyInfo = getProxyFor("http://" + host + ":" + port);
        else if (config.proxy != "none")
            proxyInfo = getProxyFor("irc://" + host + ":" + port);

        /* Since the proxy info is opaque, we need to check that we got
         * something for our HTTP proxy - we can't just check proxyInfo.type.
         */
        usingHTTPCONNECT = ((config.proxy == "http") && proxyInfo);
    }
    else
    {
        proxyInfo = getProxyFor("irc://" + host + ":" + port);
    }

    if (jsenv.HAS_STREAM_PROVIDER)
    {
        if (("isSecure" in config) && config.isSecure)
        {
            this._transport = this._sockService.
                              createTransportOfType("ssl", host, port,
                                                    proxyInfo, 0, 0);
        }
        else
        {
            this._transport = this._sockService.
                              createTransport(host, port, proxyInfo, 0, 0);
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
        if (("isSecure" in config) && config.isSecure)
        {
            this._transport = this._sockService.
                              createTransport(["ssl"], 1, host, port,
                                              proxyInfo);
        }
        else
        {
            this._transport = this._sockService.
                              createTransport(null, 0, host, port, proxyInfo);
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

    // Bootstrap the connection if we're proxying via an HTTP proxy.
    if (usingHTTPCONNECT)
    {
        this.sendData("CONNECT " + host + ":" + port + " HTTP/1.1\r\n\r\n");
    }

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
    this.close();

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

    if (!("_sInputStream" in this)) {
        this._sInputStream = toSInputStream(this._inputStream);
        dump("OMG, setting up _sInputStream!\n");
    }

    try
    {
        // XPCshell h4x
        if (typeof count == "undefined")
            count = this._sInputStream.available();
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

/* getSecurityState returns an array containing information about the security
 * of the connection. The array always has at least one item, which contains a
 * value from the STATE_IS_* enumeration at the top of this file. Iff this is
 * STATE_IS_SECURE, the array has a second item indicating the level of
 * security - a value from the STATE_SECURE_* enumeration.
 *
 * STATE_IS_BROKEN is returned if any errors occur, and STATE_IS_INSECURE is
 * returned for disconnected sockets.
 */
CBSConnection.prototype.getSecurityState =
function bc_getsecuritystate()
{
    if (!this.isConnected || !this._transport.securityInfo)
        return [STATE_IS_INSECURE];

    try
    {
        var sslSp = Components.interfaces.nsISSLStatusProvider;
        var sslStatus = Components.interfaces.nsISSLStatus;

        // Get the actual SSL Status
        sslSp = this._transport.securityInfo.QueryInterface(sslSp);
        sslStatus = sslSp.SSLStatus.QueryInterface(sslStatus);
        // Store appropriate status
        if (!("keyLength" in sslStatus) || !sslStatus.keyLength)
            return [STATE_IS_BROKEN];
        else if (sslStatus.keyLength >= 90)
            return [STATE_IS_SECURE, STATE_SECURE_HIGH];
        else
            return [STATE_IS_SECURE, STATE_SECURE_LOW];
    }
    catch (ex)
    {
        // Something goes wrong -> broken security icon
        dd("Exception getting certificate for connection: " + ex.message);
        return [STATE_IS_BROKEN];
    }
}

CBSConnection.prototype.getCertificate =
function bc_getcertificate()
{
    if (!this.isConnected || !this._transport.securityInfo)
        return null;

    var sslSp = Components.interfaces.nsISSLStatusProvider;
    var sslStatus = Components.interfaces.nsISSLStatus;

    // Get the actual SSL Status
    sslSp = this._transport.securityInfo.QueryInterface(sslSp);
    sslStatus = sslSp.SSLStatus.QueryInterface(sslStatus);

    // return the certificate
    return sslStatus.serverCert;
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

    if (!("_sInputStream" in ctxt))
        ctxt._sInputStream = toSInputStream(inStr, false);

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
