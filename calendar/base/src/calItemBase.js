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
    get isMutable() { return this. mImmutable; },
    makeImmutable: function() {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;


        this.mCreationDate.makeImmutable();
        this.mRecurrenceInfo.makeImmutable();
        this.mAlarmTime.makeImmutable();

        this.mImmutable = true;
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
        m.mRecurrenceInfo = this.mRecurrenceInfo.clone();

        m.mCreationDate = this.mCreationDate.clone();
        m.mAlarmTime = this.mAlarmTime.clone();


        m.mAttachments = this.mAttachments;
        m.mContacts = this.mContacts;
        m.mProperties = this.mProperties;

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
    mProperties: null, get properties() { return this.mProperties; }, set properties(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mProperties = v; }


};

function calItemOccurrence () {
    this.wrappedJSObject = this;
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

    item: null,
    occurrenceStartDate: null,
    occurrenceEndDate: null,

    next: null,
    previous: null
};
