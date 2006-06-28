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

function calWcapSession( thatUri ) {
    this.wrappedJSObject = this;
    // sensible default for user id login:
    var username = decodeURIComponent( thatUri.username );
    if (username != "") {
        var nColon = username.indexOf(':');
        this.m_userId = (nColon >= 0 ? username.substr(0, nColon) : username);
    }
    this.m_uri = thatUri.clone();
    this.m_uri.userPass = "";
    
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
    m_uri: null,
    m_sessionId: null,
    m_userId: null,
    m_bNoLoginsAnymore: false,
    
    get uri() { return this.m_uri; },
    
    toString:
    function( msg )
    {
        var str = this.uri.spec;
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
    
    // nsISupports:
    QueryInterface:
    function( iid )
    {
        if (iid.equals(Components.interfaces.nsIObserver) ||
            iid.equals(Components.interfaces.nsISupports)) {
            return this;
        }
        else
            throw Components.results.NS_ERROR_NO_INTERFACE;
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
    
    get userId() { return this.m_userId; },
    
    getSupportedTimezones:
    function( bRefresh )
    {
        var key = this.uri.hostPort;
        if ((bRefresh || !g_allSupportedTimezones[key]) &&
            this.m_sessionId != null)
        {
            var url = this.uri.spec +
                "get_all_timezones.wcap?appid=mozilla-lightning" +
                "&fmt-out=text%2Fcalendar&id=" + this.m_sessionId;
            var str = issueSyncUtf8Request( url );
            if (str == null)
                throw new Error("request failed!");
            var icalRootComp = getIcsService().parseICS( str );
            if (icalRootComp == null)
                throw new Error("invalid data!");
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
        var key = this.uri.hostPort;
        if ((bRefresh || !g_serverTimeDiffs[key]) &&
            this.m_sessionId != null) {
            var url = this.uri.spec +
                // xxx todo: assuming same diff for all calids:
                "gettime.wcap?appid=mozilla-lightning" +
                "&fmt-out=text%2Fcalendar&id=" + this.m_sessionId;
            // xxx todo: this is no solution!
            var localTime = getTime();
            var str = issueSyncUtf8Request( url );
            if (str == null)
                throw new Error("request failed!");
            var icalRootComp = getIcsService().parseICS( str );
            if (icalRootComp == null)
                throw new Error("invalid data!");
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
    
    getSessionId:
    function( timedOutSessionId )
    {
        if (this.m_bNoLoginsAnymore) {
            this.log( "login has failed, no logins anymore for this user." );
            return null;
        }
        if (getIoService().offline) {
            this.log( "in offline mode." );
            return null;
        }
        
        if (this.m_sessionId == null || this.m_sessionId == timedOutSessionId) {
            // xxx todo: ask dmose how to do better...
            // possible HACK here, because of lack of sync possibilities:
            // when we run into executing dialogs, the js runtime
            // concurrently executes (another getItems() request).
            // That concurrent request needs to wait for the first login
            // attempt to finish.
            // Creating a thread event queue somehow hinders the js engine
            // from scheduling another js execution.
            var eventQueueService =
                Components.classes["@mozilla.org/event-queue-service;1"]
                .getService(Components.interfaces.nsIEventQueueService);
            var eventQueue = eventQueueService.pushThreadEventQueue();
            try {
                if (this.m_sessionId == null ||
                    this.m_sessionId == timedOutSessionId)
                {
                    if (timedOutSessionId != null) {
                        this.m_sessionId = null;
                        this.log( "session timeout; prompting to reconnect." );
                        var prompt = getWindowWatcher().getNewPrompter(null);
                        var bundle = getBundle();
                        if (! prompt.confirm(
                                bundle.GetStringFromName(
                                    "reconnectConfirmation.label" ),
                                bundle.formatStringFromName(
                                    "reconnectConfirmation.text",
                                    [this.uri.hostPort], 1 ) )) {
                            this.m_bNoLoginsAnymore = true;
                        }
                    }
                    if (! this.m_bNoLoginsAnymore)
                        this.getSessionId_();
                    
                    this.getSupportedTimezones( true /* refresh */ );
                    this.getServerTimeDiff( true /* refresh */ );
                }
            }
            catch (exc) {
                eventQueueService.popThreadEventQueue( eventQueue );
                throw exc;
            }
            eventQueueService.popThreadEventQueue( eventQueue );
        }
        return this.m_sessionId;
    },
    getSessionId_:
    function()
    {
        if (this.m_sessionId == null) {
            
            var passwordManager =
                Components.classes["@mozilla.org/passwordmanager;1"]
                .getService(Components.interfaces.nsIPasswordManager);
            
            var outUser = { value: this.m_userId };
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
            
            var loginUri = this.uri.clone();
            if (loginUri.scheme.toLowerCase() != "https") {
                if (loginUri.port == -1) {
                    // no https, but no port specified
                    // => enforce login via https:
                    loginUri.scheme = "https";
                }
                else {
                    // user has specified a specific port, but no https:
                    // => leave it to her whether to connect...
                    if (! confirmUnsecureLogin( loginUri )) {
                        this.m_bNoLoginsAnymore = true;
                        this.log( "user rejected unsecure login." );
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
                    this.login( loginUri, outUser.value, outPW.value );
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
                                getBundle().formatStringFromName(
                                    "accessingServerFailedError.text",
                                    [loginUri.hostPort], 1 ) );
                        }
                        if (this.uri.scheme.toLowerCase() == "https") {
                            // user specified https, so http is no option:
                            loginText = null;
                            throw new Error(
                                getBundle() .formatStringFromName(
                                    "mandatoryHttpsError.text",
                                    [loginUri.hostPort], 1 ) );
                        }
                        // http possible, ask for it:
                        if (confirmUnsecureLogin( loginUri )) {
                            if (outPW.value != null) {
                                // user/pw has been found previously,
                                // but no login was possible,
                                // try again using http here:
                                this.login( loginUri,
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
                    Components.utils.reportError( exc );
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
                            getBundle().GetStringFromName("loginDialog.label"),
                            loginText, outUser, outPW,
                            getBundle().GetStringFromName(
                                "loginDialog.savePW.label" ),
                            savePW ))
                    {
                        try {
                            this.login( loginUri, outUser.value, outPW.value );
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
    
    getServerInfo:
    function( uri )
    {
        loginTextVars = [uri.hostPort];
        var loginText;
        var wcapVersion;
        try {
            // currently, xml parsing at an early stage during process startup
            // does not work reliably, so use libical parsing for now:
            var str = issueSyncUtf8Request(
                uri.spec + "version.wcap?fmt-out=text%2Fcalendar" );
            if (str == null)
                throw new Error("request failed!");
            var icalRootComp = getIcsService().parseICS( str );
            if (icalRootComp == null)
                throw new Error("invalid data!");
            var prop = icalRootComp.getFirstProperty( "PRODID" );
            if (prop == null)
                throw new Error("missing PRODID!");
            loginTextVars.push( prop.value );
            var prop = icalRootComp.getFirstProperty( "X-NSCP-SERVERVERSION" );
            if (prop == null)
                throw new Error("missing X-NSCP-SERVERVERSION!");
            loginTextVars.push( prop.value );
            var prop = icalRootComp.getFirstProperty( "X-NSCP-WCAPVERSION" );
            if (prop == null)
                throw new Error("missing X-NSCP-WCAPVERSION!");
            wcapVersion = prop.value;
            loginTextVars.push( wcapVersion );
            
//             var xml = issueSyncXMLRequest(
//                 uri.spec + "version.wcap?fmt-out=text%2Fxml" );
//             wcapVersion = xml.getElementsByTagName(
//                 "X-NSCP-WCAPVERSION" ).item(0).textContent;
//             if (wcapVersion == undefined || wcapVersion == "")
//                 throw new Error("invalid response!");
//             serverInfo =
//                 ("Server-Info: " +
//                  xml.getElementsByTagName(
//                      "iCal" ).item(0).attributes.getNamedItem(
//                          "prodid" ).value +
//                  ", v" + xml.getElementsByTagName(
//                      "X-NSCP-SERVERVERSION" ).item(0).textContent +
//                  ", WCAP v" + wcapVersion);            
        }
        catch (exc) {
            this.log( "error: " + exc );
            return null;
        }
        if (parseInt(wcapVersion) < 2.0) {
            this.log( "parsed server WCAP major: " + parseInt(wcapVersion) );
            throw new Error(
                getBundle("insufficientWcapVersionError.text", loginTextVars) );
        }
        return getBundle().formatStringFromName(
            "loginDialog.text", loginTextVars, loginTextVars.length );
    },
    
    login:
    function( loginUri, user, pw )
    {
        if (this.m_sessionId != null) {
            this.logout();
        }
        // currently, xml parsing at an early stage during process startup
        // does not work reliably, so use libical parsing for now:
        var str = issueSyncUtf8Request(
            loginUri.spec + "login.wcap?fmt-out=text%2Fcalendar&user=" +
            encodeURIComponent(user) + "&password=" + encodeURIComponent(pw),
            null /* receiverFunc */, false /* no logging */ );
        if (str == null)
            throw new Error("request failed!");
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
    
    logout:
    function()
    {
        if (this.m_sessionId != null) {
            // although io service's offline flag is already
            // set BEFORE notification (about to go offline, nsIOService.cpp).
            // WTF.
            var url = (this.uri.spec +
                       "logout.wcap?fmt-out=text%2Fxml&id=" + this.m_sessionId);
            try {
                checkWcapXmlErrno( issueSyncXMLRequest(url), -1 );
                this.log( "WCAP logout succeeded." );
            }
            catch (exc) {
                this.log( "WCAP logout failed: " + exc );
                Components.utils.reportError( exc );
            }
            this.m_sessionId = null;
            this.m_userId = null;
        }
        // ask next time we log in:
        var this_ = this;
        g_httpHosts = g_httpHosts.filter(
            function(hostEntry) {
                return (hostEntry.m_host != this_.uri.hostPort); } );
        this.m_bNoLoginsAnymore = false;
    }
};

g_httpHosts = [];
function confirmUnsecureLogin( uri )
{
    var host = uri.hostPort;
    for each ( var hostEntry in g_httpHosts ) {
        if (hostEntry.m_host == host) {
            return hostEntry.m_bConfirmed;
        }
    }
    var prompt = getWindowWatcher().getNewPrompter(null);
    var bundle = getBundle();
    var bConfirmed = prompt.confirm(
        bundle.GetStringFromName("noHttpsConfirmation.label"),
        bundle.formatStringFromName("noHttpsConfirmation.text", [host], 1) );
    // save decision for all calendars:
    g_httpHosts.push(
        { m_host: host, m_bConfirmed: bConfirmed } );
    return bConfirmed;
}

g_sessions = {};
function getSession( uri )
{
    var session = g_sessions[uri.spec];
    if (! session) {
        logMessage( "getSession()", "entering session for uri=" + uri.spec );
        var session = new calWcapSession( uri );
        g_sessions[uri.spec] = session;
    }
    return session;
}

