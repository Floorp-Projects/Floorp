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






    mGeneration: 0, get generation() { return this.mGeneration; }, set generation(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mGeneration = v; },
    mCreationDate: null, get creationDate() { return this.mCreationDate; }, set creationDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mCreationDate = v; },
    mLastModifiedTime: 0, get lastModifiedTime() { return this.mLastModifiedTime; }, set lastModifiedTime(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mLastModifiedTime = v; },
    mParent: null, get parent() { return this.mParent; }, set parent(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mParent = v; },
    mId: null, get id() { return this.mId; }, set id(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mId = v; },
    mTitle: "", get title() { return this.mTitle; }, set title(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mTitle = v; },
    mPriority: 0, get priority() { return this.mPriority; }, set priority(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mPriority = v; },
    mIsPrivate: 0, get isPrivate() { return this.mIsPrivate; }, set isPrivate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mIsPrivate = v; },
    mMethod: ICAL.INVALID_VALUE, get method() { return this.mMethod; }, set method(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mMethod = v; },
    mStatus: ICAL.INVALID_VALUE, get status() { return this.mStatus; }, set status(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mStatus = v; },
    mHasAlarm: false, get hasAlarm() { return this.mHasAlarm; }, set hasAlarm(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mHasAlarm = v; },
    mAlarmTime: null, get alarmTime() { return this.mAlarmTime; }, set alarmTime(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mAlarmTime = v; },
    mRecurrenceInfo: null, get recurrenceInfo() { return this.mRecurrenceInfo; }, set recurrenceInfo(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mRecurrenceInfo = v; },
    mAttachments: null, get attachments() { return this.mAttachments; }, set attachments(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mAttachments = v; },
    mProperties: null, get properties() { return this.mProperties; }, set properties(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mProperties = v; },



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

    mapPropsFromICS: function(icalcomp, propmap) {
        for (var i = 0; i < propmap.length; i++) {
            var prop = propmap[i];
            var val = icalcomp[prop.ics];
            if (val != ICAL.INVALID_VALUE)
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
    { cal: "mStatus", ics: "status" }],

    setItemBaseFromICS: function (icalcomp) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
        this.mapPropsFromICS(icalcomp, this.icsBasePropMap);

        if (icalcomp.icalClass == ICAL.VISIBILITY_PUBLIC)
            this.mIsPrivate = false;
        else
            this.mIsPrivate = true;
    },

    get icalComponent() {
        throw Components.results.NS_NOT_IMPLEMENTED;
    },

    fillIcalComponentFromBase: function (icalcomp) {
        this.mapPropsToICS(icalcomp, this.icsBasePropMap);

        if (this.mIsPrivate)
            icalcomp.icalClass = ICAL.VISIBILITY_PRIVATE;
        else
            icalcomp.icalClass = ICAL.VISIBILITY_PUBLIC;
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

function icalFromString(str) {
    const icssvc = Components.classes["@mozilla.org/calendar/ics-service;1"].
        getService(Components.interfaces.calIICSService);
    return icssvc.parseICS(str);
}
