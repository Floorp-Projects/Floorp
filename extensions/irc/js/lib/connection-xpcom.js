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
 * The Original Code is JSIRC Library
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 *  Peter Van der Beken, peter.vanderbeken@pandora.be, necko-only version
 *
 * depends on utils.js, and XPCOM/XPConnect in the JS environment
 *
 */

const NS_ERROR_MODULE_NETWORK = 2152398848;

const NS_ERROR_UNKNOWN_HOST = NS_ERROR_MODULE_NETWORK + 30;
const NS_ERROR_CONNECTION_REFUSED = NS_ERROR_MODULE_NETWORK + 13;
const NS_ERROR_NET_TIMEOUT = NS_ERROR_MODULE_NETWORK + 14;

const NS_NET_STATUS_RESOLVING_HOST = NS_ERROR_MODULE_NETWORK + 3;
const NS_NET_STATUS_CONNECTED_TO = NS_ERROR_MODULE_NETWORK + 4;
const NS_NET_STATUS_SENDING_TO = NS_ERROR_MODULE_NETWORK + 5;
const NS_NET_STATUS_RECEIVING_FROM = NS_ERROR_MODULE_NETWORK + 6;
const NS_NET_STATUS_CONNECTING_TO = NS_ERROR_MODULE_NETWORK + 7;

function toScriptableInputStream (i)
{
    var si = Components.classes["@mozilla.org/scriptableinputstream;1"];
    
    si = si.createInstance();
    si = si.QueryInterface(Components.interfaces.nsIScriptableInputStream);
    si.init(i);

    return si;
    
}

function CBSConnection ()
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

}

CBSConnection.prototype.connect =
function bc_connect(host, port, bind, tcp_flag, observer)
{
    if (typeof tcp_flag == "undefined")
		tcp_flag = false;
    
    this.host = host.toLowerCase();
    this.port = port;
    this.bind = bind;
    this.tcp_flag = tcp_flag;

    // Lets get a transportInfo for this
    var pps = Components.classes["@mozilla.org/network/protocol-proxy-service;1"].
        getService().
        QueryInterface(Components.interfaces.nsIProtocolProxyService);

    if (!pps)
        throw ("Couldn't get protocol proxy service");

    var uri = Components.classes["@mozilla.org/network/simple-uri;1"].
        createInstance(Components.interfaces.nsIURI);
    uri.spec = "irc:" + host + ':' + port;

    var info = pps.examineForProxy(uri);

    this._transport = this._sockService.createTransport (host, port, info,
                                                         0, 0);
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
    {   /* no nspr event queues in this environment, we can't use async calls,
         * so set up the streams. */
        this._outputStream = this._transport.openOutputStream(0, -1, 0);
        if (!this._outputStream)
            throw "Error getting output stream.";
        this._inputStream =
            toScriptableInputStream(this._transport.openInputStream (0, -1, 0));
        if (!this._inputStream)
            throw "Error getting input stream.";
    }    

    this.connectDate = new Date();
    this.isConnected = true;

    return this.isConnected;
  
}

CBSConnection.prototype.disconnect =
function bc_disconnect()
{
    if ("_inputStream" in this && this._inputStream)
        this._inputStream.close();
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

    if (jsenv.HAS_NSPR_EVENTQ)
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
        rv = this._inputStream.read (count);
    }
    catch (ex)
    {
        dd ("*** Caught " + ex + " while reading.")
        this.isConnected = false;
        throw (ex);
    }
    
    return rv;
}

CBSConnection.prototype.startAsyncRead =
function bc_saread (observer)
{
    this._transport.asyncRead (new StreamListener (observer), this, 0, -1, 0);
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
    return (this._streamProvider.pendingData != "");
}

CBSConnection.prototype.sendDataNow =
function bc_senddatanow(str)
{
    var rv = false;
    
    try
    {
        this._outputStream.write(str, str.length);
        rv = true;
    }
    catch (ex)
    {
        dd ("*** Caught " + ex + " while sending.")
        this.isConnected = false;
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
else
{
    CBSConnection.prototype.sendDataNow = _notimpl;
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
 
    if (this.isClosed)
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
    if (!ctxt._inputStream)
        ctxt._inputStream = toScriptableInputStream (inStr);

    if (this._observer)
        this._observer.onStreamDataAvailable(request, inStr, sourceOffset,
                                             count);
}
