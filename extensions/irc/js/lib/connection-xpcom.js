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

  this._bsc = newObject ("component://misc/bs/connection", "bsIConnection");
  if (!this._bsc)
	throw ("Error Creating component://misc/bs/connection");

}

CBSConnection.prototype.connect = function bs_connect (host, port,
                                                       bind, tcp_flag)
{
    if (typeof tcp_flag == "undefined")
	tcp_flag = false;
    
    this._bsc.init (host);
    this.host = host;
    this.port = port;
    this.bind = bind;
    this.tcp_flag = tcp_flag;

    this.isConnected = this._bsc.connect (port, bind, tcp_flag);

    return this.isConnected;
  
}

CBSConnection.prototype.disconnect = function bs_disconnect ()
{
    
    this.isConnected = false;
    return this._bsc.disconnect();

}

CBSConnection.prototype.sendData = function bs_send (str)
{

    try
    {
        var rv = this._bsc.sendData (str);
    }
    catch (ex)
    {
        if (typeof ex != "undefined")
        {
            this.isConnected = false;
            throw (ex);
        }
        else
            var rv = false;
    }

    return rv;        

}

CBSConnection.prototype.readData = function bs_read (timeout)
{
    
    try
    {
        var rv = this._bsc.readData(timeout);
    }
    catch (ex)
    {
        if (typeof ex != "undefined")
        {
            this.isConnected = false;
            throw (ex);
        }
        else
            var rv = "";
    }

    return rv;

}
