const Cc = Components.classes;
const Ci = Components.interfaces;

const providerCID = Components.ID("{14aa4b81-e266-45cb-88f8-89595dece114}");
const providerContract = "@mozilla.org/geolocation/unittest/geoprovider;1";

const categoryName = "geolocation-provider";

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
    if (enable) {
      this._seenHigh = true;
    } else {
      this._seenNotHigh = true;
    }
  },
  _seenHigh: false,
  _seenNotHigh: false
};

function run_test()
{
  Components.manager.nsIComponentRegistrar.registerFactory(providerCID,
    "Unit test geo provider", providerContract, provider);
  var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                         .getService(Components.interfaces.nsICategoryManager);
  catMan.nsICategoryManager.addCategoryEntry(categoryName, "unit test",
                                             providerContract, false, true);

  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  prefs.setBoolPref("geo.testing.ignore_ipc_principal", true);
  prefs.setBoolPref("geo.wifi.scan", false);
  run_test_in_child("test_geo_provider_accuracy.js", function() {
    do_check_true(provider._seenNotHigh);
    do_check_true(provider._seenHigh);
      do_test_finished();
  });
}