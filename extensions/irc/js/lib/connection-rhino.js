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
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 *
 *
 * depends on utils.js, XPCOM, and the XPCOM component
 *  component://misc/bs/connection
 *
 * sane wrapper around the insane bsIConnection component.  This
 * component needs to be replaced, or at least fixed, so this wrapper
 * will hopefully make it easy to do this in the future.
 *
 */

function CBSConnection ()
{
    this._socket = null;
}

CBSConnection.prototype.connect = function(host, port, bind, tcp_flag)
{
    if (typeof tcp_flag == "undefined")
        tcp_flag = false;
    
    this.host = host;
    this.port = port;
    this.bind = bind;
    this.tcp_flag = tcp_flag;
    
    this._socket = new java.net.Socket(host, port);
    this._inputStream =
        new java.io.DataInputStream(this._socket.getInputStream());
    this._outputStream = this._socket.getOutputStream();
    
    dd("connected to " + host);
	
    this.isConnected = true;

    return this.isConnected;
}

CBSConnection.prototype.disconnect = function()
{
    if (this.isConnected) {
        this.isConnected = false;
    	this._socket.close();
        delete this._socket;
        delete this._inputStream;
        delete this._outputStream;
    }
}

CBSConnection.prototype.sendData = function(str)
{
    if (!this.isConnected)
        throw "Not Connected.";

    var rv = false;

    try
    {
        this._outputStream.write(str.getBytes());
        rv = true;
    }
    catch (ex)
    {
        if (typeof ex != "undefined")
        {
            this.isConnected = false;
            throw (ex);
        }
        else
            rv = false;
    }
    
    return rv;        
}

CBSConnection.prototype.readData = function(timeout)
{
    if (!this.isConnected)
        throw "Not Connected.";

    var rv;

    dd ("readData: timeout " + timeout);
    
    try {
        this._socket.setSoTimeout(Number(timeout));
        rv = this._inputStream.read();
    } catch (ex) {
        
        if ((typeof ex != "undefined") &&
            (ex.indexOf("java.io.InterruptedIOException") != -1))
        {
            dd ("throwing " + ex);
            
            this.isConnected = false;
            throw (ex);
        } else {
            rv = "";
        }
    }

    dd ("readData: rv = '" + rv + "'");
    
    return rv;
}
