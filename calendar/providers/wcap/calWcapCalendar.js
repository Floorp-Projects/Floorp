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

function createWcapCalendar(session, /*optional*/calProps)
{
    var cal = new calWcapCalendar(session, calProps);
//     switch (CACHE) {
//     case "memory":
//     case "storage":
//         // wrap it up:
//         var cal_ = new calWcapCachedCalendar();
//         cal_.remoteCal = cal;
//         cal = cal_;
//         break;
//     }
    return cal;
}

function calWcapCalendar(session, /*optional*/calProps) {
    this.wrappedJSObject = this;
    this.m_session = session;
    this.m_calProps = calProps;
    this.m_bSuppressAlarms = SUPPRESS_ALARMS;
    
    if (this.m_calProps) {
        var ar = this.getCalProps("X-NSCP-CALPROPS-RELATIVE-CALID");
        if (ar.length > 0)
            this.m_calId = ar[0];
        else
            this.notifyError("no X-NSCP-CALPROPS-RELATIVE-CALID!");
    }
}
calWcapCalendar.prototype = {
    m_ifaces: [ calIWcapCalendar,
                calICalendar,
                Components.interfaces.calICalendarProvider,
                Components.interfaces.nsIInterfaceRequestor,
                Components.interfaces.nsIClassInfo,
                Components.interfaces.nsISupports ],
    
    // nsISupports:
    QueryInterface: function calWcapCalendar_QueryInterface(iid) {
        qiface(this.m_ifaces, iid);
        return this;
    },
    
    // nsIClassInfo:
    getInterfaces: function calWcapCalendar_getInterfaces( count ) {
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
    getHelperForLanguage:
    function calWcapCalendar_getHelperForLanguage(language) { return null; },
    implementationLanguage:
    Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0,
    
    // nsIInterfaceRequestor:
    getInterface: function calWcapCalendar_getInterface(iid, instance)
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
    
    toString: function calWcapCalendar_toString() {
        var str = this.session.toString();
        if (this.m_calId)
            str += (", calId=" + this.calId);
        else
            str += ", default calendar";
        return str;
    },
    notifyError: function calWcapCalendar_notifyError(err, suppressOnError)
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
    
    // calICalendarProvider:
    get prefChromeOverlay() {
        return null;
    },
    // displayName attribute already part of calIWcapCalendar
    createCalendar: function calWcapCalendar_createCalendar(name, url, listener) {
        throw NS_ERROR_NOT_IMPLEMENTED;
    },
    deleteCalendar: function calWcapCalendar_deleteCalendar(calendar, listener) {
        throw NS_ERROR_NOT_IMPLEMENTED;
    },
    getCalendar: function calWcapCalendar_getCalendar( url ) {
        throw NS_ERROR_NOT_IMPLEMENTED;
    },
    
    // calICalendar:
    get name() {
        return getCalendarManager().getCalendarPref(
            this.session.defaultCalendar, "NAME");
    },
    set name( name ) {
        getCalendarManager().setCalendarPref(
            this.session.defaultCalendar, "NAME", name);
        return name;
    },
    
    get type() { return "wcap"; },
    
    m_superCalendar: null,
    get superCalendar() {
        return (this.m_superCalendar || this);
    },
    set superCalendar(cal) {
        return (this.m_superCalendar = cal);
    },
    
    m_bReadOnly: false,
    get readOnly() {
        return (this.m_bReadOnly ||
                // xxx todo:
                // read-only if not logged in, this flag is tested quite
                // early, so don't log in here if not logged in already...
                !this.session.isLoggedIn ||
                // limit to write permission on components:
                !this.checkAccess(calIWcapCalendar.AC_COMP_WRITE));
    },
    set readOnly(bReadOnly) {
        return (this.m_bReadOnly = bReadOnly);
    },
    
    get uri() {
        if (this.m_calId) {
            var ret = this.session.uri.clone();
            ret.path += ("?calid=" + encodeURIComponent(this.m_calId));
            return ret;
        }
        else
            return this.session.uri;
    },
    set uri(thatUri) {
        return (this.session.uri = thatUri);
    },
    
    notifyObservers: function calWcapCalendar_notifyObservers(func, args) {
        this.session.notifyObservers(func, args);
    },
    addObserver: function calWcapCalendar_addObserver(observer) {
        this.session.addObserver(observer);
    },
    removeObserver: function calWcapCalendar_removeObserver(observer) {
        this.session.removeObserver(observer);
    },
    
    // xxx todo: batch currently not used
    startBatch: function calWcapCalendar_startBatch() {
        this.notifyObservers("onStartBatch", []);
    },
    endBatch: function calWcapCalendar_endBatch() {
        this.notifyObservers("onEndBatch", []);
    },
    
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
    set suppressAlarms(bSuppressAlarms) {
        return (this.m_bSuppressAlarms = bSuppressAlarms);
    },
    
    get canRefresh() { return (this.m_cachedResults != null); },
    refresh: function calWcapCalendar_refresh() {
        log("refresh.", this);
        // invalidate cached results:
        delete this.m_cachedResults;
    },
    
    issueNetworkRequest: function calWcapCalendar_issueNetworkRequest(
        request, respFunc, dataConvFunc, wcapCommand, params, accessRights)
    {
        var this_ = this;
        // - bootstrap problem: no cal_props, no access check, no default calId
        // - assure being logged in, thus the default cal_props are available
        // - every subscribed calendar will come along with cal_props
        return this.session.getSessionId(
            request,
            function getSessionId_resp(err, sessionId) {
                try {
                    if (err)
                        throw err;
                    this_.assureAccess(accessRights);
                    params += ("&calid=" + encodeURIComponent(this_.calId));
                    this_.session.issueNetworkRequest(
                        request, respFunc, dataConvFunc, wcapCommand, params);
                }
                catch (exc) {
                    respFunc(exc);
                }
            });
    },
    
    // calIWcapCalendar:
    
    m_session: null,
    get session() {
        return this.m_session;
    },
    
    m_calId: null,
    get calId() {
        if (this.m_calId)
            return this.m_calId;
        return this.session.defaultCalId;
    },
    
    get ownerId() {
        var ar = this.getCalProps("X-NSCP-CALPROPS-PRIMARY-OWNER");
        if (ar.length == 0) {
            var calId = this.calId;
            logError("cannot determine primary owner of calendar " + calId, this);
            // fallback to calId prefix:
            var nColon = calId.indexOf(":");
            if (nColon >= 0)
                calId = calId.substring(0, nColon);
            return calId;
        }
        return ar[0];
    },
    
    get description() {
        var ar = this.getCalProps("X-NSCP-CALPROPS-DESCRIPTION");
        if (ar.length == 0) {
            // fallback to display name:
            return this.displayName;
        }
        return ar[0];
    },
    
    get displayName() {
        var ar = this.getCalProps("X-NSCP-CALPROPS-NAME");
        if (ar.length == 0) {
            // fallback to common name:
            ar = this.getCalProps("X-S1CS-CALPROPS-COMMON-NAME");
            if (ar.length == 0) {
                return this.calId;
            }
        }
        return ar[0];
    },
    
    get isOwnedCalendar() {
        if (this.isDefaultCalendar)
            return true; // default calendar is owned
        return (this.ownerId == this.session.userId);
    },
    
    get isDefaultCalendar() {
        return !this.m_calId;
    },
    
    getCalendarProperties:
    function(propName, out_count) {
        var ret = this.getCalProps(propName);
        out_count.value = ret.length;
        return ret;
    },
    
    getCalProps: function calWcapCalendar_getCalProps(propName) {
        if (!this.m_calProps) {
            log("soft error: no calprops, most possibly not logged in.", this);
//             throw new Components.Exception("No calprops available!",
//                                            Components.results.NS_ERROR_NOT_AVAILABLE);
        }
        return filterXmlNodes(propName, this.m_calProps);
    },
    
    get defaultTimezone() {
        var tzid = this.getCalProps("X-NSCP-CALPROPS-TZID");
        if (tzid.length == 0) {
            logError("defaultTimezone: cannot get X-NSCP-CALPROPS-TZID!", this);
            return "UTC"; // fallback
        }
        return tzid[0];
    },
    
    getAlignedTimezone: function calWcapCalendar_getAlignedTimezone(tzid) {
        // check whether it is one of cs:
        if (tzid.indexOf("/mozilla.org/") == 0) {
            // cut mozilla prefix: assuming that the latter string portion
            //                     semantically equals the demanded timezone
            tzid = tzid.substring( // next slash after "/mozilla.org/"
                tzid.indexOf("/", "/mozilla.org/".length) + 1);
        }
        if (!this.session.isSupportedTimezone(tzid)) {
            // xxx todo: we could further on search for a matching region,
            //           e.g. CET (in TZNAME), but for now stick to
            //           user's default if not supported directly
            var ret = this.defaultTimezone;
            // use calendar's default:
            log(tzid + " not supported, falling back to default: " + ret, this);
            return ret;
        }
        else // is ok (supported):
            return tzid;
    },
    
    checkAccess: function calWcapCalendar_checkAccess(accessControlBits)
    {
        // xxx todo: take real acl into account
        // for now, assuming that owners have been granted full access,
        // and all others can read, but not add/modify/delete.
        var granted = calIWcapCalendar.AC_FULL;
        if (!this.isOwnedCalendar) {
            // burn out write access:
            granted &= ~(calIWcapCalendar.AC_COMP_WRITE |
                         calIWcapCalendar.AC_PROP_WRITE);
        }
        // check whether every bit fits:
        return ((accessControlBits & granted) == accessControlBits);
    },
    
    assureAccess: function calWcapCalendar_assureAccess(accessControlBits)
    {
        if (!this.checkAccess(accessControlBits)) {
            throw new Components.Exception("Access denied!",
                                           calIWcapErrors.WCAP_ACCESS_DENIED_TO_CALENDAR);
            // xxx todo: throwing different error here, no
            //           calIErrors.CAL_IS_READONLY anymore
        }
    },
    
    defineAccessControl: function calWcapCalendar_defineAccessControl(
        userId, accessControlBits)
    {
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    },
    
    resetAccessControl: function calWcapCalendar_resetAccessControl(userId)
    {
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    },
    
    getAccessControlDefinitions: function calWcapCalendar_getAccessControlDefinitions(
        out_count, out_users, out_accessControlBits)
    {
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
};

