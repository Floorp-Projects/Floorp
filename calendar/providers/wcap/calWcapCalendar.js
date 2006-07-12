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

function calWcapCalendar() {
    this.wrappedJSObject = this;
    this.m_observers = [];
}
calWcapCalendar.prototype = {
    m_ifaces: [ Components.interfaces.calIWcapCalendar,
                Components.interfaces.calICalendar,
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
        return calWcapCalendarModule.WcapCalendarInfo.classDescription;
    },
    get contractID() {
        return calWcapCalendarModule.WcapCalendarInfo.contractID;
    },
    get classID() {
        return calWcapCalendarModule.WcapCalendarInfo.classID;
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
    function()
    {
        var str;
        if (this.m_session == null) {
            str = "no session";
        }
        else {
            str = this.session.toString();
            if (this.m_calId != null && this.m_calId != this.userId)
                str += (", calId=" + this.m_calId);
        }
        return str;
    },
    log:
    function( msg, context )
    {
        return logMessage( context ? context : this.toString(), msg );
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
    
    // calICalendar:
    get name() {
        return getCalendarManager().getCalendarPref( this, "NAME" );
    },
    set name( name ) {
        getCalendarManager().setCalendarPref( this, "NAME", name );
    },
    
    get type() { return "wcap"; },
    
    m_superCalendar: null,
    get superCalendar() { return this.m_superCalendar || this; },
    set superCalendar( cal ) { this.m_superCalendar = cal; },
    
    m_bReadOnly: false,
    get readOnly() { return (this.m_bReadOnly || !this.isOwnedCalendar); },
    set readOnly( bReadOnly ) { this.m_bReadOnly = bReadOnly; },
    
    m_uri: null,
    get uri() { return this.m_uri },
    set uri( thatUri )
    {
        if (this.m_uri == null || thatUri == null ||
            !this.m_uri.equals(thatUri))
        {
            if (this.m_session != null) {
                this.m_session.logout();
                this.m_session = null;
            }
            this.m_uri = null;
            this.m_calId = null;
            if (thatUri != null) {
                this.m_uri = thatUri.clone();
                this.m_calId = decodeURIComponent( thatUri.username );
                if (this.m_calId == "") {
                    this.m_calId = null;
                }
            }
            this.refresh();
        }
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
    
    // xxx todo: batch currently not used
    startBatch: function() { this.notifyObservers( "onStartBatch", [] ); },
    endBatch: function() { this.notifyObservers( "onEndBatch", [] ); },
    
    m_bSuppressAlarms: true /* xxx todo:
                               off for now until all problems are solved */,
    get suppressAlarms() {
        return (this.m_bSuppressAlarms || this.readOnly);
    },
    set suppressAlarms( bSuppressAlarms ) {
        this.m_bSuppressAlarms = bSuppressAlarms;
    },
    
    get canRefresh() { return false; },
    
    refresh:
    function()
    {
        // no-op
        this.log("refresh()");
    },
    
    getCommandUrl:
    function( wcapCommand )
    {
        if (this.uri == null)
            throw new Error("no URI!");
        var session = this.session;
        // ensure established session, so userId is set;
        // (calId defaults to userId) if not set:
        session.getSessionId();
        return (session.uri.spec + wcapCommand +
                ".wcap?appid=mozilla-lightning");
    },
    
    issueRequest:
    function( url, issueFunc, dataConvFunc, receiverFunc )
    {
        var sessionId = this.session.getSessionId();
        
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
                            this_.session.getSessionId(
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
    
    m_session: null,
    get session() {
        if (this.m_session == null) {
            this.m_session = getSession( this.uri );
        }
        return this.m_session;
    },
    
    // calIWcapCalendar:
    
    getWcapErrorString:
    function( rc )
    {
        return wcapErrorToString(rc);
    },
    
    // xxx todo: which userId is used when for offline scheduling?
    //           if not logged in, no calId/userId is known... => UI
    m_calId: null,
    get calId() {
        var userId = this.userId; // assure being logged in
        return this.m_calId || userId;
    },
    set calId( id ) {
        this.log( "setting calId to " + id );
        this.m_calId = id;
    },
    get userId() { return this.session.userId; },
    
    get isOwnedCalendar() {
        return (this.calId == this.userId ||
                this.calId.indexOf(this.userId + ":") == 0);
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
            return (this.userId + ":" + calId);
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
        }
        catch (exc) {
            this.notifyError( exc );
            throw exc;
        }
    },
    
    getCalIds:
    function( out_count, bGetOwnedCals )
    {
        try {
            var url = this.getCommandUrl(
                bGetOwnedCals ? "list" : "list_subscribed" );
            var ret = [];
            var xml = this.issueSyncRequest(
                url + "&fmt-out=text%2Fxml", stringToXml );
            var nodeList = xml.getElementsByTagName(
                bGetOwnedCals ? "X-S1CS-CALPROPS-OWNED-CALENDAR"
                              : "X-S1CS-CALPROPS-SUBSCRIBED-CALENDAR" );
            for ( var i = 0; i < nodeList.length; ++i ) {
                ret.push( nodeList.item(i).textContent );
            }
            out_count.value = ret.length;
            return ret;
        }
        catch (exc) {
            this.notifyError( exc );
            throw exc;
        }
    },
    
    getOwnedCalendars:
    function( out_count )
    {
        return this.getCalIds( out_count, true );
    },
    
    getSubscribedCalendars:
    function( out_count )
    {
        return this.getCalIds( out_count, false );
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
    },
    
    m_calProps: null,
    m_calPropsCalid: null,
    getCalendarProperties:
    function( propName, calId, out_count )
    {
        try {
            if (calId.length == 0) {
                calId = this.calId;
            }
            if (this.m_calPropsCalid != calId) {
                var url = this.getCommandUrl( "get_calprops" );
                url += ("&calid=" + encodeURIComponent(calId));
                this.m_calProps = this.issueSyncRequest(
                    url + "&fmt-out=text%2Fxml", stringToXml );
                this.m_calPropsCalid = calId;
            }
            var ret = [];
            var nodeList = this.m_calProps.getElementsByTagName( propName );
            for ( var i = 0; i < nodeList.length; ++i ) {
                ret.push( nodeList.item(i).textContent );
            }
            out_count.value = ret.length;
            return ret;
        }
        catch (exc) {
            this.notifyError( exc );
            throw exc;
        }
    },
    
    get defaultTimezone() {
        var tzid = this.getCalendarProperties("X-NSCP-CALPROPS-TZID", "", {});
        if (tzid.length < 1) {
            return "UTC"; // fallback
        }
        return tzid[0];
    },
    
//     set defaultTimezone( tzid ) {
//         if (this.readOnly)
//             throw Components.interfaces.calIErrors.CAL_IS_READONLY;
//         // xxx todo:
//         throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
//     },
    
    getAlignedTimezone:
    function( tzid )
    {
        // check whether it is one of cs:
        if (tzid.indexOf("/mozilla.org/20050126_1/") == 0 ||
            !this.session.isSupportedTimezone(tzid)) {
            // use calendar's default:
            return this.defaultTimezone;
        }
        else // is ok (supported):
            return tzid;
    }
};

