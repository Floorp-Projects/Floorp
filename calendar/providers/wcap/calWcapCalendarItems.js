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

var calWcapCalendar;
if (!calWcapCalendar) {
    calWcapCalendar = {};
    calWcapCalendar.prototype = {};
}

calWcapCalendar.prototype.encodeAttendee =
function calWcapCalendar_encodeAttendee(att)
{
    if (LOG_LEVEL > 2)
        log("attendee.icalProperty.icalString=" + att.icalProperty.icalString, this);
    function encodeAttr(val, attr, params) {
        if (val && val.length > 0) {
            if (params.length > 0)
                params += "^";
            if (attr)
                params += (attr + "=");
            params += encodeURIComponent(val);
        }
        return params;
    }
    var params = encodeAttr(att.rsvp ? "TRUE" : "FALSE", "RSVP", "");
    params = encodeAttr(att.participationStatus, "PARTSTAT", params);
    params = encodeAttr(att.role, "ROLE", params);
    params = encodeAttr(att.commonName, "CN", params);
    return encodeAttr(att.id, null, params);
};

calWcapCalendar.prototype.getRecurrenceParams =
function calWcapCalendar_getRecurrenceParams(
    item, out_rrules, out_rdates, out_exrules, out_exdates)
{
    // recurrences:
    out_rrules.value = [];
    out_rdates.value = [];
    out_exrules.value = [];
    out_exdates.value = [];
    if (item.recurrenceInfo) {
        var rItems = item.recurrenceInfo.getRecurrenceItems({});
        for each (var rItem in rItems) {
            var isNeg = rItem.isNegative;
            if (rItem instanceof Components.interfaces.calIRecurrenceRule) {
                var rule = ("\"" + encodeURIComponent(
                                rItem.icalProperty.valueAsIcalString) +
                            "\"");
                if (isNeg)
                    out_exrules.value.push(rule);
                else
                    out_rrules.value.push(rule);
            }
            else if (rItem instanceof Components.interfaces.calIRecurrenceDateSet) {
                var d = rItem.getDates({});
                for each (var d in rdates) {
                    if (isNeg)
                        out_exdates.value.push( getIcalUTC(d.date) );
                    else
                        out_rdates.value.push( getIcalUTC(d.date) );
                }
            }
            else if (rItem instanceof Components.interfaces.calIRecurrenceDate) {
                if (isNeg)
                    out_exdates.value.push( getIcalUTC(rItem.date) );
                else
                    out_rdates.value.push( getIcalUTC(rItem.date) );
            }
            else {
                this.notifyError("don\'t know how to handle this recurrence item: " +
                                 rItem.valueAsIcalString);
            }
        }
    }
};

calWcapCalendar.prototype.encodeRecurrenceParams =
function calWcapCalendar_encodeRecurrenceParams(item, oldItem)
{
    var rrules = {};
    var rdates = {};
    var exrules = {};
    var exdates = {};
    this.getRecurrenceParams(item, rrules, rdates, exrules, exdates);
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
        
        function sameSet(list, list_) {
            return (list.length == list_.length &&
                    list.every( function everyFunc(x) {
                                    return list_.some(
                                        function someFunc(y) { return x == y; } );
                                }
                        ));
        }
        if (sameSet(rrules.value, rrules_.value))
            rrules.value = null; // don't write
        if (sameSet(rdates.value, rdates_.value))
            rdates.value = null; // don't write
        if (sameSet(exrules.value, exrules.value))
            exrules.value = null; // don't write
        if (sameSet(exdates.value, exdates_.value))
            exdates.value = null; // don't write
    }
    
    function encodeList(list) {
        var ret = "";
        for each ( var str in list ) {
            if (ret.length > 0)
                ret += ";";
            ret += str;
        }
        return ret;
    }
    var ret = "";
    if (rrules.value)
        ret += ("&rrules=" + encodeList(rrules.value));
    if (rdates.value)
        ret += ("&rdates=" + encodeList(rdates.value));
    if (exrules.value)
        ret += ("&exrules=" + encodeList(exrules.value));
    if (exdates.value)
        ret += ("&exdates=" + encodeList(exdates.value));
    return ret;
    // xxx todo:
    // rchange=1: expand recurrences,
    // or whether to replace the rrule, ambiguous documentation!!!
    // check with store(with no uid) upon adoptItem() which behaves strange
    // if rchange=0 is set!
};

calWcapCalendar.prototype.getAlarmParams =
function calWcapCalendar_getAlarmParams(item)
{
    var params = null;
    var alarmStart = item.alarmOffset;
    if (alarmStart) {
        if (item.alarmRelated == calIItemBase.ALARM_RELATED_END) {
            // cs does not support explicit RELATED=END when
            // both start|entry and end|due are written
            var dur = item.duration;
            if (dur) { // both given
                alarmStart = alarmStart.clone();
                alarmStart.addDuration(dur);
            } // else only end|due is set, alarm makes little sense though
        }
        
        var emails = "";
        if (item.hasProperty("alarmEmailAddress"))
            emails = encodeURIComponent(item.getProperty("alarmEmailAddress"));
        else {
            this.session.getDefaultAlarmEmails({}).forEach(
                function forEachFunc(email) {
                    if (emails.length > 0)
                        emails += ";";
                    emails += encodeURIComponent(email);
                });
        }
        if (emails.length > 0) {
            params = ("&alarmStart=" + alarmStart.icalString);
            params += ("&alarmEmails=" + emails);
        }
        // else popup
    }
    if (!params) // clear popup, email alarm:
        params = "&alarmStart=&alarmPopup=&alarmEmails=";
    return params;
};

// why ever, X-S1CS-EMAIL is unsupported though documented
// for get_calprops... WTF.
function getCalId(att) {
    return (att ? att.getProperty("X-S1CS-CALID") : null);
}

function getAttendeeByCalId(atts, calId) {
    for each (var att in atts) {
        if (getCalId(att) == calId)
            return att;
    }
    return null;
}

calWcapCalendar.prototype.isInvitation =
function calWcapCalendar_isInvitation(item)
{
    if (!this.session.isLoggedIn)
        return false; // don't know
    var orgCalId = getCalId(item.organizer);
    if (!orgCalId)
        return false;
    var calId = this.calId;
    if (orgCalId == calId)
        return false;
    return (getAttendeeByCalId(item.getAttendees({}), calId) != null);
};

calWcapCalendar.prototype.getInvitedAttendee =
function calWcapCalendar_getInvitedAttendee(item)
{
    return getAttendeeByCalId(item.getAttendees({}), this.calId);
};

function equalDatetimes(one, two) {
    return ((!one && !two) ||
            (one && two && (one.compare(two) == 0)));
}

function identicalDatetimes(one, two) {
    return ((!one && !two) ||
            (one && two && (one.compare(two) == 0) && (one.timezone == two.timezone)));
}

// @return null if nothing has changed else value to be written
function diffProperty(newItem, oldItem, propName) {
    var val = newItem.getProperty(propName);
    var oldVal = (oldItem ? oldItem.getProperty(propName) : null);
    if (val === null) {
        // force being set when - no old item, eg when adding new item
        //                      - property is to be deleted
        if (!oldItem || oldVal)
            val = "";
    }
    else {
        if (val == oldVal)
            val = null;
    }
    return val;
}

calWcapCalendar.prototype.storeItem =
function calWcapCalendar_storeItem(bAddItem, item, oldItem, request, netRespFunc)
{
    var this_ = this;
    var bIsEvent = isEvent(item);
    var bIsParent = isParent(item);
    
    var bAttendeeReply = false;
    var bOrgRequest = false;
    var params = "";
    
    var calId = this.calId;
    if (oldItem && this.isInvitation(oldItem)) { // REPLY
        bAttendeeReply = true;
        var att = getAttendeeByCalId(item.getAttendees({}), calId);
        if (att) {
            log("attendee: " + att.icalProperty.icalString, this);
            var oldAtt = getAttendeeByCalId(oldItem.getAttendees({}), calId);
            if (att.participationStatus != oldAtt.participationStatus) {
                // REPLY first for just this calendar:
                params += ("&attendees=PARTSTAT=" + att.participationStatus +
                           "^" + encodeURIComponent(att.id));
            }
        }
    }
    else { // PUBLISH, REQUEST
        
        // workarounds for server bugs concerning recurrences/exceptions:
        // - if first occurrence is an exception
        //   and an EXDATE for that occurrence ought to be written,
        //   then the master item's data is replaced with that EXDATEd exception. WTF.
        // - if start/end date is being written on master, the previously EXDATEd
        //   exception overwrites master, why ever.
        // So in these cases: write all data of master.
        
        var bIsAllDay = false;
        if (bIsEvent) {
            var dtstart = item.startDate;
            var dtend = item.endDate;
            bIsAllDay = (dtstart.isDate && dtend.isDate);
            if (!oldItem || !identicalDatetimes(dtstart, oldItem.startDate)
                         || !identicalDatetimes(dtend, oldItem.endDate)) {
                params += ("&dtstart=" + getIcalUTC(dtstart));
                params += ("&X-NSCP-DTSTART-TZID=" +
                           "X-NSCP-ORIGINAL-OPERATION=X-NSCP-WCAP-PROPERTY-REPLACE^" + 
                           encodeURIComponent(this.getAlignedTimezone(dtstart.timezone)));
                params += ("&dtend=" + getIcalUTC(dtend));
                params += ("&X-NSCP-DTEND-TZID=" +
                           "X-NSCP-ORIGINAL-OPERATION=X-NSCP-WCAP-PROPERTY-REPLACE^" + 
                           encodeURIComponent(this.getAlignedTimezone(dtend.timezone)));
                params += (bIsAllDay ? "&isAllDay=1" : "&isAllDay=0");
                
                if (bIsParent && item.recurrenceInfo)
                    oldItem = null; // recurrence/exceptions hack: write whole master
            }
        }
        else { // calITodo
            // xxx todo: dtstart is mandatory for cs, so if this is
            //           undefined, assume an allDay todo???
            var dtstart = item.entryDate;
            var dtend = item.dueDate;
            bIsAllDay = (dtstart && dtstart.isDate);
            if (!oldItem || !identicalDatetimes(dtstart, oldItem.entryDate)
                         || !identicalDatetimes(dtend, oldItem.dueDate)) {
                params += ("&dtstart=" + getIcalUTC(dtstart));
                if (dtstart) {
                    params += ("&X-NSCP-DTSTART-TZID=" +
                               "X-NSCP-ORIGINAL-OPERATION=X-NSCP-WCAP-PROPERTY-REPLACE^" + 
                               encodeURIComponent(this.getAlignedTimezone(dtstart.timezone)));
                }
                params += ("&due=" + getIcalUTC(dtend));
                if (dtend) {
                    params += ("&X-NSCP-DUE-TZID=" +
                               "X-NSCP-ORIGINAL-OPERATION=X-NSCP-WCAP-PROPERTY-REPLACE^" + 
                               encodeURIComponent(this.getAlignedTimezone(dtend.timezone)));
                }
                params += (bIsAllDay ? "&isAllDay=1" : "&isAllDay=0");
                
                if (bIsParent && item.recurrenceInfo)
                    oldItem = null; // recurrence/exceptions hack: write whole master
            }
        }
        if (bIsParent) {
            var recParams = this.encodeRecurrenceParams(item, oldItem);
            if (recParams.length > 0) {
                oldItem = null; // recurrence/exceptions hack: write whole master
                params += recParams;
            }
        }
        
        var attendees = item.getAttendees({});
        if (attendees.length > 0) {
            // xxx todo: why ever, X-S1CS-EMAIL is unsupported though documented
            //           for calprops... WTF.
            bOrgRequest = true;
            function encodeAttendees(atts) {
                function stringSort(one, two) {
                    if (one == two)
                        return 0;
                    return (one < two ? -1 : 1);
                }
                atts = atts.concat([]);
                atts.sort(stringSort);
                var ret = "";
                for (var i = 0; i < atts.length; ++i) {
                    if (ret.length > 0)
                        ret += ";";
                    ret += this_.encodeAttendee(atts[i]);
                }
                return ret;
            }
            var attParam = encodeAttendees(attendees);
            if (!oldItem || attParam != encodeAttendees(oldItem.getAttendees({}))) {
                params += ("&orgCalid=" + encodeURIComponent(calId));
                params += ("&attendees=" + attParam);
            }
        }
        // else using just PUBLISH (method=1)
        else if (oldItem && oldItem.getAttendees({}).length > 0) {
            params += "&attendees="; // clear attendees
        }
        
        var val = item.title;
        if (!oldItem || val != oldItem.title)
            params += ("&summary=" + encodeURIComponent(val));
        // xxx todo: missing relatedTos= in cal api
        val = diffProperty(item, oldItem, "CATEGORIES");
        if (val !== null) // xxx todo: check whether ;-separated:
            params += ("&categories=" + encodeURIComponent( val.replace(/,/g, ";") ));
        val = diffProperty(item, oldItem, "DESCRIPTION");
        if (val !== null)
            params += ("&desc=" + encodeURIComponent(val));
        val = diffProperty(item, oldItem, "LOCATION");
        if (val !== null)
            params += ("&location=" + encodeURIComponent(val));
        val = diffProperty(item, oldItem, "URL");
        if (val !== null)
            params += ("&icsUrl=" + encodeURIComponent(val));
        // xxx todo: default prio is 0 (5 in sjs cs)
        val = item.priority;
        if (!oldItem || val != oldItem.priority)
            params += ("&priority=" + encodeURIComponent(val));
        
        function getPrivacy(item) {
            return ((item.privacy && item.privacy != "") ? item.privacy : "PUBLIC");
        }
        var icsClass = getPrivacy(item);
        if (!oldItem || icsClass != getPrivacy(oldItem))
            params += ("&icsClass="+ icsClass);
        
        if (!oldItem || item.status != oldItem.status) {
            switch (item.status) {
            case "CONFIRMED":    params += "&status=0"; break;
            case "CANCELLED":    params += "&status=1"; break;
            case "TENTATIVE":    params += "&status=2"; break;
            case "NEEDS-ACTION": params += "&status=3"; break;
            case "COMPLETED":    params += "&status=4"; break;
            case "IN-PROCESS":   params += "&status=5"; break;
            case "DRAFT":        params += "&status=6"; break;
            case "FINAL":        params += "&status=7"; break;
            default:
                params += "&status=3"; // NEEDS-ACTION
                break;
            }
        }
        
        val = diffProperty(item, oldItem, "TRANSP");
        if (val !== null) {
            switch (val) {
            case "TRANSPARENT":
                params += "&transparent=1";
                break;
            case "OPAQUE":
                params += "&transparent=0";
                break;
            default:
                params += ("&transparent=" +
                           (((icsClass == "PRIVATE") || bIsAllDay) ? "1" : "0"));
                break;
            }
        }
        
        if (!bIsEvent) {
            if (!oldItem || item.percentComplete != oldItem.percentComplete)
                params += ("&percent=" + item.percentComplete.toString(10));
            if (!oldItem || !equalDatetimes(item.completedDate, oldItem.completedDate))
                params += ("&completed=" + getIcalUTC(item.completedDate));
        }
        
        // attachment urls:
        function getAttachments(item) {
            var ret = "";
            var attachments = item.attachments;
            if (attachments) {
                var strings = [];
                for each (var att in attachements) {
                    if (typeof(att) == "string")
                        strings.push(encodeURIComponent(att));
                    else // xxx todo
                        logError("only URLs supported as attachment, not: " + att, this_);
                }
                strings.sort();
                for (var i = 0; i < strings.length; ++i) {
                    if (i > 0)
                        ret += ";";
                    ret += strings[i];
                }
            }
            return ret;
        }
        var val = getAttachments(item);
        if (!oldItem || val != getAttachments(oldItem))
            params += ("&attachments=" + val);        
    } // PUBLISH, REQUEST
    
    var alarmParams = this.getAlarmParams(item);
    if (!oldItem || (this.getAlarmParams(oldItem) != alarmParams)) {
        if (bOrgRequest && params.length == 0) {
            // assure no email notifications about this change:
            params += "&smtp=0&smtpNotify=0";
        }
        params += alarmParams;
    }
    
    if (params.length == 0) {
        log("no change at all.", this);
        if (LOG_LEVEL > 2) {
            log("old item:\n" + oldItem.icalString + "\n\nnew item:\n" + item.icalString, this);
        }
        request.execRespFunc(null, item);
    }
    else {
        var someDate = (item.startDate || item.entryDate || item.dueDate);
        if (someDate) {
            // provide some date: eMail notification dates are influenced by this parameter...
            params += ("&tzid=" + encodeURIComponent(
                           this.getAlignedTimezone(someDate.timezone)));
        }
        
        if (item.id)
            params += ("&uid=" + encodeURIComponent(item.id));
        
        // be picky about create/modify:
        // WCAP_STORE_TYPE_CREATE, WCAP_STORE_TYPE_MODIFY
        params += (bAddItem ? "&storetype=1" : "&storetype=2");
        
        if (bIsParent)
            params += "&mod=4"; // THIS AND ALL INSTANCES
        else {
            var rid = item.recurrenceId;
            if (rid.isDate) {
                // cs does not accept DATE:
                rid = rid.clone();
                rid.isDate = false;
            }
            params += ("&mod=1&rid=" + getIcalUTC(rid)); // THIS INSTANCE
        }
        
        if (bOrgRequest)
            params += "&method=2"; // REQUEST
        else if (bAttendeeReply)
            params += "&method=4"; // REPLY
        // else PUBLISH (default)
        
        params += "&replace=1"; // (update) don't append to any lists    
        params += "&fetch=1&relativealarm=1&compressed=1&recurring=1";
        params += "&emailorcalid=1&fmt-out=text%2Fcalendar";
        
        this.issueNetworkRequest(request, netRespFunc, stringToIcal,
                                 bIsEvent ? "storeevents" : "storetodos", params,
                                 calIWcapCalendar.AC_COMP_READ |
                                 calIWcapCalendar.AC_COMP_WRITE);
    }
};

calWcapCalendar.prototype.tunnelXProps =
function calWcapCalendar_tunnelXProps(destItem, srcItem)
{
    // xxx todo: temp workaround for bug in calItemBase.js
    if (!isParent(srcItem))
        return;
    // tunnel alarm X-MOZ-SNOOZE only if alarm is still set:
    var alarmOffset = destItem.alarmOffset;
    var enumerator = srcItem.propertyEnumerator;
    while (enumerator.hasMoreElements()) {
        try {
            var prop = enumerator.getNext().QueryInterface(Components.interfaces.nsIProperty);
            var name = prop.name;
            if (name.indexOf("X-MOZ-") == 0) {
                switch (name) {
                // keep snooze stamps for occurrences only and if alarm is still set:
                case "X-MOZ-SNOOZE-TIME":
                    if (!alarmOffset)
                        break; // alarm has been reset
                    // fallthru intended:
                default:
                    if (LOG_LEVEL > 1)
                        log("tunneling " + name + "=" + prop.value, this);
                    destItem.setProperty(name, prop.value);
                    break;
                }
            }
        }
        catch (exc) {
            logError(exc, this);
        }
    }
};

calWcapCalendar.prototype.adoptItem =
function calWcapCalendar_adoptItem(item, listener)
{
    var this_ = this;
    var request = new calWcapRequest(
        function adoptItem_resp(request, err, newItem) {
            if (listener) {
                listener.onOperationComplete(this_.superCalendar,
                                             getResultCode(err),
                                             calIOperationListener.ADD,
                                             err ? item.id : newItem.id,
                                             err ? err : newItem);
            }
            if (err)
                this_.notifyError(err);
            else
                this_.notifyObservers("onAddItem", [newItem]);
        },
        log("adoptItem() call: " + item.title, this));
    
    try {
        if (!isParent(item)) {
            this_.logError("adoptItem(): unexpected proxy!");
            debugger;
        }
        this.storeItem(true/*bAddItem*/,
                       item, null, request,
                       function netResp(err, icalRootComp) {
                           if (err)
                               throw err;
                           var items = this_.parseItems(
                               icalRootComp, calICalendar.ITEM_FILTER_ALL_ITEMS,
                               0, null, null, true /* bLeaveMutable */);
                           if (items.length < 1)
                               throw new Components.Exception("empty VCALENDAR returned!");
                           if (items.length > 1)
                               this_.notifyError("unexpected number of items: " + items.length);
                           var newItem = items[0];
                           this_.tunnelXProps(newItem, item);
                           item.makeImmutable();
                           // invalidate cached results:
                           delete this_.m_cachedResults;
                           log("newItem.id=" + newItem.id, this_);
                           // xxx todo: may log request status
                           request.execRespFunc(null, newItem);
                       });
    }
    catch (exc) {
        request.execRespFunc(exc);
    }
    return request;
}

calWcapCalendar.prototype.addItem =
function calWcapCalendar_addItem(item, listener)
{
    this.adoptItem(item.clone(), listener);
};

calWcapCalendar.prototype.modifyItem =
function calWcapCalendar_modifyItem(newItem, oldItem, listener)
{
    var this_ = this;
    var request = new calWcapRequest(
        function modifyItem_resp(request, err, item) {
            if (listener) {
                listener.onOperationComplete(
                    this_.superCalendar, getResultCode(err),
                    calIOperationListener.MODIFY,
                    newItem.id, err ? err : item);
            }
            if (err)
                this_.notifyError(err);
            else
                this_.notifyObservers("onModifyItem", [item, oldItem]);
        },
        log("modifyItem() call: " + newItem.id, this));
    
    try {
        if (!newItem.id)
            throw new Components.Exception("new item has no id!");
        var oldItem_ = oldItem;
        if (oldItem && !isParent(newItem) &&
            !oldItem.parentItem.recurrenceInfo.getExceptionFor(newItem.recurrenceId, false)) {
            // pass null for oldItem when creating new exceptions, write whole item:
            oldItem_ = null;
        }
        this.storeItem(false/*bAddItem*/,
                       newItem, oldItem_, request,
                       function netResp(err, icalRootComp) {
                           if (err)
                               throw err;
                           var items = this_.parseItems(
                               icalRootComp,
                               calICalendar.ITEM_FILTER_ALL_ITEMS,
                               0, null, null, true /* bLeaveMutable */);
                           if (items.length < 1)
                               throw new Components.Exception("empty VCALENDAR returned!");
                           if (items.length > 1)
                               this_.notifyError("unexpected number of items: " + items.length);
                           var item = items[0];
                           this_.tunnelXProps(item, newItem);
                           item.makeImmutable();
                           // invalidate cached results:
                           delete this_.m_cachedResults;
                           // xxx todo: maybe log request status
                           request.execRespFunc(null, item);
                       });
    }
    catch (exc) {
        request.execRespFunc(exc);
    }
    return request;
};

calWcapCalendar.prototype.deleteItem =
function calWcapCalendar_deleteItem(item, listener)
{
    var this_ = this;
    var request = new calWcapRequest(
        function deleteItem_resp(request, err) {
            // xxx todo: need to notify about each deleted item if multiple?
            if (listener) {
                listener.onOperationComplete(
                    this_.superCalendar, getResultCode(err),
                    calIOperationListener.DELETE,
                    item.id, err ? err : item);
            }
            if (err)
                this_.notifyError(err);
            else
                this_.notifyObservers("onDeleteItem", [item]);
        },
        log("deleteItem() call: " + item.id, this));
    
    try {
        if (!item.id)
            throw new Components.Exception("no item id!");
        var params = ("&uid=" + encodeURIComponent(item.id));
        if (isParent(item)) // delete THIS AND ALL:
            params += "&mod=4&rid=0";
        else { // delete THIS INSTANCE:
            var rid = item.recurrenceId;
            if (rid.isDate) {
                // cs does not accept DATE:
                rid = rid.clone();
                rid.isDate = false;
            }
            params += ("&mod=1&rid=" + getIcalUTC(rid));
        }
        params += "&fmt-out=text%2Fxml";
        
        this.issueNetworkRequest(
            request,
            function netResp(err, xml) {
                if (err)
                    throw err;
                // invalidate cached results:
                delete this_.m_cachedResults;
                if (LOG_LEVEL > 0)
                    log("deleteItem(): " + getWcapRequestStatusString(xml), this_);
            },
            stringToXml, isEvent(item) ? "deleteevents_by_id" : "deletetodos_by_id",
            params, calIWcapCalendar.AC_COMP_WRITE);
    }
    catch (exc) {
        request.execRespFunc(exc);
    }
    return request;
};

calWcapCalendar.prototype.parseItems = function calWcapCalendar_parseItems(
    icalRootComp, itemFilter, maxResults, rangeStart, rangeEnd, bLeaveMutable)
{
    var items = [];
    var unexpandedItems = [];
    var uid2parent = {};
    var excItems = [];
    
    var componentType = "ANY";
    switch (itemFilter & calICalendar.ITEM_FILTER_TYPE_ALL) {
    case calICalendar.ITEM_FILTER_TYPE_TODO:
        componentType = "VTODO"; break;
    case calICalendar.ITEM_FILTER_TYPE_EVENT:
        componentType = "VEVENT"; break;
    }
    
    var this_ = this;
    forEachIcalComponent(
        icalRootComp, componentType,
        function( subComp )
        {
            function patchTimezone(subComp, attr, xprop) {
                var dt = subComp[attr];
                if (dt) {
                    if (LOG_LEVEL > 2) {
                        log(attr + " is " + dt, this_);
                    }
                    var tzid = subComp.getFirstProperty(xprop);
                    if (tzid != null) {
                        subComp[attr] = dt.getInTimezone(tzid.value);
                        if (LOG_LEVEL > 2) {
                            log("patching " + xprop + " from " +
                                dt + " to " + subComp[attr], this_);
                        }
                    }
                }
            }

            patchTimezone(subComp, "startTime", "X-NSCP-DTSTART-TZID");
            var item = null;
            switch (subComp.componentType) {
            case "VEVENT": {
                patchTimezone(subComp, "endTime", "X-NSCP-DTEND-TZID");
                item = new CalEvent();
                item.icalComponent = subComp;
                break;
            }
            case "VTODO": {
                patchTimezone(subComp, "dueTime", "X-NSCP-DUE-TZID");
                item = new CalTodo();
                item.icalComponent = subComp;
                switch (itemFilter & calICalendar.ITEM_FILTER_COMPLETED_ALL) {
                    case calICalendar.ITEM_FILTER_COMPLETED_YES:
                        if (!item.isCompleted) {
                            delete item;
                            item = null;
                        }
                    break;
                    case calICalendar.ITEM_FILTER_COMPLETED_NO:
                        if (item.isCompleted) {
                            delete item;
                            item = null;
                        }
                    break;
                }
                break;
            }
            }
            if (item) {
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
                    item.recurrenceInfo = null;
                    var startDate = (isEvent(item)
                                     ? item.startDate : item.entryDate);
                    if (startDate && startDate.isDate && !rid.isDate) {
                        // cs ought to return proper all-day RECURRENCE-ID!
                        // get into startDate's timezone before cutting:
                        rid = rid.getInTimezone(startDate.timezone);
                        rid.isDate = true;
                        item.recurrenceId = rid;
                    }
                    if (LOG_LEVEL > 1) {
                        log("exception item: " + item.title +
                            "\nrid=" + rid.icalString +
                            "\nitem.id=" + item.id, this_);
                    }
                    excItems.push(item);
                }
                else if (item.recurrenceInfo) {
                    unexpandedItems.push(item);
                    uid2parent[item.id] = item;
                }
                else if (maxResults == 0 || items.length < maxResults) {
                    if (LOG_LEVEL > 2) {
                        log("item: " + item.title + "\n" + item.icalString,
                            this_);
                    }
                    if (!bLeaveMutable)
                        item.makeImmutable();
                    items.push(item);
                }
            }
        },
        maxResults);
    
    // tag "exceptions", i.e. items with rid:
    for each (var item in excItems) {
        var parent = uid2parent[item.id];
        if (parent) {
            item.parentItem = parent;
            item.makeImmutable();
            parent.recurrenceInfo.modifyException(item);
        }
        else {
            logError("parseItems(): no parent item for " + item.title +
                     ", rid=" + item.recurrenceId.icalString +
                     ", item.id=" + item.id, this);
            // due to a server bug, in some scenarions the returned
            // data is lacking the parent item, leave parentItem open then
            if ((itemFilter & calICalendar.ITEM_FILTER_CLASS_OCCURRENCES) == 0)
                item.recurrenceId = null;
            if (!bLeaveMutable)
                item.makeImmutable();
            items.push(item);
        }
    }
    
    if (itemFilter & calICalendar.ITEM_FILTER_CLASS_OCCURRENCES) {
        for each ( var item in unexpandedItems ) {
            if (maxResults != 0 && items.length >= maxResults)
                break;
            if (!bLeaveMutable)
                item.makeImmutable();
            var occurrences = item.recurrenceInfo.getOccurrences(
                rangeStart, rangeEnd,
                maxResults == 0 ? 0 : maxResults - items.length,
                {} );
            if (LOG_LEVEL > 1) {
                log("item: " + item.title + " has " +
                    occurrences.length.toString() + " occurrences.", this);
                if (LOG_LEVEL > 2) {
                    for each ( var occ in occurrences ) {
                        log("item: " + occ.title + "\n" + occ.icalString, this);
                    }
                }
            }
            // only proxies returned:
            items = items.concat(occurrences);
        }
    }
    else {
        if (maxResults != 0 &&
            (items.length + unexpandedItems.length) > maxResults) {
            unexpandedItems.length = (maxResults - items.length);
        }
        if (!bLeaveMutable) {
            for each ( var item in unexpandedItems ) {
                item.makeImmutable();
            }
        }
        if (LOG_LEVEL > 2) {
            for each ( var item in unexpandedItems ) {
                log("item: " + item.title + "\n" + item.icalString, this);
            }
        }
        items = items.concat(unexpandedItems);
    }
    
    if (LOG_LEVEL > 1)
        log("parseItems(): returning " + items.length + " items", this);
    return items;
};

// calWcapCalendar.prototype.getItem = function( id, listener )
// {
//     // xxx todo: test
//     // xxx todo: howto detect whether to call
//     //           fetchevents_by_id ot fetchtodos_by_id?
//     //           currently drag/drop is implemented for events only,
//     //           try events first, fallback to todos... in the future...
//     this.log( ">>>>>>>>>>>>>>>> getItem() call!");
//     try {
//         this.assureAccess(calIWcapCalendar.AC_COMP_READ);
        
//         var this_ = this;
//         var syncResponseFunc = function( wcapResponse ) {
//             var icalRootComp = wcapResponse.data; // first statement, may throw
//             var items = this_.parseItems(
//                 icalRootComp,
//                 calICalendar.ITEM_FILTER_ALL_ITEMS,
//                 1, null, null );
//             if (items.length < 1)
//                 throw new Components.Exception("no such item!");
//             if (items.length > 1) {
//                 this_.notifyError(
//                     "unexpected number of items: " + items.length );
//             }
//             item = items[0];
//             if (listener) {
//                 listener.onGetResult(
//                     this_.superCalendar, NS_OK,
//                     calIItemBase,
//                     log("getItem(): success.", this_),
//                     items.length, items );
//                 listener.onOperationComplete(
//                     this_.superCalendar, NS_OK,
//                     calIOperationListener.GET,
//                     items.length == 1 ? items[0].id : null, null );
//                 this_.log( "item delivered." );
//             }
//         };
        
//         var params = ("&relativealarm=1&compressed=1&recurring=1" +
//                       "&emailorcalid=1&fmt-out=text%2Fcalendar");
//         params += ("&uid=" + encodeURIComponent(id));
//         try {
//             // xxx todo!!!!

//             // most common: event
//             this.session.issueSyncRequest(
//                 this.getCommandUrl( "fetchevents_by_id" ) + params,
//                 stringToIcal, syncResponseFunc );
//         }
//         catch (exc) {
//             // try again, may be a task:
//             this.session.issueSyncRequest(
//                 this.getCommandUrl( "fetchtodos_by_id" ) + params,
//                 stringToIcal, syncResponseFunc );
//         }
//     }
//     catch (exc) {
//         if (listener != null) {
//             listener.onOperationComplete(
//                 this.superCalendar, Components.results.NS_ERROR_FAILURE,
//                 calIOperationListener.GET,
//                 null, exc );
//         }
//         if (getResultCode(exc) == calIWcapErrors.WCAP_LOGIN_FAILED) {
//             // silently ignore login failed, no calIObserver UI:
//             this.logError( "getItem() ignored: " + errorToString(exc) );
//         }
//         else
//             this.notifyError( exc );
//     }
//     this.log( "getItem() returning." );
// };

function getItemFilterParams(itemFilter)
{
    var params = "";
    switch (itemFilter & calICalendar.ITEM_FILTER_TYPE_ALL) {
    case calICalendar.ITEM_FILTER_TYPE_TODO:
        params += "&component-type=todo"; break;
    case calICalendar.ITEM_FILTER_TYPE_EVENT:
        params += "&component-type=event"; break;
    }
    
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
        params += ("&compstate=" + compstate.substr(1));
    return params;
}

calWcapCalendar.prototype.getItems =
function calWcapCalendar_getItems(itemFilter, maxResults, rangeStart, rangeEnd, listener)
{
    // assure DATE-TIMEs:
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
        function getItems_resp(request, err, data) {
            var rc = getResultCode(err);
            if (err) {
                if (listener) {
                    listener.onOperationComplete(
                        this_.superCalendar, rc,
                        calIOperationListener.GET,
                        null, err);
                }
                this_.notifyError(err, request.suppressOnError);
            }
            else {
                log("getItems(): success.", this_);
                if (listener) {
                    listener.onOperationComplete(
                        this_.superCalendar, rc,
                        calIOperationListener.GET,
                        null, null);
                }
            }
        },
        log("getItems():\n\titemFilter=0x" + itemFilter.toString(0x10) +
            ",\n\tmaxResults=" + maxResults +
            ",\n\trangeStart=" + zRangeStart +
            ",\n\trangeEnd=" + zRangeEnd, this));
    
    if (itemFilter & calIWcapCalendar.ITEM_FILTER_SUPPRESS_ONERROR)
        request.suppressOnError = true;
    
    if (this.aboutToBeUnregistered) {
        // limiting the amount of network traffic while unregistering
        log("being unregistered, no results.", this);
        request.execRespFunc(null, []);
        return request;
    }
    
    // m_cachedResults holds the last data revtrieval. This is expecially useful when
    // switching on multiple subcriptions: the composite calendar multiplexes getItems()
    // calls to all composited calendars over and over again, most often on the same
    // date range (as the user usually looks at the same view).
    // This will most likely vanish when a better caching is implemented in the views,
    // or WCAP local storage caching has sufficient performance.
    // The cached results will be invalidated after 2 minutes to reflect incoming invitations.
    if (CACHE_LAST_RESULTS > 0 && this.m_cachedResults) {
        for each (var entry in this.m_cachedResults) {
            if ((itemFilter == entry.itemFilter) &&
                equalDatetimes(rangeStart, entry.rangeStart) &&
                equalDatetimes(rangeEnd, entry.rangeEnd)) {
                log("reusing last getItems() cached data.", this);
                if (listener) {
                    listener.onGetResult(
                        this.superCalendar, NS_OK, calIItemBase,
                        "getItems()", entry.results.length, entry.results);
                }
                request.execRespFunc(null, entry.results);
                return request;
            }
        }
    }
    
    try {
        var params = ("&relativealarm=1&compressed=1&recurring=1" +
                      "&emailorcalid=1&fmt-out=text%2Fcalendar");
        // setting component-type, compstate filters:
        params += getItemFilterParams(itemFilter);
        if (maxResults > 0)
            params += ("&maxResults=" + maxResults);
        params += ("&dtstart=" + zRangeStart);
        params += ("&dtend=" + zRangeEnd);
        
        this.issueNetworkRequest(
            request,
            function netResp(err, icalRootComp) {
                if (err) {
                    if (getResultCode(err) ==
                        calIWcapErrors.WCAP_ACCESS_DENIED_TO_CALENDAR)
                    {
                        // try free-busy times:
                        if (listener &&
                            (itemFilter & calICalendar.ITEM_FILTER_TYPE_EVENT) &&
                            rangeStart && rangeEnd)
                        {
                            var freeBusyListener = { // calIWcapRequestResultListener:
                                onRequestResult:
                                function freeBusyListener_onRequestResult(request, result) {
                                    if (!request.succeeded)
                                        throw request.status;
                                    var items = [];
                                    for each (var period in result) {
                                        var item = new CalEvent();
                                        item.id = (g_busyPhantomItemUuidPrefix +
                                                   getIcalUTC(period.start));
                                        item.calendar = this_.superCalendar;
                                        item.title = g_busyItemTitle;
                                        item.startDate = period.start;
                                        item.endDate = period.end;
                                        item.makeImmutable();
                                        items.push(item);
                                    }
                                    listener.onGetResult(
                                        this_.superCalendar, NS_OK, calIItemBase,
                                        "getItems()/free-busy", items.length, items);
                                }
                            };
                            request.attachSubRequest(
                                this_.session.getFreeBusyTimes(
                                    this_.calId, rangeStart, rangeEnd, true /*bBusy*/,
                                    freeBusyListener));
                        }
                    }
                    else
                        throw err;
                }
                else if (listener) {
                    var items = this_.parseItems(
                        icalRootComp, itemFilter, maxResults,
                        rangeStart, rangeEnd);
                    
                    if (CACHE_LAST_RESULTS > 0) {
                        // auto invalidate after X minutes:
                        if (!this_.m_cachedResultsTimer) {
                            var callback = {
                                notify: function notify(timer) {
                                    if (!this_.m_cachedResults)
                                        return;
                                    var now = (new Date()).getTime();
                                    // sort out old entries:
                                    var entries = [];
                                    for (var i = 0; i < this_.m_cachedResults.length; ++i) {
                                        var entry = this_.m_cachedResults[i];
                                        if ((now - entry.stamp) <
                                            (CACHE_LAST_RESULTS_INVALIDATE * 1000)) {
                                            entries.push(entry);
                                        }
                                        else {
                                            log("invalidating cached entry:\n\trangeStart=" +
                                                getIcalUTC(entry.rangeStart) + "\n\trangeEnd=" +
                                                getIcalUTC(entry.rangeEnd), this_);
                                        }
                                    }
                                    this_.m_cachedResults = entries;
                                }
                            };
                            // sort out freq:
                            var freq = Math.min(
                                20, // default: 20secs
                                Math.max(1, CACHE_LAST_RESULTS_INVALIDATE));
                            log("cached results sort out timer freq: " + freq, this_);
                            this_.m_cachedResultsTimer = new Timer();
                            this_.m_cachedResultsTimer.initWithCallback(
                                callback, freq * 1000,
                                Components.interfaces.nsITimer.TYPE_REPEATING_SLACK);
                        }
                        if (!this_.m_cachedResults)
                            this_.m_cachedResults = [];
                        var entry = {
                            stamp: (new Date()).getTime(),
                            itemFilter: itemFilter,
                            rangeStart: (rangeStart ? rangeStart.clone() : null),
                            rangeEnd: (rangeEnd ? rangeEnd.clone() : null),
                            results: items
                        };
                        this_.m_cachedResults.unshift(entry);
                        if (this_.m_cachedResults.length > CACHE_LAST_RESULTS)
                            this_.m_cachedResults.length = CACHE_LAST_RESULTS;
                    }
                    
                    listener.onGetResult(this_.superCalendar, NS_OK, calIItemBase,
                                         "getItems()", items.length, items);
                }
            },
            stringToIcal,
            (itemFilter & calIWcapCalendar.ITEM_FILTER_BY_ALARM_RANGE)
            ? "fetchcomponents_by_alarmrange" : "fetchcomponents_by_range",
            params, calIWcapCalendar.AC_COMP_READ);
    }
    catch (exc) {
        request.execRespFunc(exc);
    }
    return request;
};

// function calWcapSyncOperationListener() {
//     this.superClass(respFunc);
//     this.wrappedJSObject = this;
// }
// subClass(calWcapSyncOperationListener, calWcapRequest);

// calWcapSyncOperationListener.prototype.QueryInterface =
// function calWcapSyncOperationListener_QueryInterface(iid) {
//     // xxx todo:
//     const m_ifaces = [ Components.interfaces.nsISupports,
//                        Components.interfaces.calIOperationListener,
//                        Components.interfaces.calIWcapRequest ];
//     qiface(m_ifaces, iid);
//     return this;
// };

// // calIOperationListener:
// calWcapSyncOperationListener.prototype.onOperationComplete =
// function calWcapSyncOperationListener_onOperationComplete(
//     calendar, status, opType, id, detail)
// {
//     if (status != NS_OK) {
//         this.
//             this.m_syncState.abort( detail );
//     }
//     else if (this.m_opType != opType) {
//         this.m_syncState.abort(
//             new Components.Exception("unexpected operation type: " +
//                                      opType) );
//     }
//     this.m_syncState.release();
// };

// calWcapSyncOperationListener.prototype.onGetResult =
// function calWcapSyncOperationListener_onGetResult(
//     calendar, status, itemType, detail, count, items)
// {
//     this.m_syncState.abort(
//         new Components.Exception("unexpected onGetResult()!") );
// };

// calWcapCalendar.prototype.syncChangesTo_resp = function(
//     wcapResponse, syncState, listener, func )
// {
//     try {
//         var icalRootComp = wcapResponse.data; // first statement, may throw
//         var items = this.parseItems_(
//             function(items) { items.forEach(func) },
//             icalRootComp,
//             calICalendar.ITEM_FILTER_ALL_ITEMS,
//             0, null, null );
//     }
//     catch (exc) {
//         syncState.abort( exc );
//     }
//     syncState.release();
// };

calWcapCalendar.prototype.syncChangesTo =
function calWcapCalendar_syncChangesTo(destCal, itemFilter, dtFrom_, listener)
{
    // xxx todo: move to Thomas
    // do NOT puke up error box every three minutes!
    itemFilter |= calIWcapCalendar.ITEM_FILTER_SUPPRESS_ONERROR;
    
    var now = getTime(); // new stamp for this sync
    var this_ = this;
    var request_ = new calWcapRequest(
        function syncChangesTo_resp(request, err) {
            if (err) {
                log("SYNC failed!", this_);
                if (listener) {
                    listener.onOperationComplete(
                        this_.superCalendar, getResultCode(err),
                        calIWcapCalendar.SYNC, null, err);
                }
                this_.notifyError(err, request.suppressOnError);
            }
            else {
                log("SYNC succeeded.", this_);
                if (listener) {
                    listener.onOperationComplete(
                        this_.superCalendar, NS_OK,
                        calIWcapCalendar.SYNC, null, now);
                }
            }
        },
        log("syncChangesTo():\n\titemFilter=0x" + itemFilter.toString(0x10) +
            "\n\tdtFrom_=" + getIcalUTC(dtFrom_), this));
    
    if (itemFilter & calIWcapCalendar.ITEM_FILTER_SUPPRESS_ONERROR)
        request_.suppressOnError = true;
    
    try {
        var dtFrom = dtFrom_;
        if (dtFrom) {
            dtFrom = dtFrom.clone();
            // assure DATE-TIME:
            if (dtFrom.isDate)
                dtFrom.isDate = false;
            dtFrom = this.session.getServerTime(dtFrom);
        }
        var zdtFrom = getIcalUTC(dtFrom);
        
        var calObserver = null;
        if (listener) {
            try {
                calObserver = listener.QueryInterface(
                    Components.interfaces.calIObserver);
            }
            catch (exc) {
            }
        }
        
        var request = new calWcapRequest(
            function netFinishedRespFunc(err, data) {
                var modifiedIds = {};
                for each (var item in request.m_modifiedItems) {
                    var dtCreated = item.getProperty("CREATED");
                    var bAdd = (!dtCreated || !dtFrom ||
                                dtCreated.compare(dtFrom) >= 0);
                    modifiedIds[item.id] = true;
                    if (bAdd) {
                        // xxx todo: verify whether exceptions
                        //           have been written
                        log("syncChangesTo(): new item " + item.id, this_);
                        if (destCal) {
//                                 destCal.addItem(item, addItemListener);
                        }
                        if (calObserver)
                            calObserver.onAddItem(item);
                    }
                    else {
                        log("syncChangesTo(): modified item " + item.id, this_);
                        if (destCal) {
//                             destCal.modifyItem(item, null, modifyItemListener);
                        }
                        if (calObserver)
                            calObserver.onModifyItem(item, null);
                    }
                }
                for each (var item in request.m_deletedItems) {
                    // don't delete anything that has been touched by lastmods:
                    if (modifiedIds[item.id])
                        log("syncChangesTo(): skipping deletion of " + item.id, this_);
                    else if (isParent(item)) {
                        log("syncChangesTo(): deleted item " + item.id, this_);
                        if (destCal) {
//                             destCal.deleteItem(item, deleteItemListener);
                        }
                        if (calObserver)
                            calObserver.onDeleteItem(item);
                    }
                    else { // modify parent instead of
                           // straight-forward deleteItem(). WTF.
                        var parent = item.parentItem.clone();
                        parent.recurrenceInfo.removeOccurrenceAt(item.recurrenceId);
                        log("syncChangesTo(): modified parent "+ parent.id, this_);
                        if (destCal) {
//                             destCal.modifyItem(parent, item, deleteItemListener);
                        }
                        if (calObserver)
                            calObserver.onModifyItem(parent, item);
                    }
                }
            }, "syncChangesTo() netFinishedRespFunc");
        request_.attachSubRequest(request);
        
        var params = ("&relativealarm=1&compressed=1&recurring=1" +
                      "&emailorcalid=1&fmt-out=text%2Fcalendar");
        params += ("&dtstart=" + zdtFrom);
        params += ("&dtend=" + getIcalUTC(this.session.getServerTime(now)));
        
        log("syncChangesTo(): getting last modifications...", this);
        this.issueNetworkRequest(
            request,
            function modifiedNetResp(err, icalRootComp) {
                if (err)
                    throw err;
                request.m_modifiedItems = this_.parseItems(
                    icalRootComp, calICalendar.ITEM_FILTER_ALL_ITEMS, 0, null, null);
            },
            stringToIcal, "fetchcomponents_by_lastmod",
            params + getItemFilterParams(itemFilter),
            calIWcapCalendar.AC_COMP_READ);        
        
        log("syncChangesTo(): getting deleted items...", this);
        this.issueNetworkRequest(
            request,
            function modifiedNetResp(err, icalRootComp) {
                if (err)
                    throw err;
                request.m_deletedItems = this_.parseItems(
                    icalRootComp, calICalendar.ITEM_FILTER_ALL_ITEMS, 0, null, null);
            },
            stringToIcal, "fetch_deletedcomponents",
            params + getItemFilterParams(itemFilter & // only component types
                                         calICalendar.ITEM_FILTER_TYPE_ALL),
            calIWcapCalendar.AC_COMP_READ);
    }
    catch (exc) {
        request_.execRespFunc(exc);
    }
    return request_;
};

