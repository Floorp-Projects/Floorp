/* -*- Mode: javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2006 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Daniel Boelzle (daniel.boelzle@sun.com)
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// WCAP request helpers
//

function getWcapRequestStatusString( xml )
{
    var str = "request status: ";
    var items = xml.getElementsByTagName("RSTATUS");
    if (items != null && items.length > 0)
        str += items.item(0).textContent;
    else
        str += "none";
    return str;
}

// response object for Calendar.issueRequest()
function WcapResponse() {
}
WcapResponse.prototype = {
    m_response: null,
    m_exc: null,
    
    get data() {
        if (this.m_exc != null)
            throw this.m_exc;
        return this.m_data;
    },
    set data(d) {
        this.m_data = d;
        this.m_exc = null;
    },
    set exception(exc) {
        this.m_exc = exc;
    }
};

function utf8ToIcal( utf8Data, wcapResponse )
{
    if (!utf8Data || utf8Data == "")
        return 1; // assuming session timeout
    var icalRootComp = getIcsService().parseICS( utf8Data );
    wcapResponse.data = icalRootComp;
    return getWcapIcalErrno( icalRootComp );
}

function utf8ToXml( utf8Data, wcapResponse )
{
    if (!utf8Data || utf8Data == "")
        return 1; // assuming session timeout
    var xml = getDomParser().parseFromString( utf8Data, "text/xml");
    wcapResponse.data = xml;
    return getWcapXmlErrno( xml );
}

function Utf8Reader( url, receiverFunc ) {
    this.wrappedJSObject = this;
    this.m_url = url;
    this.m_receiverFunc = receiverFunc;
}
Utf8Reader.prototype = {
    m_url: null,
    m_receiverFunc: null,
    
    // nsIUnicharStreamLoaderObserver:
    onDetermineCharset:
    function( loader, context, firstSegment, length )
    {
        return "UTF-8";
    },
    
    onStreamComplete:
    function( loader, context, status, /* nsIUnicharInputStream */ unicharData )
    {
        if (status == Components.results.NS_OK) {
            var str = "";
            var str_ = {};
            while (unicharData.readString( -1, str_ )) {
                str += str_.value;
            }
            if (LOG_LEVEL > 1) {
                logMessage( "issueAsyncUtf8Request( \"" + this.m_url + "\" )",
                            "request result: " + str );
            }
            this.m_receiverFunc( str );
        }
    }
};

function issueAsyncUtf8Request( url, receiverFunc )
{
    var reader = null;
    if (receiverFunc != null) {
        reader = new Utf8Reader( url, receiverFunc );
    }
    var loader =
        Components.classes["@mozilla.org/network/unichar-stream-loader;1"]
        .createInstance(Components.interfaces.nsIUnicharStreamLoader);
    logMessage( "issueAsyncUtf8Request( \"" + url + "\" )", "opening channel.");
    var channel = getIoService().newChannel(
        url, "UTF-8" /* charset */, null /* baseURI */ );
    loader.init( channel, reader, null /* context */, 0 /* segment size */ );
}

function streamToUtf8String( inStream )
{
    // byte-array to utf8 string:
    var convStream =
        Components.classes["@mozilla.org/intl/converter-input-stream;1"]
        .createInstance(Components.interfaces.nsIConverterInputStream);
    convStream.init( inStream, "UTF-8", 0, 0x0000 );
    var str = "";
    var str_ = {};
    while (convStream.readString( -1, str_ )) {
        str += str_.value;
    }
    return str;
}

function issueSyncUtf8Request( url, receiverFunc, bLogging )
{
    if (bLogging == undefined)
        bLogging = true;
    if (bLogging && LOG_LEVEL > 0) {
        logMessage( "issueSyncUtf8Request( \"" + url + "\" )",
                    "opening channel." );
    }
    var channel = getIoService().newChannel(
        url, "UTF-8" /* charset */, null /* baseURI */ );
    var stream = channel.open();
    var status = channel.status;
    if (status == Components.results.NS_OK) {
        var str = streamToUtf8String( stream );
        if (bLogging && LOG_LEVEL > 1) {
            logMessage( "issueSyncUtf8Request( \"" + url + "\" )",
                        "returned: " + str );
        }
        if (receiverFunc != null) {
            receiverFunc( str );
        }
        return str;
    }
    else if (bLogging && LOG_LEVEL > 0) {
        logMessage( "issueSyncUtf8Request( \"" + url + "\" )",
                    "failed: " + status );
    }
    return null;
}

function issueSyncXMLRequest( url, receiverFunc, bLogging )
{
    var str = issueSyncUtf8Request( url, null, bLogging );
    if (str != null) {
        var xml = getDomParser().parseFromString( str, "text/xml" );
        if (receiverFunc != null) {
            receiverFunc( xml );
        }
        return xml;
    }
    return null;
}

// response object for Calendar.issueRequest()
function RequestQueue() {
    this.m_requests = [];
}
RequestQueue.prototype = {
    m_requests: null,
    m_token: 0,
    
    postRequest:
    function( func )
    {
        var token = this.m_token;
        this.m_token += 1;
        this.m_requests.push( { m_token: token, m_func: func } );
        var len = this.m_requests.length;
        logMessage( "RequestQueue::postRequest()",
                    "queueing request. token=" + token +
                    ", open requests=" + len );
        if (len == 1) {
            func( token );
        }
    },
    
    requestCompleted:
    function( requestToken )
    {
        var len_ = this.m_requests.length;
        this.m_requests = this.m_requests.filter(
            function(x) { return x.m_token != requestToken; } );
        var len = this.m_requests.length;
        logMessage( "RequestQueue::requestCompleted()",
                    "token=" + requestToken +
                    ((len > 0 && len_ == len) ? "(expired !!!)" : "") +
                    ", open requests=" + len );
        if (len > 0) {
            var entry = this.m_requests[0];
            entry.m_func( entry.m_token );
        }
    }
};

