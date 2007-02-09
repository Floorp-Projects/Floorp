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

function calWcapSession() {
    this.wrappedJSObject = this;
    this.m_observers = [];
    this.m_loginQueue = [];
    this.m_subscribedCals = {};
}
calWcapSession.prototype = {
    m_ifaces: [ calIWcapSession,
                Components.interfaces.calICalendarManagerObserver,
                Components.interfaces.nsIInterfaceRequestor,
                Components.interfaces.nsIClassInfo,
                Components.interfaces.nsISupports ],
    
    // nsISupports:
    QueryInterface: function calWcapSession_QueryInterface(iid) {
        qiface(this.m_ifaces, iid);
        return this;
    },
    
    // nsIClassInfo:
    getInterfaces: function calWcapSession_getInterfaces(count)
    {
        count.value = this.m_ifaces.length;
        return this.m_ifaces;
    },
    get classDescription() {
        return calWcapCalendarModule.WcapSessionInfo.classDescription;
    },
    get contractID() {
        return calWcapCalendarModule.WcapSessionInfo.contractID;
    },
    get classID() {
        return calWcapCalendarModule.WcapSessionInfo.classID;
    },
    getHelperForLanguage:
    function calWcapSession_getHelperForLanguage(language) { return null; },
    implementationLanguage:
    Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0,
    
    // nsIInterfaceRequestor:
    getInterface: function calWcapSession_getInterface(iid, instance) {
        if (iid.equals(Components.interfaces.nsIAuthPrompt)) {
            // use the window watcher service to get a nsIAuthPrompt impl
            return getWindowWatcher().getNewAuthPrompter(null);
        }
        else if (iid.equals(Components.interfaces.nsIPrompt)) {
            // use the window watcher service to get a nsIPrompt impl
            return getWindowWatcher().getNewPrompter(null);
        }
        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },
    
    toString: function calWcapSession_toString(msg)
    {
        var str = (this.uri ? this.uri.spec : "no uri");
        if (this.credentials.userId) {
            str += (", userId=" + this.credentials.userId);
        }
        if (!this.m_sessionId) {
            str += (getIoService().offline ? ", offline" : ", not logged in");
        }
        return str;
    },
    notifyError: function calWcapSession_notifyError(err, suppressOnError)
    {
        debugger;
        var msg = logError(err, this);
        if (!suppressOnError) {
            this.notifyObservers(
                "onError",
                err instanceof Components.interfaces.nsIException
                ? [err.result, err.message] : [isNaN(err) ? -1 : err, msg]);
        }
    },
    
    m_lastOnErrorTime: 0,
    m_lastOnErrorNo: 0,
    m_lastOnErrorMsg: null,
    
    m_observers: null,
    notifyObservers: function calWcapSession_notifyObservers(func, args)
    {
        if (g_bShutdown)
            return;
        
        // xxx todo: hack
        // suppress identical error bursts when multiple similar calls eg on getItems() fail.
        if (func == "onError") {
            var now = (new Date()).getTime();
            if ((now - this.m_lastOnError) < 2000 &&
                (args[0] == this.m_lastOnErrorNo) &&
                (args[1] == this.m_lastOnErrorMsg)) {
                log("suppressing calIObserver::onError.", this);
                return;
            }
            this.m_lastOnError = now;
            this.m_lastOnErrorNo = args[0];
            this.m_lastOnErrorMsg = args[1];
        }
        
        this.m_observers.forEach(
            function notifyFunc(obj) {
                try {
                    obj[func].apply(obj, args);
                }
                catch (exc) {
                    // don't call notifyError() here:
                    Components.utils.reportError(exc);
                }
            });
    },
    
    addObserver: function calWcapSession_addObserver(observer)
    {
        if (this.m_observers.indexOf(observer) == -1)
            this.m_observers.push(observer);
    },
    
    removeObserver: function calWcapSession_removeObserver(observer)
    {
        this.m_observers = this.m_observers.filter(
            function filterFunc(x) { return x != observer; } );
    },
    
    m_serverTimezones: null,
    isSupportedTimezone: function calWcapSession_isSupportedTimezone(tzid)
    {
        if (!this.m_serverTimezones) {
            throw new Components.Exception(
                "early run into getSupportedTimezones()!",
                Components.results.NS_ERROR_NOT_AVAILABLE);
        }
        return this.m_serverTimezones.some(
            function someFunc(id) { return tzid == id; } );
    },
    
    m_serverTimeDiff: null,
    getServerTime: function calWcapSession_getServerTime(localTime)
    {
        if (this.m_serverTimeDiff === null) {
            throw new Components.Exception(
                "early run into getServerTime()!",
                Components.results.NS_ERROR_NOT_AVAILABLE);
        }
        var ret = (localTime ? localTime.clone() : getTime());
        ret.addDuration(this.m_serverTimeDiff);
        return ret;
    },
    
    m_sessionId: null,
    m_loginQueue: null,
    m_loginLock: false,
    m_subscribedCals: null,
    
    getSessionId:
    function calWcapSession_getSessionId(request, respFunc, timedOutSessionId)
    {
        if (getIoService().offline) {
            log("in offline mode.", this);
            respFunc(new Components.Exception(
                         "The requested action could not be completed while the " +
                         "networking library is in the offline state.",
                         NS_ERROR_OFFLINE));
            return;
        }
        
        log("login queue lock: " + this.m_loginLock +
            ", length: " + this.m_loginQueue.length, this);
        
        if (this.m_loginLock) {
            var entry = {
                request: request,
                respFunc: respFunc
            };
            this.m_loginQueue.push(entry);
            log("login queue: " + this.m_loginQueue.length);
        }
        else {
            if (this.m_sessionId && this.m_sessionId != timedOutSessionId) {
                respFunc(null, this.m_sessionId);
                return;
            }
            
            this.m_loginLock = true;
            log("locked login queue.", this);
            this.m_sessionId = null; // invalidate for relogin
            this.assureInstalledLogoutObservers();
            
            if (timedOutSessionId)
                log("reconnecting due to session timeout...", this);
            
            var this_ = this;
            this.getSessionId_(
                request,
                function getSessionId_resp(err, sessionId) {
                    log("getSessionId_resp(): " + sessionId, this_);
                    if (err) {
                        this_.notifyError(err, request.suppressOnError);
                    }
                    else {
                        this_.m_sessionId = sessionId;
                    }
                    
                    var queue = this_.m_loginQueue;
                    this_.m_loginLock = false;
                    this_.m_loginQueue = [];
                    log("unlocked login queue.", this_);
                    
                    if (request.isPending) {
                        // answere first request:
                        respFunc(err, sessionId);
                    }
                    // and any remaining:
                    while (queue.length > 0) {
                        var entry = queue.shift();
                        if (entry.request.isPending)
                            entry.respFunc(err, sessionId);
                    }
                });
        }
    },
    
    getSessionId_: function calWcapSession_getSessionId_(request, respFunc)
    {
        var this_ = this;
        this.getLoginText(
            request,
            // probe whether server is accessible and responds login text:
            function getLoginText_resp(err, loginText) {
                if (err) {
                    respFunc(err);
                    return;
                }
                // lookup password manager, then try login or prompt/login:
                log("attempting to get a session id for " + this_.sessionUri.spec, this_);
                
                if (!this_.sessionUri.schemeIs("https") &&
                    !confirmInsecureLogin(this_.sessionUri)) {
                    log("user rejected insecure login on " + this_.sessionUri.spec, this_);
                    respFunc(new Components.Exception(
                                 "Login failed. Invalid session ID.",
                                 calIWcapErrors.WCAP_LOGIN_FAILED));
                    return;
                }
                
                var outUser = { value: this_.credentials.userId };
                var outPW = { value: this_.credentials.pw };
                var outSavePW = { value: false };
                
                // pw mgr host names must not have a trailing slash
                var passwordManager =
                    Components.classes["@mozilla.org/passwordmanager;1"]
                              .getService(Components.interfaces.nsIPasswordManager);
                var pwHost = this_.sessionUri.spec;
                if (pwHost[pwHost.length - 1] == '/')
                    pwHost = pwHost.substr(0, pwHost.length - 1);
                
                if (!outPW.value) { // lookup pw manager
                    log("looking in pw db for: " + pwHost, this_);
                    try {
                        var enumerator = passwordManager.enumerator;
                        while (enumerator.hasMoreElements()) {
                            var pwEntry = enumerator.getNext().QueryInterface(
                                Components.interfaces.nsIPassword);
                            if (LOG_LEVEL > 1) {
                                log("pw entry:\n\thost=" + pwEntry.host +
                                    "\n\tuser=" + pwEntry.user, this_);
                            }
                            if (pwEntry.host == pwHost) {
                                // found an entry matching URI:
                                outUser.value = pwEntry.user;
                                outPW.value = pwEntry.password;
                                log("password entry found for host " + pwHost +
                                    "\nuser is " + outUser.value, this_);
                                break;
                            }
                        }
                    }
                    catch (exc) { // just log error
                        logError("[password manager lookup] " + errorToString(exc), this_);
                    }
                }
                
                function promptAndLoginLoop_resp(err, sessionId) {
                    if (getResultCode(err) == calIWcapErrors.WCAP_LOGIN_FAILED) {
                        log("prompting for user/pw...", this_);
                        var prompt = getWindowWatcher().getNewPrompter(null);
                        if (prompt.promptUsernameAndPassword(
                                getWcapBundle().GetStringFromName("loginDialog.label"),
                                loginText, outUser, outPW,
                                getPref("signon.rememberSignons", true)
                                ? getWcapBundle().GetStringFromName("loginDialog.check.text")
                                : null, outSavePW)) {
                            this_.login(request, promptAndLoginLoop_resp,
                                        outUser.value, outPW.value);
                        }
                        else {
                            log("login prompt cancelled.", this_);
                            respFunc(new Components.Exception(
                                         "Login failed. Invalid session ID.",
                                         calIWcapErrors.WCAP_LOGIN_FAILED));
                        }
                    }
                    else if (err)
                        respFunc(err);
                    else {
                        if (outSavePW.value) {
                            // so try to remove old pw from db first:
                            try {
                                passwordManager.removeUser(pwHost, outUser.value);
                                log("removed from pw db: " + pwHost, this_);
                            }
                            catch (exc) {
                            }
                            try { // to save pw under session uri:
                                passwordManager.addUser(pwHost, outUser.value, outPW.value);
                                log("added to pw db: " + pwHost, this_);
                            }
                            catch (exc) {
                                logError("[adding pw to db] " + errorToString(exc), this_);
                            }
                        }
                        this_.credentials.userId = outUser.value;
                        this_.credentials.pw = outPW.value;
                        this_.setupSession(sessionId,
                                           request,
                                           function setupSession_resp(err) {
                                               respFunc(err, sessionId);
                                           });
                    }
                }
                    
                if (outPW.value) {
                    this_.login(request, promptAndLoginLoop_resp,
                                outUser.value, outPW.value);
                }
                else {
                    promptAndLoginLoop_resp(calIWcapErrors.WCAP_LOGIN_FAILED);
                }
            });
    },
    
    login: function calWcapSession_login(request, respFunc, user, pw)
    {
        var this_ = this;
        issueNetworkRequest(
            request,
            function netResp(err, str) {
                var sessionId;
                try {
                    if (err)
                        throw err;
                    // currently, xml parsing at an early stage during
                    // process startup does not work reliably, so use
                    // libical parsing for now:
                    var icalRootComp = stringToIcal(str);
                    var prop = icalRootComp.getFirstProperty("X-NSCP-WCAP-SESSION-ID");
                    if (!prop) {
                        throw new Components.Exception(
                            "missing X-NSCP-WCAP-SESSION-ID in\n" + str);
                    }
                    sessionId = prop.value;       
                    log("login succeeded: " + sessionId, this_);
                }
                catch (exc) {
                    err = exc;
                    var rc = getResultCode(exc);
                    if (rc == calIWcapErrors.WCAP_LOGIN_FAILED) {
                        logError(exc, this_); // log login failure
                    }
                    else if (getErrorModule(rc) == NS_ERROR_MODULE_NETWORK) {
                        // server seems unavailable:
                        err = new Components.Exception(
                            getWcapBundle().formatStringFromName(
                                "accessingServerFailedError.text",
                                [this_.sessionUri.hostPort], 1),
                            exc);
                    }
                }
                respFunc(err, sessionId);
            },
            this_.sessionUri.spec + "login.wcap?fmt-out=text%2Fcalendar&user=" +
            encodeURIComponent(user) + "&password=" + encodeURIComponent(pw),
            false /* no logging */);
    },
    
    logout: function calWcapSession_logout(listener)
    {
        this.unregisterSubscribedCals();
        
        var this_ = this;
        var request = new calWcapRequest(
            function logout_resp(request, err) {
                if (err)
                    logError(err, this_);
                else
                    log("logout succeeded.", this_);
                if (listener)
                    listener.onRequestResult(request, err);
            },
            log("logout", this));
        
        var url = null;
        if (this.m_sessionId) {
            log("attempting to log out...", this);
            // although io service's offline flag is already
            // set BEFORE notification
            // (about to go offline, nsIOService.cpp).
            // WTF.
            url = (this.sessionUri.spec + "logout.wcap?fmt-out=text%2Fxml&id=" +
                   encodeURIComponent(this.m_sessionId));
            this.m_sessionId = null;
        }
        this.m_credentials = null;
        
        if (url) {
            issueNetworkRequest(
                request,
                function netResp(err, str) {
                    if (err)
                        throw err;
                    stringToXml(str, -1 /* logout successfull */);
                }, url);
        }
        else {
            request.execRespFunc();
        }
        return request;
    },
    
    getLoginText: function calWcapSession_getLoginText(request, respFunc)
    {
        // currently, xml parsing at an early stage during process startup
        // does not work reliably, so use libical:
        var this_ = this;
        issueNetworkRequest(
            request,
            function netResp(err, str) {
                var loginText;
                try {
                    var icalRootComp;
                    if (!err) {
                        try {
                            icalRootComp = stringToIcal(str);
                        }
                        catch (exc) {
                            err = exc;
                        }
                    }
                    if (err) { // soft error; request denied etc.
                               // map into localized message:
                        throw new Components.Exception(
                            getWcapBundle().formatStringFromName(
                                "accessingServerFailedError.text",
                                [this_.sessionUri.hostPort], 1),
                            calIWcapErrors.WCAP_LOGIN_FAILED);
                    }
                    var prop = icalRootComp.getFirstProperty("X-NSCP-WCAPVERSION");
                    if (!prop)
                        throw new Components.Exception("missing X-NSCP-WCAPVERSION!");
                    var wcapVersion = parseInt(prop.value);
                    if (wcapVersion < 3) {
                        var strVers = prop.value;
                        var vars = [this_.sessionUri.hostPort];
                        prop = icalRootComp.getFirstProperty("PRODID");
                        vars.push(prop ? prop.value : "<unknown>");
                        prop = icalRootComp.getFirstProperty("X-NSCP-SERVERVERSION");
                        vars.push(prop ? prop.value : "<unknown>");
                        vars.push(strVers);
                        
                        var prompt = getWindowWatcher().getNewPrompter(null);
                        var bundle = getWcapBundle();
                        var labelText = bundle.GetStringFromName(
                            "insufficientWcapVersionConfirmation.label");
                        if (!prompt.confirm(
                                labelText,
                                bundle.formatStringFromName(
                                    "insufficientWcapVersionConfirmation.text",
                                    vars, vars.length))) {
                            throw new Components.Exception(
                                labelText, calIWcapErrors.WCAP_LOGIN_FAILED);
                        }
                    }
                    loginText = getWcapBundle().formatStringFromName(
                        "loginDialog.text", [this_.sessionUri.hostPort], 1);
                }
                catch (exc) {
                    err = exc;
                }
                respFunc(err, loginText);
            },
            this_.sessionUri.spec + "version.wcap?fmt-out=text%2Fcalendar");
    },
    
    setupSession:
    function calWcapSession_setupSession(sessionId, request_, respFunc)
    {
        var this_ = this;
        var request = new calWcapRequest(
            function setupSession_resp(request_, err) {
                log("setupSession_resp finished: " + errorToString(err), this_);
                respFunc(err);
            },
            log("setupSession", this));
        request_.attachSubRequest(request);
        
        request.lockPending();
        try {
            var this_ = this;
            this.issueNetworkRequest_(
                request,
                function userprefs_resp(err, data) {
                    if (err)
                        throw err;
                    this_.credentials.userPrefs = data;
                    log("installed user prefs.", this_);
                    
                    this_.issueNetworkRequest_(
                        request,
                        function calprops_resp(err, data) {
                            if (err)
                                throw err;
                            this_.defaultCalendar.m_calProps = data;
                            log("installed default cal props.", this_);
                        },
                        stringToXml, "search_calprops",
                        "&fmt-out=text%2Fxml&searchOpts=3&calid=1&search-string=" +
                        encodeURIComponent(this_.defaultCalId),
                        sessionId);
                    if (getPref("calendar.wcap.subscriptions", false))
                        this_.installSubscribedCals(sessionId, request);
                },
                stringToXml, "get_userprefs",
                "&fmt-out=text%2Fxml&userid=" + encodeURIComponent(this.credentials.userId),
                sessionId);
            this.installServerTimeDiff(sessionId, request);
            this.installServerTimezones(sessionId, request);
        }
        finally {
            request.unlockPending();
        }
    },
    
    installServerTimeDiff:
    function calWcapSession_installServerTimeDiff(sessionId, request)
    {
        var this_ = this;
        this.issueNetworkRequest_(
            request,
            function netResp(err, data) {
                if (err)
                    throw err;
                // xxx todo: think about
                // assure that locally calculated server time is smaller
                // than the current (real) server time:
                var localTime = getTime();
                var serverTime = getDatetimeFromIcalProp(
                    data.getFirstProperty("X-NSCP-WCAPTIME"));
                this_.m_serverTimeDiff = serverTime.subtractDate(localTime);
                log("server time diff is: " + this_.m_serverTimeDiff, this_);
            },
            stringToIcal, "gettime", "&fmt-out=text%2Fcalendar",
            sessionId);
    },
    
    installServerTimezones:
    function calWcapSession_installServerTimezones(sessionId, request)
    {
        this.m_serverTimezones = [];
        var this_ = this;
        this_.issueNetworkRequest_(
            request,
            function netResp(err, data) {
                if (err)
                    throw err;
                var tzids = [];
                var icsService = getIcsService();
                forEachIcalComponent(
                    data, "VTIMEZONE",
                    function eachComp(subComp) {
                        try {
                            var tzCal = icsService.createIcalComponent("VCALENDAR");
                            subComp = subComp.clone();
                            tzCal.addSubcomponent(subComp);
                            icsService.addTimezone(tzCal, "", "");
                            this_.m_serverTimezones.push(
                                subComp.getFirstProperty("TZID").value);
                        }
                        catch (exc) { // ignore but errors:
                            logError(exc, this_);
                        }
                    });
                log("installed timezones.", this_);
            },
            stringToIcal, "get_all_timezones", "&fmt-out=text%2Fcalendar",
            sessionId);
    },
    
    installSubscribedCals:
    function calWcapSession_installSubscribedCals(sessionId, request)
    {
        var this_ = this;
        // user may have dangling users referred in his subscription list, so
        // retrieve each by each, don't break:
        var list = this.getUserPreferences("X-NSCP-WCAP-PREF-icsSubscribed");
        var calIds = {};
        for each (var item in list) {
            var ar = item.split(',');
            // ',', '$' are not encoded. ',' can be handled here. WTF.
            for each (var a in ar) {
                var dollar = a.indexOf('$');
                if (dollar >= 0) {
                    var calId = a.substring(0, dollar);
                    if (calId != this.defaultCalId)
                        calIds[calId] = true;
                }
            }
        }
        var issuedSearchRequests = {};
        for (var calId in calIds) {
            if (!this_.m_subscribedCals[calId]) {
                var listener = {
                    onRequestResult: function search_onRequestResult(request, result) {
                        try {
                            if (!request.succeeded)
                                throw request.status;
                            if (result.length < 1)
                                throw Components.results.NS_ERROR_UNEXPECTED;
                            for each (var cal in result) {
                                try {
                                    var calId = cal.calId;
                                    if (calIds[calId] && !this_.m_subscribedCals[calId]) {
                                        this_.m_subscribedCals[calId] = cal;
                                        getCalendarManager().registerCalendar(cal);
                                        log("installed subscribed calendar: " + calId, this_);
                                    }
                                }
                                catch (exc) { // ignore but log any errors on subscribed calendars:
                                    logError(exc, this_);
                                }
                            }
                        }
                        catch (exc) { // ignore but log any errors on subscribed calendars:
                            logError(exc, this_);
                        }
                    }
                };
                
                var colon = calId.indexOf(':');
                if (colon >= 0) // searching for secondary calendars doesn't work. WTF.
                    calId = calId.substring(0, colon);
                if (!issuedSearchRequests[calId]) {
                    issuedSearchRequests[calId] = true;
                    this.searchForCalendars(
                        calId,
                        calIWcapSession.SEARCH_STRING_EXACT |
                        calIWcapSession.SEARCH_INCLUDE_CALID |
                        // else searching for secondary calendars doesn't work:
                        calIWcapSession.SEARCH_INCLUDE_OWNER,
                        20, listener);
                }
            }
        }
    },
    
    unregisterSubscribedCals:
    function calWcapSession_unregisterSubscribedCals() {
        // unregister all subscribed cals, don't modify subscriptions upon unregisterCalendar:
        var subscribedCals = this.m_subscribedCals;
        this.m_subscribedCals = {};
        // tag al to limit network traffic, listener chain will cause getItems calls:
        for each (var cal in subscribedCals) {
            cal.aboutToBeUnregistered = true;
        }
        if (!g_bShutdown) {
            for each (var cal in subscribedCals) {
                try {
                    getCalendarManager().unregisterCalendar(cal);
                }
                catch (exc) {
                    this.notifyError(exc);
                }
            }
        }
    },
    
    getCommandUrl: function calWcapSession_getCommandUrl(wcapCommand, params, sessionId)
    {
        var url = this.sessionUri.spec;
        url += (wcapCommand + ".wcap?appid=mozilla-calendar");
        url += params;
        url += ("&id=" + encodeURIComponent(sessionId));
        return url;
    },

    issueNetworkRequest: function calWcapSession_issueNetworkRequest(
        request, respFunc, dataConvFunc, wcapCommand, params)
    {
        var this_ = this;
        function getSessionId_resp(err, sessionId) {
            if (err)
                respFunc(err);
            else {
                // else have session uri and id:
                this_.issueNetworkRequest_(
                    request,
                    function issueNetworkRequest_resp(err, data) {
                        // timeout?
                        if (getResultCode(err) == calIWcapErrors.WCAP_LOGIN_FAILED) {
                            // try again:
                            this_.getSessionId(
                                request,
                                getSessionId_resp,
                                sessionId/* (old) timed-out session */);
                            return;
                        }
                        respFunc(err, data);
                    },
                    dataConvFunc, wcapCommand, params, sessionId);
            }
        }
        this.getSessionId(request, getSessionId_resp);
    },
    
    issueNetworkRequest_: function calWcapSession_issueNetworkRequest_(
        request, respFunc, dataConvFunc, wcapCommand, params, sessionId)
    {
        var url = this.getCommandUrl(wcapCommand, params, sessionId);
        issueNetworkRequest(
            request,
            function netResp(err, str) {
                var data;
                if (!err) {
                    try {
                        data = dataConvFunc(str);
                    }
                    catch (exc) {
                        err = exc;
                    }
                }
                respFunc(err, data);
            }, url);
    },
    
    m_credentials: null,
    get credentials() {
        if (!this.m_credentials)
            this.m_credentials = {
                userId: "",
                pw: "",
                userPrefs: null
            };
        return this.m_credentials;
    },
    
    // calIWcapSession:
    
    m_uri: null,
    m_sessionUri: null,
    get uri() { return this.m_uri; },
    get sessionUri() { return this.m_sessionUri; },
    set uri(thatUri) {
        if (!this.m_uri || !thatUri || !this.m_uri.equals(thatUri)) {
            this.logout(null);
            this.m_uri = null;
            this.m_sessionUri = null;
            if (thatUri) {
                this.m_uri = thatUri.clone();
                this.m_sessionUri = thatUri.clone();
                this.m_sessionUri.userPass = "";
                // sensible default for user id login:
                var username = decodeURIComponent(thatUri.username);
                if (username.length > 0)
                    this.credentials.userId = username;
                log("set uri: " + this.uri.spec, this);
            }
        }
        return thatUri;
    },
    
    get userId() { return this.credentials.userId; },
    
    get defaultCalId() {
        var list = this.getUserPreferences("X-NSCP-WCAP-PREF-icsCalendar");
        return (list.length > 0 ? list[0] : this.credentials.userId);
    },
    
    get isLoggedIn() {
        return (this.m_sessionId != null);
    },
    
    m_defaultCalendar: null,
    get defaultCalendar() {
        if (!this.m_defaultCalendar)
            this.m_defaultCalendar = createWcapCalendar(this);
        return this.m_defaultCalendar;
    },
    
    getUserPreferences: function calWcapSession_getUserPreferences(prefName) {
        var prefs = filterXmlNodes(prefName, this.credentials.userPrefs);
        return prefs;
    },
    
    get defaultAlarmStart() {
        var alarmStart = null;
        var ar = this.getUserPreferences("X-NSCP-WCAP-PREF-ceDefaultAlarmStart");
        if (ar.length > 0 && ar[0].length > 0) {
            // workarounding cs duration bug, missing "T":
            var dur = ar[0].replace(/(^P)(\d+[HMS]$)/, "$1T$2");
            alarmStart = new CalDuration();
            alarmStart.icalString = dur;
            alarmStart.isNegative = !alarmStart.isNegative;
        }
        return alarmStart;
    },
    
    getDefaultAlarmEmails: function calWcapSession_getDefaultAlarmEmails(out_count)
    {
        var ret = [];
        var ar = this.getUserPreferences("X-NSCP-WCAP-PREF-ceDefaultAlarmEmail");
        if (ar.length > 0 && ar[0].length > 0) {
            for each (var i in ar) {
                ret = ret.concat( i.split(/[;,]/).map(trimString) );
            }
        }
        out_count.value = ret.length;
        return ret;
    },
    
    searchForCalendars:
    function calWcapSession_searchForCalendars(searchString, searchOptions, maxResults, listener)
    {
        var this_ = this;
        var request = new calWcapRequest(
            function searchForCalendars_resp(request, err, data) {
                if (err)
                    this_.notifyError(err);
                if (listener)
                    listener.onRequestResult(request, data);
            },
            log("searchForCalendars, searchString=" + searchString, this));
        
        try {
            var params = ("&fmt-out=text%2Fxml&search-string=" +
                          encodeURIComponent(searchString));
            params += ("&searchOpts=" + (searchOptions & 3).toString(10));
            if (maxResults > 0)
                params += ("&maxResults=" + maxResults);
            if (searchOptions & calIWcapSession.SEARCH_INCLUDE_CALID)
                params += "&calid=1";
            if (searchOptions & calIWcapSession.SEARCH_INCLUDE_NAME)
                params += "&name=1";
            if (searchOptions & calIWcapSession.SEARCH_INCLUDE_OWNER)
                params += "&primaryOwner=1";
            
            this.issueNetworkRequest(
                request,
                function searchForCalendars_netResp(err, data) {
                    if (err)
                        throw err;
                    // string to xml converter func without WCAP errno check:
                    if (!data || data.length == 0) { // assuming time-out
                        throw new Components.Exception(
                            "Login failed. Invalid session ID.",
                            calIWcapErrors.WCAP_LOGIN_FAILED);
                    }
                    var xml = getDomParser().parseFromString(data, "text/xml");
                    var ret = [];
                    var nodeList = xml.getElementsByTagName("iCal");
                    for ( var i = 0; i < nodeList.length; ++i ) {
                        var node = nodeList.item(i);
                        try {
                            checkWcapXmlErrno(node);
                            var ar = filterXmlNodes("X-NSCP-CALPROPS-RELATIVE-CALID", node);
                            if (ar.length > 0) {
                                var calId = ar[0];
                                var cal = this_.m_subscribedCals[calId];
                                if (!cal) {
                                    if (calId == this_.defaultCalId)
                                        cal = this_.defaultCalendar;
                                    else
                                        cal = createWcapCalendar(this_, node);
                                }
                                ret.push(cal);
                            }
                        }
                        catch (exc) {
                            switch (getResultCode(exc)) {
                            case calIWcapErrors.WCAP_NO_ERRNO: // workaround
                            case calIWcapErrors.WCAP_ACCESS_DENIED_TO_CALENDAR:
                                log("searchForCalendars_netResp() ignored error: " +
                                    errorToString(exc), this_);
                                break;
                            default:
                                this_.notifyError(exc);
                                break;
                            }
                        }
                    }
                    log("search done. number of found calendars: " + ret.length, this_);
                    request.execRespFunc(null, ret);
                },
                function identity(data) { return data; },
                "search_calprops", params);
        }
        catch (exc) {
            request.execRespFunc(exc);
        }
        return request;
    },
    
    getFreeBusyTimes: function calWcapCalendar_getFreeBusyTimes(
        calId, rangeStart, rangeEnd, bBusy, listener)
    {
        // assure DATETIMEs:
        if (rangeStart && rangeStart.isDate) {
            rangeStart = rangeStart.clone();
            rangeStart.isDate = false;
        }
        if (rangeEnd && rangeEnd.isDate) {
            rangeEnd = rangeEnd.clone();
            rangeEnd.isDate = false;
        }
        var zRangeStart = getIcalUTC(rangeStart);
        var zRangeEnd = getIcalUTC(rangeEnd);
        
        var this_ = this;
        var request = new calWcapRequest(
            function getFreeBusyTimes_resp(request, err, data) {
                var rc = getResultCode(err);
                switch (rc) {
                case calIWcapErrors.WCAP_NO_ERRNO: // workaround
                case calIWcapErrors.WCAP_ACCESS_DENIED_TO_CALENDAR:
                case calIWcapErrors.WCAP_CALENDAR_DOES_NOT_EXIST:
                    log("getFreeBusyTimes_resp() error: " + errorToString(err), this_);
                    break;
                default:
                    if (!Components.isSuccessCode(rc))
                        this_.notifyError(err);
                    break;
                }
                if (listener)
                    listener.onRequestResult(request, data);
            },
            log("getFreeBusyTimes():\n\tcalId=" + calId +
                "\n\trangeStart=" + zRangeStart + ",\n\trangeEnd=" + zRangeEnd, this));
        
        try {
            var params = ("&calid=" + encodeURIComponent(calId));
            params += ("&busyonly=" + (bBusy ? "1" : "0"));
            params += ("&dtstart=" + zRangeStart);
            params += ("&dtend=" + zRangeEnd);
            params += "&fmt-out=text%2Fxml";
            
            this.issueNetworkRequest(
                request,
                function getFreeBusyTimes_resp(err, xml) {
                    if (err)
                        throw err;
                    if (LOG_LEVEL > 0) {
                        log("getFreeBusyTimes_resp(): " +
                            getWcapRequestStatusString(xml), this_);
                    }
                    if (listener) {
                        var ret = [];
                        var nodeList = xml.getElementsByTagName("FB");
                        for ( var i = 0; i < nodeList.length; ++i ) {
                            var node = nodeList.item(i);
                            if ((node.attributes.getNamedItem("FBTYPE").nodeValue
                                 == "BUSY") != bBusy) {
                                continue;
                            }
                            var str = node.textContent;
                            var slash = str.indexOf('/');
                            var period = new CalPeriod();
                            period.start = getDatetimeFromIcalString(str.substr(0, slash));
                            period.end = getDatetimeFromIcalString(str.substr(slash + 1));
                            period.makeImmutable();
                            ret.push(period);
                        }
                        request.execRespFunc(null, ret);
                    }
                },
                stringToXml, "get_freebusy", params);
        }
        catch (exc) {
            request.execRespFunc(exc);
        }
        return request;
    },
    
    m_bInstalledLogoutObservers: false,
    assureInstalledLogoutObservers:
    function calWcapSession_assureInstalledLogoutObservers()
    {
        // don't do this in ctor, calendar manager calls back to all
        // registered calendars!
        if (!this.m_bInstalledLogoutObservers) {
            this.m_bInstalledLogoutObservers = true;
            // listen for shutdown, being logged out:
            var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                            .getService(Components.interfaces.nsIObserverService);
            observerService.addObserver(this, "quit-application", false /* don't hold weakly */);
            getCalendarManager().addObserver(this);
        }
    },
    
    // nsIObserver:
    observe: function calWcapSession_observer(subject, topic, data)
    {
        log("observing: " + topic + ", data: " + data, this);
        if (topic == "quit-application") {
            g_bShutdown = true;
            this.logout(null);
            // xxx todo: valid upon notification?
            getCalendarManager().removeObserver(this);
            var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                            .getService(Components.interfaces.nsIObserverService);
            observerService.removeObserver(this, "quit-application");
        }
    },
    
    modifySubscriptions: function calWcapSession_modifySubscriptions(cal, bSubscribe)
    {
        var wcapCommand = ((bSubscribe ? "" : "un") + "subscribe_calendars");
        var this_ = this;
        var request = new calWcapRequest(
            function subscr_resp(request, err) {
                if (err)
                    this_.notifyError(err);
            },
            log(wcapCommand + ": " + cal.calId));
        this.issueNetworkRequest(
            request,
            function netResp(err, xml) {
                if (err)
                    throw err;
            },
            stringToXml, wcapCommand,
            "&fmt-out=text%2Fxml&calid=" + encodeURIComponent(cal.calId));
        return request;
    },
    
    // calICalendarManagerObserver:
    
    // called after the calendar is registered
    onCalendarRegistered: function calWcapSession_onCalendarRegistered(cal)
    {
        try {
            cal = cal.QueryInterface(calIWcapCalendar);
        }
        catch (exc) {
            cal = null;
        }
        try {
            // make sure the calendar belongs to this session and is a subscription:
            if (cal && cal.session.uri.equals(this.uri) && !cal.isDefaultCalendar) {
                var calId = cal.calId;
                if (!this.m_subscribedCals[calId])
                    this.modifySubscriptions(cal, true/*bSubscibe*/);
                this.m_subscribedCals[calId] = cal.wrappedJSObject;
            }
        }
        catch (exc) { // never break the listener chain
            this.notifyError(exc);
        }
    },
    
    // called before the unregister actually takes place
    onCalendarUnregistering: function calWcapSession_onCalendarUnregistering(cal)
    {
        try {
            cal = cal.QueryInterface(calIWcapCalendar);
        }
        catch (exc) {
            cal = null;
        }
        try {
            // don't logout here (even if this is the default calendar):
            // upcoming calls may occur.
            // make sure the calendar belongs to this session and is a subscription:
            if (cal && cal.session.uri.equals(this.uri)) {
                if (cal.isDefaultCalendar) {
                    this.unregisterSubscribedCals();
                }
                else {
                    var calId = cal.calId;
                    if (this.m_subscribedCals[calId]) {
                        delete this.m_subscribedCals[calId];
                        this.modifySubscriptions(cal, false/*bSubscibe*/);
                    }
                }
            }
        }
        catch (exc) { // never break the listener chain
            this.notifyError(exc);
        }
    },
    
    // called before the delete actually takes place
    onCalendarDeleting: function calWcapSession_onCalendarDeleting(cal)
    {
    },
    
    // called after the pref is set
    onCalendarPrefSet: function calWcapSession_onCalendarPrefSet(cal, name, value)
    {
    },
    
    // called before the pref is deleted
    onCalendarPrefDeleting: function calWcapSession_onCalendarPrefDeleting(cal, name)
    {
    }
};

var g_confirmedHttpLogins = null;
function confirmInsecureLogin(uri)
{
    if (!g_confirmedHttpLogins) {
        g_confirmedHttpLogins = {};
        var confirmedHttpLogins = getPref(
            "calendar.wcap.confirmed_http_logins", "");
        var tuples = confirmedHttpLogins.split(',');
        for each ( var tuple in tuples ) {
            var ar = tuple.split(':');
            g_confirmedHttpLogins[ar[0]] = ar[1];
        }
    }
    
    var bConfirmed = false;
    
    var host = uri.hostPort;
    var encodedHost = encodeURIComponent(host);
    var confirmedEntry = g_confirmedHttpLogins[encodedHost];
    if (confirmedEntry) {
        bConfirmed = (confirmedEntry == "1");
    }
    else {
        var prompt = getWindowWatcher().getNewPrompter(null);
        var bundle = getWcapBundle();
        var out_dontAskAgain = { value: false };
        var bConfirmed = prompt.confirmCheck(
            bundle.GetStringFromName("noHttpsConfirmation.label"),
            bundle.formatStringFromName("noHttpsConfirmation.text", [host], 1),
            bundle.GetStringFromName("noHttpsConfirmation.check.text"),
            out_dontAskAgain);
        
        if (out_dontAskAgain.value) {
            // save decision for all running calendars and
            // all future confirmations:
            var confirmedHttpLogins = getPref(
                "calendar.wcap.confirmed_http_logins", "");
            if (confirmedHttpLogins.length > 0)
                confirmedHttpLogins += ",";
            confirmedEntry = (bConfirmed ? "1" : "0");
            confirmedHttpLogins += (encodedHost + ":" + confirmedEntry);
            setPref("calendar.wcap.confirmed_http_logins", confirmedHttpLogins);
            g_confirmedHttpLogins[encodedHost] = confirmedEntry;
        }
    }
    
    log("returned: " + bConfirmed, "confirmInsecureLogin(" + host + ")");
    return bConfirmed;
}

