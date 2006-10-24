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

// globals:
var g_serverTimeDiffs = {};
var g_allSupportedTimezones = {};

function calWcapSession() {
    this.wrappedJSObject = this;
    this.m_observers = [];
    this.m_calIdToCalendar = {};
    // listen for shutdown, being logged out:
    // network:offline-about-to-go-offline will be fired for
    // XPCOM shutdown, too.
    // xxx todo: alternatively, add shutdown notifications to cal manager
    // xxx todo: how to simplify this for multiple topics?
    var observerService = Components.classes["@mozilla.org/observer-service;1"]
        .getService(Components.interfaces.nsIObserverService);
    observerService.addObserver( this, "quit-application",
                                 false /* don't hold weakly: xxx todo */ );
    observerService.addObserver( this, "network:offline-about-to-go-offline",
                                 false /* don't hold weakly: xxx todo */ );
}
calWcapSession.prototype = {
    m_ifaces: [ Components.interfaces.calIWcapSession,
                Components.interfaces.nsIInterfaceRequestor,
                Components.interfaces.nsIClassInfo,
                Components.interfaces.nsISupports ],
    
    // nsISupports:
    QueryInterface:
    function( iid )
    {
        for each ( var iface in this.m_ifaces ) {
            if (iid.equals( iface ))
                return this;
        }
        throw Components.results.NS_ERROR_NO_INTERFACE;
    },
    
    // nsIClassInfo:
    getInterfaces:
    function( count )
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
    getHelperForLanguage: function( language ) { return null; },
    implementationLanguage:
    Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0,
    
    // nsIInterfaceRequestor:
    getInterface:
    function( iid, instance )
    {
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
    
    toString:
    function( msg )
    {
        var str = (this.uri ? this.uri.spec : "no uri");
        if (this.m_userId != null) {
            str += (", userId=" + this.userId);
        }
        if (this.m_sessionId == null) {
            str += (getIoService().offline ? ", offline" : ", not logged in");
        }
//         else {
//             str += (", session-id=" + this.m_sessionId);
//         }
        return str;
    },
    log:
    function( msg )
    {
        return logMessage( this.toString(), msg );
    },
    logError:
    function( err, context )
    {
        var msg = errorToString(err);
        Components.utils.reportError( this.log("error: " + msg, context) );
        return msg;
    },
    notifyError:
    function( err )
    {
        debugger;
        var msg = this.logError(err);
        this.notifyObservers(
            "onError",
            err instanceof Components.interfaces.nsIException
            ? [err.result, err.message] : [-1, msg] );
    },
    
    m_observers: null,
    notifyObservers:
    function( func, args )
    {
        this.m_observers.forEach(
            function( obj ) {
                try {
                    obj[func].apply( obj, args );
                }
                catch (exc) {
                    // don't call notifyError() here:
                    Components.utils.reportError( exc );
                }
            } );
    },
    
    addObserver:
    function( observer )
    {
        if (this.m_observers.indexOf( observer ) == -1) {
            this.m_observers.push( observer );
        }
    },
    
    removeObserver:
    function( observer )
    {
        this.m_observers = this.m_observers.filter(
            function(x) { return x != observer; } );
    },
    
    // nsIObserver:
    observe:
    function( subject, topic, data )
    {
        this.log( "observing: " + topic + ", data: " + data );
        if (topic == "network:offline-about-to-go-offline") {
            this.logout();
        }
        else if (topic == "quit-application") {
            this.logout();
            // xxx todo: valid upon notification?
            var observerService =
                Components.classes["@mozilla.org/observer-service;1"]
                .getService(Components.interfaces.nsIObserverService);
            observerService.removeObserver(
                this, "quit-application" );
            observerService.removeObserver(
                this, "network:offline-about-to-go-offline" );
        }
    },
    
    getSupportedTimezones:
    function( bRefresh )
    {
        var key = this.sessionUri.hostPort;
        if ((bRefresh || !g_allSupportedTimezones[key]) &&
            this.m_sessionId != null)
        {
            var url = (this.sessionUri.spec +
                       "get_all_timezones.wcap?appid=mozilla-calendar" +
                       "&fmt-out=text%2Fcalendar&id=" +
                       encodeURIComponent(this.m_sessionId));
            var str = issueSyncRequest( url );
            var icalRootComp = getIcsService().parseICS( str );
            if (icalRootComp == null)
                throw new Components.Exception("invalid data, expected ical!");
            checkWcapIcalErrno( icalRootComp );
            var tzids = [];
            var this_ = this;
            forEachIcalComponent(
                icalRootComp, "VTIMEZONE",
                function( subComp ) {
                    try {
                        var tzCal = getIcsService().createIcalComponent(
                            "VCALENDAR" );
                        subComp = subComp.clone();
                        tzCal.addSubcomponent( subComp );
                        getIcsService().addTimezone( tzCal, "", "" );
                        tzids.push( subComp.getFirstProperty("TZID").value );
                    }
                    catch (exc) { // ignore errors:
                        this_.log( "error: " + exc );
                    }
                } );
            g_allSupportedTimezones[key] = tzids;
        }
        return g_allSupportedTimezones[key];
    },
    
    getServerTime:
    function( localTime )
    {
        var ret = (localTime ? localTime.clone() : getTime());
        ret.addDuration( this.getServerTimeDiff() );
        return ret;
    },
    
    getServerTimeDiff:
    function( bRefresh )
    {
        var key = this.sessionUri.hostPort;
        if ((bRefresh || !g_serverTimeDiffs[key]) &&
            this.m_sessionId != null) {
            var url = (this.sessionUri.spec +
                       // xxx todo: assuming same diff for all calids:
                       "gettime.wcap?appid=mozilla-calendar" +
                       "&fmt-out=text%2Fcalendar&id=" +
                       encodeURIComponent(this.m_sessionId));
            // xxx todo: think about
            // assure that locally calculated server time is smaller than the
            // current (real) server time:
            var str = issueSyncRequest( url );
            var localTime = getTime();
            var icalRootComp = getIcsService().parseICS( str );
            if (icalRootComp == null)
                throw new Components.Exception("invalid data, expected ical!");
            checkWcapIcalErrno( icalRootComp );
            var serverTime = getDatetimeFromIcalProp(
                icalRootComp.getFirstProperty( "X-NSCP-WCAPTIME" ) );
            var diff = serverTime.subtractDate( localTime );
            this.log( "server time diff is: " + diff );
            g_serverTimeDiffs[key] = diff;
        }
        return g_serverTimeDiffs[key];
    },
    
    isSupportedTimezone:
    function( tzid )
    {
        var tzids = this.getSupportedTimezones();
        for each ( var id in tzids ) {
            if (id == tzid)
                return true;
        }
        return false;
    },
    
    m_sessionId: null,
    m_bNoLoginsAnymore: false,
    
    getSessionId:
    function( timedOutSessionId )
    {
        if (getIoService().offline) {
            this.log( "in offline mode." );
            throw new Components.Exception(
                "The requested action could not be completed while the " +
                "networking library is in the offline state.",
                NS_ERROR_OFFLINE );
        }
        if (this.m_bNoLoginsAnymore) {
            this.log( "login has failed, no logins anymore for this user." );
            throw new Components.Exception(
                "Login failed. Invalid session ID.",
                Components.interfaces.calIWcapErrors.WCAP_LOGIN_FAILED );
        }
        
        if (!this.m_sessionId || this.m_sessionId == timedOutSessionId) {
            
            var this_ = this;
            lockedExec(
                function() {
                    if (!this_.m_bNoLoginsAnymore &&
                        (!this_.m_sessionId ||
                         this_.m_sessionId == timedOutSessionId)) {
                        
                        try {
                            this_.m_sessionId = null;
                            if (timedOutSessionId != null) {
                                this_.log( "session timeout; " +
                                           "prompting to reconnect." );
                                var prompt =
                                    getWindowWatcher().getNewPrompter(null);
                                var bundle = getWcapBundle();
                                if (!prompt.confirm(
                                        bundle.GetStringFromName(
                                            "reconnectConfirmation.label"),
                                        bundle.formatStringFromName(
                                            "reconnectConfirmation.text",
                                            [this_.uri.hostPort], 1 ) )) {
                                    this.log( "reconnect cancelled." );
                                    throw new Components.Exception(
                                        "Login failed. Invalid session ID.",
                                        Components.interfaces.
                                        calIWcapErrors.WCAP_LOGIN_FAILED );
                                }
                            }
                            
                            var sessionUri = this_.uri.clone();
                            sessionUri.userPass = "";
                            if (sessionUri.scheme.toLowerCase() != "https" &&
                                sessionUri.port == -1 /* no port specified */)
                            {
                                // silently probe for https support:
                                try { // enforce https:
                                    sessionUri.scheme = "https";
                                    this_.getSessionId_(sessionUri);
                                }
                                catch (exc) {
                                    // restore scheme:
                                    sessionUri.scheme = this_.uri.scheme;
                                    if (testResultCode(
                                            exc, Components.interfaces.
                                            calIWcapErrors.WCAP_LOGIN_FAILED))
                                        throw exc; // forward login failures
                                    // but ignore connection errors
                                }
                            }
                            if (!this_.m_sessionId)
                                this_.getSessionId_(sessionUri);
                        }
                        catch (exc) {
                            this_.m_bNoLoginsAnymore = true;
                            this_.logError( exc );
                            this_.log( "no logins anymore." );
                            throw exc;
                        }
                        
                        this_.getSupportedTimezones(true /* refresh */);
                        this_.getServerTimeDiff(true /* refresh */);
                        // preread calprops for subscribed calendars:
                        var cals = this_.getSubscribedCalendars({});
                        for each ( cal in cals ) {
                            cal.getCalProps_(true /* async */);
                        }
                    }
                } );
        }
        
        if (!this.m_sessionId) {
            throw new Components.Exception(
                "Login failed. Invalid session ID.",
                Components.interfaces.calIWcapErrors.WCAP_LOGIN_FAILED );
        }
        return this.m_sessionId;
    },
    
    assureSecureLogin:
    function( sessionUri )
    {
        if (sessionUri.scheme.toLowerCase() != "https" &&
            !confirmInsecureLogin(sessionUri)) {
            this.log( "user rejected insecure login on " + sessionUri.spec );
            throw new Components.Exception(
                "Login failed. Invalid session ID.",
                Components.interfaces.calIWcapErrors.WCAP_LOGIN_FAILED );
        }
    },
    
    getSessionId_:
    function( sessionUri )
    {
        this.log( "attempting to get a session id for " + sessionUri.spec );
        
        var outUser = { value: this.userId };
        var outPW = { value: null };
        
        var passwordManager =
            Components.classes["@mozilla.org/passwordmanager;1"]
            .getService(Components.interfaces.nsIPasswordManager);
        
        // xxx todo: pw host names must not have a trailing slash
        var pwHost = sessionUri.spec;
        if (pwHost[pwHost.length - 1] == '/')
            pwHost = pwHost.substr(0, pwHost.length - 1);
        this.log( "looking in pw db for: " + pwHost );
        var enumerator = passwordManager.enumerator;
        while (enumerator.hasMoreElements()) {
            var pwEntry = enumerator.getNext().QueryInterface(
                Components.interfaces.nsIPassword );
            if (LOG_LEVEL > 1) {
                this.log( "pw entry:\n\thost=" + pwEntry.host +
                          "\n\tuser=" + pwEntry.user );
            }
            if (pwEntry.host == pwHost) {
                // found an entry matching URI:
                outUser.value = pwEntry.user;
                outPW.value = pwEntry.password;
                break;
            }
        }
        
        if (outPW.value) {
            this.log( "password entry found for host " + pwHost +
                      "\nuser is " + outUser.value );
            this.assureSecureLogin(sessionUri);
            this.login_( sessionUri, outUser.value, outPW.value );
        }
        else
            this.log( "no password entry found for " + pwHost );
        
        if (!this.m_sessionId) {
            var loginText = this.getServerInfo(sessionUri);
            if (outPW.value) {
                // login failed before, so try to remove from pw db:
                try {
                    passwordManager.removeUser( pwHost, outUser.value );
                    this.log( "removed from pw db: " + pwHost );
                }
                catch (exc) {
                    this.logError( "error removing from pw db: " + exc );
                }
            }
            else // if not already checked in pw manager run
                this.assureSecureLogin(sessionUri);
            
            var savePW = { value: false };
            while (!this.m_sessionId) {
                this.log( "prompting for user/pw..." );
                var prompt = getWindowWatcher().getNewPrompter(null);
                if (prompt.promptUsernameAndPassword(
                        getWcapBundle().GetStringFromName(
                            "loginDialog.label"),
                        loginText, outUser, outPW,
                        getWcapBundle().GetStringFromName(
                            "loginDialog.check.text"),
                        savePW ))
                {
                    if (this.login_( sessionUri, outUser.value, outPW.value ))
                        break;
                }
                else {
                    this.log( "login prompt cancelled." );
                    throw new Components.Exception(
                        "Login failed. Invalid session ID.",
                        Components.interfaces.calIWcapErrors.WCAP_LOGIN_FAILED);
                }
            }
            if (savePW.value) {
                try { // save pw under session uri:
                    passwordManager.addUser( pwHost,
                                             outUser.value, outPW.value );
                    this.log( "added to pw db: " + pwHost );
                }
                catch (exc) {
                    this.logError( "error adding pw to db: " + exc );
                }
            }
        }
        return this.m_sessionId;
    },
    
    login_:
    function( sessionUri, user, pw )
    {
        if (this.m_sessionId != null) {
            this.logout();
        }
        var str;
        var icalRootComp = null;
        try {
            // currently, xml parsing at an early stage during process startup
            // does not work reliably, so use libical parsing for now:
            str = issueSyncRequest(
                sessionUri.spec + "login.wcap?fmt-out=text%2Fcalendar&user=" +
                encodeURIComponent(user) +
                "&password=" + encodeURIComponent(pw),
                null /* receiverFunc */, false /* no logging */ );
            if (str.indexOf("BEGIN:VCALENDAR") < 0)
                throw new Components.Exception("no ical data returned!");
            icalRootComp = getIcsService().parseICS( str );
            checkWcapIcalErrno( icalRootComp );
        }
        catch (exc) {
            if (testResultCode(exc, Components.interfaces.
                               calIWcapErrors.WCAP_LOGIN_FAILED)) {
                this.logError( exc ); // log wrong pw
                return false;
            }
            if (!isNaN(exc) && getErrorModule(exc) == NS_ERROR_MODULE_NETWORK) {
                // server seems unavailable:
                throw new Components.Exception(
                    getWcapBundle().formatStringFromName(
                        "accessingServerFailedError.text", [uri.hostPort], 1 ),
                    exc );
            }
            throw exc;
        }
        var prop = icalRootComp.getFirstProperty("X-NSCP-WCAP-SESSION-ID");
        if (!prop) {
            throw new Components.Exception(
                "missing X-NSCP-WCAP-SESSION-ID in\n" + str );
        }
        this.m_sessionId = prop.value;        
        this.m_userId = user;
        this.m_sessionUri = sessionUri;
        this.log( "WCAP login succeeded, setting sessionUri to " +
                  this.sessionUri.spec );
        return true;
    },
    
    getServerInfo:
    function( uri )
    {
        var str;
        var icalRootComp = null;
        try {
            // currently, xml parsing at an early stage during process startup
            // does not work reliably, so use libical:
            str = issueSyncRequest(
                uri.spec + "version.wcap?fmt-out=text%2Fcalendar" );
            if (str.indexOf("BEGIN:VCALENDAR") < 0)
                throw new Components.Exception("no ical data returned!");
            icalRootComp = getIcsService().parseICS( str );
        }
        catch (exc) {
            this.log( exc ); // soft error; request denied etc.
            throw new Components.Exception(
                getWcapBundle().formatStringFromName(
                    "accessingServerFailedError.text", [uri.hostPort], 1 ),
                isNaN(exc) ? Components.results.NS_ERROR_FAILURE : exc );
        }
        
        var prop = icalRootComp.getFirstProperty( "X-NSCP-WCAPVERSION" );
        if (!prop) {
            throw new Components.Exception(
                "missing X-NSCP-WCAPVERSION in\n" + str );
        }
        var wcapVersion = parseInt(prop.value);
        if (wcapVersion < 3) {
            var strVers = prop.value;
            var vars = [uri.hostPort];
            prop = icalRootComp.getFirstProperty( "PRODID" );
            vars.push( prop ? prop.value : "<unknown>" );
            prop = icalRootComp.getFirstProperty( "X-NSCP-SERVERVERSION" );
            vars.push( prop ? prop.value : "<unknown>" );
            vars.push( strVers );
            
            var prompt = getWindowWatcher().getNewPrompter(null);
            var bundle = getWcapBundle();
            var labelText = bundle.GetStringFromName(
                "insufficientWcapVersionConfirmation.label");
            if (!prompt.confirm( labelText,
                                 bundle.formatStringFromName(
                                     "insufficientWcapVersionConfirmation.text",
                                     vars, vars.length ) )) {
                throw new Components.Exception(labelText);
            }
        }
        return getWcapBundle().formatStringFromName( "loginDialog.text",
                                                     [uri.hostPort], 1 );
    },
    
    getCommandUrl:
    function( wcapCommand )
    {
        if (!this.uri)
            throw new Components.Exception("no URI!");
        // ensure established session, so sesionUri and userId is set;
        // (calId defaults to userId) if not set:
        this.getSessionId();
        return (this.sessionUri.spec +
                wcapCommand + ".wcap?appid=mozilla-calendar");
    },
    
    issueRequest:
    function( url, issueFunc, dataConvFunc, receiverFunc )
    {
        var sessionId = this.getSessionId();
        
        var this_ = this;
        issueFunc(
            url + ("&id=" + encodeURIComponent(sessionId)),
            function( data ) {
                var wcapResponse = new WcapResponse();
                try {
                    try {
                        wcapResponse.data = dataConvFunc( data );
                    }
                    catch (exc) {
                        if (testResultCode(
                                exc, Components.interfaces.
                                calIWcapErrors.WCAP_LOGIN_FAILED)) /* timeout */
                        {
                            // getting a new session will throw any exception in
                            // this block, thus it is notified into receiverFunc
                            this_.getSessionId(
                                sessionId /* (old) timed-out session */ );
                            // try again:
                            this_.issueRequest(
                                url, issueFunc, dataConvFunc, receiverFunc );
                            return;
                        }
                        throw exc; // rethrow
                    }
                }
                catch (exc) {
                    // setting the request's exception will rethrow exception
                    // when request's data is retrieved.
                    wcapResponse.exception = exc;
                }
                receiverFunc( wcapResponse );
            } );
    },
    
    issueAsyncRequest:
    function( url, dataConvFunc, receiverFunc )
    {
        this.issueRequest(
            url, issueAsyncRequest, dataConvFunc, receiverFunc );
    },
    
    issueSyncRequest:
    function( url, dataConvFunc, receiverFunc )
    {
        var ret = null;
        this.issueRequest(
            url, issueSyncRequest,
            dataConvFunc,
            function( wcapResponse ) {
                if (receiverFunc) {
                    receiverFunc( wcapResponse );
                }
                ret = wcapResponse.data; // may throw
            } );
        return ret;
    },
    
    // calIWcapSession:
    
    m_uri: null,
    m_sessionUri: null,
    get uri() { return this.m_uri; },
    get sessionUri() { return this.m_sessionUri; },
    set uri( thatUri )
    {
        if (this.m_uri == null || thatUri == null ||
            !this.m_uri.equals(thatUri))
        {
            this.logout();
            this.m_uri = null;
            if (thatUri != null) {
                // sensible default for user id login:
                var username = decodeURIComponent( thatUri.username );
                if (username != "") {
                    this.m_defaultCalId = username;
                    var nColon = username.indexOf(':');
                    this.m_userId = (nColon >= 0
                                     ? username.substr(0, nColon) : username);
                }
                this.m_uri = thatUri.clone();
                this.log( "setting uri to " + this.uri.spec );
            }
        }
    },
    
    m_userId: null,
    get userId() { return this.m_userId; },
    
    m_defaultCalId: null,
    get defaultCalId() { return this.m_defaultCalId || this.userId; },
    
    get isLoggedIn() { return this.m_sessionId != null; },
    
    login:
    function()
    {
        this.logout(); // assure being logged out
        this.getSessionId();
    },
    
    logout:
    function()
    {
        if (this.m_sessionId != null) {
            this.log( "attempting to log out..." );
            // although io service's offline flag is already
            // set BEFORE notification (about to go offline, nsIOService.cpp).
            // WTF.
            var url = (this.sessionUri.spec +
                       "logout.wcap?fmt-out=text%2Fxml&id=" +
                       encodeURIComponent(this.m_sessionId));
            try {
                checkWcapXmlErrno( issueSyncXMLRequest(url),
                                   -1 /* logout successfull */ );
                this.log( "WCAP logout succeeded." );
            }
            catch (exc) {
                this.log( "WCAP logout failed: " + exc );
                Components.utils.reportError( exc );
            }
            this.m_sessionId = null;
        }
        this.m_sessionUri = null;
        this.m_userId = null;
        this.m_userPrefs = null; // reread prefs
        this.m_defaultCalId = null;
        this.m_bNoLoginsAnymore = false;
    },
    
    getWcapErrorString:
    function( rc )
    {
        return wcapErrorToString(rc);
    },
    
    get defaultCalendar() {
        return this.getCalendarByCalId(null);
    },
    set defaultCalendar(cal) {
        this.m_defaultCalendar = cal;
    },
    
    m_defaultCalendar: null,
    m_calIdToCalendar: {},
    getCalendarByCalId:
    function( calId )
    {
        var ret;
        // xxx todo: for now the default calendar (calId=null)
        //           is separated (own instance) from subscribed calendars
        if (calId == null /*|| this.userId == calId*/) {
            if (this.m_defaultCalendar == null) {
                this.m_defaultCalendar = createWcapCalendar(
                    null/*this.userId*/, this);
            }
            ret = this.m_defaultCalendar;
        }
        else {
            var key = encodeURIComponent(calId);
            ret = this.m_calIdToCalendar[key];
            if (!ret) {
                ret = createWcapCalendar(calId, this);
                this.m_calIdToCalendar[key] = ret;
            }
        }
        return ret;
    },
    
    getCalendars:
    function( out_count, bGetOwnedCals )
    {
        var list = this.getUserPreferences(
            bGetOwnedCals ? "X-NSCP-WCAP-PREF-icsCalendarOwned"
                          : "X-NSCP-WCAP-PREF-icsSubscribed", {} );
        var ret = [];
        for each( var item in list ) {
            var ar = item.split(',');
            // ',', '$' are not encoded. ',' can be handled here. WTF.
            for each ( a in ar ) {
                var dollar = a.indexOf('$');
                if (dollar >= 0) {
                    ret.push(
                        this.getCalendarByCalId( a.substring(0, dollar) ) );
                }
            }
        }
        out_count.value = ret.length;
        return ret;
    },
    getOwnedCalendars:
    function( out_count )
    {
        return this.getCalendars( out_count, true );
    },
    getSubscribedCalendars:
    function( out_count )
    {
        return this.getCalendars( out_count, false );
    },
    
    createCalendar:
    function( calId, name, bAllowDoubleBooking, bSetCalProps, bAddToSubscribed )
    {
        try {
            var url = this.getCommandUrl( "createcalendar" );
            url += ("&allowdoublebook=" + (bAllowDoubleBooking ? "1" : "0"));
            url += ("&set_calprops=" + (bSetCalProps ? "1" : "0"));
            url += ("&subscribe=" + (bAddToSubscribed ? "1" : "0"));
            url += ("&calid=" + encodeURIComponent(calId));
            // xxx todo: name undocumented!
            url += ("&name=" + encodeURIComponent(name));
            // xxx todo: what about categories param???
            this.issueSyncRequest( url + "&fmt-out=text%2Fxml", stringToXml );
            this.m_userPrefs = null; // reread prefs
            return this.getCalendarByCalId( this.userId + ":" + calId );
        }
        catch (exc) {
            this.notifyError( exc );
            throw exc;
        }
    },
    
    deleteCalendar:
    function( calId, bRemoveFromSubscribed )
    {
        try {
            var url = this.getCommandUrl( "deletecalendar" );
            url += ("&unsubscribe=" + (bRemoveFromSubscribed ? "1" : "0"));
            url += ("&calid=" + encodeURIComponent(calId));
            this.issueSyncRequest( url + "&fmt-out=text%2Fxml", stringToXml );
            this.m_userPrefs = null; // reread prefs
            this.m_calIdToCalendar[encodeURIComponent(calId)] = null;
        }
        catch (exc) {
            this.notifyError( exc );
            throw exc;
        }
    },    
    
    modifyCalendarSubscriptions:
    function( calIds, bSubscribe )
    {
        try {
            var url = this.getCommandUrl(
                bSubscribe ? "subscribe_calendars" : "unsubscribe_calendars" );
            var calId = "";
            for ( var i = 0; i < calIds.length; ++i ) {
                if (i > 0)
                    calId += ";";
                calId += encodeURIComponent(calIds[i]);
            }
            url += ("&calid=" + calId);
            this.issueSyncRequest( url + "&fmt-out=text%2Fxml", stringToXml );
            this.m_userPrefs = null; // reread prefs
        }
        catch (exc) {
            this.notifyError( exc );
            throw exc;
        }
    },
    
    subscribeToCalendars:
    function( count, calIds )
    {
        this.modifyCalendarSubscriptions( calIds, true );
    },
    
    unsubscribeFromCalendars:
    function( count, calIds )
    {
        this.modifyCalendarSubscriptions( calIds, false );
    },
    
    m_userPrefs: null,
    getUserPreferences:
    function( prefName, out_count )
    {
        try {
            if (this.m_userPrefs == null) {
                var url = this.getCommandUrl( "get_userprefs" );
                url += ("&userid=" + encodeURIComponent(this.userId));
                this.m_userPrefs = this.issueSyncRequest(
                    url + "&fmt-out=text%2Fxml", stringToXml );
            }
            var ret = [];
            var nodeList = this.m_userPrefs.getElementsByTagName(prefName);
            for ( var i = 0; i < nodeList.length; ++i ) {
                ret.push( trimString(nodeList.item(i).textContent) );
            }
            out_count.value = ret.length;
            return ret;
        }
        catch (exc) {
            this.notifyError( exc );
            throw exc;
        }
    },
    
    getFreeBusyTimes_resp:
    function( wcapResponse, calId, listener, requestId )
    {
        try {
            var xml = wcapResponse.data; // first statement, may throw
            if (listener != null) {
                var ret = [];
                var nodeList = xml.getElementsByTagName("FB");
                for ( var i = 0; i < nodeList.length; ++i ) {
                    var item = nodeList.item(i);
                    var str = item.textContent;
                    var slash = str.indexOf( '/' );
                    var start = new CalDateTime();
                    start.icalString = str.substr( 0, slash );
                    var end = new CalDateTime();
                    end.icalString = str.substr( slash + 1 );
                    var entry = {
                        isBusyEntry:
                        (item.attributes.getNamedItem("FBTYPE").nodeValue
                         == "BUSY"),
                        dtRangeStart: start,
                        dtRangeEnd: end
                    };
                    ret.push( entry );
                }
                listener.onGetFreeBusyTimes(
                    Components.results.NS_OK,
                    requestId, calId, ret.length, ret );
            }
            if (LOG_LEVEL > 0) {
                this.log( "getFreeBusyTimes_resp() calId=" + calId + ", " +
                          getWcapRequestStatusString(xml) );
            }
        }
        catch (exc) {
            const calIWcapErrors = Components.interfaces.calIWcapErrors;
            switch (getResultCode(exc)) {
            case calIWcapErrors.WCAP_NO_ERRNO: // workaround
            case calIWcapErrors.WCAP_ACCESS_DENIED_TO_CALENDAR:
            case calIWcapErrors.WCAP_CALENDAR_DOES_NOT_EXIST:
                this.log( "getFreeBusyTimes_resp() ignored: " +
                          errorToString(exc) ); // no error
                break;
            default:
                this.notifyError( exc );
                break;
            }
            if (listener != null)
                listener.onGetFreeBusyTimes( exc, requestId, calId, 0, [] );
        }
    },
    
    getFreeBusyTimes:
    function( calId, rangeStart, rangeEnd, bBusyOnly, listener,
              bAsync, requestId )
    {
        try {
            // assure DATETIMEs:
            if (rangeStart != null && rangeStart.isDate) {
                rangeStart = rangeStart.clone();
                rangeStart.isDate = false;
            }
            if (rangeEnd != null && rangeEnd.isDate) {
                rangeEnd = rangeEnd.clone();
                rangeEnd.isDate = false;
            }
            var zRangeStart = getIcalUTC(rangeStart);
            var zRangeEnd = getIcalUTC(rangeEnd);
            this.log( "getFreeBusyTimes():\n\trangeStart=" + zRangeStart +
                      ",\n\trangeEnd=" + zRangeEnd );
            
            var url = this.getCommandUrl( "get_freebusy" );
            url += ("&calid=" + encodeURIComponent(calId));
            url += ("&busyonly=" + (bBusyOnly ? "1" : "0"));
            url += ("&dtstart=" + zRangeStart);
            url += ("&dtend=" + zRangeEnd);
            url += "&fmt-out=text%2Fxml";
            
            var this_ = this;
            function resp( wcapResponse ) {
                this_.getFreeBusyTimes_resp(
                    wcapResponse, calId, listener, requestId );
            }
            if (bAsync)
                this.issueAsyncRequest( url, stringToXml, resp );
            else
                this.issueSyncRequest( url, stringToXml, resp );
        }
        catch (exc) {
            this.notifyError( exc );
            if (listener != null)
                listener.onGetFreeBusyTimes( exc, requestId, calId, 0, [] );
            throw exc;
        }
    }
};

var g_confirmedHttpLogins = null;
function confirmInsecureLogin( uri )
{
    if (g_confirmedHttpLogins == null) {
        g_confirmedHttpLogins = {};
        var confirmedHttpLogins = getPref(
            "calendar.wcap.confirmed_http_logins", "");
        var tuples = confirmedHttpLogins.split(',');
        for each ( var tuple in tuples ) {
            var ar = tuple.split(':');
            g_confirmedHttpLogins[ar[0]] = ar[1];
        }
    }
    
    var host = uri.hostPort;
    var encodedHost = encodeURIComponent(host);
    var confirmedEntry = g_confirmedHttpLogins[encodedHost];
    if (confirmedEntry)
        return (confirmedEntry == "1" ? true : false);
    
    var prompt = getWindowWatcher().getNewPrompter(null);
    var bundle = getWcapBundle();
    var dontAskAgain = { value: false };
    var bConfirmed = prompt.confirmCheck(
        bundle.GetStringFromName("noHttpsConfirmation.label"),
        bundle.formatStringFromName("noHttpsConfirmation.text", [host], 1),
        bundle.GetStringFromName("noHttpsConfirmation.check.text"),
        dontAskAgain );
    
    if (dontAskAgain.value) {
        // save decision for all running calendars and all future confirmations:
        var confirmedHttpLogins = getPref(
            "calendar.wcap.confirmed_http_logins", "");
        if (confirmedHttpLogins.length > 0)
            confirmedHttpLogins += ",";
        confirmedEntry = (bConfirmed ? "1" : "0");
        confirmedHttpLogins += (encodedHost + ":" + confirmedEntry);
        setPref("calendar.wcap.confirmed_http_logins", confirmedHttpLogins);
        g_confirmedHttpLogins[encodedHost] = confirmedEntry;
    }
    
    return bConfirmed;
}

