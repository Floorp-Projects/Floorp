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

        return this.__proto__.QueryInterface(aIID);
    },

    initEvent: function () {
        this.mStartDate = createCalDateTime();
        this.mEndDate = createCalDateTime();
        this.mStampDate = createCalDateTime();
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
        var dur = createCalDateTime();
        dur.setTimeInTimezone (this.mEndDate.nativeTime - this.mStartDate.nativeTime, null);
        return dur;
    },






    mStartDate: null, get startDate() { return this.mStartDate; }, set startDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mStartDate = v; },
    mEndDate: null, get endDate() { return this.mEndDate; }, set endDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mEndDate = v; },
    mStampDate: null, get stampDate() { return this.mStampDate; }, set stampDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mStampDate = v; }


};
