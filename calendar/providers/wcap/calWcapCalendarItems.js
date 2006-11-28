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

// xxx todo: just to avoid registration errors, how to do better?
var calWcapCalendar;
if (!calWcapCalendar) {
    calWcapCalendar = {};
    calWcapCalendar.prototype = {};
}

calWcapCalendar.prototype.encodeAttendee = function(
    /* calIAttendee */ attendee, forceRSVP )
{
    this.log( "\tattendee.icalProperty.icalString=" +
              attendee.icalProperty.icalString +
              "\n\tattendee.id=" + attendee.id +
              "\n\tattendee.commonName=" + attendee.commonName +
              "\n\tattendee.role=" + attendee.role +
              "\n\tattendee.participationStatus=" +
              attendee.participationStatus );
    var ret = ("RSVP=" + (forceRSVP || attendee.rsvp ? "TRUE" : "FALSE"));
    if (attendee.participationStatus != null)
        ret += ("^PARTSTAT=" + attendee.participationStatus);
    if (attendee.role != null)
        ret += ("^ROLE=" + attendee.role);
    if (attendee.commonName != null)
        ret += ("^CN=" + encodeURIComponent(attendee.commonName));
    ret += ("^" + encodeURIComponent(attendee.id));
    return ret;
};

calWcapCalendar.prototype.encodeRecurrenceParams = function( item, oldItem )
{
    var rrules = {};
    var rdates = {};
    var exrules = {};
    var exdates = {};
    this.getRecurrenceParams( item, rrules, rdates, exrules, exdates );
    if (oldItem) {
        // actually only write changes if an old item has been changed, because
        // cs recreates the whole series if a rule has changed.
        // xxx todo: one problem is left when a master only holds EXDATEs,
        //           and effectively no item is present anymore.
        //           cs seems not to clean up automatically, but it does when
        //           when deleting an occurrence {id, rec-id}!
        //           So this still leaves the question open why deleteOccurrence
        //           does not directly call deleteItem rather than modifyItem,
        //           which leads to a much cleaner usage.
        //           I assume this mimic has been chosen for easier undo/redo
        //           support (Undo would then have to distinguish whether
        //           it has previously deleted an occurrence or ordinary item:
        //            - entering an exception again
        //            - or adding an item)
        //           Currently it can just modifyItem(newItem/oldItem) back.
        var rrules_ = {};
        var rdates_ = {};
        var exrules_ = {};
        var exdates_ = {};
        this.getRecurrenceParams(oldItem, rrules_, rdates_, exrules_, exdates_);
        
        function sameSet( list, list_ ) {
            return (list.length == list_.length &&
                    list.every( function(x) {
                                    return list_.some(
                                        function(y) { return x == y; } );
                                }
                        ));
        }
        if (sameSet( rrules.value, rrules_.value ))
            rrules.value = null; // don't write
        if (sameSet( rdates.value, rdates_.value ))
            rdates.value = null; // don't write
        if (sameSet( exrules.value, exrules.value ))
            exrules.value = null; // don't write
        if (sameSet( exdates.value, exdates_.value ))
            exdates.value = null; // don't write
    }
    
    function encodeList( list ) {
        var ret = "";
        for each ( var str in list ) {
            if (ret.length > 0)
                ret += ";";
            ret += str;
        }
        return ret;
    }
    var ret = "";
    if (rrules.value != null)
        ret += ("&rrules=" + encodeList(rrules.value));
    if (rdates.value != null)
        ret += ("&rdates=" + encodeList(rdates.value));
    if (exrules.value != null)
        ret += ("&exrules=" + encodeList(exrules.value));
    if (exdates.value != null)
        ret += ("&exdates=" + encodeList(exdates.value));
    return ret;
    // xxx todo:
    // rchange=1: expand recurrences,
    // or whether to replace the rrule, ambiguous documentation!!!
    // check with store(with no uid) upon adoptItem() which behaves strange
    // if rchange=0 is set!
};

calWcapCalendar.prototype.getRecurrenceParams = function(
    item, out_rrules, out_rdates, out_exrules, out_exdates )
{
    // recurrences:
    out_rrules.value = [];
    out_rdates.value = [];
    out_exrules.value = [];
    out_exdates.value = [];
    if (item.recurrenceInfo != null) {
        var rItems = item.recurrenceInfo.getRecurrenceItems( {} );
        for each ( var rItem in rItems ) {
            var isNeg = rItem.isNegative;
            // xxx todo: need to QueryInterface() here?
            if (rItem instanceof Components.interfaces.calIRecurrenceRule)
            {
                var rule = ("\"" + encodeURIComponent(
                                rItem.icalProperty.valueAsIcalString) +
                            "\"");
                if (isNeg)
                    out_exrules.value.push( rule );
                else
                    out_rrules.value.push( rule );
            }
            // xxx todo: need to QueryInterface() here?
            else if (
                rItem instanceof Components.interfaces.calIRecurrenceDateSet)
            {
                var d = rItem.getDates({});
                for each ( var d in rdates ) {
                    if (isNeg)
                        out_exdates.value.push( getIcalUTC(d.date) );
                    else
                        out_rdates.value.push( getIcalUTC(d.date) );
                }
            }
            // xxx todo: need to QueryInterface() here?
            else if (
                rItem instanceof Components.interfaces.calIRecurrenceDate)
            {
                if (isNeg)
                    out_exdates.value.push( getIcalUTC(rItem.date) );
                else
                    out_rdates.value.push( getIcalUTC(rItem.date) );
            }
            else {
                this.notifyError(
                    "don\'t know how t handle this recurrence item: " +
                    rItem.valueAsIcalString );
            }
        }
    }
};

calWcapCalendar.prototype.storeItem = function( item, oldItem, receiverFunc )
{
    var bIsEvent = isEvent(item);
    var bIsParent = isParent(item);
    
    var url = this.getCommandUrl( bIsEvent ? "storeevents" : "storetodos" );
    
    if (oldItem) { // modifying
        url += ("&uid=" + encodeURIComponent(item.id));
        if (bIsParent) {
            // (WCAP_STORE_TYPE_MODIFY) error if not existing:
            url += "&storetype=2";
            url += "&mod=4"; // THIS AND ALL INSTANCES
        }
        else { // modifying occurences lands here: occurence may not exist
            url += "&mod=1"; // THIS INSTANCE
            // if not set, whole series of events is modified:
            url += ("&rid=" + getIcalUTC(item.recurrenceId));
        }
    }
    else { // adding
        // (WCAP_STORE_TYPE_CREATE) error if existing item:
        url += "&storetype=1";
    }
    
    var ownerId = this.ownerId;
    var orgUID = ((item.organizer && item.organizer.id)
                  ? item.organizer.id : ownerId);
    
    var bOrgRequest = false;
    
    // attendees:
    var attendees =  item.getAttendees({});
    if (attendees.length > 0) {
        // ORGANIZER is owner fo this cal?
        if (!oldItem || (orgUID == ownerId &&
                         // xxx todo:
                         // we assume that the alarm service writes a new
                         // lastAck here (only); then we update only
                         // the organizer's copy of the item, no REQUEST:
                         getIcalUTC(oldItem.alarmLastAck) ==
                         getIcalUTC(item.alarmLastAck)))
        {
            bOrgRequest = true;
            url += "&method=2"; // REQUEST
            url += ("&orgUID=" + encodeURIComponent(orgUID));
            url += "&attendees=";
            for ( var i = 0; i < attendees.length; ++i ) {
                if (i > 0)
                    url += ";";
                url += this.encodeAttendee( attendees[i], true /*forceRSVP*/ );
            }
        }
        else { // in modifyItem(), attendee's calendar:
            var attendee = item.getAttendeeById(ownerId);
            if (attendee) {
                this.log( "attendee: " + attendee.icalProperty.icalString );
                var oldAttendee = oldItem.getAttendeeById(ownerId);
                if (!oldAttendee ||
                    attendee.participationStatus !=
                    oldAttendee.participationStatus)
                {
                    // REPLY first for just this calendar owner:
                    var replyUrl = (url + "&method=4&attendees=PARTSTAT=" +
                                    attendee.participationStatus);
                    replyUrl += ("^" + ownerId);
                    this.session.issueSyncRequest(
                        replyUrl + "&fmt-out=text%2Fcalendar", stringToIcal );
                }
            }
            // continue with UPDATE of attendee's copy of item:
            url += "&method=256";
        }
    }
    else
        url += "&orgUID=&attendees="; // using just PUBLISH (method=1)
    
    url += "&fetch=1&relativealarm=1&compressed=1&recurring=1";
    url += "&replace=1"; // (update) don't append to any lists
    
    // xxx todo: default prio is 0 (5 in sjs cs)
    // save PRIORITY only if actually set, else delete:
    url += "&priority=";
    if (item.hasProperty("PRIORITY"))
        url += encodeURIComponent( item.getProperty("PRIORITY") );
    
    var icsClass = ((item.privacy != null && item.privacy != "")
                    ? item.privacy : "PUBLIC");
    url += ("&icsClass="+ icsClass);
    
    if (!bIsEvent && item.isCompleted) {
        url += "&status=4"; // force to COMPLETED
    }
    else {
        switch (item.status) {
        case "CONFIRMED":    url += "&status=0"; break;
        case "CANCELLED":    url += "&status=1"; break;
        case "TENTATIVE":    url += "&status=2"; break;
        case "NEEDS-ACTION": url += "&status=3"; break;
        case "COMPLETED":    url += "&status=4"; break;
        case "IN-PROCESS":   url += "&status=5"; break;
        case "DRAFT":        url += "&status=6"; break;
        case "FINAL":        url += "&status=7"; break;
        default:
            url += "&status=3"; // NEEDS-ACTION
//             this.logError( "storeItem(): unexpected item status=" +
//                            item.status );
            break;
        }
    }
    
    // attachment urls:
    url += "&attachments=";
    var attachments = item.attachments;
    if (attachments != null) {
        for ( var i = 0; i < attachments.length; ++i ) {
            if (i > 0)
                url += ";";
            var obj = attachments[i];
            if (obj instanceof String) {
                url += encodeURIComponent(obj);
            }
            else {
                this.notifyError(
                    "only URLs supported as attachment, not: " + obj );
            }
        }
    }
    
    // categories:
    // xxx todo: check whether ;-separated:
    url += "&categories=";
    if (item.hasProperty( "CATEGORIES" )) {
        url += encodeURIComponent(
            item.getProperty( "CATEGORIES" ).replace(/,/g, ";") );
    }
    
    // xxx todo: missing relatedTos= in cal api
    
    url += ("&summary=" + encodeURIComponent(item.title));
    // desc: xxx todo attribute "description" not impl in calItemBase.js
    url += "&desc=";
    if (item.hasProperty( "DESCRIPTION" )) {
        url += encodeURIComponent( item.getProperty( "DESCRIPTION" ) );
    }
    // location: xxx todo currently not impl in calItemBase.js
    url += "&location=";
    if (item.hasProperty( "LOCATION" )) {
        url += encodeURIComponent( item.getProperty( "LOCATION" ) );
    }
    url += "&icsUrl=";
    if (item.hasProperty( "URL" )) {
        url += encodeURIComponent( item.getProperty( "URL" ) );
    }
    
    var dtstart = null;
    var dtend = null;
    var bIsAllDay = false;
    
    if (bIsEvent) {
        dtstart = item.startDate;
        var dtend = item.endDate;
        url += ("&dtend=" + getIcalUTC(dtend));
        url += ("&X-NSCP-DTEND-TZID=" +
                "X-NSCP-ORIGINAL-OPERATION=X-NSCP-WCAP-PROPERTY-REPLACE^" + 
                encodeURIComponent(this.getAlignedTimezone(dtend.timezone)));
        this.log("dtstart=" + dtstart + "\ndtend=" + dtend, item.id);
        bIsAllDay = (dtstart.isDate && dtend.isDate);
    }
    else { // calITodo:
        // xxx todo: dtstart is mandatory for cs, so if this is
        //           undefined, assume an allDay todo???
        dtstart = item.entryDate;
        dtend = item.dueDate;
        url += ("&due=" + getIcalUTC(dtend));
        if (dtend) {
            url += ("&X-NSCP-DUE-TZID=" +
                    "X-NSCP-ORIGINAL-OPERATION=X-NSCP-WCAP-PROPERTY-REPLACE^" + 
                    encodeURIComponent(
                        this.getAlignedTimezone(dtend.timezone)));
        }
        
        bIsAllDay = (dtstart && dtstart.isDate);
        if (item.isCompleted)
            url += "&percent=100";
        else
            url += ("&percent=" +
                    (item.percentComplete ? item.percentComplete : "0"));
        url += "&completed=";
        if (item.completedDate != null)
            url += getIcalUTC(item.completedDate);
        else if (item.isCompleted)
            url += getIcalUTC(getTime()); // repair missing completedDate
        else
            url += "0"; // not yet completed
        // xxx todo: sentBy sentUID fields in cs: missing in cal api
    }
    
    var strTransp = null;
    if (item.hasProperty("TRANSP"))
        strTransp = item.getProperty("TRANSP");
    switch (strTransp) {
    case "TRANSPARENT":
        url += "&transparent=1";
        break;
    case "OPAQUE":
        url += "&transparent=0";
        break;
    default:
        url += ("&transparent=" +
                (((icsClass == "PRIVATE") || bIsAllDay) ? "1" : "0"));
        break;
    }
    
    url += ("&isAllDay=" + (bIsAllDay ? "1" : "0"));
    
    url += ("&dtstart=" + getIcalUTC(dtstart));
    if (dtstart) {
        // important to provide tz info with entry date for proper
        // occurrence calculation (daylight savings):
        url += ("&X-NSCP-DTSTART-TZID=" +
                "X-NSCP-ORIGINAL-OPERATION=X-NSCP-WCAP-PROPERTY-REPLACE^" + 
                encodeURIComponent(this.getAlignedTimezone(dtstart.timezone)));
        // xxx todo: still needed?
        url += ("&tzid=" + encodeURIComponent(
                    this.getAlignedTimezone(dtstart.timezone)));
    }
    
    // xxx todo: alarm mimic of todos currently different:
    //           WCAP offsets relate to DUE
    if (bIsEvent) {

    // alarm support:
    var alarmStart = item.alarmOffset;
    if (alarmStart) {
        var alarmRelated = item.alarmRelated;
        if (alarmRelated==Components.interfaces.calIItemBase.ALARM_RELATED_END){
            // cs does not support explicit RELATED=END when
            // both dtstart/due are written
            var dur = item.duration;
            if (dur) { // dtstart+due given
                alarmStart = alarmStart.clone();
                alarmStart.addDuration(dur);
            } // else only dtend is set
        }
    }
    if (alarmStart) {
        var emails = "";
        if (item.hasProperty("alarmEmailAddress"))
            emails = encodeURIComponent(item.getProperty("alarmEmailAddress"));
        else {
            // minimal alarm server support:
            // Alarms are currently off by default,
            // so let server at least send reminder eMails...
            this.session.getDefaultAlarmEmails({}).forEach(
                function(email) {
                    if (emails.length > 0)
                        emails += ";";
                    emails += encodeURIComponent(email);
                } );
        }
        url += ("&alarmStart=" + alarmStart.icalString);
        url += ("&alarmEmails=" + emails);
        url += "&alarmPopup=";
        if (emails.length == 0)
            url += alarmStart.icalString;
        // xxx todo: missing: alarm triggers for flashing, etc.
    }
    else {
        // clear popup, emails:
        url += "&alarmStart=&alarmPopup=&alarmEmails=";
    }

    } // if (bIsEvent)
    
    // xxx todo: however, sometimes socs just returns an empty calendar when
    //           nothing or only optional props like attendee ROLE has changed,
    //           although fetch=1.
    //           This also occurs when the event organizer is in the attendees
    //           list and switches its PARTSTAT too often...
    //
    // hack #1:  ever changing X-dummy prop, then the cs engine seems to write
    //           every time. => does not work for recurring items,
    //                          operation REPLACE does not work, just adds
    //                          more and more props. WTF.
    
    // storing X-props does not work properly for
    // recurring items or single occurrences
    
    // misusing CONTACTS for now to store additional X- data
    // (not used in cs web-frontend nor in mozilla, but in Outlook...)
    
    // stamp[:lastack]
    var contacts = getIcalUTC(getTime());
    if (bIsParent && !bOrgRequest) {
        var lastAck = item.alarmLastAck;
        if (lastAck) {
            contacts += ":";
            contacts += getIcalUTC(lastAck);
        }
    }
    url += ("&contacts=" + encodeURIComponent(contacts));
    
    if (bIsParent)
        url += this.encodeRecurrenceParams( item, oldItem );
    
    this.session.issueAsyncRequest(
        url + "&fmt-out=text%2Fcalendar", stringToIcal, receiverFunc );
};

calWcapCalendar.prototype.tunnelXProps = function( destItem, srcItem )
{
    // xxx todo: temp workaround for bug in calItemBase.js
    if (!isParent(srcItem))
        return;
    var en = srcItem.propertyEnumerator;
    while (en.hasMoreElements()) {
        var prop = en.getNext().QueryInterface(
            Components.interfaces.nsIProperty);
        var name = prop.name;
        if (name.indexOf("X-MOZ-") == 0) {
            if (LOG_LEVEL > 1)
                this.log( "tunneling " + name );
            destItem.setProperty(name, prop.value);
        }
    }
};

calWcapCalendar.prototype.adoptItem_resp = function(
    wcapResponse, newItem_, listener )
{
    var item = null;
    try {
        var icalRootComp = wcapResponse.data; // first statement, may throw
        
        var items = this.parseItems(
            icalRootComp,
            Components.interfaces.calICalendar.ITEM_FILTER_ALL_ITEMS,
            0, null, null, true /* bLeaveMutable */ );
        if (items.length < 1)
            throw new Components.Exception("empty VCALENDAR returned!");
        if (items.length > 1)
            this.notifyError( "unexpected number of items: " + items.length );
        item = items[0];
        this.tunnelXProps(item, newItem_);
        item.makeImmutable();
        
        this.log( "item.id=" + item.id );        
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_OK,
                Components.interfaces.calIOperationListener.ADD,
                item.id, item );
        }
        this.notifyObservers( "onAddItem", [item] );
        // xxx todo: maybe log request status
    }
    catch (exc) {
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_ERROR_FAILURE,
                Components.interfaces.calIOperationListener.ADD,
                item == null ? null : item.id, exc );
        }
        this.notifyError( exc );
    }
};

calWcapCalendar.prototype.adoptItem_queued = function( item, listener )
{
    this.log( "adoptItem() call: " + item.title );
    try {
        this.assureAccess(Components.interfaces.calIWcapCalendar.AC_COMP_WRITE);
        
        // xxx todo: workaround really necessary for adding an occurrence?
        var oldItem = null;
        if (!isParent(item)) {
            this.logError( "adoptItem(): unexpected proxy!" );
            debugger;
            item.parentItem.recurrenceInfo.modifyException( item );
            oldItem = item; // patch to modify
        }
        
        var this_ = this;
        this.storeItem(
            item, oldItem,
            function( wcapResponse ) {
                this_.adoptItem_resp( wcapResponse, item, listener );
            } );
    }
    catch (exc) {
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_ERROR_FAILURE,
                Components.interfaces.calIOperationListener.ADD,
                item.id, exc );
        }
        this.notifyError( exc );
    }
    this.log( "adoptItem() returning." );
};

calWcapCalendar.prototype.addItem = function( item, listener )
{
    this.adoptItem( item.clone(), listener );
};

calWcapCalendar.prototype.modifyItem_resp = function(
    wcapResponse, newItem_, oldItem, listener )
{
    var item = null;
    try {
        var icalRootComp = wcapResponse.data; // first statement, may throw
        
        var items = this.parseItems(
            icalRootComp,
            Components.interfaces.calICalendar.ITEM_FILTER_ALL_ITEMS,
            0, null, null, true /* bLeaveMutable */ );
        if (items.length < 1)
            throw new Components.Exception("empty VCALENDAR returned!");
        if (items.length > 1)
            this.notifyError( "unexpected number of items: " + items.length );
        item = items[0];
        this.tunnelXProps(item, newItem_);
        item.makeImmutable();
        
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_OK,
                Components.interfaces.calIOperationListener.MODIFY,
                item.id, item );
        }
        this.notifyObservers( "onModifyItem", [item, oldItem] );
        // xxx todo: maybe log request status
    }
    catch (exc) {
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_ERROR_FAILURE,
                Components.interfaces.calIOperationListener.MODIFY,
                item == null ? null : item.id, exc );
        }
        this.notifyError( exc );
    }
};

calWcapCalendar.prototype.modifyItem_queued = function(
    newItem, oldItem, listener )
{
    this.log( "modifyItem() call: " + newItem.id );    
    try {
        this.assureAccess(Components.interfaces.calIWcapCalendar.AC_COMP_WRITE);
        
        if (!newItem.id)
            throw new Components.Exception("new item has no id!");
        
        var this_ = this;
        this.storeItem(
            newItem, oldItem,
            function( wcapResponse ) {
                this_.modifyItem_resp(wcapResponse, newItem, oldItem, listener);
            } );
    }
    catch (exc) {
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_ERROR_FAILURE,
                Components.interfaces.calIOperationListener.MODIFY,
                newItem.id, exc );
        }
        this.notifyError( exc );
    }
    this.log( "modifyItem() returning." );
};

calWcapCalendar.prototype.deleteItem_resp = function(
    wcapResponse, item, listener )
{
    try {
        var xml = wcapResponse.data; // first statement, may throw
        
        // xxx todo: need to notify about each deleted item if multiple?
        if (item.isMutable) {
            item.makeImmutable();
        }
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_OK,
                Components.interfaces.calIOperationListener.DELETE,
                item.id, item );
        }
        this.notifyObservers( "onDeleteItem", [item] );
        if (LOG_LEVEL > 0) {
            this.log( "deleteItem_resp(): " +
                      getWcapRequestStatusString(xml) );
        }
    }
    catch (exc) {
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_ERROR_FAILURE,
                Components.interfaces.calIOperationListener.DELETE,
                item.id, exc );
        }
        this.notifyError( exc );
    }
};

calWcapCalendar.prototype.deleteItem_queued = function( item, listener )
{
    this.log( "deleteItem() call: " + item.id );
    try {
        this.assureAccess(Components.interfaces.calIWcapCalendar.AC_COMP_WRITE);
        
        if (item.id == null)
            throw new Components.Exception("no item id!");
        
        var url = this.getCommandUrl(
            isEvent(item) ? "deleteevents_by_id" : "deletetodos_by_id" );
        url += ("&uid=" + encodeURIComponent(item.id));
        
        if (isParent(item)) // delete THIS AND ALL:
            url += "&mod=4&rid=0";
        else // delete THIS INSTANCE:
            url += ("&mod=1&rid=" + getIcalUTC(item.recurrenceId));
        
        var this_ = this;
        this.session.issueAsyncRequest(
            url + "&fmt-out=text%2Fxml", stringToXml,
            function( wcapResponse ) {
                this_.deleteItem_resp( wcapResponse, item, listener );
            } );
    }
    catch (exc) {
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_ERROR_FAILURE,
                Components.interfaces.calIOperationListener.DELETE,
                item.id, exc );
        }
        this.notifyError( exc );
    }
    this.log( "deleteItem() returning." );
};

calWcapCalendar.prototype.parseItems = function(
    icalRootComp, itemFilter, maxResult, rangeStart, rangeEnd,
    bLeaveMutable )
{
    var items = [];
    this.parseItems_(
        function(srcItems) { items = items.concat(srcItems) },
        icalRootComp, items, maxResult, rangeStart, rangeEnd,
        bLeaveMutable);
    return items;
};

calWcapCalendar.prototype.parseItems_ = function(
    receiverFunc,
    icalRootComp, itemFilter, maxResult, rangeStart, rangeEnd,
    bLeaveMutable )
{
    var nItems = 0;
    var unexpandedItems = [];
    var uid2parent = {};
    var excItems = [];
    
    var componentType = "ANY";
    switch (itemFilter &Components.interfaces.calICalendar.ITEM_FILTER_TYPE_ALL)
    {
    case Components.interfaces.calICalendar.ITEM_FILTER_TYPE_TODO:
        componentType = "VTODO"; break;
    case Components.interfaces.calICalendar.ITEM_FILTER_TYPE_EVENT:
        componentType = "VEVENT"; break;
    }
    
    var this_ = this;
    forEachIcalComponent(
        icalRootComp, componentType,
        function( subComp )
        {
            function patchTimezone( subComp, attr, xprop ) {
                var dt = subComp[attr];
                if (dt != null) {
                    if (LOG_LEVEL > 2) {
                        this_.log( attr + " is " + dt );
                    }
                    var tzid = subComp.getFirstProperty( xprop );
                    if (tzid != null) {
                        subComp[attr] = dt.getInTimezone(tzid.value);
                        if (LOG_LEVEL > 2) {
                            this_.log( "patching " + xprop + " from " +
                                       dt + " to " + subComp[attr] );
                        }
                    }
                }
            }

            patchTimezone( subComp, "startTime", "X-NSCP-DTSTART-TZID" );
            var item = null;
            switch (subComp.componentType) {
            case "VEVENT": {
                patchTimezone( subComp, "endTime", "X-NSCP-DTEND-TZID" );
                item = new CalEvent();
                item.icalComponent = subComp;
                break;
            }
            case "VTODO": {
                patchTimezone( subComp, "dueTime", "X-NSCP-DUE-TZID" );
                item = new CalTodo();
                item.icalComponent = subComp;
                switch (itemFilter & Components.interfaces.calICalendar
                                     .ITEM_FILTER_COMPLETED_ALL)
                {
                    case Components.interfaces.calICalendar
                        .ITEM_FILTER_COMPLETED_YES:
                        if (!item.isCompleted) {
                            delete item;
                            item = null;
                        }
                    break;
                    case Components.interfaces.calICalendar
                        .ITEM_FILTER_COMPLETED_NO:
                        if (item.isCompleted) {
                            delete item;
                            item = null;
                        }
                    break;
                }
//                 if (item &&
//                     item.alarmOffset && !item.entryDate && !item.dueDate) {
//                     // xxx todo: loss on roundtrip
//                     this_.log( "app currently does not support " +
//                                "absolute alarm trigger datetimes. " +
//                                "Removing alarm from item: " + item.title );
                if (item) { // xxx todo: todo alarms currently off
                    item.alarmOffset = null;
                    item.alarmLastAck = null;
                }
                break;
            }
            }
            if (item != null) {
                var contactsProp = subComp.getFirstProperty("CONTACT");
                if (contactsProp) { // stamp[:lastack]
                    var ar = contactsProp.value.split(":");
                    if (ar.length > 1) {
                        var lastAck = ar[1];
                        if (lastAck.length > 0) { // shift to alarm comp:
                            item.alarmLastAck = getDatetimeFromIcalString(
                                lastAck); // TZID is UTC
                        }
                    }
                }
                
                if (!item.title) {
                    // assumed to look at a subscribed calendar,
                    // so patch title for private items:
                    switch (item.privacy) {
                    case "PRIVATE":
                        item.title = g_privateItemTitle;
                        break;
                    case "CONFIDENTIAL":
                        item.title = g_confidentialItemTitle;
                        break;
                    }
                }
                
                item.calendar = this_.superCalendar;
                var rid = item.recurrenceId;
                if (rid) {
                    var startDate = (isEvent(item)
                                     ? item.startDate : item.entryDate);
                    if (startDate.isDate && !rid.isDate) {
                        // cs ought to return proper all-day RECURRENCE-ID!
                        // get into startDate's timezone before cutting:
                        rid = rid.getInTimezone(startDate.timezone);
                        rid.isDate = true;
                        item.recurrenceId = rid;
                    }
                    if (LOG_LEVEL > 1) {
                        this_.log( "exception item: " + item.title +
                                   "\nrid=" + rid.icalString,
                                   "item.id=" + item.id );
                    }
                    excItems.push( item );
                }
                else if (item.recurrenceInfo) {
                    unexpandedItems.push( item );
                    uid2parent[item.id] = item;
                }
                else if (maxResult == 0 || nItems < maxResult) {
                    if (LOG_LEVEL > 2) {
                        this_.log( "item: " + item.title + "\n" +
                                   item.icalString );
                    }
                    if (!bLeaveMutable)
                        item.makeImmutable();
                    receiverFunc( [item] );
                }
            }
        },
        maxResult );
    
    // tag "exceptions", i.e. items with rid:
    for each ( var item in excItems ) {
        var parent = uid2parent[item.id];
        if (parent) {
            item.parentItem = parent;
            item.makeImmutable();
            parent.recurrenceInfo.modifyException( item );
        }
        else {
            this.logError( "parseItems(): no parent item for " + item.title +
                           ", rid=" + item.recurrenceId.icalString,
                           "item.id=" + item.id );
            // xxx todo: due to a server bug, in some scenarions the returned
            //           data is lacking the parent item, leave parentItem open
            if (!bLeaveMutable)
                item.makeImmutable();
            receiverFunc( [item] );
        }
    }
    
    if (itemFilter & Components.interfaces.calICalendar
                     .ITEM_FILTER_CLASS_OCCURRENCES)
    {
        for each ( var item in unexpandedItems ) {
            if (maxResult != 0 && nItems >= maxResult)
                break;
            if (!bLeaveMutable)
                item.makeImmutable();
            var occurrences = item.recurrenceInfo.getOccurrences(
                rangeStart, rangeEnd,
                maxResult == 0 ? 0 : maxResult - nItems,
                {} );
            if (LOG_LEVEL > 1) {
                this.log( "item: " + item.title + " has " +
                          occurrences.length.toString() + " occurrences." );
                if (LOG_LEVEL > 2) {
                    for each ( var occ in occurrences ) {
                        this.log("item: " + occ.title + "\n" + occ.icalString);
                    }
                }
            }
            // only proxies returned:
            receiverFunc( occurrences );
            nItems += occurrences.length;
        }
    }
    else {
        if (maxResult != 0 &&
            (nItems + unexpandedItems.length) > maxResult) {
            unexpandedItems.length = (maxResult - nItems);
        }
        if (!bLeaveMutable) {
            for each ( var item in unexpandedItems ) {
                item.makeImmutable();
            }
        }
        if (LOG_LEVEL > 2) {
            for each ( var item in unexpandedItems ) {
                this.log( "item: " + item.title + "\n" + item.icalString );
            }
        }
        receiverFunc( unexpandedItems );
        nItems += unexpandedItems.length;
    }
    
    if (LOG_LEVEL > 1) {
        this.log( "parseItems_(): notified " + nItems + " items" );
    }
};

calWcapCalendar.prototype.getItem_queued = function( id, listener )
{
    // xxx todo: test
    // xxx todo: howto detect whether to call
    //           fetchevents_by_id ot fetchtodos_by_id?
    //           currently drag/drop is implemented for events only,
    //           try events first, fallback to todos... in the future...
    this.log( ">>>>>>>>>>>>>>>> getItem() call!");
    try {
        this.assureAccess(Components.interfaces.calIWcapCalendar.AC_COMP_READ);
        
        var this_ = this;
        var syncResponseFunc = function( wcapResponse ) {
            var icalRootComp = wcapResponse.data; // first statement, may throw
            var items = this_.parseItems(
                icalRootComp,
                Components.interfaces.calICalendar.ITEM_FILTER_ALL_ITEMS,
                1, null, null );
            if (items.length < 1)
                throw new Components.Exception("no such item!");
            if (items.length > 1) {
                this_.notifyError(
                    "unexpected number of items: " + items.length );
            }
            item = items[0];
            if (listener != null) {
                listener.onGetResult(
                    this_.superCalendar, Components.results.NS_OK,
                    Components.interfaces.calIItemBase,
                    this_.log( "getItem(): success." ),
                    items.length, items );
                listener.onOperationComplete(
                    this_.superCalendar, Components.results.NS_OK,
                    Components.interfaces.calIOperationListener.GET,
                    items.length == 1 ? items[0].id : null, null );
                this_.log( "item delivered." );
            }
        };
        
        var params = ("&relativealarm=1&compressed=1&recurring=1" +
                      "&fmt-out=text%2Fcalendar");
        params += ("&uid=" + encodeURIComponent(id));
        try {
            // most common: event
            this.session.issueSyncRequest(
                this.getCommandUrl( "fetchevents_by_id" ) + params,
                stringToIcal, syncResponseFunc );
        }
        catch (exc) {
            // try again, may be a task:
            this.session.issueSyncRequest(
                this.getCommandUrl( "fetchtodos_by_id" ) + params,
                stringToIcal, syncResponseFunc );
        }
    }
    catch (exc) {
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_ERROR_FAILURE,
                Components.interfaces.calIOperationListener.GET,
                null, exc );
        }
        if (testResultCode(exc, Components.interfaces.
                           calIWcapErrors.WCAP_LOGIN_FAILED)) {
            // silently ignore login failed, no calIObserver UI:
            this.logError( "getItem() ignored: " + errorToString(exc) );
        }
        else
            this.notifyError( exc );
    }
    this.log( "getItem() returning." );
};

calWcapCalendar.prototype.getItems_resp = function(
    wcapResponse,
    itemFilter, maxResult, rangeStart, rangeEnd, listener )
{
    try {
        var exc = wcapResponse.exception;
        // check whether access is denied,
        // then try to use free-busy information instead:
        // xxx todo: reuse these bits here; should be shifted to
        //           getItems_queued directly if (!checkAccess(AC_COMP_READ))...
        if (testResultCode( exc, Components.interfaces.
                            calIWcapErrors.WCAP_ACCESS_DENIED_TO_CALENDAR)) {
            if (listener &&
                // xxx todo: ignore errors on Todo retrieval, callers ought
                //           to check whether Read-Access is granted before
                //           calling getItems() in the future.
                (itemFilter &
                 Components.interfaces.calICalendar.ITEM_FILTER_TYPE_EVENT) &&
                rangeStart && rangeEnd)
            {
                var this_ = this;
                var freeBusyListener = { // calIWcapFreeBusyListener:
                    onGetFreeBusyTimes:
                    function( rc, requestId, calId, count, entries )
                    {
                        if (rc == Components.results.NS_OK) {
                            var items = [];
                            for each ( var entry in entries ) {
                                var item = new CalEvent();
                                item.id = (g_busyPhantomItemUuidPrefix +
                                           entry.dtRangeStart.icalString);
                                item.calendar = this_.superCalendar;
                                item.title = g_busyItemTitle;
                                item.startDate = entry.dtRangeStart;
                                item.endDate = entry.dtRangeEnd;
                                item.makeImmutable();
                                items.push(item);
                            }
                            listener.onGetResult(
                                this_.superCalendar, Components.results.NS_OK,
                                Components.interfaces.calIItemBase,
                                this_.log( "getItems_resp() using free-busy " +
                                           "information: success." ),
                                items.length, items );
                            listener.onOperationComplete(
                                this_.superCalendar, Components.results.NS_OK,
                                Components.interfaces.calIOperationListener.GET,
                                items.length == 1 ? items[0].id : null, null );
                            this_.log( items.length.toString() +
                                       " freebusy items delivered." );
                        }
                        else {
                            // if even availability is denied:
                            listener.onOperationComplete(
                                this_.superCalendar,
                                Components.results.NS_ERROR_FAILURE,
                                Components.interfaces.calIOperationListener.GET,
                                null, rc );
                        }
                    }
                };
                // for the exotic case that someone can read this calendar,
                // but has no freebusy access...
                // cannot check this in session, because that API is also for
                // looking up users...
                this.assureAccess(
                    Components.interfaces.calIWcapCalendar.AC_FREEBUSY);
                this.session.getFreeBusyTimes(
                    this.calId, rangeStart, rangeEnd, true /*bBusyOnly*/,
                    freeBusyListener, true/*async*/, 0 /*requestId*/ );
            }
            return;
        }
        
        var icalRootComp = wcapResponse.data; // first statement, may throw
        
        if (listener) {
            var this_ = this;
            function deliverItems(items) {
                listener.onGetResult(
                    this_.superCalendar, Components.results.NS_OK,
                    Components.interfaces.calIItemBase,
                    "WCAP getItems_resp()",
                    items.length, items );
            }
            this.parseItems_(
                deliverItems,
                icalRootComp, itemFilter, maxResult, rangeStart, rangeEnd );
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_OK,
                Components.interfaces.calIOperationListener.GET,
                null, null );
        }
        else {
            // just to check returned data:
            this.parseItems(
                icalRootComp, itemFilter, maxResult, rangeStart, rangeEnd );
        }
        this.log( "getItems(): success." );
    }
    catch (exc) {
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_ERROR_FAILURE,
                Components.interfaces.calIOperationListener.GET,
                null, exc );
        }
        this.notifyError( exc );
    }
};

function getItemFilterUrlPortions( itemFilter )
{
    var url = "";
    switch (itemFilter &
            Components.interfaces.calICalendar.ITEM_FILTER_TYPE_ALL) {
    case Components.interfaces.calICalendar.ITEM_FILTER_TYPE_TODO:
        url += "&component-type=todo"; break;
    case Components.interfaces.calICalendar.ITEM_FILTER_TYPE_EVENT:
        url += "&component-type=event"; break;
    }
    
    const calIWcapCalendar = Components.interfaces.calIWcapCalendar;
    var compstate = "";
//     if (itemFilter & calIWcapCalendar.ITEM_FILTER_REPLY_DECLINED)
//         compstate += ";REPLY-DECLINED";
//     if (itemFilter & calIWcapCalendar.ITEM_FILTER_REPLY_ACCEPTED)
//         compstate += ";REPLY-ACCEPTED";
//     if (itemFilter & calIWcapCalendar.ITEM_FILTER_REQUEST_COMPLETED)
//         compstate += ";REQUEST-COMPLETED";
    if (itemFilter & calIWcapCalendar.ITEM_FILTER_REQUEST_NEEDS_ACTION)
        compstate += ";REQUEST-NEEDS-ACTION";
    if (itemFilter & calIWcapCalendar.ITEM_FILTER_REQUEST_NEEDSNOACTION)
        compstate += ";REQUEST-NEEDSNOACTION";
//     if (itemFilter & calIWcapCalendar.ITEM_FILTER_REQUEST_PENDING)
//         compstate += ";REQUEST-PENDING";
//     if (itemFilter & calIWcapCalendar.ITEM_FILTER_REQUEST_WAITFORREPLY)
//         compstate += ";REQUEST-WAITFORREPLY";
    if (compstate.length > 0)
        url += ("&compstate=" + compstate.substr(1));
    return url;
}

calWcapCalendar.prototype.getItems_queued = function(
    itemFilter, maxResult, rangeStart, rangeEnd, listener )
{
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
    this.log( "getItems():\n\titemFilter=" + itemFilter +
              ",\n\tmaxResult=" + maxResult +
              ",\n\trangeStart=" + zRangeStart +
              ",\n\trangeEnd=" + zRangeEnd );
    try {
        this.assureAccess(Components.interfaces.calIWcapCalendar.AC_COMP_READ);
        
        var url = this.getCommandUrl( "fetchcomponents_by_range" );
        url += ("&relativealarm=1&compressed=1&recurring=1" +
                "&fmt-out=text%2Fcalendar");
        
        // setting component-type, compstate filters:
        url += getItemFilterUrlPortions(itemFilter);
        
        if (maxResult > 0)
            url += ("&maxResult=" + maxResult);
        url += ("&dtstart=" + zRangeStart);
        url += ("&dtend=" + zRangeEnd);
        
        var this_ = this;
        this.session.issueAsyncRequest(
            url, stringToIcal,
            function( wcapResponse ) {
                this_.getItems_resp( wcapResponse,
                                     itemFilter, maxResult,
                                     rangeStart, rangeEnd, listener );
            } );
    }
    catch (exc) {
        if (listener != null) {
            listener.onOperationComplete(
                this.superCalendar, Components.results.NS_ERROR_FAILURE,
                Components.interfaces.calIOperationListener.GET,
                null, exc );
        }
        if (testResultCode(exc, Components.interfaces.
                           calIWcapErrors.WCAP_LOGIN_FAILED)) {
            // silently ignore login failed, no calIObserver UI:
            this.logError( "getItems() ignored: " + errorToString(exc) );
        }
        else
            this.notifyError( exc );
    }
    this.log( "getItems() returning." );
};


function SyncState( finishFunc, abortFunc ) {
    this.m_finishFunc = finishFunc;
    this.m_abortFunc = abortFunc;
}
SyncState.prototype = {
    m_state: 0,
    m_finishFunc: null,
    m_abortFunc: null,
    m_exc: null,
    
    acquire: function() { /*this.checkAborted();*/ ++this.m_state; },
    release: function() {
        /*this.checkAborted();*/
        --this.m_state;
//         logMessage( "sync-state", "m_state = " + this.m_state );
        if (this.m_state == 0) {
            this.m_finishFunc();
        }
    },
    
    checkAborted: function() {
        if (this.m_exc)
            throw this.m_exc;
    },
    get isAborted() { return this.m_exc != null; },
    abort: function( exc ) {
        if (!this.isAborted) // store only first error that has occurred
            this.m_exc = exc;
        this.m_abortFunc( exc );
    }
};

function FinishListener( opType, syncState ) {
    this.wrappedJSObject = this;
    this.m_opType = opType;
    this.m_syncState = syncState;
}
FinishListener.prototype = {
    m_opType: 0,
    m_syncState: null,
    
    // calIOperationListener:
    onOperationComplete:
    function( calendar, status, opType, id, detail )
    {
        if (status != Components.results.NS_OK) {
            this.m_syncState.abort( detail );
        }
        else if (this.m_opType != opType) {
            this.m_syncState.abort(
                new Components.Exception("unexpected operation type: " +
                                         opType) );
        }
        this.m_syncState.release();
    },
    onGetResult:
    function( calendar, status, itemType, detail, count, items )
    {
        this.m_syncState.abort(
            new Components.Exception("unexpected onGetResult()!") );
    }
};

calWcapCalendar.prototype.syncChangesTo_resp = function(
    wcapResponse, syncState, listener, func )
{
    try {
        var icalRootComp = wcapResponse.data; // first statement, may throw
        var items = this.parseItems_(
            function(items) { items.forEach(func) },
            icalRootComp,
            Components.interfaces.calICalendar.ITEM_FILTER_ALL_ITEMS,
            0, null, null );
    }
    catch (exc) {
        syncState.abort( exc );
    }
    syncState.release();
};

calWcapCalendar.prototype.syncChangesTo_queued = function(
    destCal, itemFilter, dtFrom_, listener )
{
    var dtFrom = dtFrom_;
    if (dtFrom) {
        dtFrom = dtFrom.clone();
        // assure DATETIMEs:
        if (dtFrom.isDate)
            dtFrom.isDate = false;
        dtFrom = this.session.getServerTime(dtFrom);
    }
    var zdtFrom = getIcalUTC(dtFrom);
    this.log( "syncChangesTo():\n\titemFilter=" + itemFilter +
              "\n\tdtFrom=" + zdtFrom );
    
    var calObserver = null;
    try {
        calObserver = listener.QueryInterface(
            Components.interfaces.calIObserver );
    }
    catch (exc) {
    }
    
    const SYNC = Components.interfaces.calIWcapCalendar.SYNC;
    
    try {
        this.assureAccess(Components.interfaces.calIWcapCalendar.AC_COMP_READ);
        
        var this_ = this;
        // new stamp for this sync:
        var now = getTime();
        
        var syncState = new SyncState(
            // finishFunc:
            function() {
                if (listener) {
                    if (syncState.isAborted) {
                        this_.log( "firing SYNC failed!" );
                        listener.onOperationComplete(
                            this_.superCalendar,
                            Components.results.NS_ERROR_FAILURE,
                            SYNC, null, syncState.m_exc );
                    }
                    else {
                        this_.log( "firing SYNC succeeded." );
                        listener.onOperationComplete(
                            this_.superCalendar, Components.results.NS_OK,
                            SYNC, null, now );
                    }
                }
            },
            // abortFunc:
            function( exc ) {
                if (listener) {
                    listener.onOperationComplete(
                        this_.superCalendar,
                        Components.results.NS_ERROR_FAILURE,
                        SYNC, null, exc );
                }
                this_.logError( exc );
            } );
        
        var addItemListener = new FinishListener(
            Components.interfaces.calIOperationListener.ADD, syncState );
        var modifiedItems = [];
        
        this.log( "getting last modifications...", "syncChangesTo()" );
        var modifyItemListener = new FinishListener(
            Components.interfaces.calIOperationListener.MODIFY, syncState );
        var params = ("&relativealarm=1&compressed=1&recurring=1" +
                      "&fmt-out=text%2Fcalendar");
        params += ("&dtstart=" + zdtFrom);
        params += ("&dtend=" + getIcalUTC(this.session.getServerTime(now)));
        
        syncState.acquire();
        this.session.issueAsyncRequest(
            this.getCommandUrl("fetchcomponents_by_lastmod") +
            params + getItemFilterUrlPortions(itemFilter),
            stringToIcal,
            function( wcapResponse ) {
                this_.syncChangesTo_resp(
                    wcapResponse, syncState, listener,
                    function( item ) {
                        var dtCreated = item.getProperty("CREATED");
                        var bAdd = (dtCreated == null || dtFrom == null ||
                                    dtCreated.compare(dtFrom) >= 0);
                        modifiedItems.push( item.id );
                        if (bAdd) {
                            // xxx todo: verify whether exceptions
                            //           have been written
                            this_.log( "new item: " + item.id,
                                       "syncChangesTo_resp()" );
                            if (destCal) {
                                syncState.acquire();
                                destCal.addItem( item, addItemListener );
                            }
                            if (calObserver)
                                calObserver.onAddItem( item );
                        }
                        else {
                            this_.log( "modified item: " + item.id,
                                       "syncChangesTo_resp()" );
                            if (destCal) {
                                syncState.acquire();
                                destCal.modifyItem( item, null,
                                                    modifyItemListener );
                            }
                            if (calObserver)
                                calObserver.onModifyItem( item, null );
                        }
                    } );
            } );
        
        this.log( "getting deleted items...", "syncChangesTo()" );
        var deleteItemListener = new FinishListener(
            Components.interfaces.calIOperationListener.DELETE, syncState );
        syncState.acquire();
        this.session.issueAsyncRequest(
            this.getCommandUrl("fetch_deletedcomponents") + params +
            getItemFilterUrlPortions( itemFilter & // only component-type
                                      Components.interfaces.calICalendar
                                      .ITEM_FILTER_TYPE_ALL ),
            stringToIcal,
            function( wcapResponse ) {
                this_.syncChangesTo_resp(
                    wcapResponse, syncState, listener,
                    function( item ) {
                        // don't delete anything that has been touched
                        // by lastmods:
                        if (modifiedItems.some(
                                function(mid) { return (item.id == mid); } )) {
                            this_.log( "skipping deletion of " + item.id,
                                       "syncChangesTo_resp()" );
                            return;
                        }
                        if (isParent(item)) {
                            this_.log( "deleted item: " + item.id,
                                       "syncChangesTo_resp()" );
                            if (destCal) {
                                syncState.acquire();
                                destCal.deleteItem(
                                    item, deleteItemListener );
                            }
                            if (calObserver)
                                calObserver.onDeleteItem( item );
                        }
                        else {
                            // modify parent instead of
                            // straight-forward deleteItem(). WTF.
                            var parent = item.parentItem.clone();
                            parent.recurrenceInfo.removeOccurrenceAt(
                                item.recurrenceId );
                            this_.log( "modified parent: " + parent.id,
                                       "syncChangesTo_resp()" );
                            if (destCal) {
                                syncState.acquire();
                                destCal.modifyItem( parent, item,
                                                    deleteItemListener );
                            }
                            if (calObserver)
                                calObserver.onModifyItem( parent, item );
                        }
                    } );
            } );
    }
    catch (exc) {
        if (testResultCode(exc, Components.interfaces.
                           calIWcapErrors.WCAP_LOGIN_FAILED)) {
            // silently ignore login failed, no calIObserver UI,
            // state everything ok and return empty range:
            if (listener) {
                listener.onOperationComplete(
                    this.superCalendar, Components.results.NS_OK,
                    SYNC, null, dtFrom_ /* pass original stamp:
                                           => empty sync range */ );
            }
            this.logError("ignored: " + errorToString(exc), "syncChangesTo()");
        }
        else {
            if (listener) {
                listener.onOperationComplete(
                    this.superCalendar, Components.results.NS_ERROR_FAILURE,
                    SYNC, null, exc );
            }
            this.notifyError( exc );
        }
    }
    this.log( "finished.", "syncChangesTo()" );
};

