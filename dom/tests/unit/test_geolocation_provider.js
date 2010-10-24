
const Ci = Components.interfaces;
const Cc = Components.classes;

function successCallback(pos){}

var observer = {
    QueryInterface: function(iid) {
	if (iid.equals(Components.interfaces.nsISupports) ||
	    iid.equals(Components.interfaces.nsIObserver))
	    return this;
	throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    observe: function(subject, topic, data) {
	if (data == "shutdown") {
	    do_check_true(1)
	    do_test_finished();
	}
	else if (data == "starting") {
	    do_check_true(1)
	}

    },
};


function run_test()
{
    // only kill this test when shutdown is called on the provider.
    do_test_pending();

    var obs = Cc["@mozilla.org/observer-service;1"].getService();
    obs = obs.QueryInterface(Ci.nsIObserverService);
    obs.addObserver(observer, "geolocation-device-events", false); 

    var geolocation = Cc["@mozilla.org/geolocation;1"].getService(Ci.nsIDOMGeoGeolocation);
    var watchID = geolocation.watchPosition(successCallback);
    do_timeout(1000, function() { geolocation.clearWatch(watchID);})
}

