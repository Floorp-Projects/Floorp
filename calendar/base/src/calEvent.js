/* GENERATED FILE; DO NOT EDIT. SOURCE IS calEvent.js.pre */
function calEvent() {
    this.wrappedJSObject = this;
    this.initItemBase();
    this.initEvent();
}


var calItemBase;

calEvent.prototype = {
    __proto__: calItemBase ? (new calItemBase()) : {},

    QueryInterface: function (aIID) {
        if (aIID.equals(Components.interfaces.calIEvent))
            return this;

        return this.__proto__.QueryInterface.apply(this, aIID);
    },

    initEvent: function () {
        this.mStartDate = new CalDateTime();
        this.mEndDate = new CalDateTime();
        this.mStampDate = new CalDateTime();
        this.mStampDate.jsDate = new Date();
    },

    clone: function () {
        var m = new calEvent();
        this.cloneItemBaseInto(m);
        m.mStartDate = this.mStartDate.clone();
        m.mEndDate = this.mEndDate.clone();
        m.mStampDate = this.mStampDate.clone();

        return m;
    },

    makeImmutable: function () {
        this.mStartDate.makeImmutable();
        this.mEndDate.makeImmutable();
        this.mStampDate.makeImmutable();

        this.makeItemBaseImmutable();
    },

    get duration() {
        var dur = new CalDateTime();
        dur.setTimeInTimezone (this.mEndDate.nativeTime - this.mStartDate.nativeTime, null);
        return dur;
    },






    mStartDate: null, get startDate() { return this.mStartDate; }, set startDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mStartDate = v; },
    mEndDate: null, get endDate() { return this.mEndDate; }, set endDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mEndDate = v; },
    mStampDate: null, get stampDate() { return this.mStampDate; }, set stampDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mStampDate = v; },
    mIsAllDay: false, get isAllDay() { return this.mIsAllDay; }, set isAllDay(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mIsAllDay = v; },



    icsEventPropMap: [
    { cal: "mStartDate", ics: "startTime" },
    { cal: "mEndDate", ics: "endTime" },
    { cal: "mStampDate", ics: "stampTime" } ],

    set icalString(value) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
        var ical = icalFromString(value);
        var event = ical.getFirstSubcomponent(Components.interfaces.calIIcalComponent.VEVENT_COMPONENT);
        if (!event)

            throw Components.results.NS_ERROR_INVALID_ARG;

        this.setItemBaseFromICS(event);
        this.mapPropsFromICS(event, this.icsEventPropMap);
        this.mIsAllDay = this.mStartDate && this.mStartDate.isDate;
    },

    get icalString() {
        const icssvc = Components.
          classes["@mozilla.org/calendar/ics-service;1"].
          getService(Components.interfaces.calIICSService);
        var calcomp = icssvc.createIcalComponent(ICAL.VCALENDAR_COMPONENT);
        calcomp.addSubcomponent(this.icalComponent);
        return calcomp.serializeToICS();
    },

    get icalComponent() {
        const icssvc = Components.
          classes["@mozilla.org/calendar/ics-service;1"].
          getService(Components.interfaces.calIICSService);
        var icalcomp = icssvc.createIcalComponent(ICAL.VEVENT_COMPONENT);
        this.fillIcalComponentFromBase(icalcomp);
        this.mapPropsToICS(icalcomp, this.icsEventPropMap);
        return icalcomp;
    },
};
