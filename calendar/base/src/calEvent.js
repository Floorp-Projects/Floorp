function calEvent() {
    this.wrappedJSObject = this;
}

calEvent.prototype = {
    __proto__: (new calItemBase),

    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIItemBase) &&
            !aIID.equals(Components.interfaces.calIEvent))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },





    mStartDate: null, get startDate() { return this.mStartDate; },
    mEndDate: null, get endDate() { return this.mEndDate; },
    mStampDate: null, get stampDate() { return this.mStampDate; }


};

function calMutableEvent() {
    this.wrappedJSObject = this;
}

calMutableEvent.prototype = {
    __proto__: (new calMutableItemBase),

    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIMutableItemBase) &&
            !aIID.equals(Components.interfaces.calIMutableEvent))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },






    mStartDate: null, get startDate() { return this.mStartDate; }, set startDate(v) { this.mStartDate = v; },
    mEndDate: null, get endDate() { return this.mEndDate; }, set endDate(v) { this.mEndDate = v; },
    mStampDate: null, get stampDate() { return this.mStampDate; }, set stampDate(v) { this.mStampDate = v; }


};
