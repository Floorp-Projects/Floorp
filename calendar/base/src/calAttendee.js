/* GENERATED FILE; DO NOT EDIT. SOURCE IS calAttendee.js.pre */
function calAttendee() {
    this.wrappedJSObject = this;
    this.mProperties = Components.classes["@mozilla.org/hash-property-bag;1"].
        createInstance(Components.interfaces.nsIWritablePropertyBag);
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
    mRsvp: null, get rsvp() { return this.mRsvp; }, set rsvp(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mRsvp = v; },
    mRole: null, get role() { return this.mRole; }, set role(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mRole = v; },
    mParticipationStatus: "NEEDSACTION", get participationStatus() { return this.mParticipationStatus; }, set participationStatus(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mParticipationStatus = v; },
    mUserType: "INDIVIDUAL", get userType() { return this.mUserType; }, set userType(v) { if (this.mImmutable) throw Components.results.NS_ERROR_FAILURE; else this.mUserType = v; },



    icalAttendeePropMap: [
    { cal: "rsvp", ics: "RSVP" },
    { cal: "commonName", ics: "CN" },
    { cal: "participationStatus", ics: "PARTSTAT" },
    { cal: "userType", ics: "CUTYPE" },
    { cal: "role", ics: "ROLE" } ],


    set icalProperty (icalatt) {
        if (icalatt.propertyName != "ATTENDEE")
            throw Components.results.NS_ERROR_INVALID_ARG;
        this.id = icalatt.stringValue;
        var promotedProps = { };
        for (var i = 0; i < this.icalAttendeePropMap.length; i++) {
            var prop = this.icalAttendeePropMap[i];
            this[prop.cal] = icalatt.getParameter(prop.ics);

            promotedProps[prop.ics] = true;
        }

        for (var paramname = icalatt.getFirstParameterName();
             paramname;
             paramname = icalatt.getNextParameterName()) {
            if (promotedProps[paramname])
                continue;
            this.setProperty(paramname, icalatt.getParameter(paramname));
        }
    },

    get icalProperty() {
        const icssvc =
            Components.classes["@mozilla.org/calendar/ics-service;1"].
                getService(Components.interfaces.calIICSService);
        var icalatt = icssvc.createIcalProperty("ATTENDEE");
        if (!this.id)
            throw Components.results.NS_ERROR_NOT_INITIALIZED;
        icalatt.stringValue = this.id;
        for (var i = 0; i < this.icalAttendeePropMap.length; i++) {
            var prop = this.icalAttendeePropMap[i];
            if (this[prop.cal])
                icalatt.setParameter(prop.ics, this[prop.cal]);
        }
        var bagenum = this.mProperties.enumerator;
        while (bagenum.hasMoreElements()) {
            var iprop = bagenum.getNext().
                QueryInterface(Components.interfaces.nsIProperty);
            icalatt.setParameter(iprop.name, iprop.value);
        }
        return icalatt;
    },

    get propertyEnumerator() { return this.mProperties.enumerator; },
    getProperty: function (aName) {
        try {
            return this.mProperties.getProperty(aName);
        } catch (e) {
            return null;
        }
    },
    setProperty: function (aName, aValue) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
        this.mProperties.setProperty(aName, aValue);
    },
    deleteProperty: function (aName) {
        if (this.mImmutable)
            throw Components.results.NS_ERROR_FAILURE;
        try {
            this.mProperties.deleteProperty(aName);
        } catch (e) {
        }
    },

};
