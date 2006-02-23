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

function calAttendee() {
    this.wrappedJSObject = this;
    this.mProperties = Components.classes["@mozilla.org/hash-property-bag;1"].
        createInstance(Components.interfaces.nsIWritablePropertyBag);
}

var calAttendeeClassInfo = {
    getInterfaces: function (count) {
        var ifaces = [
            Components.interfaces.nsISupports,
            Components.interfaces.calIAttendee,
            Components.interfaces.nsIClassInfo
        ];
        count.value = ifaces.length;
        return ifaces;
    },

    getHelperForLanguage: function (language) {
        return null;
    },

    contractID: "@mozilla.org/calendar/attendee;1",
    classDescription: "Calendar Attendee",
    classID: Components.ID("{5c8dcaa3-170c-4a73-8142-d531156f664d}"),
    implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0
};


calAttendee.prototype = {
    QueryInterface: function (aIID) {
        if (aIID.equals(Components.interfaces.nsIClassInfo))
            return calAttendeeClassInfo;

        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIAttendee))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    mImmutable: false,
    get isMutable() { return !this.mImmutable; },

    modify: function() {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
    },

    makeImmutable : function() {
        this.mImmutable = true;
    },

    clone: function() {
        var a = new calAttendee();

        if (this.mIsOrganizer) {
            a.isOrganizer = true;
        }

        var allProps = ["id", "commonName", "rsvp", "role", "participationStatus",
                        "userType"];
        for (var i in allProps)
            a[allProps[i]] = this[allProps[i]];
        // clone properties!
        return a;
    },
    // XXX enforce legal values for our properties;

    icalAttendeePropMap: [
    { cal: "rsvp",                ics: "RSVP" },
    { cal: "commonName",          ics: "CN" },
    { cal: "participationStatus", ics: "PARTSTAT" },
    { cal: "userType",            ics: "CUTYPE" },
    { cal: "role",                ics: "ROLE" } ],

    mIsOrganizer: false,
    get isOrganizer() { return this.mIsOrganizer; },
    set isOrganizer(bool) { this.mIsOrganizer = bool; },

    // icalatt is a calIcalProperty of type attendee
    set icalProperty (icalatt) {
        this.modify();
        this.id = icalatt.valueAsIcalString;
        var promotedProps = { };
        for (var i = 0; i < this.icalAttendeePropMap.length; i++) {
            var prop = this.icalAttendeePropMap[i];
            this[prop.cal] = icalatt.getParameter(prop.ics);
            // Don't copy these to the property bag.
            promotedProps[prop.ics] = true;
        }

        for (var paramname = icalatt.getFirstParameterName();
             paramname;
             paramname = icalatt.getNextParameterName()) {
            if (promotedProps[paramname])
                continue;
            this.setProperty(paramname, icalatt.getParameter(paramname));
        }
    },

    get icalProperty() {
        const icssvc =
            Components.classes["@mozilla.org/calendar/ics-service;1"].
                getService(Components.interfaces.calIICSService);

        var icalatt;
        if (!this.mIsOrganizer) {
            icalatt = icssvc.createIcalProperty("ATTENDEE");
        } else {
            icalatt = icssvc.createIcalProperty("ORGANIZER");
        }

        if (!this.id)
            throw Components.results.NS_ERROR_NOT_INITIALIZED;
        icalatt.valueAsIcalString = this.id;
        for (var i = 0; i < this.icalAttendeePropMap.length; i++) {
            var prop = this.icalAttendeePropMap[i];
            if (this[prop.cal])
                icalatt.setParameter(prop.ics, this[prop.cal]);
        }
        var bagenum = this.mProperties.enumerator;
        while (bagenum.hasMoreElements()) {
            var iprop = bagenum.getNext().
                QueryInterface(Components.interfaces.nsIProperty);
            icalatt.setParameter(iprop.name, iprop.value);
        }
        return icalatt;
    },

    get propertyEnumerator() { return this.mProperties.enumerator; },
    getProperty: function (aName) {
        try {
            return this.mProperties.getProperty(aName);
        } catch (e) {
            return null;
        }
    },
    setProperty: function (aName, aValue) {
        this.modify();
        this.mProperties.setProperty(aName, aValue);
    },
    deleteProperty: function (aName) {
        this.modify();
        try {
            this.mProperties.deleteProperty(aName);
        } catch (e) {
        }
    }
};

var makeMemberAttr;
if (makeMemberAttr) {
    makeMemberAttr(calAttendee, "mId", null, "id");
    makeMemberAttr(calAttendee, "mCommonName", null, "commonName");
    makeMemberAttr(calAttendee, "mRsvp", null, "rsvp");
    makeMemberAttr(calAttendee, "mRole", null, "role");
    makeMemberAttr(calAttendee, "mParticipationStatus", "NEEDS-ACTION",
                   "participationStatus");
    makeMemberAttr(calAttendee, "mUserType", "INDIVIDUAL", "userType");
}
