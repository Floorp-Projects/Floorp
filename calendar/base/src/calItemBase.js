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
// calItemBase.js
//

const ICAL = Components.interfaces.calIIcalComponent;

function calItemBase() { }

calItemBase.prototype = {
    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIItemBase))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    mImmutable: false,
    get isMutable() { return !this.mImmutable; },

    mDirty: false,
    modify: function() {
        if (this.mImmutable)
            // Components.results.NS_ERROR_CALENDAR_IMMUTABLE;
            throw Components.results.NS_ERROR_FAILURE;
        this.mDirty = true;
    },

    makeItemBaseImmutable: function() {
        // make all our components immutable
        this.mCreationDate.makeImmutable();

        if (this.mRecurrenceInfo)
            this.mRecurrenceInfo.makeImmutable();
        if (this.mAlarmTime)
            this.mAlarmTime.makeImmutable();

        if (this.mOrganizer)
            this.mOrganizer.makeImmutable();
        for (var i = 0; i < this.mAttendees.length; i++)
            this.mAttendees[i].makeImmutable();
        this.mImmutable = true;
    },

    // initialize this class's members
    initItemBase: function () {
        this.mCreationDate = new CalDateTime();
        this.mAlarmTime = new CalDateTime();
        this.mLastModifiedTime = new CalDateTime();
        this.mStampTime = new CalDateTime();

        this.mCreationDate.jsDate = new Date();
        this.mLastModifiedTime.jsDate = new Date();
        this.mStampTime.jsDate = new Date();
        
        this.mProperties = Components.classes["@mozilla.org/hash-property-bag;1"].
                           createInstance(Components.interfaces.nsIWritablePropertyBag);

        this.mAttachments = Components.classes["@mozilla.org/array;1"].
                            createInstance(Components.interfaces.nsIArray);

        this.mAttendees = [];

        this.mRecurrenceInfo = null;

        this.mAttachments = null;
    },

    // for subclasses to use; copies the ItemBase's values
    // into m
    cloneItemBaseInto: function (m) {
        var suppressDCE = this.lastModifiedTime;
        suppressDCE = this.stampTime;

        m.mImmutable = false;
        m.mGeneration = this.mGeneration;
        m.mLastModifiedTime = this.mLastModifiedTime.clone();
        m.mParent = this.mParent;
        m.mId = this.mId;
        m.mTitle = this.mTitle;
        m.mPriority = this.mPriority;
        m.mPrivacy = this.mPrivacy;
        m.mStatus = this.mStatus;
        m.mHasAlarm = this.mHasAlarm;

        m.mCreationDate = this.mCreationDate.clone();
        m.mStampTime = this.mStampTime.clone();
        if (this.mRecurrenceInfo) {
            m.mRecurrenceInfo = this.mRecurrenceInfo.clone();
            dump ("old recurType: " + this.mRecurrenceInfo.recurType + " new type: " + m.mRecurrenceInfo.recurType + "\n");
        }
        if (this.mAlarmTime)
            m.mAlarmTime = this.mAlarmTime.clone();

        m.mAttendees = [];
        for (var i = 0; i < this.mAttendees.length; i++)
            m.mAttendees[i] = this.mAttendees[i].clone();

        // these need fixing
        m.mAttachments = this.mAttachments;

        m.mProperties = Components.classes["@mozilla.org/hash-property-bag;1"].
                        createInstance(Components.interfaces.nsIWritablePropertyBag);

        var e = this.mProperties.enumerator;
        while (e.hasMoreElements()) {
            var prop = e.getNext().QueryInterface(Components.interfaces.nsIProperty);
            m.mProperties.setProperty (prop.name, prop.value);
        }

        m.mDirty = this.mDirty;

        return m;
    },

    get lastModifiedTime() {
        if (this.mDirty) {
            this.mLastModifiedTime.jsDate = new Date();
            this.mDirty = false;
        }
        return this.mLastModifiedTime;
    },

    mStampTime: null,
    get stampTime() {
        if (this.mStampTime.isValid)
            return this.mStampTime;
        return this.mLastModifiedTime;
    },

    updateStampTime: function() {
        this.modify();
        this.mStampTime.jsDate = new Date();
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
    },

    getAttendees: function (countObj) {
        countObj.value = this.mAttendees.length;
        return this.mAttendees.concat([]); // clone
    },

    getAttendeeById: function (id) {
        for (var i = 0; i < this.mAttendees.length; i++)
            if (this.mAttendees[i].id == id)
                return this.mAttendees[i];
        return null;
    },

    removeAttendee: function (attendee) {
        this.modify();
        var found = false, newAttendees = [];
        for (var i = 0; i < this.mAttendees.length; i++) {
            if (this.mAttendees[i] != attendee)
                newAttendees.push(this.mAttendees[i]);
            else
                found = true;
        }
        if (found)
            this.mAttendees = newAttendees;
        else
            throw Component.results.NS_ERROR_INVALID_ARG;
    },

    removeAllAttendees: function() {
        this.modify();
        this.mAttendees = [];
    },

    addAttendee: function (attendee) {
        this.modify();
        this.mAttendees.push(attendee);
    },

    get parent () {
        return this.mParent;
    },

    set parent (v) {
        if (this.mImmutable)
            // Components.results.NS_ERROR_CALENDAR_IMMUTABLE;
            throw Components.results.NS_ERROR_FAILURE;
        this.mParent = v;
    },

    mOrganizer: null,
    get organizer() {
        return this.mOrganizer;
    },

    set organizer(v) {
        this.modify();
        this.mOrganizer = v;
    },

    /* MEMBER_ATTR(mIcalString, "", icalString), */
    get icalString() {
        throw Components.results.NS_NOT_IMPLEMENTED;
    },

    set icalString() {
        throw Components.results.NS_NOT_IMPLEMENTED;
    },

    itemBasePromotedProps: {
        "CREATED": true,
        "UID": true,
        "LAST-MODIFIED": true,
        "SUMMARY": true,
        "PRIORITY": true,
        "METHOD": true,
        "STATUS": true,
        "CLASS": true,
        "DTSTAMP": true,
        "X-MOZILLA-GENERATION": true,
        "RRULE": true,
        "EXDATE": true,
        "RDATE": true,
        "ATTENDEE": true,
        "ORGANIZER": true,
    },

    mapPropsFromICS: function(icalcomp, propmap) {
        for (var i = 0; i < propmap.length; i++) {
            var prop = propmap[i];
            var val = icalcomp[prop.ics];
            if (val != null && val != ICAL.INVALID_VALUE)
                this[prop.cal] = val;
        }
    },

    mapPropsToICS: function(icalcomp, propmap) {
        for (var i = 0; i < propmap.length; i++) {
            var prop = propmap[i];
            if (!(prop.cal in this))
                continue;
            var val = this[prop.cal];
            if (val != null && val != ICAL.INVALID_VALUE)
                icalcomp[prop.ics] = val;
        }
    },

    icsBasePropMap: [
    { cal: "mCreationDate", ics: "createdTime" },
    { cal: "mLastModifiedTime", ics: "lastModified" },
    { cal: "mStampTime", ics: "stampTime" },
    { cal: "mId", ics: "uid" },
    { cal: "mTitle", ics: "summary" },
    { cal: "mPriority", ics: "priority" },
    { cal: "mStatus", ics: "status" },
    { cal: "mPrivacy", ics: "icalClass" }],

    setItemBaseFromICS: function (icalcomp) {
        this.modify();

        this.mapPropsFromICS(icalcomp, this.icsBasePropMap);

        for (var attprop = icalcomp.getFirstProperty("ATTENDEE");
             attprop;
             attprop = icalcomp.getNextProperty("ATTENDEE")) {
            
            var att = new CalAttendee();
            att.icalProperty = attprop;
            this.addAttendee(att);
        }

        var orgprop = icalcomp.getFirstProperty("ORGANIZER");
        if (orgprop) {
            var org = new CalAttendee();
            org.icalProperty = orgprop;
            this.mOrganizer = org;
        }
        
        var gen = icalcomp.getFirstProperty("X-MOZILLA-GENERATION");
        if (gen)
            this.mGeneration = parseInt(gen.stringValue);

        // find recurrence properties
        var rec = null;
        for (var recprop = icalcomp.getFirstProperty("ANY");
             recprop;
             recprop = icalcomp.getNextProperty("ANY"))
        {
            var ritem = null;
            if (recprop.propertyName == "RRULE" ||
                recprop.propertyName == "EXRULE")
            {
                ritem = new CalRecurrenceRule();
            } else if (recprop.propertyName == "RDATE" ||
                       recprop.propertyName == "EXDATE")
            {
                ritem = new CalRecurrenceDate();
            } else {
                continue;
            }

            ritem.icalProperty = recprop;

            if (!rec) {
                rec = new CalRecurrenceInfo();
                rec.initialize(this);
            }

            rec.appendRecurrenceItem(ritem);
        }
        this.mRecurrenceInfo = rec;

    },

    importUnpromotedProperties: function (icalcomp, promoted) {
        for (var prop = icalcomp.getFirstProperty("ANY");
             prop;
             prop = icalcomp.getNextProperty("ANY")) {
            if (!promoted[prop.propertyName]) {
                // XXX keep parameters around, sigh
                this.setProperty(prop.propertyName, prop.stringValue);
            }
        }
    },

    get icalComponent() {
        throw Components.results.NS_NOT_IMPLEMENTED;
    },

    fillIcalComponentFromBase: function (icalcomp) {
        // Make sure that the LMT and ST are updated
        var suppressDCE = this.lastModifiedTime;
        suppressDCE = this.stampTime;
        this.mapPropsToICS(icalcomp, this.icsBasePropMap);

        if (this.mOrganizer)
            icalcomp.addProperty(this.mOrganizer.icalProperty);
        for (var i = 0; i < this.mAttendees.length; i++)
            icalcomp.addProperty(this.mAttendees[i].icalProperty);

        if (this.mGeneration != 0) {
            var genprop = icalProp("X-MOZILLA-GENERATION");
            genprop.stringValue = String(this.mGeneration);
            icalcomp.addProperty(genprop);
        }

        if (this.mRecurrenceInfo) {
            var ritems = this.mRecurrenceInfo.getRecurrenceItems({});
            for (i in ritems) {
                icalcomp.addProperty(ritems[i].icalProperty);
            }
        }

    },
    
    getOccurrencesBetween: function(aStartDate, aEndDate, aCount) {
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
};

function calItemOccurrence () {
    this.wrappedJSObject = this;

    this.occurrenceStartDate = new CalDateTime();
    this.occurrenceEndDate = new CalDateTime();
}

calItemOccurrenceClassInfo = {
    getInterfaces: function (count) {
        var ifaces = [
            Components.interfaces.nsISupports,
            Components.interfaces.calIItemOccurrence
        ];
        count.value = ifaces.length;
        return ifaces;
    },

    getHelperForLanguage: function (language) {
        return null;
    },

    contractID: "@mozilla.org/calendar/item-occurrence;1",
    classDescription: "Calendar Item Occurrence",
    classID: Components.ID("{bad672b3-30b8-4ecd-8075-7153313d1f2c}"),
    implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0
};

calItemOccurrence.prototype = {
    QueryInterface: function (aIID) {
        if (aIID.equals(Components.interfaces.nsIClassInfo))
            return calItemOccurrenceClassInfo;

        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIItemOccurrence))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    initialize: function (aItem, aStartDate, aEndDate) {
        this.item = aItem;
        this.occurrenceStartDate = aStartDate.clone();
        this.occurrenceStartDate.makeImmutable();
        this.occurrenceEndDate = aEndDate.clone();
        this.occurrenceEndDate.makeImmutable();
    },

    item: null,
    occurrenceStartDate: null,
    occurrenceEndDate: null,

    get next() {
        if (this.item.recurrenceInfo)
            return this.item.recurrenceInfo.getNextOccurrence(this.item, aEndDate);
        return null;
    },
    get previous() {
        if (this.item.recurrenceInfo)
            return this.item.recurrenceInfo.getPreviousOccurrence(this.item, aStartDate);
        return null;
    },

    equals: function(aOther) {
        if (this.item.id != aOther.item.id)
            return false;

        if (this.occurrenceStartDate.compare(aOther.occurrenceStartDate) != 0)
            return false;

        if (this.occurrenceEndDate.compare(aOther.occurrenceEndDate) != 0)
            return false;

        return true;
    }
};

makeMemberAttr(calItemBase, "mGeneration", 0, "generation");
makeMemberAttr(calItemBase, "mCreationDate", null, "creationDate");
makeMemberAttr(calItemBase, "mId", null, "id");
makeMemberAttr(calItemBase, "mTitle", null, "title");
makeMemberAttr(calItemBase, "mPriority", 0, "priority");
makeMemberAttr(calItemBase, "mPrivacy", "PUBLIC", "privacy");
makeMemberAttr(calItemBase, "mStatus", null, "status");
makeMemberAttr(calItemBase, "mHasAlarm", false, "hasAlarm");
makeMemberAttr(calItemBase, "mAlarmTime", null, "alarmTime");
makeMemberAttr(calItemBase, "mRecurrenceInfo", null, "recurrenceInfo");
makeMemberAttr(calItemBase, "mAttachments", null, "attachments");
makeMemberAttr(calItemBase, "mProperties", null, "properties");

function makeMemberAttr(ctor, varname, dflt, attr)
{
    ctor.prototype[varname] = dflt;
    var getter = function () {
        return this[varname];
    };
    var setter = function (v) {
        this.modify();
        this[varname] = v;
    };
    ctor.prototype.__defineGetter__(attr, getter);
    ctor.prototype.__defineSetter__(attr, setter);
}

//
// helper functions
//

function icalFromString(str)
{
    const icssvc = Components.classes["@mozilla.org/calendar/ics-service;1"].
        getService(Components.interfaces.calIICSService);
    return icssvc.parseICS(str);
}

function icalProp(kind)
{
    const icssvc = Components.classes["@mozilla.org/calendar/ics-service;1"].
        getService(Components.interfaces.calIICSService);
    return icssvc.createIcalProperty(kind);
}
