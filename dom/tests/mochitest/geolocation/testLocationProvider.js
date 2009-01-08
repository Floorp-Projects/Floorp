Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Ci = Components.interfaces;
const Cc = Components.classes;

function GeopositionObject() {};
GeopositionObject.prototype = {

    QueryInterface:   XPCOMUtils.generateQI([Ci.nsIDOMGeoPosition, Ci.nsIClassInfo]),

    // Class Info is required to be able to pass objects back into the DOM.
    
    getInterfaces: function(countRef) {
        var interfaces = [Ci.nsIDOMGeoPosition, Ci.nsIClassInfo, Ci.nsISupports];
        countRef.value = interfaces.length;
        return interfaces;
    },
    getHelperForLanguage: function(language) null,
    contractID: "",
    classDescription: "geolocation object",
    classID: null,
    implementationLanguage: Ci.nsIProgrammingLanguage.JAVASCRIPT,
    flags: Ci.nsIClassInfo.DOM_OBJECT,

    latitude: 1,
    longitude: 1,
    altitude: 1,
    accuracy: 1,
    altitudeAccuracy: 1,
    heading: 1,
    speed: 1,
    timestamp: 1,
};

function dump(msg) {
    var consoleService = Components.classes["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService);
    consoleService.logStringMessage("mylocation: " + msg);
}

function MyLocation() {};
MyLocation.prototype = {
    classDescription: "A component that returns a geolocation",
    classID:          Components.ID("{10F622A4-6D7F-43A1-A938-5FFCBE2B1D1D}"),
    contractID:       "@mozilla.org/geolocation/provider;1",
    QueryInterface:   XPCOMUtils.generateQI([Ci.nsIGeolocationProvider]),
  
    geolocation:     null,
    timer:           null,
    timerInterval:   500,

    startup:         function() { dump("startup");},
    isReady:         function() { dump("isReady"); return true},
    watch:           function(c) { 
                        var watchCallback = {
                            notify: function(timer) {
	                            c.update(this.parent.currentLocation);
	                          }
                        };
                        watchCallback.parent = this;

                        if(!this.timer)
                           this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
                        this.timer.initWithCallback(watchCallback, this.timerInterval, 
                           Ci.nsITimer.TYPE_REPEATING_SLACK);
                     },

    shutdown:        function() { 
                       dump("shutdown"); 
                       if(this.timer)
                         this.timer.cancel();
                     },
};

var components = [MyLocation];
function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}

