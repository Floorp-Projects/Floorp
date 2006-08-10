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
        var str = ("error: " + errorToString(err));
        Components.utils.reportError( this.log( str, context ) );
        return str;
    },
    notifyError:
    function( err )
    {
        debugger;
        var str = this.logError( err );
        this.notifyObservers( "onError",
                              [err instanceof Error ? -1 : err, str] );
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
            var url = this.sessionUri.spec +
                "get_all_timezones.wcap?appid=mozilla-lightning" +
                "&fmt-out=text%2Fcalendar&id=" + this.m_sessionId;
            var str = issueSyncRequest( url );
            var icalRootComp = getIcsService().parseICS( str );
            if (icalRootComp == null)
                throw new Error("invalid data, expected ical!");
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
            var url = this.sessionUri.spec +
                // xxx todo: assuming same diff for all calids:
                "gettime.wcap?appid=mozilla-lightning" +
                "&fmt-out=text%2Fcalendar&id=" + this.m_sessionId;
            // xxx todo: this is no solution!
            var localTime = getTime();
            var str = issueSyncRequest( url );
            var icalRootComp = getIcsService().parseICS( str );
            if (icalRootComp == null)
                throw new Error("invalid data, expected ical!");
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
    
    getRealmName:
    function( uri )
    {
        // xxx todo: realm names must not have a trailing slash
        var realm = uri.spec;
        if (realm[realm.length - 1] == '/')
            realm = realm.substr(0, realm.length - 1);
        return realm;
    },
    
    m_sessionId: null,
    m_bNoLoginsAnymore: false,
    
    getSessionId:
    function( timedOutSessionId )
    {
        if (this.m_bNoLoginsAnymore) {
            this.log( "login has failed, no logins anymore for this user." );
            throw Components.interfaces.calIWcapErrors.WCAP_LOGIN_FAILED;
        }
        if (getIoService().offline) {
            this.log( "in offline mode." );
            throw NS_ERROR_OFFLINE;
        }
        
        if (this.m_sessionId == null || this.m_sessionId == timedOutSessionId) {
            
            var this_ = this;
            syncExec(
                function() {
                    if (this_.m_sessionId == null ||
                        this_.m_sessionId == timedOutSessionId)
                    {
                        if (timedOutSessionId != null) {
                            this_.m_sessionId = null;
                            this_.log(
                                "session timeout; prompting to reconnect." );
                            var prompt =getWindowWatcher().getNewPrompter(null);
                            var bundle = getWcapBundle();
                            if (!prompt.confirm(
                                    bundle.GetStringFromName(
                                        "reconnectConfirmation.label" ),
                                    bundle.formatStringFromName(
                                        "reconnectConfirmation.text",
                                        [this_.uri.hostPort], 1 ) )) {
                                this_.m_bNoLoginsAnymore = true;
                            }
                        }
                        if (!this_.m_bNoLoginsAnymore)
                            this_.getSessionId_();
                        
                        if (this_.m_sessionId != null) {
                            this_.getSupportedTimezones(true /* refresh */);
                            this_.getServerTimeDiff(true /* refresh */);
                            // preread calprops for subscribed calendars:
                            var cals = this_.getSubscribedCalendars({});
                            for each ( cal in cals ) {
                                cal.getCalProps_(true /* async */);
                            }
                        }
                    }
                } );
        }
        if (this.m_sessionId == null) {
            throw Components.interfaces.calIWcapErrors.WCAP_LOGIN_FAILED;
        }
        return this.m_sessionId;
    },
    getSessionId_:
    function()
    {
        if (this.m_sessionId == null) {
            
            this.log( "attempting to get a session id..." );
            
            var passwordManager =
                Components.classes["@mozilla.org/passwordmanager;1"]
                .getService(Components.interfaces.nsIPasswordManager);
            
            var outUser = { value: this.userId };
            var outPW = { value: null };
            
            var enumerator = passwordManager.enumerator;
            var realm = this.getRealmName(this.uri);
            while (enumerator.hasMoreElements()) {
                var pwEntry = enumerator.getNext().QueryInterface(
                    Components.interfaces.nsIPassword );
                if (LOG_LEVEL > 1) {
                    this.log( "pw entry:\n\thost=" + pwEntry.host +
                              "\n\tuser=" + pwEntry.user );
                }
                if (pwEntry.host == realm) { // found an entry matching URI:
                    outUser.value = pwEntry.user;
                    outPW.value = pwEntry.password;
                    break;
                }
            }
            
            var loginUri = this.sessionUri.clone();
            if (loginUri.scheme.toLowerCase() != "https") {
                if (loginUri.port == -1) {
                    // no https, but no port specified
                    // => enforce login via https:
                    loginUri.scheme = "https";
                }
                else {
                    // user has specified a specific port, but no https:
                    // => leave it to her whether to connect...
                    if (!confirmInsecureLogin( loginUri )) {
                        this.m_bNoLoginsAnymore = true;
                        this.log( "user rejected insecure login." );
                        return null;
                    }
                }
            }
            
            if (outUser.value == null || outPW.value == null) {
                this.log( "no password entry found." );
            }
            else {
                this.log( "password entry found for user " + outUser.value );
                try {
                    this.login_( loginUri, outUser.value, outPW.value );
                }
                catch (exc) { // ignore silently
                }
            }
            
            if (this.m_sessionId == null) {
                // preparing login:
                var loginText = null;
                try {
                    loginText = this.getServerInfo( loginUri );
                    if (loginText == null) {
                        if (loginUri.scheme.toLowerCase() == "https") {
                            // gathering server info via https has failed,
                            // try http:
                            loginUri.scheme = "http";
                            loginText = this.getServerInfo( loginUri );
                        }
                        if (loginText == null) {
                            throw new Error(
                                getWcapBundle().formatStringFromName(
                                    "accessingServerFailedError.text",
                                    [loginUri.hostPort], 1 ) );
                        }
                        if (this.sessionUri.scheme.toLowerCase() == "https") {
                            // user specified https, so http is no option:
                            loginText = null;
                            throw new Error(
                                getWcapBundle().formatStringFromName(
                                    "mandatoryHttpsError.text",
                                    [loginUri.hostPort], 1 ) );
                        }
                        // http possible, ask for it:
                        if (confirmInsecureLogin( loginUri )) {
                            if (outPW.value != null) {
                                // user/pw has been found previously,
                                // but no login was possible,
                                // try again using http here:
                                this.login_( loginUri,
                                             outUser.value, outPW.value );
                                if (this.m_sessionId != null)
                                    return this.m_sessionId;
                            }
                        }
                        else {
                            this.m_bNoLoginsAnymore = true;
                            this.log( "user rejected unsecure login." );
                            return null;
                        }
                    }
                }
                catch (exc) {
                    this.logError( exc );
                    if (loginText == null) {
                        // accessing server or invalid protocol,
                        // no logins anymore:
                        this.m_bNoLoginsAnymore = true;
                        throw exc; // propagate error message
                    }
                    // else maybe user pw has changed or similar...
                }
                
                if (outPW.value != null) {
                    // login failed before, so try to remove from pw db:
                    try {
                        passwordManager.removeUser(
                            this.getRealmName(this.uri), outUser.value );
                    }
                    catch (exc) {
                        this.log( "error removing from pw db: " + exc );
                    }
                }
                
                var savePW = { value: false };
                while (this.m_sessionId == null) {
                    var prompt = getWindowWatcher().getNewPrompter(null);
                    if (prompt.promptUsernameAndPassword(
                            getWcapBundle().GetStringFromName(
                                "loginDialog.label"),
                            loginText, outUser, outPW,
                            getWcapBundle().GetStringFromName(
                                "loginDialog.check.text"),
                            savePW ))
                    {
                        try {
                            this.login_( loginUri, outUser.value, outPW.value );
                        }
                        catch (exc) {
                            Components.utils.reportError( exc );
                            // xxx todo: UI?
                        }
                    }
                    else { // dialog cancelled, don't login anymore:
                        this.m_bNoLoginsAnymore = true;
                        this.log( "login cancelled, will not prompt again." );
                        break;
                    }
                }
                if (this.m_sessionId != null && savePW.value) {
                    // save pw under session uri:
                    passwordManager.addUser( this.getRealmName(this.uri),
                                             outUser.value, outPW.value );
                }
            }
        }
        
        return this.m_sessionId;
    },
    login_:
    function( loginUri, user, pw )
    {
        if (this.m_sessionId != null) {
            this.logout();
        }
        // currently, xml parsing at an early stage during process startup
        // does not work reliably, so use libical parsing for now:
        var str = issueSyncRequest(
            loginUri.spec + "login.wcap?fmt-out=text%2Fcalendar&user=" +
            encodeURIComponent(user) + "&password=" + encodeURIComponent(pw),
            null /* receiverFunc */, false /* no logging */ );
        var icalRootComp = getIcsService().parseICS( str );
        checkWcapIcalErrno( icalRootComp );
        var prop = icalRootComp.getFirstProperty( "X-NSCP-WCAP-SESSION-ID" );
        if (prop == null)
            throw new Error("missing X-NSCP-WCAP-SESSION-ID!");
        this.m_sessionId = prop.value;
        
//         var xml = issueSyncXMLRequest(
//             loginUri.spec + "login.wcap?fmt-out=text%2Fxml&user=" +
//             encodeURIComponent(user) + "&password=" + encodeURIComponent(pw) );
//         checkWcapXmlErrno( xml );
//         this.m_sessionId = xml.getElementsByTagName(
//             "X-NSCP-WCAP-SESSION-ID" ).item(0).textContent;
        this.m_userId = user;
        this.log( "WCAP login succeeded." );
    },    
    
    getServerInfo:
    function( uri )
    {
        var icalRootComp;
        try {
            // currently, xml parsing at an early stage during process startup
            // does not work reliably, so use libical parsing for now:
            var str = issueSyncRequest(
                uri.spec + "version.wcap?fmt-out=text%2Fcalendar" );
            if (str.indexOf("BEGIN:VCALENDAR") < 0)
                return null; // no ical data returned
            icalRootComp = getIcsService().parseICS( str );
            if (icalRootComp == null)
                throw new Error("invalid ical data!");
        }
        catch (exc) { // soft error; request denied etc.
            this.log( "server version request failed: " + errorToString(exc) );
            return null;
        }
        
        loginTextVars = [uri.hostPort];
        var prop = icalRootComp.getFirstProperty( "PRODID" );
        loginTextVars.push( prop ? prop.value : "<unknown>" );
        prop = icalRootComp.getFirstProperty( "X-NSCP-SERVERVERSION" );
        loginTextVars.push( prop ? prop.value : "<unknown>" );
        prop = icalRootComp.getFirstProperty( "X-NSCP-WCAPVERSION" );
        if (prop == null)
            throw new Error("missing X-NSCP-WCAPVERSION!");
        loginTextVars.push( prop.value );
        var wcapVersion = parseInt(prop.value);
        if (wcapVersion < 3) {
            var prompt = getWindowWatcher().getNewPrompter(null);
            var bundle = getWcapBundle();
            var labelText = bundle.GetStringFromName(
                "insufficientWcapVersionConfirmation.label");
            if (!prompt.confirm( labelText,
                                 bundle.formatStringFromName(
                                     "insufficientWcapVersionConfirmation.text",
                                     loginTextVars, loginTextVars.length ) )) {
                throw new Error(labelText);
            }
        }
        return getWcapBundle().formatStringFromName(
            "loginDialog.text", loginTextVars, loginTextVars.length );
    },
    
    getCommandUrl:
    function( wcapCommand )
    {
        if (this.sessionUri == null)
            throw new Error("no URI!");
        // ensure established session, so userId is set;
        // (calId defaults to userId) if not set:
        this.getSessionId();
        return (this.sessionUri.spec +
                wcapCommand + ".wcap?appid=mozilla-lightning");
    },
    
    issueRequest:
    function( url, issueFunc, dataConvFunc, receiverFunc )
    {
        var sessionId = this.getSessionId();
        
        var this_ = this;
        issueFunc(
            url + ("&id=" + sessionId),
            function( data ) {
                var wcapResponse = new WcapResponse();
                try {
                    try {
                        wcapResponse.data = dataConvFunc(
                            data, wcapResponse );
                    }
                    catch (exc) {
                        if (exc == Components.interfaces.
                            calIWcapErrors.WCAP_LOGIN_FAILED) /* timeout */ {
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
                    var nColon = username.indexOf(':');
                    this.m_userId =
                        (nColon >= 0 ? username.substr(0, nColon) : username);
                }
                this.m_uri = thatUri.clone();
                this.log( "setting uri to " + this.uri.spec );
                this.m_sessionUri = thatUri.clone();
                this.m_sessionUri.userPass = "";
                this.log( "setting sessionUri to " + this.sessionUri.spec );
            }
        }
    },
    
    m_userId: null,
    get userId() { return this.m_userId; },
    
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
            // although io service's offline flag is already
            // set BEFORE notification (about to go offline, nsIOService.cpp).
            // WTF.
            var url = (this.sessionUri.spec +
                       "logout.wcap?fmt-out=text%2Fxml&id=" + this.m_sessionId);
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
        this.m_userId = null;
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
    function( wcapResponse, calId, iListener, requestId )
    {
        try {
            var xml = wcapResponse.data; // first statement, may throw
            if (iListener != null) {
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
                iListener.onGetFreeBusyTimes(
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
            switch (exc) {
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
            if (iListener != null)
                iListener.onGetFreeBusyTimes( exc, requestId, calId, 0, [] );
        }
    },
    
    getFreeBusyTimes:
    function( calId, rangeStart, rangeEnd, bBusyOnly, iListener,
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
                    wcapResponse, calId, iListener, requestId );
            }
            if (bAsync)
                this.issueAsyncRequest( url, stringToXml, resp );
            else
                this.issueSyncRequest( url, stringToXml, resp );
        }
        catch (exc) {
            this.notifyError( exc );
            if (iListener != null)
                iListener.onGetFreeBusyTimes( exc, requestId, calId, 0, [] );
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
            "calendar.wcap.confirmedHttpLogins", "");
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
            "calendar.wcap.confirmedHttpLogins", "");
        if (confirmedHttpLogins.length > 0)
            confirmedHttpLogins += ",";
        confirmedEntry = (bConfirmed ? "1" : "0");
        confirmedHttpLogins += (encodedHost + ":" + confirmedEntry);
        setPref("calendar.wcap.confirmedHttpLogins", confirmedHttpLogins);
        g_confirmedHttpLogins[encodedHost] = confirmedEntry;
    }
    
    return bConfirmed;
}

