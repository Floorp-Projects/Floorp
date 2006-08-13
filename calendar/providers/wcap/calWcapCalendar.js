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

function createWcapCalendar( calId, session )
{
    var cal = new calWcapCalendar( calId, session );
    switch (CACHE) {
    case "memory":
    case "storage":
        // wrap it up:
        var cal_ = new calWcapCachedCalendar();
        cal_.remoteCal = cal;
        cal = cal_;
        break;
    }
    return cal;
}

function calWcapCalendar( calId, session ) {
    this.wrappedJSObject = this;
    this.m_calId = calId;
    this.m_session = session;
    this.m_bSuppressAlarms = SUPPRESS_ALARMS;
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
        var str = this.session.toString();
        str += (", calId=" + this.calId);
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
    
    // xxx todo: will potentially vanish from calICalendar:
    get uri() { return this.session.uri; },
    set uri( thatUri ) {
        this.session.uri = thatUri;
        this.m_calProps = null;
    },
    notifyObservers:
    function( func, args ) {
        this.session.notifyObservers( func, args );
    },
    addObserver:
    function( observer ) {
        this.session.addObserver( observer );
    },
    removeObserver:
    function( observer ) {
        this.session.removeObserver( observer );
    },
    
    // xxx todo: batch currently not used
    startBatch: function() { this.notifyObservers( "onStartBatch", [] ); },
    endBatch: function() { this.notifyObservers( "onEndBatch", [] ); },
    
    // xxx todo: rework like in
    //           https://bugzilla.mozilla.org/show_bug.cgi?id=257428
    m_bSuppressAlarms: false,
    get suppressAlarms() {
        return (this.m_bSuppressAlarms || this.readOnly);
    },
    set suppressAlarms( bSuppressAlarms ) {
        this.m_bSuppressAlarms = bSuppressAlarms;
    },
    
    get canRefresh() { return false; },
    refresh: function() {
        // no-op
        this.log("refresh()");
    },
    
    // calIWcapCalendar:
    
    m_session: null,
    get session() { return this.m_session; },
    
    // xxx todo: for now to make subscriptions context menu work,
    //           will vanish when UI has been revised and every subscribed
    //           calendar has its own calICalendar object...
    //           poking calId, so default calendar will then behave
    //           like a subscribed one...
    m_calId: null,
    get calId() {
        var userId = this.session.userId; // assure being logged in
        return this.m_calId || userId;
    },
    set calId( id ) {
        this.log( "setting calId to " + id );
        this.m_calId = id;
        // refresh calprops:
        this.m_calProps = null;
        this.getCalProps_( true /* async */ );
    },
    
    get ownerId() {
        var ar = this.getCalendarProperties("X-NSCP-CALPROPS-PRIMARY-OWNER",{});
        if (ar.length < 1) {
            this.notifyError(
                "cannot determine primary owner of calendar " + this.calId );
            // fallback to userId:
            return this.session.userId;
        }
        return ar[0];
    },
    
    get description() {
        var ar = this.getCalendarProperties("X-NSCP-CALPROPS-DESCRIPTION", {});
        if (ar.length < 1) {
            // fallback to display name:
            return this.displayName;
        }
        return ar[0];
    },
    
    get displayName() {
        var ar = this.getCalendarProperties("X-NSCP-CALPROPS-NAME", {});
        if (ar.length < 1) {
            // fallback to common name:
            ar = this.getCalendarProperties(
                "X-S1CS-CALPROPS-COMMON-NAME", {});
            if (ar.length < 1) {
                return this.calId;
            }
        }
        return ar[0];
    },
    
    get isOwnedCalendar() {
        if (!this.session.isLoggedIn)
            return false;
        var ownerId = this.ownerId;
        return (this.calId == ownerId ||
                this.calId.indexOf(ownerId + ":") == 0);
    },
    
    getCalendarProperties:
    function( propName, out_count )
    {
        var ret = [];
        var calProps = this.getCalProps_(false /* !async: waits for response*/);
        if (calProps != null) {
            var nodeList = calProps.getElementsByTagName(propName);
            for ( var i = 0; i < nodeList.length; ++i ) {
                ret.push( trimString(nodeList.item(i).textContent) );
            }
        }
        out_count.value = ret.length;
        return ret;
    },
    m_calProps: null,
    getCalProps_:
    function( bAsync )
    {
        try {
            if (this.m_calProps == null) {
                var url = this.session.getCommandUrl( "get_calprops" );
                url += ("&calid=" + encodeURIComponent(this.calId));
                url += "&fmt-out=text%2Fxml";
                var this_ = this;
                function resp( wcapResponse ) {
                    try {
                        // first statement, may throw:
                        var xml = wcapResponse.data;
                        if (this_.m_calProps == null)
                            this_.m_calProps = xml;
                    }
                    catch (exc) {
                        this_.notifyError( exc );
                    }
                }
                if (bAsync)
                    this.session.issueAsyncRequest( url, stringToXml, resp );
                else
                    this.session.issueSyncRequest( url, stringToXml, resp );
            }
        }
        catch (exc) {
            this.notifyError( exc );
            throw exc;
        }
        return this.m_calProps;
    },
    
    get defaultTimezone() {
        var tzid = this.getCalendarProperties("X-NSCP-CALPROPS-TZID", {});
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

