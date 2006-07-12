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

function stringToIcal( data )
{
    if (!data || data == "") // assuming time-out
        throw Components.interfaces.calIWcapErrors.WCAP_LOGIN_FAILED;
    var icalRootComp = getIcsService().parseICS( data );
    checkWcapIcalErrno( icalRootComp );
    return icalRootComp;
}

function stringToXml( data )
{
    if (!data || data == "") // assuming time-out
        throw Components.interfaces.calIWcapErrors.WCAP_LOGIN_FAILED;
    var xml = getDomParser().parseFromString( data, "text/xml" );
    checkWcapXmlErrno( xml );
    return xml;
}

function UnicharReader( receiverFunc ) {
    this.wrappedJSObject = this;
    this.m_receiverFunc = receiverFunc;
}
UnicharReader.prototype = {
    m_receiverFunc: null,
    
    // nsIUnicharStreamLoaderObserver:
    onDetermineCharset:
    function( loader, context, firstSegment, length )
    {
        var charset = loader.channel.contentCharset;
        if (!charset || charset == "")
            charset = "UTF-8";
        return charset;
    },
    
    onStreamComplete:
    function( loader, context, status, /* nsIUnicharInputStream */ unicharData )
    {
        if (status == Components.results.NS_OK) {
            if (LOG_LEVEL > 2) {
                logMessage( "issueAsyncRequest( \"" +
                            loader.channel.URI.spec + "\" )",
                            "received stream." );
            }
            var str = "";
            if (unicharData) {
                var str_ = {};
                while (unicharData.readString( -1, str_ )) {
                    str += str_.value;
                }
            }
            if (LOG_LEVEL > 1) {
                logMessage( "issueAsyncRequest( \"" +
                            loader.channel.URI.spec + "\" )",
                            "contentCharset = " + loader.channel.contentCharset+
                            "\nrequest result:\n" + str );
            }
            this.m_receiverFunc( str );
        }
    }
};

function issueAsyncRequest( url, receiverFunc )
{
    var reader = null;
    if (receiverFunc != null) {
        reader = new UnicharReader( receiverFunc );
    }
    var loader =
        Components.classes["@mozilla.org/network/unichar-stream-loader;1"]
        .createInstance(Components.interfaces.nsIUnicharStreamLoader);
    logMessage( "issueAsyncRequest( \"" + url + "\" )", "opening channel." );
    var channel = getIoService().newChannel(
        url, "" /* charset */, null /* baseURI */ );
    loader.init( channel, reader, null /* context */, 0 /* segment size */ );
}

function streamToString( inStream, charset )
{
    if (LOG_LEVEL > 2) {
        logMessage( "streamToString()",
                    "inStream.available() = " + inStream.available() +
                    ", charset = " + charset );
    }
    // byte-array to string:
    var convStream =
        Components.classes["@mozilla.org/intl/converter-input-stream;1"]
        .createInstance(Components.interfaces.nsIConverterInputStream);
    convStream.init( inStream, charset, 0, 0x0000 );
    var str = "";
    var str_ = {};
    while (convStream.readString( -1, str_ )) {
        str += str_.value;
    }
    return str;
}

function issueSyncRequest( url, receiverFunc, bLogging )
{
    if (bLogging == undefined)
        bLogging = true;
    if (bLogging && LOG_LEVEL > 0) {
        logMessage( "issueSyncRequest( \"" + url + "\" )",
                    "opening channel." );
    }
    var channel = getIoService().newChannel(
        url, "" /* charset */, null /* baseURI */ );
    var stream = channel.open();
    if (bLogging && LOG_LEVEL > 1) {
        logMessage( "issueSyncRequest( \"" + url + "\" )",
                    "contentCharset = " + channel.contentCharset +
                    ", contentLength = " + channel.contentLength +
                    ", contentType = " + channel.contentType );
    }
    var status = channel.status;
    if (status == Components.results.NS_OK) {
        var charset = channel.contentCharset;
        if (!charset || charset == "")
            charset = "UTF-8";
        var str = streamToString( stream, charset );
        if (bLogging && LOG_LEVEL > 1) {
            logMessage( "issueSyncRequest( \"" + url + "\" )",
                        "returned: " + str );
        }
        if (receiverFunc) {
            receiverFunc( str );
        }
        return str;
    }
    else if (bLogging && LOG_LEVEL > 0) {
        logMessage( "issueSyncRequest( \"" + url + "\" )",
                    "failed: " + status );
    }
    throw status;
}

function issueSyncXMLRequest( url, receiverFunc, bLogging )
{
    var str = issueSyncRequest( url, null, bLogging );
    var xml = getDomParser().parseFromString( str, "text/xml" );
    if (receiverFunc) {
        receiverFunc( xml );
    }
    return xml;
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

