/* GENERATED FILE; DO NOT EDIT. SOURCE IS calItemBase.js.pre */
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
        this.mCreationDate = createCalDateTime();
        this.mAlarmTime = createCalDateTime();

        this.mProperties = Components.classes["@mozilla.org/hash-property-bag;1"].
                           createInstance(Components.interfaces.nsIWritablePropertyBag);

        this.mAttendees = [];


        this.mRecurrenceInfo = null;
        this.mAttachments = null;
        this.mContacts = null;
    },



    cloneItemBaseInto: function (m) {
        m.mImmutable = false;
        m.mGeneration = this.mGeneration;
        m.mLastModifiedTime = this.mLastModifiedTime;
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
        m.mContacts = this.mContacts;



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
    mMethod: 0, get method() { return this.mMethod; }, set method(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mMethod = v; },
    mStatus: 0, get status() { return this.mStatus; }, set status(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mStatus = v; },
    mIcalString: "", get icalString() { return this.mIcalString; }, set icalString(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mIcalString = v; },
    mHasAlarm: false, get hasAlarm() { return this.mHasAlarm; }, set hasAlarm(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mHasAlarm = v; },
    mAlarmTime: null, get alarmTime() { return this.mAlarmTime; }, set alarmTime(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mAlarmTime = v; },
    mRecurrenceInfo: null, get recurrenceInfo() { return this.mRecurrenceInfo; }, set recurrenceInfo(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mRecurrenceInfo = v; },
    mAttachments: null, get attachments() { return this.mAttachments; }, set attachments(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mAttachments = v; },
    mContacts: null, get contacts() { return this.mContacts; }, set contacts(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mContacts = v; },
    mProperties: null, get properties() { return this.mProperties; }, set properties(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mProperties = v; },



    get propertyEnumerator() { return this.mProperties.enumerator; },
    getProperty: function (aName) {
        return this.mProperties.getProperty(aName);
    },
    setProperty: function (aName, aValue) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
        this.mProperties.setProperty(aName, aValue);
    },
    deleteProperty: function (aName) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
        this.mProperties.deleteProperty(aName);
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
    }
};

function calItemOccurrence () {
    this.wrappedJSObject = this;

    this.occurrenceStartDate = createCalDateTime();
    this.occurrenceEndDate = createCalDateTime();
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
    }
};





function createCalDateTime() {
    const kCdtClass = Components.classes["@mozilla.org/calendar/datetime;1"];
    const kCdtInterface = Components.interfaces.calIDateTime;

    return kCdtClass.createInstance(kCdtInterface);
}
