function calEvent() {
    this.wrappedJSObject = this;
}

calEvent.prototype = {
    __proto__: (new calItemBase()),

    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIItemBase) &&
            !aIID.equals(Components.interfaces.calIEvent))
        {
            dump ("calEvent QI failed to " + aIID + "\n");
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    clone: function () {
        var m = new calEvent();
        this.cloneItemBaseInto(m);
        if (this.mStartDate)
            m.mStartDate = this.mStartDate.clone();
        if (this.mEndDate)
            m.mEndDate = this.mEndDate.clone();
        if (this.mStampDate)
            m.mStampDate = this.mStampDate.clone();

        return m;
    },

    makeImmutable: function () {
        if (this.mStartDate)
            this.mStartDate.makeImmutable();
        if (this.mEndDate)
            this.mEndDate.makeImmutable();
        if (this.mStampDate)
            this.mStampDate.makeImmutable();
        this.makeItemBaseImmutable();
    },






    mStartDate: null, get startDate() { return this.mStartDate; }, set startDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mStartDate = v; },
    mEndDate: null, get endDate() { return this.mEndDate; }, set endDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mEndDate = v; },
    mStampDate: null, get stampDate() { return this.mStampDate; }, set stampDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mStampDate = v; }


};
