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
 *   Patrick C. Beard <beard@netscape.com,
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
    this._inputStream = this._socket.getInputStream();
    this._outputStream = new java.io.PrintStream(this._socket.getOutputStream());
    
    // create a 512 byte buffer for reading into.
    this._buffer = java.lang.reflect.Array.newInstance(java.lang.Byte.TYPE, 512);
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
    // dd ("sendData: str=" + str);

    if (!this.isConnected)
        throw "Not Connected.";

    var rv = false;

        
    try
    {
        /* var s = new java.lang.String (str);
        b = s.getBytes();
        
        this._outputStream.write(b, 0, b.length); */
        this._outputStream.print(str);
        rv = true;
    }
    catch (ex)
    {
        dd ("write caught exception " + ex + ".");
        
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

    // dd ("readData: timeout " + timeout);
    
    try {
        this._socket.setSoTimeout(Number(timeout));
        var len = this._inputStream.read(this._buffer);
        // dd ("read done, len = " + len + ", buffer = " + this._buffer);

        rv = String(new java.lang.String(this._buffer, 0, 0, len));
    } catch (ex) {
        // dd ("read caught exception " + ex + ".");
        
        if ((typeof ex != "undefined") &&
            (!(ex instanceof java.io.InterruptedIOException)))
        {
            dd ("@@@ throwing " + ex);
            
            this.isConnected = false;
            throw (ex);
        } else {
            // dd ("int");
            rv = "";
        }
    }

    // dd ("readData: rv = '" + rv + "'");
    
    return rv;
}
