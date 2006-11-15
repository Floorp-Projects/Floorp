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
    
    // init queued calls:
    this.adoptItem = makeQueuedCall(this.session.asyncQueue,
                                    this, this.adoptItem_queued);
    this.modifyItem = makeQueuedCall(this.session.asyncQueue,
                                     this, this.modifyItem_queued);
    this.deleteItem = makeQueuedCall(this.session.asyncQueue,
                                     this, this.deleteItem_queued);
    this.getItem = makeQueuedCall(this.session.asyncQueue,
                                  this, this.getItem_queued);
    this.getItems = makeQueuedCall(this.session.asyncQueue,
                                   this, this.getItems_queued);
    this.syncChangesTo = makeQueuedCall(this.session.asyncQueue,
                                        this, this.syncChangesTo_queued);
}
calWcapCalendar.prototype = {
    m_ifaces: [ Components.interfaces.calIWcapCalendar,
                Components.interfaces.calICalendar,
                Components.interfaces.calICalendarProvider,
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
        str += (", calId=" +
                (this.session.isLoggedIn ? this.calId : this.m_calId));
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
    
    // calICalendarProvider:
    get prefChromeOverlay() {
        return null;
    },
    // displayName attribute already part of calIWcapCalendar
    createCalendar:
    function( name, url, listener )
    {
        throw NS_ERROR_NOT_IMPLEMENTED;
    },
    deleteCalendar:
    function( calendar, listener )
    {
        throw NS_ERROR_NOT_IMPLEMENTED;
    },
    getCalendar:
    function( url )
    {
        throw NS_ERROR_NOT_IMPLEMENTED;
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
    get readOnly() {
        return (this.m_bReadOnly ||
                // read-only if not logged in, this flag is tested quite
                // early, so don't log in here if not logged in already...
                !this.session.isLoggedIn ||
                // xxx todo: take permissions into account:
                !this.isOwnedCalendar);
    },
    set readOnly( bReadOnly ) { this.m_bReadOnly = bReadOnly; },
    
    assureReadWrite:
    function()
    {
        this.session.getSessionId(); // assure being logged in
        if (this.readOnly) {
            throw new Components.Exception(
                "Calendar is read-only.",
                Components.interfaces.calIErrors.CAL_IS_READONLY );
        }
    },
    
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
        return (this.m_bSuppressAlarms ||
                // writing lastAck does currently not work on readOnly cals,
                // so avoid alarms if not writable at all... discuss!
                // use m_bReadOnly here instead of attribute, because this
                // calendar acts read-only if not logged in
                this.m_bReadOnly ||
                // xxx todo: check write permissions in advance
                // alarms only for own calendars:
                // xxx todo: assume alarms if not logged in already
                (this.session.isLoggedIn && !this.isOwnedCalendar));
    },
    set suppressAlarms( bSuppressAlarms ) {
        this.m_bSuppressAlarms = bSuppressAlarms;
    },
    
    get canRefresh() { return false; },
    refresh: function() {
        // no-op
        this.log("refresh()");
    },
    
    getCommandUrl:
    function( wcapCommand )
    {
        var url = this.session.getCommandUrl(wcapCommand);
        url += ("&calid=" + encodeURIComponent(this.calId));
        return url;
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
        if (this.m_calId)
            return this.m_calId;
        this.session.assureLoggedIn();
        return this.session.defaultCalId;
    },
    set calId( id ) {
        this.log( "setting calId to " + id );
        this.m_calId = id;
        // refresh calprops:
        this.getCalProps_( true /*async*/, true /*refresh*/ );
    },
    
    get ownerId() {
        var ar = this.getCalendarProperties("X-NSCP-CALPROPS-PRIMARY-OWNER",{});
        if (ar.length < 1 || ar[0].length == 0) {
            var calId = this.calId;
            this.logError(
                "cannot determine primary owner of calendar " + calId );
            // fallback to calId prefix:
            var nColon = calId.indexOf(":");
            if (nColon >= 0)
                calId = calId.substring(0, nColon);
            return calId;
        }
        return ar[0];
    },
    
    get description() {
        var ar = this.getCalendarProperties("X-NSCP-CALPROPS-DESCRIPTION", {});
        if (ar.length < 1 || ar[0].length == 0) {
            // fallback to display name:
            return this.displayName;
        }
        return ar[0];
    },
    
    get displayName() {
        var ar = this.getCalendarProperties("X-NSCP-CALPROPS-NAME", {});
        if (ar.length < 1 || ar[0].length == 0) {
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
        return (this.ownerId == this.session.userId);
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
    function( bAsync, bRefresh )
    {
        this.session.assureLoggedIn();
        try {
            if (bRefresh || !this.m_calProps) {
                var url = this.getCommandUrl( "get_calprops" );
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
                        // patch to read-only here, because error notification
                        // call the readOnly attribute, thus we would run into
                        // endless recursion here:
                        this_.m_bReadOnly = true;
                        // just logging here, because user may have dangling
                        // users referred in his subscription list:
                        this_.logError( exc );
                        // though we continue to try to get that calprops
                        // next time...
                    }
                }
                if (bAsync)
                    this.session.issueAsyncRequest( url, stringToXml, resp );
                else
                    this.session.issueSyncRequest( url, stringToXml, resp );
            }
        }
        catch (exc) {
            // patch to read-only here, because error notification call the
            // readOnly attribute, thus we would run into endless recursion here
            this.m_bReadOnly = true;
            this.logError( exc );
            if (!bAsync)
                throw exc;
        }
        return this.m_calProps;
    },
    
    get defaultTimezone() {
        var tzid = this.getCalendarProperties("X-NSCP-CALPROPS-TZID", {});
        if (tzid.length < 1 || tzid[0].length == 0) {
            this.logError( "cannot get X-NSCP-CALPROPS-TZID!" );
            return "UTC"; // fallback
        }
        return tzid[0];
    },
    
    getAlignedTimezone:
    function( tzid )
    {
        // check whether it is one of cs:
        if (tzid.indexOf("/mozilla.org/") == 0) {
            // cut mozilla prefix: assuming that the latter string portion
            //                     semantically equals the demanded timezone
            tzid = tzid.substring( // next slash after "/mozilla.org/"
                tzid.indexOf("/", "/mozilla.org/".length) + 1 );
        }
        if (!this.session.isSupportedTimezone(tzid)) {
            // xxx todo: we could further on search for a matching region,
            //           e.g. CET (in TZNAME), but for now stick to
            //           user's default if not supported directly
            var ret = this.defaultTimezone;
            // use calendar's default:
            this.log(tzid + " not supported, falling back to default: " + ret);
            return ret;
        }
        else // is ok (supported):
            return tzid;
    }
};

