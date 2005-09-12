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
const kHashPropertyBagContractID = "@mozilla.org/hash-property-bag;1";
const kIWritablePropertyBag = Components.interfaces.nsIWritablePropertyBag;
const HashPropertyBag = new Components.Constructor(kHashPropertyBagContractID, kIWritablePropertyBag);

function NewCalDateTime(aJSDate) {
    var c = new CalDateTime();
    if (aJSDate)
        c.jsDate = aJSDate;
    return c;
}

function calItemBase() {
}

calItemBase.prototype = {
    mIsProxy: false,

    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIItemBase))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    mParentItem: null,
    get parentItem() {
        if (this.mParentItem)
            return this.mParentItem;
        else
            return this;
    },
    set parentItem(value) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_OBJECT_IS_IMMUTABLE;
        this.mIsProxy = true;
        this.mParentItem = value;
    },

    initializeProxy: function (aParentItem) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_OBJECT_IS_IMMUTABLE;

        if (this.mParentItem != null)
            throw Components.results.NS_ERROR_FAILURE;

        this.mParentItem = aParentItem;
        this.mCalendar = aParentItem.mCalendar;
        this.mIsProxy = true;
    },

    //
    // calIItemBase
    //
    mImmutable: false,
    get isMutable() { return !this.mImmutable; },

    mDirty: false,
    modify: function() {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_OBJECT_IS_IMMUTABLE;
        this.mDirty = true;
    },

    ensureNotDirty: function() {
        if (!this.mDirty)
            return;

        if (this.mImmutable) {
            dump ("### Something tried to undirty a dirty immutable event!\n");
            throw Components.results.NS_ERROR_OBJECT_IS_IMMUTABLE;
        }

        this.setProperty("LAST-MODIFIED", NewCalDateTime(new Date()));
        this.mDirty = false;
    },

    makeItemBaseImmutable: function() {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_OBJECT_IS_IMMUTABLE;

        // make all our components immutable
        if (this.mRecurrenceInfo)
            this.mRecurrenceInfo.makeImmutable();
        if (this.mAlarmTime)
            this.mAlarmTime.makeImmutable();

        if (this.mOrganizer)
            this.mOrganizer.makeImmutable();
        for (var i = 0; i < this.mAttendees.length; i++)
            this.mAttendees[i].makeImmutable();

        var e = this.mProperties.enumerator;
        while (e.hasMoreElements()) {
            var prop = e.getNext().QueryInterface(Components.interfaces.nsIProperty);
            var val = prop.value;

            if (prop.value instanceof Components.interfaces.calIDateTime) {
                if (prop.value.isMutable)
                    prop.value.makeImmutable();
            }
        }

        this.ensureNotDirty();
        this.mImmutable = true;
    },

    // initialize this class's members
    initItemBase: function () {
        var now = new Date();

        this.mProperties = new HashPropertyBag();

        this.setProperty("CREATED", NewCalDateTime(now));
        this.setProperty("LAST-MODIFIED", NewCalDateTime(now));
        this.setProperty("DTSTAMP", NewCalDateTime(now));

        this.mAttendees = [];

        this.mRecurrenceInfo = null;

        this.mAttachments = null;
    },

    // for subclasses to use; copies the ItemBase's values
    // into m. aNewParent is optional
    cloneItemBaseInto: function (m, aNewParent) {
        this.ensureNotDirty();

        m.mImmutable = false;
        m.mIsProxy = this.mIsProxy;
        m.mParentItem = aNewParent || this.mParentItem;

        m.mCalendar = this.mCalendar;
        if (this.mRecurrenceInfo) {
            m.mRecurrenceInfo = this.mRecurrenceInfo.clone();
            m.mRecurrenceInfo.item = m;
        }

        m.mAttendees = [];
        for (var i = 0; i < this.mAttendees.length; i++)
            m.mAttendees[i] = this.mAttendees[i].clone();

        m.mProperties = Components.classes["@mozilla.org/hash-property-bag;1"].
                        createInstance(Components.interfaces.nsIWritablePropertyBag);

        var e = this.mProperties.enumerator;
        while (e.hasMoreElements()) {
            var prop = e.getNext().QueryInterface(Components.interfaces.nsIProperty);
            var val = prop.value;

            if (prop.value instanceof Components.interfaces.calIDateTime)
                val = prop.value.clone();

            m.mProperties.setProperty (prop.name, val);
        }

        m.mDirty = false;

        // these need fixing
        m.mAttachments = this.mAttachments;
        return m;
    },

    get lastModifiedTime() {
        this.ensureNotDirty();
        return this.getProperty("LAST-MODIFIED");
    },

    get stampTime() {
        var prop = this.getProperty("DTSTAMP");
        if (prop && prop.isValid)
            return prop;
        return this.getProperty("LAST-MODIFIED");
    },

    updateStampTime: function() {
        // can't update the stamp time on an immutable event
        if (this.mImmutable)
            return;

        this.modify();
        this.setProperty("DTSTAMP", NewCalDateTime(new Date()));
    },

    get unproxiedPropertyEnumerator() {
        return this.mProperties.enumerator;
    },

    get propertyEnumerator() {
        if (this.mIsProxy) {
            // nsISimpleEnumerator sucks.  It really, really sucks.
            // The interface is badly defined, it's not clear
            // what happens if you just keep calling getNext() without
            // calling hasMoreElements in between, which seems like more
            // of an informational thing.  An interface with
            // "advance()" which returns true or false, and with "item()",
            // which returns the item the enumerator is pointing to, makes
            // far more sense.  Right now we have getNext() doing both
            // item returning and enumerator advancing, which makes
            // no sense.
            return {
                firstEnumerator: this.mProperties.eumerator,
                secondEnumerator: this.mParentItem.propertyEnumerator,
                handledProperties: { },

                currentItem: null,

                QueryInterface: function(aIID) {
                    if (!aIID.equals(Components.interfaces.nsISimpleEnumerator) ||
                        !aIID.equals(Components.interfaces.nsISupports))
                    {
                        throw Components.results.NS_ERROR_NO_INTERFACE;
                    }
                    return this;
                },

                hasMoreElements: function() {
                    if (!this.secondEnumerator)
                        return false;

                    if (this.firstEnumerator) {
                        var moreFirst = this.firstEnumerator.hasMoreElements();
                        if (moreFirst) {
                            this.currentItem = this.firstEnumerator.getNext();
                            this.handledProperties[this.currentItem.name] = true;
                            return true;
                        }
                        this.firstEnumerator = null;
                    }

                    var moreSecond = this.secondEnumerator.hasMoreElements();
                    if (moreSecond) {
                        while (this.currentItem.name in this.handledProperties &&
                               this.secondEnumerator.hasMoreElements())
                        do {
                            this.currentItem = this.secondEnumerator.getNext();
                        } while (this.currentItem.name in this.handledProperties &&
                                 ((this.currentItem = null) == null) && // hack
                                 this.secondEnumerator.hasMoreElements());

                        if (!this.currentItem)
                            return false;

                        return true;
                    }

                    this.secondEnumerator = null;

                    return false;
                },

                getNext: function() {
                    if (!currentItem)
                        throw Components.results.NS_ERROR_UNEXPECTED;

                    var rval = this.currentItem;
                    this.currentItem = null;
                    return rval;
                }
            };
        } else {
            return this.mProperties.enumerator;
        }
    },

    getProperty: function (aName) {
        try {
            return this.mProperties.getProperty(aName);
        } catch (e) {
            try {
                if (this.mIsProxy) {
                    return this.mParentItem.getProperty(aName);
                }
            } catch (e) {}

            return null;
        }
    },

    getUnproxiedProperty: function (aName) {
        try {
            return this.mProperties.getProperty(aName);
        } catch (e) { }
        return null;
    },

    hasProperty: function (aName) {
        return (this.getProperty(aName) != null);
    },

    setProperty: function (aName, aValue) {
        this.modify();
        this.mProperties.setProperty(aName, aValue);
    },

    deleteProperty: function (aName) {
        this.modify();
        try {
            this.mProperties.deleteProperty(aName);
        } catch (e) { }
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

    get calendar () {
        return this.mCalendar;
    },

    set calendar (v) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_OBJECT_IS_IMMUTABLE;
        this.mCalendar = v;
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
        "RECURRENCE-ID": true,
        "ALARMTIME": true,
    },

    icsBasePropMap: [
    { cal: "CREATED", ics: "createdTime" },
    { cal: "LAST-MODIFIED", ics: "lastModified" },
    { cal: "DTSTAMP", ics: "stampTime" },
    { cal: "UID", ics: "uid" },
    { cal: "SUMMARY", ics: "summary" },
    { cal: "PRIORITY", ics: "priority" },
    { cal: "STATUS", ics: "status" },
    { cal: "CLASS", ics: "icalClass" } ],

    mapPropsFromICS: function(icalcomp, propmap) {
        for (var i = 0; i < propmap.length; i++) {
            var prop = propmap[i];
            var val = icalcomp[prop.ics];
            if (val != null && val != ICAL.INVALID_VALUE)
                this.setProperty(prop.cal, val);
        }
    },

    mapPropsToICS: function(icalcomp, propmap) {
        for (var i = 0; i < propmap.length; i++) {
            var prop = propmap[i];
            var val = this.getProperty(prop.cal);
            if (val != null && val != ICAL.INVALID_VALUE)
                icalcomp[prop.ics] = val;
        }
    },

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
                rec.item = this;
            }

            rec.appendRecurrenceItem(ritem);
        }
        this.mRecurrenceInfo = rec;

        var alarmComp = icalcomp.getFirstSubcomponent("VALARM");
        if (alarmComp) {
            var triggerProp = alarmComp.getFirstProperty("TRIGGER");
            var duration = Components.classes["@mozilla.org/calendar/duration;1"]
                                     .createInstance(Components.interfaces.calIDuration);
            duration.icalString = triggerProp.stringValue;

            if (duration.minutes) {
                this.setProperty("alarmLength", duration.minutes);
                this.setProperty("alarmUnits", "minutes");
            } else if (duration.hours) {
                this.setProperty("alarmLength", duration.hours);
                this.setProperty("alarmUnits", "hours");
            } else if (duration.days) {
                this.setProperty("alarmLength", duration.days);
                this.setProperty("alarmUnits", "days");
            }

            var related = triggerProp.getParameter("RELATED");
            if (related && related == "END")
                this.setProperty("alarmRelated", "END");
            else
                this.setProperty("alarmRelated", "START");

            var email = alarmComp.getFirstProperty("X-EMAILADDRESS");
            if (email)
                this.setProperty("alarmEmailAddress", email);
        }
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

    isPropertyPromoted: function (name) {
        return (this.itemBasePromotedProps[name]);
    },

    get icalComponent() {
        throw Components.results.NS_NOT_IMPLEMENTED;
    },

    fillIcalComponentFromBase: function (icalcomp) {
        // Make sure that the LMT and ST are updated
        this.updateStampTime();
        this.ensureNotDirty();

        this.mapPropsToICS(icalcomp, this.icsBasePropMap);

        if (this.mOrganizer)
            icalcomp.addProperty(this.mOrganizer.icalProperty);
        for (var i = 0; i < this.mAttendees.length; i++)
            icalcomp.addProperty(this.mAttendees[i].icalProperty);

        if (this.mGeneration) {
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
        
        if (this.alarmTime) {
            const icssvc = Components.classes["@mozilla.org/calendar/ics-service;1"]
                                     .getService(Components.interfaces.calIICSService);
            var alarmComp = icssvc.createIcalComponent("VALARM");

            var duration = Components.classes["@mozilla.org/calendar/duration;1"]
                                     .createInstance(Components.interfaces.calIDuration);
            duration.isNegative = true;
            duration[this.getProperty("alarmUnits")] = this.getProperty("alarmLength");

            var triggerProp = icssvc.createIcalProperty("TRIGGER");
            triggerProp.stringValue = duration.icalString;

            if (this.getProperty("alarmRelated") == "END") 
                triggerProp.setParameter("RELATED", "END");

            alarmComp.addProperty(triggerProp);

            if (this.getProperty("alarmEmailAddress")) {
                var emailProp = icssvc.createIcalProperty("X-EMAILADDRESS");
                emailProp.stringValue = this.getProperty("alarmEmailAddress");
                alarmComp.addProperty(emailProp);
            }

            icalcomp.addSubcomponent(alarmComp);
        }
    },
    
    getOccurrencesBetween: function(aStartDate, aEndDate, aCount) {
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
};

makeMemberAttr(calItemBase, "X-MOZILLA-GENERATION", 0, "generation", true);
makeMemberAttr(calItemBase, "CREATED", null, "creationDate", true);
makeMemberAttr(calItemBase, "UID", null, "id", true);
makeMemberAttr(calItemBase, "SUMMARY", null, "title", true);
makeMemberAttr(calItemBase, "PRIORITY", 0, "priority", true);
makeMemberAttr(calItemBase, "CLASS", "PUBLIC", "privacy", true);
makeMemberAttr(calItemBase, "STATUS", null, "status", true);
makeMemberAttr(calItemBase, "ALARMTIME", null, "alarmTime", true);
makeMemberAttr(calItemBase, "RECURRENCE-ID", null, "recurrenceId", true);

makeMemberAttr(calItemBase, "mRecurrenceInfo", null, "recurrenceInfo");
makeMemberAttr(calItemBase, "mAttachments", null, "attachments");
makeMemberAttr(calItemBase, "mProperties", null, "properties");

function makeMemberAttr(ctor, varname, dflt, attr, asProperty)
{
    // XXX handle defaults!
    var getter = function () {
        if (asProperty)
            return this.getProperty(varname);
        else
            return this[varname];
    };
    var setter = function (v) {
        this.modify();
        if (asProperty)
            this.setProperty(varname, v);
        else
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
