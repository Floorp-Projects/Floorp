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

/**
   Requests, either the queued calWcapRequest or an async network request.
   
   A request object is used to track an async action.
   While the action is running, isPending is true.
   Functions issuing an async action usually take a response function along
   with their parameters, typically named respFunc.
   That function is called *after* the action has ended (i.e. isPending of the
   issued action/request is false when called, status remains stable).
   The response function gets the ended request as first parameter to check
   whether the request has been successful and get its data.
   The request function itself may return either
   - a further calIWcapRequest request object, i.e. an async continuation
   - some data (incl null/undefined) which is the result of the async function,
     indicating that there is no further continuation
*/

var g_requestId = 0;
function generateRequestId() {
    ++g_requestId;
    return g_requestId;
}

function calWcapRequest(respFunc, logContext) {
    this.wrappedJSObject = this;
    this.m_logContext = logContext;
    this.m_id = generateRequestId();
    this.m_isPending = true;
    this.m_status = NS_OK;
    this.m_respFunc = respFunc;
    this.m_attachedRequests = [];
}
calWcapRequest.prototype = {
    m_logContext: null,
    m_parentRequest: null,
    m_id: 0,
    m_isPending: true,
    m_status: NS_OK,
    m_respFunc: null,
    m_attachedRequests: null,
    m_locked: false,
    
    get parentRequest() { return this.m_parentRequest; },
    set parentRequest(req) {
        if (this.parentRequest)
            logError("already has parent!", this);
        this.detachFromParent(); // detach without error
        return (this.m_parentRequest = req);
    },
    
    /** The following locking is necessary when scheduling multiple async
        requests; one cannot be sure that e.g. the first completes quickly
        and responds the whole parent request when detaching.
    */
    lockPending: function calWcapRequest_lockPending() {
        this.m_locked = true;
    },
    unlockPending: function calWcapRequest_unlockPending() {
        if (this.m_locked) {
            this.m_locked = false;
            // assures that respFunc is executed:
            if (this.m_attachedRequests.length == 0) {
                this.execRespFunc();
            }
        }
    },
    
    toString: function calWcapRequest_toString() {
        var ret = ("calWcapRequest id=" + this.id +
                   ", parent-id=" +
                   (this.parentRequest ? this.parentRequest.id : "<none>") +
                   " (" + this.m_logContext + ")");
        if (LOG_LEVEL > 2 && this.m_attachedRequests.length > 0) {
            ret += "\nattached requests:";
            for each ( var req in this.m_attachedRequests ) {
                ret += ("\n#" + req.id + "\t" + req);
            }
        }
        return ret;
    },
    
    attachSubRequest: function calWcapRequest_attachSubRequest(req)
    {
        if (req) {
            if (!this.m_attachedRequests.some(
                    function(req_) { return req.id == req_.id; } )) {
                if (req.isPending) {
                    this.m_attachedRequests.push(req);
                    req.parentRequest = this;
                    log("attachSubRequest()", this);
                }
                else if (!this.m_locked && this.m_attachedRequests.length == 0) {
                    this.execRespFunc(req.status);
                }
            }
            else {
                logError("request already attached: " + req.id, this);
            }
        }
    },
    
    detachSubRequest: function calWcapRequest_detachSubRequest(req, err)
    {
        this.m_attachedRequests = this.m_attachedRequests.filter(
            function(req_) { return req.id != req_.id; } );
        if (err) {
            // first failing sub request stops whole request:
            this.execRespFunc(err);
        }
        // assures that respFunc is executed after every sub request has been completed:
        else if (!this.m_locked && this.m_attachedRequests.length == 0) {
            this.execRespFunc();
        }
    },
    
    cancelAllSubRequests: function calWcapRequest_cancelAllSubRequests(status) {
        var attachedRequests = this.m_attachedRequests;
        this.m_attachedRequests = [];
        attachedRequests.forEach( function(req) { req.cancel(status); } );
    },
    
    detachFromParent: function calWcapRequest_detachFromParent(err) {
        var parentRequest = this.m_parentRequest;
        if (parentRequest) {
            this.m_parentRequest = null;
            parentRequest.detachSubRequest(this, err);
        }
    },
    
    execRespFunc: function calWcapRequest_execRespFunc(err, data)
    {
        if (this.isPending) {
            this.m_isPending = false;
            if (err)
                this.m_status = err;
            this.cancelAllSubRequests();
            var respFunc = this.m_respFunc;
            if (respFunc) {
                this.m_respFunc = null; // call once only
                if (LOG_LEVEL > 2) {
                    log("response exec: " + errorToString(err), this);
                }
                try {
                    respFunc(this, err, data);
                }
                catch (exc) {
                    this.m_status = exc;
                    logError(exc, this);
                }
            }
            this.detachFromParent(this.m_status);
        }
    },
    
    // calIWcapRequest:
    get id() {
        return this.m_id;
    },
    get isPending() {
        return this.m_isPending;
    },
    get succeeded() {
        return (!this.isPending && Components.isSuccessCode( getResultCode(this.status) ));
    },
    get status() {
        return (this.m_status === null ? NS_OK : this.m_status);
    },
    
    cancel: function calWcapRequest_cancel(status)
    {
        if (this.isPending) {
            this.m_isPending = false;
            log("cancel.", this);
            this.m_respFunc = null;
            if (!status)
                status = Components.results.NS_ERROR_FAILURE;
            this.m_status = status;
            this.cancelAllSubRequests();
            this.detachFromParent(); // detach without error
        }
    }
};

function calWcapNetworkRequest(channel, respFunc, bLogging) {
//     this.superClass(respFunc);
    this.wrappedJSObject = this;
    this.m_id = generateRequestId();
    this.m_channel = channel;
    this.m_respFunc = respFunc;
    this.m_bLogging = (bLogging === undefined ? true : bLogging);
}
// subClass(calWcapNetworkRequest, calWcapRequest);

calWcapNetworkRequest.prototype = {
    m_id: 0,
    m_channel: null,
    m_respFunc: null,
    m_bLogging: false,
    
    m_isPending: true,
    get isPending() { return this.m_isPending; },
    
    toString: function calWcapNetworkRequest_toString() {
        var ret = ("calWcapNetworkRequest id=" + this.id +
                   ", parent-id=" +
                   (this.parentRequest ? this.parentRequest.id : "<none>"));
        if (this.m_bLogging)
            ret += (" (" + this.m_channel.URI.spec + ")");
        return ret;
    },
    
    m_parentRequest: null,
    get parentRequest() { return this.m_parentRequest; },
    set parentRequest(req) {
        if (this.parentRequest)
            logError("already has parent!", this);
        this.detachFromParent(); // detach without error
        return (this.m_parentRequest = req);
    },
    
    get id() {
        return this.m_id;
    },
    
    detachFromParent: function calWcapNetworkRequest_detachFromParent(err) {
        var parentRequest = this.m_parentRequest;
        if (parentRequest) {
            this.m_parentRequest = null;
            parentRequest.detachSubRequest(this, err);
        }
    },
    
    cancel: function calWcapNetworkRequest_cancel(status) {
        if (this.isPending) {
            this.m_isPending = false;
            log("cancel.", this);
            this.m_respFunc = null;
            this.detachFromParent(); // detach without error
            // xxx todo: check whether this works on redirected channels!
            if (this.m_channel.isPending()) {
                log("cancelling netwerk request...", this);
                this.m_channel.cancel(NS_BINDING_FAILED);
            }
        }
    },
    
    execRespFunc: function calWcapNetworkRequest_execRespFunc(err, str)
    {
        if (this.isPending) {
            this.m_isPending = false;
            var respFunc = this.m_respFunc;
            if (respFunc) {
                this.m_respFunc = null; // call once only
                if (LOG_LEVEL > 2 && this.m_bLogging) {
                    log("response exec: " + errorToString(err), this);
                }
                try {
                    respFunc(err, str);
                    err = null; // may have been handled
                }
                catch (exc) {
                    logError(exc, this);
                    err = exc;
                }
            }
            this.detachFromParent(err);
        }
    },
    
    // nsIUnicharStreamLoaderObserver:
    onDetermineCharset: function calWcapNetworkRequest_onDetermineCharset(
        loader, context, firstSegment, length)
    {
        var channel = null;
        if (loader)
            channel = loader.channel;
        var charset = null;
        if (channel)
            charset = channel.contentCharset;
        if (!charset || charset.length == 0)
            charset = "UTF-8";
        return charset;
    },
    
    onStreamComplete: function calWcapNetworkRequest_onStreamComplete(
        loader, context, status, /* nsIUnicharInputStream */ unicharData)
    {
        if (LOG_LEVEL > 0 && this.m_bLogging) {
            log("status: " + errorToString(status), this);
        }
        switch (status) {
        case NS_BINDING_SUCCEEDED: {
            var err = null;
            var str = "";
            try {
                if (unicharData) {
                    var str_ = {};
                    while (unicharData.readString(-1, str_)) {
                        str += str_.value;
                    }
                }
                if (LOG_LEVEL > 2 && this.m_bLogging) {
                    log("contentCharset = " + this.onDetermineCharset(loader) +
                        "\nrequest result:\n" + str, this);
                }
            }
            catch (exc) {
                err = exc;
            }
            this.execRespFunc(err, str);
            break;
        }
        case NS_BINDING_REDIRECTED:
        case NS_BINDING_RETARGETED:
            // just status
            // xxx todo: in case of a redirected channel,
            // how to get that channel => cancel feature!
            break;
        default: // errors:
            this.execRespFunc(status);
            break;
        }
    }
};

function issueNetworkRequest(parentRequest, respFunc, url, bLogging)
{
    var channel;
    try {
        var loader = Components.classes["@mozilla.org/network/unichar-stream-loader;1"]
                               .createInstance(Components.interfaces.nsIUnicharStreamLoader);
        channel = getIoService().newChannel(url, "" /* charset */, null /* baseURI */);
        channel.loadFlags |= Components.interfaces.nsIRequest.LOAD_BYPASS_CACHE;
    }
    catch (exc) {
        respFunc(exc);
        return;
    }
    var netRequest = new calWcapNetworkRequest(channel, respFunc, bLogging);
    parentRequest.attachSubRequest(netRequest);
    log("opening channel.", netRequest);
    try {
        loader.init(channel, netRequest, null /*context*/, 0 /*segment size*/);
    }
    catch (exc) {
        netRequest.execRespFunc(exc);
    }
}

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

function stringToIcal( data, expectedErrno )
{
    if (!data || data == "") { // assuming time-out; WTF.
        throw new Components.Exception(
            "Login failed. Invalid session ID.",
            Components.interfaces.calIWcapErrors.WCAP_LOGIN_FAILED);
    }
    var icalRootComp;
    try {
        icalRootComp = getIcsService().parseICS(data);
    }
    catch (exc) { // map into more useful error string:
        throw new Components.Exception("error parsing ical data!",
                                       Components.interfaces.calIErrors.ICS_PARSE);
    }
    checkWcapIcalErrno(icalRootComp, expectedErrno);
    return icalRootComp;
}

function stringToXml( data, expectedErrno )
{
    if (!data || data == "") { // assuming time-out
        throw new Components.Exception(
            "Login failed. Invalid session ID.",
            Components.interfaces.calIWcapErrors.WCAP_LOGIN_FAILED);
    }
    var xml = getDomParser().parseFromString(data, "text/xml");
    checkWcapXmlErrno(xml, expectedErrno);
    return xml;
}

