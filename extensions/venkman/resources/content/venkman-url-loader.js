/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is The JavaScript Debugger.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>, original author
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

const IOSERVICE_CTRID = "@mozilla.org/network/io-service;1";
const nsIIOService    = Components.interfaces.nsIIOService;
const SIS_CTRID       = "@mozilla.org/scriptableinputstream;1"
const nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;
const nsIChannel      = Components.interfaces.nsIChannel;
const nsIInputStream  = Components.interfaces.nsIInputStream;
const nsIRequest      = Components.interfaces.nsIRequest;

function _getChannelForURL (url)
{
    var serv = Components.classes[IOSERVICE_CTRID].getService(nsIIOService);
    if (!serv)
        throw new BadMojo(ERR_FAILURE);
    
    return serv.newChannel(url, null, null);

}

function loadURLNow (url)
{
    var chan = _getChannelForURL (url);
    chan.loadFlags |= nsIRequest.LOAD_BYPASS_CACHE;
    var instream = 
        Components.classes[SIS_CTRID].createInstance(nsIScriptableInputStream);
    instream.init (chan.open());

    var result = "";
    var avail;

    while ((avail = instream.available()) > 0)
        result += instream.read(avail);

    return result;
}

function loadURLAsync (url, observer)
{
    var chan = _getChannelForURL (url);
    chan.loadFlags |= nsIRequest.LOAD_BYPASS_CACHE;
    return chan.asyncOpen (new StreamListener (url, observer), null);
}
    
function StreamListener(url, observer)
{
    this.data = "";
    this.url = url;
    this.observer = observer;
}

StreamListener.prototype.onStartRequest =
function (request, context)
{
    //dd ("onStartRequest()");
}

StreamListener.prototype.onStopRequest =
function (request, context, status)
{
    //dd ("onStopRequest(): status: " + status /* + "\n"  + this.data*/);
    if (typeof this.observer.onComplete == "function")
        this.observer.onComplete (this.data, this.url, status);
}

StreamListener.prototype.onDataAvailable =
function (request, context, inStr, sourceOffset, count)
{
    //dd ("onDataAvailable(): " + count);
    // sometimes the inStr changes between onDataAvailable calls, so we
    // can't cache it.
    var sis = 
        Components.classes[SIS_CTRID].createInstance(nsIScriptableInputStream);
    sis.init(inStr);
    this.data += sis.read(count);
}
