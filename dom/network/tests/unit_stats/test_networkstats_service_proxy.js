/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "nssProxy",
                                   "@mozilla.org/networkstatsServiceProxy;1",
                                   "nsINetworkStatsServiceProxy");

function mokConvertNetworkInterface() {
  NetworkStatsService.convertNetworkInterface = function(aNetwork) {
    if (aNetwork.type != Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE &&
        aNetwork.type != Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
      return null;
    }

    let id = '0';
    if (aNetwork.type == Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE) {
      id = '1234'
    }

    let netId = this.getNetworkId(id, aNetwork.type);

    if (!this._networks[netId]) {
      this._networks[netId] = Object.create(null);
      this._networks[netId].network = { id: id,
                                        type: aNetwork.type };
    }

    return netId;
  };
}

add_test(function test_saveAppStats() {
  var cachedAppStats = NetworkStatsService.cachedAppStats;
  var timestamp = NetworkStatsService.cachedAppStatsDate.getTime();
  var samples = 5;

  // Create to fake nsINetworkInterfaces. As nsINetworkInterface can not
  // be instantiated, these two vars will emulate it by filling the properties
  // that will be used.
  var wifi = {type: Ci.nsINetworkInterface.NETWORK_TYPE_WIFI, id: "0"};
  var mobile = {type: Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE, id: "1234"};

  // Insert fake mobile network interface in NetworkStatsService
  var mobileNetId = NetworkStatsService.getNetworkId(mobile.id, mobile.type);

  do_check_eq(Object.keys(cachedAppStats).length, 0);

  for (var i = 0; i < samples; i++) {
    nssProxy.saveAppStats(1, wifi, timestamp, 10, 20);

    nssProxy.saveAppStats(1, mobile, timestamp, 10, 20);
  }

  var key1 = 1 + NetworkStatsService.getNetworkId(wifi.id, wifi.type);
  var key2 = 1 + mobileNetId;

  do_check_eq(Object.keys(cachedAppStats).length, 2);
  do_check_eq(cachedAppStats[key1].appId, 1);
  do_check_eq(cachedAppStats[key1].networkId, wifi.id);
  do_check_eq(cachedAppStats[key1].networkType, wifi.type);
  do_check_eq(new Date(cachedAppStats[key1].date).getTime() / 1000,
              Math.floor(timestamp / 1000));
  do_check_eq(cachedAppStats[key1].rxBytes, 50);
  do_check_eq(cachedAppStats[key1].txBytes, 100);
  do_check_eq(cachedAppStats[key2].appId, 1);
  do_check_eq(cachedAppStats[key2].networkId, mobile.id);
  do_check_eq(cachedAppStats[key2].networkType, mobile.type);
  do_check_eq(new Date(cachedAppStats[key2].date).getTime() / 1000,
              Math.floor(timestamp / 1000));
  do_check_eq(cachedAppStats[key2].rxBytes, 50);
  do_check_eq(cachedAppStats[key2].txBytes, 100);

  run_next_test();
});

add_test(function test_saveAppStatsWithDifferentDates() {
  var today = NetworkStatsService.cachedAppStatsDate;
  var tomorrow = new Date(today.getTime() + (24 * 60 * 60 * 1000));

  var wifi = {type: Ci.nsINetworkInterface.NETWORK_TYPE_WIFI, id: "0"};
  var mobile = {type: Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE, id: "1234"};

  var key = 1 + NetworkStatsService.getNetworkId(wifi.id, wifi.type);

  NetworkStatsService.updateCachedAppStats(
    function (success, msg) {
      do_check_eq(success, true);

      do_check_eq(Object.keys(NetworkStatsService.cachedAppStats).length, 0);

      nssProxy.saveAppStats(1, wifi, today.getTime(), 10, 20);

      nssProxy.saveAppStats(1, mobile, today.getTime(), 10, 20);

      var saveAppStatsCb = {
        notify: function notify(success, message) {
          do_check_eq(success, true);

          var cachedAppStats = NetworkStatsService.cachedAppStats;
          var key = 2 + NetworkStatsService.getNetworkId(mobile.id, mobile.type);
          do_check_eq(Object.keys(cachedAppStats).length, 1);
          do_check_eq(cachedAppStats[key].appId, 2);
          do_check_eq(cachedAppStats[key].networkId, mobile.id);
          do_check_eq(cachedAppStats[key].networkType, mobile.type);
          do_check_eq(new Date(cachedAppStats[key].date).getTime() / 1000,
                      Math.floor(tomorrow.getTime() / 1000));
          do_check_eq(cachedAppStats[key].rxBytes, 30);
          do_check_eq(cachedAppStats[key].txBytes, 40);

          run_next_test();
        }
      };

      nssProxy.saveAppStats(2, mobile, tomorrow.getTime(), 30, 40, saveAppStatsCb);
    }
  );
});

add_test(function test_saveAppStatsWithMaxCachedTraffic() {
  var timestamp = NetworkStatsService.cachedAppStatsDate.getTime();
  var maxtraffic = NetworkStatsService.maxCachedTraffic;
  var wifi = {type: Ci.nsINetworkInterface.NETWORK_TYPE_WIFI, id: "0"};

  NetworkStatsService.updateCachedAppStats(
    function (success, msg) {
      do_check_eq(success, true);

      var cachedAppStats = NetworkStatsService.cachedAppStats;
      do_check_eq(Object.keys(cachedAppStats).length, 0);

      nssProxy.saveAppStats(1, wifi, timestamp, 10, 20);

      do_check_eq(Object.keys(cachedAppStats).length, 1);

      nssProxy.saveAppStats(1, wifi, timestamp, maxtraffic, 20);

      do_check_eq(Object.keys(cachedAppStats).length, 0);

      run_next_test();
  });
});

function run_test() {
  do_get_profile();

  Cu.import("resource://gre/modules/NetworkStatsService.jsm");

  // Function convertNetworkInterface of NetworkStatsService causes errors when dealing
  // with RIL to get the iccid, so overwrite it.
  mokConvertNetworkInterface();

  run_next_test();
}
