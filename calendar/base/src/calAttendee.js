/* GENERATED FILE; DO NOT EDIT. SOURCE IS calAttendee.js.pre */
function calAttendee() {
    this.wrappedJSObject = this;
}

calAttendee.prototype = {
    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIAttendee))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    mImmutable: false,
    get isMutable() { return !this.mImmutable; },

    makeImmutable : function() {
 this.mImmutable = true;
    },

    clone: function() {
 var a = new calAttendee();
 var allProps = ["id", "commonName", "rsvp", "role", "participantStatus",
   "userType"];
 for (var i in allProps)
     a[allProps[i]] = this[allProps[i]];
 return a;
    },






    mId: null, get id() { return this.mId; }, set id(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mId = v; },
    mCommonName: null, get commonName() { return this.mCommonName; }, set commonName(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mCommonName = v; },
    mRsvp: false, get rsvp() { return this.mRsvp; }, set rsvp(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mRsvp = v; },
    mRole: Components.interfaces.calIAttendee.ROLE_OPT_PARTICIPANT, get role() { return this.mRole; }, set role(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mRole = v; },
    mRole: Components.interfaces.calIAttendee.ROLE_OPT_PARTICIPANT, get role() { return this.mRole; }, set role(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mRole = v; },
    mParticipationStatus: Components.interfaces.calIAttendee.PARTSTAT_NEEDSACTION, get participationStatus() { return this.mParticipationStatus; }, set participationStatus(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mParticipationStatus = v; },


    mUserType: Components.interfaces.calIAttendee.CUTYPE_INDIVIDUAL, get userType() { return this.mUserType; }, set userType(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mUserType = v; }



};
