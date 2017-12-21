var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

var httpserver = null;
var geolocation = null;
var success = false;
var watchId = -1;

function terminate(succ) {
      success = succ;
      geolocation.clearWatch(watchID);
    }

function successCallback(pos){ terminate(true); }
function errorCallback(pos) { terminate(false); }

var observer = {
    QueryInterface: function(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIObserver))
        return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
    },

    observe: function(subject, topic, data) {
        if (data == "shutdown") {
            Assert.ok(1);
            this._numProviders--;
            if (!this._numProviders) {
                httpserver.stop(function() {
                        Assert.ok(success);
                        do_test_finished();
                    });
            }
        }
        else if (data == "starting") {
            Assert.ok(1);
            this._numProviders++;
        }
    },

    _numProviders: 0,
};

function geoHandler(metadata, response)
{
    var georesponse = {
        status: "OK",
        location: {
            lat: 42,
            lng: 42,
        },
        accuracy: 42,
    };
  var position = JSON.stringify(georesponse);
  response.setStatusLine("1.0", 200, "OK");
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "aplication/x-javascript", false);
  response.write(position);
}

function run_test()
{
    // XPCShell does not get a profile by default. The geolocation service
    // depends on the settings service which uses IndexedDB and IndexedDB
    // needs a place where it can store databases.
    do_get_profile();

    // only kill this test when shutdown is called on the provider.
    do_test_pending();

    httpserver = new HttpServer();
    httpserver.registerPathHandler("/geo", geoHandler);
    httpserver.start(-1);

    var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    prefs.setCharPref("geo.wifi.uri", "http://localhost:" +
                      httpserver.identity.primaryPort + "/geo");
    prefs.setBoolPref("dom.testing.ignore_ipc_principal", true);
    prefs.setBoolPref("geo.wifi.scan", false);

    var obs = Cc["@mozilla.org/observer-service;1"].getService();
    obs = obs.QueryInterface(Ci.nsIObserverService);
    obs.addObserver(observer, "geolocation-device-events");

    geolocation = Cc["@mozilla.org/geolocation;1"].getService(Ci.nsISupports);
    watchID = geolocation.watchPosition(successCallback, errorCallback);
}
