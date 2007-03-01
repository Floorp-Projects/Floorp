/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Google Calendar Provider code.
 *
 * The Initial Developer of the Original Code is
 *   Philipp Kewisch (mozilla@kewis.ch)
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/**
 * calGoogleRequest
 * This class represents a HTTP request sent to Google
 *
 * @constructor
 * @class
 */
function calGoogleRequest(aSession) {
    this.mQueryParameters = new Array();
    this.mSession = aSession;
}

calGoogleRequest.prototype = {

    /* Members */
    mMethod: null,
    mUriString: null,
    mUploadContent: null,
    mUploadData: null,
    mResponseListenerObject: null,
    mResponseListenerFunc: null,
    mSession: null,
    mExtraData: null,
    mQueryParameters: null,
    mType: null,
    mCalendar: null,

    /* Constants */
    LOGIN: 1,
    GET: 2,
    ADD: 3,
    MODIFY: 4,
    DELETE: 5,

    QueryInterface: function cGR_QueryInterface(aIID) {
        if (!aIID.equals(Ci.nsISupports) &&
            !aIID.equals(Ci.nsIStreamLoaderObserver) &&
            !aIID.equals(Ci.nsIDocShellTreeItem) &&
            !aIID.equals(Ci.nsIInterfaceRequestor) &&
            !aIID.equals(Ci.nsIChannelEventSink) &&
            !aIID.equals(Ci.nsIProgressEventSink) &&
            !aIID.equals(Ci.nsIHttpEventSink) &&
            !aIID.equals(Ci.nsIStreamListener)) {
                throw Cr.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },

    /**
     * attribute type
     * The type of this reqest. Must be one of
     * LOGIN, GET, ADD, MODIFY, DELETE
     * This also sets the Request Method and for the LOGIN request also the uri
     */
    get type() { return this.mType; },

    set type(v) {
        switch (v) {
            case this.LOGIN:
                this.mMethod = "POST";
                this.mUriString = "https://www.google.com/accounts/ClientLogin";
                break;
            case this.META:
                this.uri = "http://www.google.com/calendar/feeds/" +
                           encodeURIComponent(this.mSession.googleUser);
                // Fall through
            case this.GET:
                this.mMethod = "GET";
                break;
            case this.ADD:
                this.mMethod = "POST";
                break;
            case this.MODIFY:
                this.mMethod = "PUT";
                break;
            case this.DELETE:
                this.mMethod = "DELETE";
                break;
            default:
                throw new Components.Exception("", Cr.NS_ERROR_ILLEGAL_VALUE);
                break;
        }
        this.mType = v;
        return v;
    },

    /**
     * attribute method
     * The Request Method to use (GET, POST, PUT, DELETE)
     */
    get method() {
        return this.mMethod;
    },
    set method(v) {
        return this.mMethod = v;
    },

    /**
     * attribute uri
     * The String representation of the uri to be requested
     */
    get uri() {
        return this.mUriString;
    },
    set uri(v) {
        return this.mUriString = v;
    },

    /**
     * setResponseListener
     * The Function and Object of the listener to be called on complete
     * operation.
     *
     * @param aObj    The object for the callback
     * @param aFunc   The function for the callback
     */
    setResponseListener: function cGR_setResponseListener(aObj, aFunc) {
        this.mResponseListenerObject = aObj;
        this.mResponseListenerFunc = aFunc;
    },

    /**
     * setUploadData
     * The HTTP body data for a POST or PUT request.
     *
     * @param aContentType The Content type of the Data
     * @param aData        The Data to upload
     */
    setUploadData: function cGR_setUploadData(aContentType, aData) {
        this.mUploadContent = aContentType;
        this.mUploadData = aData;
        if (this.mType == this.LOGIN) {
            LOG("Setting upload data for login request (hidden)");
        } else {
            LOG({action:"Setting Upload Data:",
                 content:aContentType,
                 data:aData});
        }
    },

    /**
     * attribute extraData
     * Extra Data to save with the request
     */
    get extraData() {
        return this.mExtraData;
    },
    set extraData(v) {
        return this.mExtraData = v;
    },

    /**
     * attribute calendar
     * The calendar that initiated this request. Will be used by the login
     * response function to determine how and where to set passwords.
     */
    get calendar() {
        return this.mCalendar;
    },
    set calendar(v) {
        return this.mCalendar = v;
    },

    /**
     * addQueryParameter
     * Adds a query parameter to this request. This will be used in conjunction
     * with the uri.
     *
     * @param aKey      The key of the request parameter.
     * @param aValue    The value of the request parameter. This parameter will
     *                  be escaped.
     */
    addQueryParameter: function cGR_addQueryParameter(aKey, aValue) {
        if (aValue == null || aValue == "") {
            // Silently ignore empty values.
            return;
        }
        this.mQueryParameters.push(aKey + "=" + encodeURIComponent(aValue));
    },

    /**
     * commit
     * Starts the request process. This can be called multiple times if the
     * request should be repeated
     *
     * @param aSession  The session object this request should be made with.
     *                  This parameter is optional
     */
    commit: function cGR_commit(aSession) {

        try {
            // Set the session to request with
            if (aSession) {
                this.mSession = aSession;
                if (this.mType == this.META) {
                    this.mUriString = "http://www.google.com/calendar/feeds/"
                                      + encodeURIComponent(aSession.googleUser);
                }
            }

            // create the channel
            var ioService = Cc["@mozilla.org/network/io-service;1"].
                            getService(Ci.nsIIOService);

            var uristring = this.mUriString;
            if (this.mQueryParameters.length > 0) {
                uristring += "?" + this.mQueryParameters.join("&");
            }
            var uri = ioService.newURI(uristring, null, null);
            var channel = ioService.newChannelFromURI(uri);

            this.prepareChannel(channel);

            channel = channel.QueryInterface(Ci.nsIHttpChannel);
            channel.redirectionLimit = 3;

            var streamLoader = Cc["@mozilla.org/network/stream-loader;1"].
                               createInstance(Ci.nsIStreamLoader);

            LOG("calGoogleRequest: Requesting " + this.mMethod + " " +
                channel.URI.spec);

            channel.notificationCallbacks = this;

            // Required to be trunk and branch compatible.
            if (Ci.nsIStreamLoader.number ==
                "{31d37360-8e5a-11d3-93ad-00104ba0fd40}") {
                streamLoader.init(channel, this, this);
            } else if (Ci.nsIStreamLoader.number ==
                      "{8ea7e890-8211-11d9-8bde-f66bad1e3f3a}") {
                streamLoader.init(this);
                channel.asyncOpen(streamLoader, this);
            }
        } catch (e) {
            // Let the response function handle the error that happens here
            this.fail(e.result, e.message);
        }
    },

    /**
     * fail
     * Call this request's listener with the given code and Message
     *
     * @param aCode     The Error code to fail with
     * @param aMessage  The Error message. If this is null, an error Message
     *                  from calIGoogleErrors will be used.
     */
    fail: function cGR_fail(aCode, aMessage) {
        this.mResponseListenerFunc.call(this.mResponseListenerObject,
                                        this,
                                        aCode,
                                        aMessage);
    },

    /**
     * succeed
     * Call this request's listener with a Success Code and the given Result.
     *
     * @param aResult   The result Text of this request.
     */
    succeed: function cGR_succeed(aResult) {
        // Succeeding is nothing more than failing with the result code set to
        // NS_OK.
        this.fail(Cr.NS_OK, aResult);
    },

    /**
     * prepareChannel
     * Prepares the passed channel to match this objects properties
     *
     * @param aChannel    The Channel to be prepared
     */
    prepareChannel: function cGR_prepareChannel(aChannel) {

        // No caching
        aChannel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;

        // Set upload Data
        if (this.mUploadData) {
            var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                            createInstance(Ci.nsIScriptableUnicodeConverter);
            converter.charset = "UTF-8";

            var stream = converter.convertToInputStream(this.mUploadData);
            aChannel = aChannel.QueryInterface(Ci.nsIUploadChannel);
            aChannel.setUploadStream(stream, this.mUploadContent, -1);
        }

        aChannel  = aChannel.QueryInterface(Ci.nsIHttpChannel);

        // Depending on the preference, we will use X-HTTP-Method-Override to
        // get around some proxies. This will default to true.
        if (getPrefSafe("calendar.google.useHTTPMethodOverride", true) &&
            (this.mMethod == "PUT" || this.mMethod == "DELETE")) {

            aChannel.requestMethod = "POST";
            aChannel.setRequestHeader("X-HTTP-Method-Override",
                                      this.mMethod,
                                      false);
            if (this.mMethod == "DELETE") {
                // DELETE has no body, set an empty one so that Google accepts
                // the request.
                aChannel.setRequestHeader("Content-Type",
                                          "application/atom+xml; charset=UTF-8",
                                          false);
                aChannel.setRequestHeader("Content-Length", 0, false);
            }
        } else {
            aChannel.requestMethod = this.mMethod;
        }

        // Add Authorization
        if (this.mSession.authToken) {
            aChannel.setRequestHeader("Authorization",
                                      "GoogleLogin auth="
                                      +  this.mSession.authToken,
                                      false);
        }
    },

    /**
     * @see nsIInterfaceRequestor
     */
    getInterface: function cGR_getInterface(aIID) {
        try {
            return this.QueryInterface(aIID);
        } catch (e) {
            throw Cr.NS_NOINTERFACE;
        }
    },

    /**
     * @see nsIChannelEventSink
     */
    onChannelRedirect: function cGR_onChannelRedirect(aOldChannel,
                                                      aNewChannel,
                                                      aFlags) {
        // all we need to do to the new channel is the basic preparation
        this.prepareChannel(aNewChannel);
    },

    /**
     * @see nsIHttpEventSink
     * (not implementing will cause annoying exceptions)
     */
    onRedirect: function cGR_onRedirect(aOldChannel, aNewChannel) { },

    /**
     * @see nsIProgressEventSink
     * (not implementing will cause annoying exceptions)
     */
    onProgress: function cGR_onProgress(aRequest,
                                        aContext,
                                        aProgress,
                                        aProgressMax) { },

    onStatus: function cGR_onStatus(aRequest,
                                    aContext,
                                    aStatus,
                                    aStatusArg) { },

    /**
     * @see nsIStreamLoaderObserver
     */
    onStreamComplete: function cGR_onStreamComplete(aLoader,
                                                    aContext,
                                                    aStatus,
                                                    aResultLength,
                                                    aResult) {
        if (!aResult || aStatus != Cr.NS_OK) {
            this.fail(aStatus, aResult);
            return;
        }

        var httpChannel = aLoader.request.QueryInterface(Ci.nsIHttpChannel);
        var uploadChannel = aLoader.request.QueryInterface(Ci.nsIUploadChannel);

        // Convert the stream, falling back to utf-8 in case its not given.
        var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                        createInstance(Ci.nsIScriptableUnicodeConverter);
        converter.charset = httpChannel.contentCharset || "UTF-8";
        var result;
        try {
            result = converter.convertFromByteArray(aResult, aResultLength);
        } catch (e) {
            this.fail(Cr.NS_ERROR_FAILURE,
                      "Could not convert bytestream to Unicode: " + e);
            return;
        }

        // Handle all (documented) error codes
        switch (httpChannel.responseStatus) {
            case 200: /* No error. */
            case 201: /* Creation of a resource was successful. */
                // Everything worked out, we are done
                this.succeed(result);
                break;

            case 401: /* Authorization required. */
            case 403: /* Unsupported standard parameter, or authentication or
                         Authorization failed. */
                LOG("Login failed for " + this.mSession.googleUser +
                    " HTTP Status " + httpChannel.responseStatus );

                // login failed. auth token must be invalid, password too

                if (this.type == this.MODIFY ||
                    this.type == this.DELETE ||
                    this.type == this.ADD) {
                    // Encountering this error on a write request means the
                    // calendar is readonly
                    this.fail(Ci.calIErrors.CAL_IS_READONLY, result);
                } else if (this.type == this.LOGIN) {
                    // If this was a login request itself, then fail it.
                    // That will take care of logging in again
                    this.mSession.invalidate();
                    this.fail(kGOOGLE_LOGIN_FAILED, result);
                } else {
                    // Retry the request. Invalidating the session will trigger
                    // a new login dialog.
                    this.mSession.invalidate();
                    this.mSession.asyncItemRequest(this);
                }

                break;
            case 409: /* Specified version number doesn't match resource's
                         latest version number. */

                // 409 Conflict. The client should get a newer version of the
                // event
                // and edit that one.

                // TODO Enhancement tracked in bug 362645
                // Fall through, if 409 is not handled then the event is not
                // modified/deleted, which is definitly an error.
            default:
                // The following codes are caught here:
                //  400 BAD REQUEST: Invalid request URI or header, or
                //                   unsupported nonstandard parameter.
                //  404 NOT FOUND: Resource (such as a feed or entry) not found.
                //  500 INTERNAL SERVER ERROR: Internal error. This is the
                //                             default code that is used for
                //                             all unrecognized errors.
                //

                // Something else went wrong
                var error = "A request Error Occurred. Status Code: " +
                            httpChannel.responseStatus + " " +
                            httpChannel.responseStatusText + " Body: " +
                            result;

                this.fail(Cr.NS_ERROR_FAILURE, error);
                break;
        }
    }
};
