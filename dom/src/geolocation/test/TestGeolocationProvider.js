Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Ci = Components.interfaces;
const Cc = Components.classes;

function GeoPositionCoordsObject() {
};

GeoPositionCoordsObject.prototype = {

    QueryInterface:   XPCOMUtils.generateQI([Ci.nsIDOMGeoPositionCoords, Ci.nsIClassInfo]),

    // Class Info is required to be able to pass objects back into the DOM.
    getInterfaces: function(countRef) {
        var interfaces = [Ci.nsIDOMGeoPositionCoords, Ci.nsIClassInfo, Ci.nsISupports];
        countRef.value = interfaces.length;
        return interfaces;
    },

    getHelperForLanguage: function(language) null,
    contractID: "",
    classDescription: "testing geolocation coords object",
    classID: null,
    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: Ci.nsIClassInfo.DOM_OBJECT,

    latitude: 37.41857,
    longitude: -122.08769,
    
    // some numbers that we can test against
    altitude: 42,
    accuracy: 42,
    altitudeAccuracy: 42,
    heading: 42,
    speed: 42,
};

function GeoPositionObject() {
};

GeoPositionObject.prototype = {

    QueryInterface:   XPCOMUtils.generateQI([Ci.nsIDOMGeoPosition, Ci.nsIClassInfo]),

    // Class Info is required to be able to pass objects back into the DOM.
    getInterfaces: function(countRef) {
        var interfaces = [Ci.nsIDOMGeoPosition, Ci.nsIClassInfo, Ci.nsISupports];
        countRef.value = interfaces.length;
        return interfaces;
    },

    getHelperForLanguage: function(language) null,
    contractID: "",
    classDescription: "test geolocation object",
    classID: null,
    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: Ci.nsIClassInfo.DOM_OBJECT,

    coords: new GeoPositionCoordsObject(),
    timestamp: Date.now(),

};

function MyLocation() {};
MyLocation.prototype = {
    classDescription: "A component that returns a geolocation",
    classID:          Components.ID("{10F622A4-6D7F-43A1-A938-5FFCBE2B1D1D}"),
    contractID:       "@mozilla.org/geolocation/provider;1",
    QueryInterface:   XPCOMUtils.generateQI([Ci.nsIGeolocationProvider]),
  
    geolocation:     null,
    timer:           null,
    timerInterval:   500,

    startup:         function() { 

        var observerService = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
        observerService.addObserver(this, "geolocation-test-control", false);
    
    },
    
    isReady:         function() {return true},
  
    watch:           function(c) {

                        var watchCallback = 
                        {
                            notify: function(timer) { this.parent.currentLocation.timestamp = Date.now();
                                                      c.update(this.parent.currentLocation); }
                        };

                        watchCallback.parent = this;

                        if(!this.timer) 
                           this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

                        this.timer.initWithCallback(watchCallback,
                                                    this.timerInterval, 
                                                    Ci.nsITimer.TYPE_REPEATING_SLACK);
    },

    currentLocation: new GeoPositionObject(),
    shutdown:        function() { 
                       if(this.timer)
                           this.timer.cancel();

                       var observerService = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
                       observerService.removeObserver(this, "geolocation-test-control");

    },

    observe: function (subject, topic, data) {

        if (topic == "geolocation-test-control") {
            if (data == "stop-responding")
            {
                if(this.timer) 
                    this.timer.cancel();
            }
            else if (data == "start-responding")
            {

                if(!this.timer) 
                    this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

                this.timer.initWithCallback(watchCallback,
                                            this.timerInterval, 
                                            Ci.nsITimer.TYPE_REPEATING_SLACK);
            }
        }
    },
};

var components = [MyLocation];
function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}
