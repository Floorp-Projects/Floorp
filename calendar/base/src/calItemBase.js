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

    getMutable: function () {
        var m = new calMutableItemBase();
        m.mGeneration = this.mGeneration;
        m.mCreationDate = this.mCreationDate.getMutable();
        m.mLastModifiedTime = this.mLastModifiedTime;
        m.mParent = this.mParent;
        m.mId = this.mId;
        m.mTitle = this.mTitle;
        m.mPriority = this.mPriority;
        m.mIsPrivate = this.mIsPrivate;
        m.mMethod = this.mMethod;
        m.mStatus = this.mStatus;
        m.mHasAlarm = this.mHasAlarm;
        m.mAlarmTime = this.mAlarmTime.getMutable();
        m.mRecurrenceInfo = this.mRecurrenceInfo.getMutable();


        m.mAttachments = this.mAttachments;
        m.mContacts = this.mContacts;
        m.mProperties = this.mProperties;

        return m;
    },





    mGeneration: 0, get generation() { return this.mGeneration; },
    mCreationDate: null, get creationDate() { return this.mCreationDate; },
    mLastModifiedTime: 0, get lastModifiedTime() { return this.mLastModifiedTime; },
    mParent: null, get parent() { return this.mParent; },
    mId: null, get id() { return this.mId; },
    mTitle: "", get title() { return this.mTitle; },
    mPriority: 0, get priority() { return this.mPriority; },
    mIsPrivate: 0, get isPrivate() { return this.mIsPrivate; },
    mMethod: 0, get method() { return this.mMethod; },
    mStatus: 0, get status() { return this.mStatus; },
    mIcalString: "", get icalString() { return this.mIcalString; },
    mHasAlarm: false, get hasAlarm() { return this.mHasAlarm; },
    mAlarmTime: null, get alarmTime() { return this.mAlarmTime; },
    mRecurrenceInfo: null, get recurrenceInfo() { return this.mRecurrenceInfo; },
    mAttachments: null, get attachments() { return this.mAttachments; },
    mContacts: null, get contacts() { return this.mContacts; },
    mProperties: null, get properties() { return this.mProperties; }


};

function calMutableItemBase() { }

calMutableItemBase.prototype = {
    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIMutableItemBase))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },






    mGeneration: 0, get generation() { return this.mGeneration; }, set generation(v) { this.mGeneration = v; },
    mCreationDate: null, get creationDate() { return this.mCreationDate; }, set creationDate(v) { this.mCreationDate = v; },
    mLastModifiedTime: 0, get lastModifiedTime() { return this.mLastModifiedTime; }, set lastModifiedTime(v) { this.mLastModifiedTime = v; },
    mParent: null, get parent() { return this.mParent; }, set parent(v) { this.mParent = v; },
    mId: null, get id() { return this.mId; }, set id(v) { this.mId = v; },
    mTitle: null, get title() { return this.mTitle; }, set title(v) { this.mTitle = v; },
    mPriority: 0, get priority() { return this.mPriority; }, set priority(v) { this.mPriority = v; },
    mIsPrivate: 0, get isPrivate() { return this.mIsPrivate; }, set isPrivate(v) { this.mIsPrivate = v; },
    mMethod: 0, get method() { return this.mMethod; }, set method(v) { this.mMethod = v; },
    mStatus: 0, get status() { return this.mStatus; }, set status(v) { this.mStatus = v; },
    mIcalString: "", get icalString() { return this.mIcalString; }, set icalString(v) { this.mIcalString = v; },
    mHasAlarm: false, get hasAlarm() { return this.mHasAlarm; }, set hasAlarm(v) { this.mHasAlarm = v; },
    mAlarmTime: null, get alarmTime() { return this.mAlarmTime; }, set alarmTime(v) { this.mAlarmTime = v; },
    mRecurrenceInfo: null, get recurrenceInfo() { return this.mRecurrenceInfo; }, set recurrenceInfo(v) { this.mRecurrenceInfo = v; },
    mAttachments: null, get attachments() { return this.mAttachments; }, set attachments(v) { this.mAttachments = v; },
    mContacts: null, get contacts() { return this.mContacts; }, set contacts(v) { this.mContacts = v; },
    mProperties: null, get properties() { return this.mProperties; }, set properties(v) { this.mProperties = v; }


};
