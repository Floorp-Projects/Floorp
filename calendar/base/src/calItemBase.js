/* GENERATED FILE; DO NOT EDIT. SOURCE IS calItemBase.js.pre */
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

    makeItemBaseImmutable: function() {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;


        if (this.mCreationDate)
            this.mCreationDate.makeImmutable();
        if (this.mRecurrenceInfo)
            this.mRecurrenceInfo.makeImmutable();
        if (this.mAlarmTime)
            this.mAlarmTime.makeImmutable();

        for (var i = 0; i < this.mAttendees.length; i++)
            this.mAttendees[i].makeImmutable();
        this.mImmutable = true;
    },


    initItemBase: function () {
        this.mCreationDate = new CalDateTime();
        this.mAlarmTime = new CalDateTime();
        this.mLastModifiedTime = new CalDateTime();

        this.mCreationDate.jsDate = new Date();
        this.mLastModifiedTime.jsDate = new Date();

        this.mProperties = Components.classes["@mozilla.org/hash-property-bag;1"].
                           createInstance(Components.interfaces.nsIWritablePropertyBag);

        this.mAttendees = [];


        this.mRecurrenceInfo = null;
        this.mAttachments = null;
    },



    cloneItemBaseInto: function (m) {
        m.mImmutable = false;
        m.mGeneration = this.mGeneration;
        m.mLastModifiedTime = this.mLastModifiedTime.clone();
        m.mParent = this.mParent;
        m.mId = this.mId;
        m.mTitle = this.mTitle;
        m.mPriority = this.mPriority;
        m.mIsPrivate = this.mIsPrivate;
        m.mMethod = this.mMethod;
        m.mStatus = this.mStatus;
        m.mHasAlarm = this.mHasAlarm;

        if (this.mRecurrenceInfo)
            m.mRecurrenceInfo = this.mRecurrenceInfo.clone();
        if (this.mCreationDate)
            m.mCreationDate = this.mCreationDate.clone();
        if (this.mAlarmTime)
            m.mAlarmTime = this.mAlarmTime.clone();


        m.mAttachments = this.mAttachments;



        return m;
    },






    mGeneration: 0, get generation() { return this.mGeneration; }, set generation(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mGeneration = v; dump("set " + "generation" + " to " + v + "\n");},
    mCreationDate: null, get creationDate() { return this.mCreationDate; }, set creationDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mCreationDate = v; dump("set " + "creationDate" + " to " + v + "\n");},
    mLastModifiedTime: null, get lastModifiedTime() { return this.mLastModifiedTime; }, set lastModifiedTime(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mLastModifiedTime = v; dump("set " + "lastModifiedTime" + " to " + v + "\n");},
    mParent: null, get parent() { return this.mParent; }, set parent(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mParent = v; dump("set " + "parent" + " to " + v + "\n");},
    mId: null, get id() { return this.mId; }, set id(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mId = v; dump("set " + "id" + " to " + v + "\n");},
    mTitle: "", get title() { return this.mTitle; }, set title(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mTitle = v; dump("set " + "title" + " to " + v + "\n");},
    mPriority: 0, get priority() { return this.mPriority; }, set priority(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mPriority = v; dump("set " + "priority" + " to " + v + "\n");},
    mPrivacy: "PUBLIC", get isPrivate() { return this.mPrivacy; }, set isPrivate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mPrivacy = v; dump("set " + "isPrivate" + " to " + v + "\n");},
    mStatus: null, get status() { return this.mStatus; }, set status(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mStatus = v; dump("set " + "status" + " to " + v + "\n");},
    mHasAlarm: false, get hasAlarm() { return this.mHasAlarm; }, set hasAlarm(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mHasAlarm = v; dump("set " + "hasAlarm" + " to " + v + "\n");},
    mAlarmTime: null, get alarmTime() { return this.mAlarmTime; }, set alarmTime(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mAlarmTime = v; dump("set " + "alarmTime" + " to " + v + "\n");},
    mRecurrenceInfo: null, get recurrenceInfo() { return this.mRecurrenceInfo; }, set recurrenceInfo(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mRecurrenceInfo = v; dump("set " + "recurrenceInfo" + " to " + v + "\n");},
    mAttachments: null, get attachments() { return this.mAttachments; }, set attachments(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mAttachments = v; dump("set " + "attachments" + " to " + v + "\n");},
    mProperties: null, get properties() { return this.mProperties; }, set properties(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mProperties = v; dump("set " + "properties" + " to " + v + "\n");},



    get propertyEnumerator() { return this.mProperties.enumerator; },
    getProperty: function (aName) {
        try {
            return this.mProperties.getProperty(aName);
        } catch (e) {
            return null;
        }
    },
    setProperty: function (aName, aValue) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
        this.mProperties.setProperty(aName, aValue);
    },
    deleteProperty: function (aName) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
        try {
            this.mProperties.deleteProperty(aName);
        } catch (e) {
        }
    },

    getAttendees: function (countObj) {
        countObj.value = this.mAttendees.length;
        return this.mAttendees.concat([]);
    },
    getAttendeeById: function (id) {
        for (var i = 0; i < this.mAttendees.length; i++)
            if (this.mAttendees[i].id == id)
                return this.mAttendees[i];
        return null;
    },
    removeAttendee: function (attendee) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
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
    addAttendee: function (attendee) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
        this.mAttendees.push(attendee);
    },


    get icalString() {
        throw Components.results.NS_NOT_IMPLEMENTED;
    },

    set icalString() {
        throw Components.results.NS_NOT_IMPLEMENTED;
    },

    itemBasePromotedProps: {
        "CREATED": true,
        "UID": true,
        "LASTMODIFIED": true,
        "SUMMARY": true,
        "PRIORITY": true,
        "METHOD": true,
        "STATUS": true,
        "CLASS": true,
        "DTALARM": true,
        "X-MOZILLA-GENERATION": true,
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
    { cal: "mId", ics: "uid" },
    { cal: "mTitle", ics: "summary" },
    { cal: "mPriority", ics: "priority" },
    { cal: "mMethod", ics: "method" },
    { cal: "mStatus", ics: "status" },
    { cal: "mPrivacy", ics: "icalClass" }],

    setItemBaseFromICS: function (icalcomp) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
        this.mapPropsFromICS(icalcomp, this.icsBasePropMap);
        this.mPrivacy = icalcomp.icalClass;
        if (icalcomp.icalClass == "PUBLIC")
            this.mIsPrivate = false;
        else
            this.mIsPrivate = true;

        for (var attprop = icalcomp.getFirstProperty("ATTENDEE");
             attprop;
             attprop = icalcomp.getNextProperty("ATTENDEE")) {

            var att = new CalAttendee();
            att.icalProperty = attprop;
            this.addAttendee(att);
        }

        var gen = icalcomp.getFirstProperty("X-MOZILLA-GENERATION");
        if (gen)
            this.mGeneration = parseInt(gen.stringValue);
    },

    importUnpromotedProperties: function (icalcomp, promoted) {
        for (var prop = icalcomp.getFirstProperty("ANY");
             prop;
             prop = icalcomp.getNextProperty("ANY")) {
            if (!promoted[prop.propertyName]) {

                this.setProperty(prop.propertyName, prop.stringValue);
            }
        }
    },

    get icalComponent() {
        throw Components.results.NS_NOT_IMPLEMENTED;
    },

    fillIcalComponentFromBase: function (icalcomp) {
        this.mapPropsToICS(icalcomp, this.icsBasePropMap);
        icalcomp.icalClass = this.mPrivacy;
        for (var i = 0; i < this.mAttendees.length; i++)
            icalcomp.addProperty(this.mAttendees[i].icalProperty);
        if (this.mGeneration != 0) {
            var genprop = icalProp("X-MOZILLA-GENERATION");
            genprop.stringValue = String(this.mGeneration);
            icalcomp.addProperty(genprop);
        }
    },

};

function calItemOccurrence () {
    this.wrappedJSObject = this;

    this.occurrenceStartDate = new CalDateTime();
    this.occurrenceEndDate = new CalDateTime();
}

calItemOccurrence.prototype = {
    QueryInterface: function (aIID) {
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

};





const CalDateTime =
    new Components.Constructor("@mozilla.org/calendar/datetime;1",
                               Components.interfaces.calIDateTime);

const CalAttendee =
    new Components.Constructor("@mozilla.org/calendar/attendee;1",
                               Components.interfaces.calIAttendee);

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
