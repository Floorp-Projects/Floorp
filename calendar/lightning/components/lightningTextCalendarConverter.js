/* -*- Mode: javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

const CI = Components.interfaces;

function ltnMimeConverter() { }

ltnMimeConverter.prototype = {
    QueryInterface: function (aIID) {
        if (!aIID.equals(CI.nsISupports) &&
            !aIID.equals(CI.nsISimpleMimeConverter))
            throw Components.interfaces.NS_ERROR_NO_INTERFACE;

        return this;
    },

    convertToHTML: function(contentType, data) {
        dump("converting " + contentType + " to HTML\n");

        var event = Components.classes["@mozilla.org/calendar/event;1"]
            .createInstance(CI.calIEvent);
        event.icalString = data;

        var html = "<center><table bgcolor='#CCFFFF'>\n";
        var organizer = event.organizer;
        html += "<tr><td>Invitation from</td><td><a href='" +
          organizer.id + "'>" + organizer.commonName + "</a></td></tr>\n";
        html += "<tr><td>Subject:</td><td>" + event.title + "</td></tr>\n";
        html += "</table></center>";

        return html;
    }
};

var myModule = {
    registerSelf: function (compMgr, fileSpec, location, type) {
        debug("*** Registering Lightning text/calendar handler\n");
        compMgr = compMgr.QueryInterface(CI.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.myCID,
                                        "Lightning text/calendar handler",
                                        this.myContractID,
                                        fileSpec,
                                        location,
                                        type);

        var catman = Components.classes["@mozilla.org/categorymanager;1"]
            .getService(CI.nsICategoryManager);

        catman.addCategoryEntry("simple-mime-converters", "text/calendar",
                                this.myContractID, true, true);
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.myCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.myFactory;
    },

    myCID: Components.ID("{c70acb08-464e-4e55-899d-b2c84c5409fa}"),

    /* ProgID for this class */
    myContractID: "@mozilla.org/lightning/mime-converter;1",

    /* factory object */
    myFactory: {
        /*
         * Construct an instance of the interface specified by iid, possibly
         * aggregating it with the provided outer.  (If you don't know what
         * aggregation is all about, you don't need to.  It reduces even the
         * mightiest of XPCOM warriors to snivelling cowards.)
         */
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new ltnMimeConverter()).QueryInterface(iid);
        }
    },

    /*
     * The canUnload method signals that the component is about to be unloaded.
     * C++ components can return false to indicate that they don't wish to be
     * unloaded, but the return value from JS components' canUnload is ignored:
     * mark-and-sweep will keep everything around until it's no longer in use,
     * making unconditional ``unload'' safe.
     *
     * You still need to provide a (likely useless) canUnload method, though:
     * it's part of the nsIModule interface contract, and the JS loader _will_
     * call it.
     */
    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return myModule;
}


