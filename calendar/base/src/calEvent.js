/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Mike Shaver <shaver@off.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// calEvent.js
//

//
// constructor
//
function calEvent() {
    this.wrappedJSObject = this;
    this.initItemBase();
    this.initEvent();
}

// var trickery to suppress lib-as-component errors from loader
var calItemBase;

calEvent.prototype = {
    __proto__: calItemBase ? (new calItemBase()) : {},

    QueryInterface: function (aIID) {
        if (aIID.equals(Components.interfaces.calIEvent))
            return this;

        return this.__proto__.__proto__.QueryInterface.call(this, aIID);
    },

    clone: function () {
        var m = new calEvent();
        this.cloneItemBaseInto(m);
        m.mStartDate = this.mStartDate.clone();
        m.mEndDate = this.mEndDate.clone();
        m.isAllDay = this.isAllDay;

        return m;
    },

    makeImmutable: function () {
        this.mStartDate.makeImmutable();
        this.mEndDate.makeImmutable();

        this.makeItemBaseImmutable();
    },

    initEvent: function() {
        this.mStartDate = new CalDateTime();
        this.mEndDate = new CalDateTime();
    },

    get duration() {
        var dur = new CalDateTime();
        dur.setTimeInTimezone (this.mEndDate.nativeTime - this.mStartDate.nativeTime, null);
        return dur;
    },

    get recurrenceStartDate() {
        return this.mStartDate;
    },

    icsEventPropMap: [
    { cal: "mStartDate", ics: "startTime" },
    { cal: "mEndDate", ics: "endTime" }],

    set icalString(value) {
        this.icalComponent = icalFromString(value);
    },

    get icalString() {
        const icssvc = Components.
          classes["@mozilla.org/calendar/ics-service;1"].
          getService(Components.interfaces.calIICSService);
        var calcomp = icssvc.createIcalComponent("VCALENDAR");
        calcomp.prodid = "-//Mozilla Calendar//NONSGML Sunbird//EN";
        calcomp.version = "2.0";
        calcomp.addSubcomponent(this.icalComponent);
        return calcomp.serializeToICS();
    },

    get icalComponent() {
        const icssvc = Components.
          classes["@mozilla.org/calendar/ics-service;1"].
          getService(Components.interfaces.calIICSService);
        var icalcomp = icssvc.createIcalComponent("VEVENT");
        this.fillIcalComponentFromBase(icalcomp);
        this.mapPropsToICS(icalcomp, this.icsEventPropMap);
        
        var bagenum = this.mProperties.enumerator;
        while (bagenum.hasMoreElements()) {
            var iprop = bagenum.getNext().
                QueryInterface(Components.interfaces.nsIProperty);
            try {
                var icalprop = icssvc.createIcalProperty(iprop.name);
                icalprop.stringValue = iprop.value;
                icalcomp.addProperty(icalprop);
            } catch (e) {
                // dump("failed to set " + iprop.name + " to " + iprop.value +
                // ": " + e + "\n");
            }
        }
        return icalcomp;
    },

    set icalComponent(event) {
        this.modify();
        if (event.componentType != "VEVENT") {
            event = event.getFirstSubcomponent("VEVENT");
            if (!event)
                throw Components.results.NS_ERROR_INVALID_ARG;
        }

        this.setItemBaseFromICS(event);
        this.mapPropsFromICS(event, this.icsEventPropMap);
        this.mIsAllDay = this.mStartDate && this.mStartDate.isDate;

        var promotedProps = {
            "DTSTART": true,
            "DTEND": true,
            "DTSTAMP": true,
            __proto__: this.itemBasePromotedProps
        };
        this.importUnpromotedProperties(event, promotedProps);
        // Importing didn't really change anything
        this.mDirty = false;
    }
};

// var to avoid spurious errors when loaded as component during autoreg

var makeMemberAttr;
if (makeMemberAttr) {
    makeMemberAttr(calEvent, "mStartDate", null, "startDate");
    makeMemberAttr(calEvent, "mEndDate", null, "endDate");
    makeMemberAttr(calEvent, "mIsAllDay", false, "isAllDay");
}
