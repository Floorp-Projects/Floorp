/* GENERATED FILE; DO NOT EDIT. SOURCE IS calTodo.js.pre */
function calTodo() {
    this.wrappedJSObject = this;
    this.initItemBase();
    this.initTodo();
}


var calItemBase;

calTodo.prototype = {
    __proto__: calItemBase ? (new calItemBase()) : {},

    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIItemBase) &&
            !aIID.equals(Components.interfaces.calITodo))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    initTodo: function () {
        this.mEntryTime = createCalDateTime();
        this.mDueDate = createCalDateTime();
        this.mCompletedDate = createCalDateTime();
        this.mPercentComplete = 0;
    },

    clone: function () {
        var m = new calEvent();
        this.cloneItemBaseInto(m);
        m.mEntryDate = this.mEntryDate.clone();
        m.mDueDate = this.mDueDate.clone();
        m.mCompletedDate = this.mCompletedDate.clone();
        m.mPercentComplete = this.mPercentComplete;

        return m;
    },

    makeImmutable: function () {
        this.mEntryDate.makeImmutable();
        this.mDueDate.makeImmutable();
        this.mCompletedDate.makeImmutable();

        this.makeItemBaseImmutable();
    },






    mEntryDate: null, get entryDate() { return this.mEntryDate; }, set entryDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mEntryDate = v; },
    mEndDate: null, get endDate() { return this.mEndDate; }, set endDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mEndDate = v; },
    mCompletedDate: null, get completedDate() { return this.mCompletedDate; }, set completedDate(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mCompletedDate = v; },
    mPercentComplete: 0, get percentComplete() { return this.mPercentComplete; }, set percentComplete(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mPercentComplete = v; },


};
