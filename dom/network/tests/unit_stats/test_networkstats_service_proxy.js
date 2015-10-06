/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "nssProxy",
                                   "@mozilla.org/networkstatsServiceProxy;1",
                                   "nsINetworkStatsServiceProxy");

function mokConvertNetworkInfo() {
  NetworkStatsService.convertNetworkInfo = function(aNetworkInfo) {
    if (aNetworkInfo.type != Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE &&
        aNetworkInfo.type != Ci.nsINetworkInfo.NETWORK_TYPE_WIFI) {
      return null;
    }

    let id = '0';
    if (aNetworkInfo.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE) {
      id = '1234'
    }

    let netId = this.getNetworkId(id, aNetworkInfo.type);

    if (!this._networks[netId]) {
      this._networks[netId] = Object.create(null);
      this._networks[netId].network = { id: id,
                                        type: aNetworkInfo.type };
    }

    return netId;
  };
}

add_test(function test_saveAppStats() {
  var cachedStats = NetworkStatsService.cachedStats;
  var timestamp = NetworkStatsService.cachedStatsDate.getTime();

  // Create to fake nsINetworkInfos. As nsINetworkInfo can not be instantiated,
  // these two vars will emulate it by filling the properties that will be used.
  var wifi = {type: Ci.nsINetworkInfo.NETWORK_TYPE_WIFI, id: "0"};
  var mobile = {type: Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE, id: "1234"};

  // Insert fake mobile network info in NetworkStatsService
  var mobileNetId = NetworkStatsService.getNetworkId(mobile.id, mobile.type);

  do_check_eq(Object.keys(cachedStats).length, 0);

  nssProxy.saveAppStats(1, false, wifi, timestamp, 10, 20, false,
                        function (success, message) {
    do_check_eq(success, true);
    nssProxy.saveAppStats(1, false, mobile, timestamp, 10, 20, false,
                          function (success, message) {
      var key1 = 1 + "" + false + "" + NetworkStatsService.getNetworkId(wifi.id, wifi.type);
      var key2 = 1 + "" + false + "" + mobileNetId + "";

      do_check_eq(Object.keys(cachedStats).length, 2);
      do_check_eq(cachedStats[key1].appId, 1);
      do_check_eq(cachedStats[key1].isInBrowser, false);
      do_check_eq(cachedStats[key1].serviceType.length, 0);
      do_check_eq(cachedStats[key1].networkId, wifi.id);
      do_check_eq(cachedStats[key1].networkType, wifi.type);
      do_check_eq(new Date(cachedStats[key1].date).getTime() / 1000,
                  Math.floor(timestamp / 1000));
      do_check_eq(cachedStats[key1].rxBytes, 10);
      do_check_eq(cachedStats[key1].txBytes, 20);
      do_check_eq(cachedStats[key2].appId, 1);
      do_check_eq(cachedStats[key1].serviceType.length, 0);
      do_check_eq(cachedStats[key2].networkId, mobile.id);
      do_check_eq(cachedStats[key2].networkType, mobile.type);
      do_check_eq(new Date(cachedStats[key2].date).getTime() / 1000,
                  Math.floor(timestamp / 1000));
      do_check_eq(cachedStats[key2].rxBytes, 10);
      do_check_eq(cachedStats[key2].txBytes, 20);

      run_next_test();
    });
  });
});

add_test(function test_saveServiceStats() {
  var timestamp = NetworkStatsService.cachedStatsDate.getTime();

  // Create to fake nsINetworkInfos. As nsINetworkInfo can not be instantiated,
  // these two vars will emulate it by filling the properties that will be used.
  var wifi = {type: Ci.nsINetworkInfo.NETWORK_TYPE_WIFI, id: "0"};
  var mobile = {type: Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE, id: "1234"};

  // Insert fake mobile network info in NetworkStatsService
  var mobileNetId = NetworkStatsService.getNetworkId(mobile.id, mobile.type);

  NetworkStatsService.updateCachedStats(function (success, msg) {
    do_check_eq(success, true);

    var cachedStats = NetworkStatsService.cachedStats;
    do_check_eq(Object.keys(cachedStats).length, 0);

    var serviceType = 'FakeType';
    nssProxy.saveServiceStats(serviceType, wifi, timestamp, 10, 20, false,
                              function (success, message) {
      do_check_eq(success, true);
      nssProxy.saveServiceStats(serviceType, mobile, timestamp, 10, 20, false,
                                function (success, message) {
        do_check_eq(success, true);
        var key1 = 0 + "" + false + "" + serviceType +
                   NetworkStatsService.getNetworkId(wifi.id, wifi.type);
        var key2 = 0 + "" + false + "" + serviceType + mobileNetId + "";

        do_check_eq(Object.keys(cachedStats).length, 2);
        do_check_eq(cachedStats[key1].appId, 0);
        do_check_eq(cachedStats[key1].isInBrowser, false);
        do_check_eq(cachedStats[key1].serviceType, serviceType);
        do_check_eq(cachedStats[key1].networkId, wifi.id);
        do_check_eq(cachedStats[key1].networkType, wifi.type);
        do_check_eq(new Date(cachedStats[key1].date).getTime() / 1000,
                    Math.floor(timestamp / 1000));
        do_check_eq(cachedStats[key1].rxBytes, 10);
        do_check_eq(cachedStats[key1].txBytes, 20);
        do_check_eq(cachedStats[key2].appId, 0);
        do_check_eq(cachedStats[key1].serviceType, serviceType);
        do_check_eq(cachedStats[key2].networkId, mobile.id);
        do_check_eq(cachedStats[key2].networkType, mobile.type);
        do_check_eq(new Date(cachedStats[key2].date).getTime() / 1000,
                    Math.floor(timestamp / 1000));
        do_check_eq(cachedStats[key2].rxBytes, 10);
        do_check_eq(cachedStats[key2].txBytes, 20);

        run_next_test();
      });
    });
  });
});

add_test(function test_saveStatsWithDifferentDates() {
  var today = NetworkStatsService.cachedStatsDate;
  var tomorrow = new Date(today.getTime() + (24 * 60 * 60 * 1000));

  var mobile = {type: Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE, id: "1234"};

  NetworkStatsService.updateCachedStats(function (success, message) {
    do_check_eq(success, true);

    do_check_eq(Object.keys(NetworkStatsService.cachedStats).length, 0);
    nssProxy.saveAppStats(1, false, mobile, today.getTime(), 10, 20, false,
                          function (success, message) {
      do_check_eq(success, true);
      nssProxy.saveAppStats(2, false, mobile, tomorrow.getTime(), 30, 40, false,
                            function (success, message) {
        do_check_eq(success, true);

        var cachedStats = NetworkStatsService.cachedStats;
        var key = 2 + "" + false + "" +
                  NetworkStatsService.getNetworkId(mobile.id, mobile.type);
        do_check_eq(Object.keys(cachedStats).length, 1);
        do_check_eq(cachedStats[key].appId, 2);
        do_check_eq(cachedStats[key].isInBrowser, false);
        do_check_eq(cachedStats[key].networkId, mobile.id);
        do_check_eq(cachedStats[key].networkType, mobile.type);
        do_check_eq(new Date(cachedStats[key].date).getTime() / 1000,
                    Math.floor(tomorrow.getTime() / 1000));
        do_check_eq(cachedStats[key].rxBytes, 30);
        do_check_eq(cachedStats[key].txBytes, 40);

        run_next_test();
      });
    });
  });
});

add_test(function test_saveStatsWithMaxCachedTraffic() {
  var timestamp = NetworkStatsService.cachedStatsDate.getTime();
  var maxtraffic = NetworkStatsService.maxCachedTraffic;
  var wifi = {type: Ci.nsINetworkInfo.NETWORK_TYPE_WIFI, id: "0"};

  NetworkStatsService.updateCachedStats(function (success, message) {
    do_check_eq(success, true);

    var cachedStats = NetworkStatsService.cachedStats;
    do_check_eq(Object.keys(cachedStats).length, 0);
    nssProxy.saveAppStats(1, false, wifi, timestamp, 10, 20, false,
                          function (success, message) {
      do_check_eq(success, true);
      do_check_eq(Object.keys(cachedStats).length, 1);
      nssProxy.saveAppStats(1, false, wifi, timestamp, maxtraffic, 20, false,
                            function (success, message) {
        do_check_eq(success, true);
        do_check_eq(Object.keys(cachedStats).length, 0);

        run_next_test();
      });
    });
  });
});

add_test(function test_saveAppStats() {
  var cachedStats = NetworkStatsService.cachedStats;
  var timestamp = NetworkStatsService.cachedStatsDate.getTime();

  // Create to fake nsINetworkInfo. As nsINetworkInfo can not
  // be instantiated, these two vars will emulate it by filling the properties
  // that will be used.
  var wifi = {type: Ci.nsINetworkInfo.NETWORK_TYPE_WIFI, id: "0"};
  var mobile = {type: Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE, id: "1234"};

  // Insert fake mobile network interface in NetworkStatsService
  var mobileNetId = NetworkStatsService.getNetworkId(mobile.id, mobile.type);

  do_check_eq(Object.keys(cachedStats).length, 0);

  nssProxy.saveAppStats(1, false, wifi, timestamp, 10, 20, false, { notify:
                        function (success, message) {
    do_check_eq(success, true);
    var iterations = 10;
    var counter = 0;
    var callback = function (success, message) {
      if (counter == iterations - 1)
        run_next_test();
      counter++;
    };

    for (var i = 0; i < iterations; i++) {
      nssProxy.saveAppStats(1, false, mobile, timestamp, 10, 20, false, callback);
    }
  }});
});

function run_test() {
  do_get_profile();

  Cu.import("resource://gre/modules/NetworkStatsService.jsm");

  // Function convertNetworkInfo of NetworkStatsService causes errors when dealing
  // with RIL to get the iccid, so overwrite it.
  mokConvertNetworkInfo();

  run_next_test();
}
