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


            }
        }
        return icalcomp;
    },

    set icalComponent(event) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
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
    }
};
