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

// This constant is an arbitrary large number. It is used to tell google to get
// many events, the exact number is not important.
const kMANY_EVENTS = 0x7FFFFFFF;

/**
 * calGoogleSession
 * This Implements a Session object to communicate with google
 *
 * @constructor
 * @class
 */
function calGoogleSession(aUsername) {

    this.mItemQueue = new Array();
    this.mObservers = new Array();
    this.mGoogleUser = aUsername;

    var username = { value: aUsername };
    var password = { value: null };

    // Try to get the password from the password manager
    if (passwordManagerGet(aUsername, password)) {
        this.mGooglePass = password.value;
        this.savePassword = true;
        LOG("Retrieved Password for " + aUsername + " in constructor");
    }
}

calGoogleSession.prototype = {

    QueryInterface: function cGS_QueryInterface(aIID) {
        if (!aIID.equals(Ci.nsIInterfaceRequestor) &&
            !aIID.equals(Ci.nsISupports)) {
            throw Cr.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },

    /* Member Variables */
    mGoogleUser: null,
    mGooglePass: null,
    mGoogleFullName: null,
    mAuthToken: null,
    mSessionID: null,

    mLoggingIn: false,
    mSavePassword: false,
    mItemQueue: null,

    mCalendarName: null,
    mObservers: null,

    /**
     * readonly attribute authToken
     *
     * The auth token returned from Google Accounts
     */
    get authToken() {
        return this.mAuthToken;
    },

    /**
     * attribute savePassword
     *
     * Sets if the password for this user should be saved or not
     */
    get savePassword() {
        return this.mSavePassword;
    },
    set savePassword(v) {
        return this.mSavePassword = v;
    },

    /**
     * attribute googleFullName
     *
     * The Full Name of the user. If this is unset, it will return the
     * this.googleUser, to ensure a non-zero value
     */
    get googleFullName() {
        return (this.mGoogleFullName ? this.mGoogleFullName :
                this.googleUser);
    },
    set googleFullName(v) {
        return this.mGoogleFullName = v;
    },

    /**
     * readonly attribute googleUser
     *
     * The username of the session. This does not necessarily have to be the
     * email found in /calendar/feeds/email/private/full
     */
    get googleUser() {
        return this.mGoogleUser;
    },

    /**
     * attribute googlePassword
     *
     * Sets the password used for the login process
     */
    get googlePassword() {
        return this.mGooglePass;
    },
    set googlePassword(v) {
        return this.mGooglePass = v;
    },

    /**
     * invalidate
     * Resets the Auth token and password.
     */
    invalidate: function cGS_invalidate() {
        this.mAuthToken = null;
        this.mGooglePass = null;

        passwordManagerRemove(this.mGoogleUser);
    },

    /**
     * failQueue
     * Fails all requests in this session's queue. Optionally only fail requests
     * for a certain calendar
     *
     * @param aCode     Failure Code
     * @param aCalendar The calendar to fail for. Can be null.
     */
    failQueue: function cGS_failQueue(aCode, aCalendar) {
        function cGS_failQueue_failInQueue(element, index, arr) {
            if (!aCalendar || (aCalendar && element.calendar == aCalendar)) {
                element.fail(aCode);
                return false;
            }
            return true;
        }

        this.mItemQueue = this.mItemQueue.filter(cGS_failQueue_failInQueue);
    },

    /**
     * loginAndContinue
     * Prepares a login request, then requests via #asyncRawRequest
     *
     *
     * @param     aRequest      The request that initiated the login
     */
    loginAndContinue: function cGS_loginAndContinue(aRequest) {

        if (this.mLoggingIn) {
            LOG("loginAndContinue called while logging in");
            return;
        }
        try {
            LOG("Logging in to " + this.mGoogleUser);

            // We need to have a user and should not be logging in
            ASSERT(!this.mLoggingIn);
            ASSERT(this.mGoogleUser);

            // Check if we have a password. If not, authentication may have
            // failed.
            if (!this.mGooglePass) {
                var username= { value: this.mGoogleUser };
                var password = { value: null };
                var savePassword = { value: false };

                // Try getting a new password, potentially switching sesssions.
                var calendarName = (aRequest.calendar ?
                                    aRequest.calendar.googleCalendarName :
                                    this.mGoogleUser);

                if (getCalendarCredentials(calendarName,
                                           username,
                                           password,
                                           savePassword)) {

                    LOG("Got the pw from the calendar credentials: " +
                        calendarName);

                    // If a different username was entered, switch sessions

                    if (aRequest.calendar &&
                        username.value != this.mGoogleUser) {

                        var newSession = getSessionByUsername(username.value);
                        newSession.googlePassword = password.value;
                        newSession.savePassword = savePassword.value;
                        setCalendarPref(aRequest.calendar,
                                        "googleUser",
                                        "CHAR",
                                        username.value);

                        // Set the new session for the calendar
                        aRequest.calendar.session = newSession;
                        LOG("Setting " + aRequest.calendar.name +
                            "'s Session to " + newSession.googleUser);

                        // Move all requests by this calendar to its new session
                        function cGS_login_moveToSession(element, index, arr) {
                            if (element.calendar == aRequest.calendar) {
                                LOG("Moving " + element.uri + " to " +
                                    newSession.googleUser);
                                newSession.asyncItemRequest(element);
                                return false;
                            }
                            return true;
                        }
                        this.mItemQueue = this.mItemQueue
                                          .filter(cGS_login_moveToSession);

                        // Do not complete the request here, since it has been
                        // moved. This is not an error though, so nothing is
                        // thrown.
                        return;
                    }

                    // If we arrive at this point, then the session was not
                    // changed. Just adapt the password from the dialog and
                    // continue.
                    this.mGooglePass = password.value;
                    this.savePassword = savePassword.value;
                } else {
                    LOG("Could not get any credentials for " +
                        calendarName + " (" +
                        this.mGoogleUser + ")");

                    // The User even canceled the login prompt asking for
                    // the user. This means we have to fail all requests
                    // that belong to that calendar and are in the queue. This
                    // will also include the request that initiated the login
                    // request, so that dosent need to be handled extra.
                    this.failQueue(Cr.NS_ERROR_NOT_AVAILABLE,
                                   aRequest.calendar);

                    // Unset the session in the requesting calendar, if the user
                    // canceled the login dialog that also asks for the
                    // username, then the session is not valid. This also
                    // prevents multiple login windows.
                    if (aRequest.calendar)
                        aRequest.calendar.session = null;

                    return;
                }
            }

            // Now we should have a password
            ASSERT(this.mGooglePass);

            // Start logging in
            this.mLoggingIn = true;

            // Get Version info
            var appInfo = Cc["@mozilla.org/xre/app-info;1"].
                          getService(Ci.nsIXULAppInfo);
            var source = appInfo.vendor + "-" +
                         appInfo.name + "-" +
                         appInfo.version;

            // Request Login
            var request = new calGoogleRequest(this);

            request.type = request.LOGIN;
            request.extraData = aRequest;
            request.setResponseListener(this, this.loginComplete);
            request.setUploadData("application/x-www-form-urlencoded",
                                  "Email=" + encodeURIComponent(this.mGoogleUser) +
                                  "&Passwd=" + encodeURIComponent(this.mGooglePass) +
                                  "&accountType=HOSTED_OR_GOOGLE" +
                                  "&source=" + encodeURIComponent(source) +
                                  "&service=cl");
            this.asyncRawRequest(request);
        } catch (e) {
            // If something went wrong, reset the login state just in case
            this.mLoggingIn = false;
            LOG({action:"Error Logging In",
                 result: e.result,
                 message: e.message});

            // If something went wrong, then this.loginComplete should handle
            // the error. We don't need to take care of aRequest, since it is
            // also in this.mItemQueue.
            this.loginComplete(null, e.result, e.message);
        }
    },

    /**
     * loginComplete
     * Callback function that is called when the login request to Google
     * Accounts has finished
     *  - Retrieves the Authentication Token
     *  - Saves the Password in the Password Manager
     *  - Processes the Item Queue
     *
     * @private
     * @param aRequest      The request object that initiated the login
     * @param aStatus       The return status of the request.
     * @param aResult       The (String) Result of the Request
     *                      (or an Error Message)
     */
    loginComplete: function cGS_loginComplete(aRequest, aStatus, aResult) {

        // About mLoggingIn: this should only be set to false when either
        // something went wrong or mAuthToken is set. This avoids redundant
        // logins to Google. Hence mLoggingIn is set three times in the course
        // of this function

        if (!aResult || aStatus != Cr.NS_OK) {
            this.mLoggingIn = false;
            LOG("Login failed. Status: " + aStatus);

            if (aStatus == kGOOGLE_LOGIN_FAILED) {
                // If the login failed, then retry the login. This is not an
                // error that should trigger failing the calICalendar's request.
                // The login request's extraData contains the request object
                // that triggered the login initially
                this.loginAndContinue(aRequest.extraData);
            } else {
                LOG("Failing queue with " + aStatus);
                this.failQueue(aStatus);
            }
        } else {
            var start = aResult.indexOf("Auth=");
            if (start == -1) {
                // The Auth token could not be extracted
                this.mLoggingIn = false;
                this.invalidate();

                // Retry login
                this.loginAndContinue(aRequest.extraData);
            } else {

                this.mAuthToken = aResult.substring(start+5,
                                                    aResult.length - 1);
                this.mLoggingIn = false;

                if (this.savePassword) {
                    try {
                        passwordManagerSave(this.mGoogleUser,
                                            this.mGooglePass);
                    } catch (e) {
                        // This error is non-fatal, but would constrict
                        // functionality
                        LOG("Error adding password to manager");
                    }
                }

                // Process Items that were requested while logging in
                var request;
                while (request = this.mItemQueue.shift()) {
                    LOG("Processing Queue Item: " + request.uri);
                    request.commit(this);
                }
            }
        }
    },

    /**
     * addItem
     * Add a single item to google.
     *
     * @param   aCalendar               An instance of calIGoogleCalendar this
     *                                  request belongs to. The event will be
     *                                  added to this calendar.
     * @param   aItem                   An instance of calIEvent to add
     * @param   aResponseListener       The function in aCalendar to call at
     *                                  completion.
     * @param   aExtraData              Extra data to be passed to the response
     *                                  listener
     */
    addItem: function cGS_addItem(aCalendar,
                                  aItem,
                                  aResponseListener,
                                  aExtraData) {

        var request = new calGoogleRequest(this);
        var xmlEntry = ItemToXMLEntry(aItem,
                                      this.mGoogleUser,
                                      this.googleFullName);

        request.type = request.ADD;
        request.uri = aCalendar.fullUri.spec;
        request.setUploadData("application/atom+xml; charset=UTF-8", xmlEntry);
        request.setResponseListener(aCalendar, aResponseListener);
        request.extraData = aExtraData;
        request.calendar = aCalendar;

        this.asyncItemRequest(request);
    },

    /**
     * modifyItem
     * Modify a single item from google.
     *
     * @param   aCalendar               An instance of calIGoogleCalendar this
     *                                  request belongs to.
     * @param   aNewItem                An instance of calIEvent to modify
     * @param   aResponseListener       The function in aCalendar to call at
     *                                  completion.
     * @param   aExtraData              Extra data to be passed to the response
     *                                  listener
     */
    modifyItem: function cGS_modifyItem(aCalendar,
                                        aNewItem,
                                        aResponseListener,
                                        aExtraData) {

        var request = new calGoogleRequest(this);

        var xmlEntry = ItemToXMLEntry(aNewItem,
                                      this.mGoogleUser,
                                      this.googleFullName);

        request.type = request.MODIFY;
        request.uri = getItemEditURI(aNewItem);
        request.setUploadData("application/atom+xml; charset=UTF-8", xmlEntry);
        request.setResponseListener(aCalendar, aResponseListener);
        request.extraData = aExtraData;
        request.calendar = aCalendar;

        this.asyncItemRequest(request);
    },

    /**
     * deleteItem
     * Delete a single item from google.
     *
     * @param   aCalendar               An instance of calIGoogleCalendar this
     *                                  request belongs to.
     *                                  belongs to.
     * @param   aItem                   An instance of calIEvent to delete
     * @param   aResponseListener       The function in aCalendar to call at
     *                                  completion.
     * @param   aExtraData              Extra data to be passed to the response
     *                                  listener
     */
    deleteItem: function cGS_deleteItem(aCalendar,
                                        aItem,
                                        aResponseListener,
                                        aExtraData) {

        var request = new calGoogleRequest(this);

        request.type = request.DELETE;
        request.uri = getItemEditURI(aItem);
        request.setResponseListener(aCalendar, aResponseListener);
        request.extraData = aExtraData;
        request.calendar = aCalendar;

        this.asyncItemRequest(request);
    },

    /**
     * getItem
     * Get a single item from google.
     *
     * @param   aCalendar               An instance of calIGoogleCalendar this
     *                                  request belongs to.
     * @param   aId                     The ID of the item requested
     * @param   aResponseListener       The function in aCalendar to call at
     *                                  completion.
     * @param   aExtraData              Extra data to be passed to the response
     *                                  listener
     */
    getItem: function cGS_getItem(aCalendar,
                                  aId,
                                  aResponseListener,
                                  aExtraData) {

        var request = new calGoogleRequest(this);

        request.type = request.GET;
        request.uri = aCalendar.fullUri.spec + "/" + aId;
        request.setResponseListener(aCalendar, aResponseListener);
        request.extraData = aExtraData;
        request.calendar = aCalendar;

        this.asyncItemRequest(request);
    },

    /**
     * getItems
     * Get a Range of items from google.
     *
     * @param   aCalendar               An instance of calIGoogleCalendar this
     *                                  request belongs to.
     * @param   aCount                  The maximum number of items to return
     * @param   aRangeStart             An instance of calIDateTime that limits
     *                                  the start date.
     * @param   aRangeEnd               An instance of calIDateTime that limits
     *                                  the end date.
     * @param   aItemReturnOccurrences  A boolean, wether to return single
     *                                  occurrences or not.
     * @param   aResponseListener       The function in aCalendar to call at
     *                                  completion.
     * @param   aExtraData              Extra data to be passed to the response
     *                                  listener
     */

    getItems: function cGS_getItems(aCalendar,
                                    aCount,
                                    aRangeStart,
                                    aRangeEnd,
                                    aItemReturnOccurrences,
                                    aResponseListener,
                                    aExtraData) {

        var rfcRangeStart = toRFC3339(aRangeStart);
        var rfcRangeEnd = toRFC3339(aRangeEnd);

        var request = new calGoogleRequest(this);

        request.type = request.GET;
        request.uri = aCalendar.fullUri.spec;
        request.setResponseListener(aCalendar, aResponseListener);
        request.extraData = aExtraData;
        request.calendar = aCalendar;

        // Request Parameters
        request.addQueryParameter("max-results",
                                  aCount ? aCount : kMANY_EVENTS);
        request.addQueryParameter("singleevents",
                                  aItemReturnOccurrences ? "true" : "false");
        request.addQueryParameter("start-min", rfcRangeStart);
        request.addQueryParameter("start-max", rfcRangeEnd);

        this.asyncItemRequest(request);
    },

    /**
     * asyncItemRequest
     * get or post an Item from or to Google using the Queue.
     *
     * @param aRequest          The Request Object. This is an instance of
     *                          calGoogleRequest
     */
    asyncItemRequest: function cGS_asyncItemRequest(aRequest) {

        if (!this.mLoggingIn && this.mAuthToken) {
            // We are not currently logging in and we have an auth token, so
            // directly try the login request
            this.asyncRawRequest(aRequest);
        } else {
            // Push the request in the queue to be executed later
            this.mItemQueue.push(aRequest);

            LOG("Adding item " + aRequest.uri + " to queue");

            // If we are logging in, then we are done since the passed request
            // will be processed when the login is complete. Otherwise start
            // logging in.
            if (!this.mLoggingIn && this.mAuthToken == null) {
                this.loginAndContinue(aRequest);
            }
        }
    },

    /**
     * asyncRawRequest
     * get or post an Item from or to Google without the Queue.
     *
     * @param aRequest          The Request Object. This is an instance of
     *                          calGoogleRequest
     */
    asyncRawRequest: function cGS_asyncRawRequest(aRequest) {
        // Request is handled by an instance of the calGoogleRequest
        // We don't need to keep track of these requests, they
        // pass to a listener or just die

        ASSERT(aRequest);
        aRequest.commit(this);
    }
};
