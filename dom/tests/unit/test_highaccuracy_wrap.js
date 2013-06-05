const Cc = Components.classes;
const Ci = Components.interfaces;

const providerCID = Components.ID("{14aa4b81-e266-45cb-88f8-89595dece114}");
const providerContract = "@mozilla.org/geolocation/provider;1";

var provider = {
  QueryInterface: function eventsink_qi(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIFactory) ||
        iid.equals(Components.interfaces.nsIGeolocationProvider))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  createInstance: function eventsink_ci(outer, iid) {
    if (outer)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },
  lockFactory: function eventsink_lockf(lock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  startup: function() {
  },
  watch: function(callback, isPrivate) {
    do_execute_soon(function() {
      callback.update({coords: {latitude: 42, longitude: 42}, timestamp: 0});
    });
  },
  shutdown: function() {
  },
  setHighAccuracy: function(enable) {
    this._status = enable;
  },
  _status: false
};

function run_test() {
  Components.manager.nsIComponentRegistrar.registerFactory(providerCID,
    "Unit test geo provider", providerContract, provider);
  var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                         .getService(Components.interfaces.nsICategoryManager);

  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  prefs.setBoolPref("geo.testing.ignore_ipc_principal", true);
  prefs.setBoolPref("geo.wifi.scan", false);

  run_test_in_child("test_highaccuracy.js", function() {
    do_check_false(provider._status);
    do_test_finished();
  });
}