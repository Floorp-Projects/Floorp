/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "nssProxy",
                                   "@mozilla.org/networkstatsServiceProxy;1",
                                   "nsINetworkStatsServiceProxy");

add_test(function test_saveAppStats() {
  var cachedAppStats = NetworkStatsService.cachedAppStats;
  var timestamp = NetworkStatsService.cachedAppStatsDate.getTime();
  var samples = 5;

  do_check_eq(Object.keys(cachedAppStats).length, 0);

  for (var i = 0; i < samples; i++) {
    nssProxy.saveAppStats(1, Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
                          timestamp, 10, 20);

    nssProxy.saveAppStats(1, Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
                          timestamp, 10, 20);
  }

  var key1 = 1 + 'wifi';
  var key2 = 1 + 'mobile';

  do_check_eq(Object.keys(cachedAppStats).length, 2);
  do_check_eq(cachedAppStats[key1].appId, 1);
  do_check_eq(cachedAppStats[key1].connectionType, 'wifi');
  do_check_eq(new Date(cachedAppStats[key1].date).getTime() / 1000,
              Math.floor(timestamp / 1000));
  do_check_eq(cachedAppStats[key1].rxBytes, 50);
  do_check_eq(cachedAppStats[key1].txBytes, 100);
  do_check_eq(cachedAppStats[key2].appId, 1);
  do_check_eq(cachedAppStats[key2].connectionType, 'mobile');
  do_check_eq(new Date(cachedAppStats[key2].date).getTime() / 1000,
              Math.floor(timestamp / 1000));
  do_check_eq(cachedAppStats[key2].rxBytes, 50);
  do_check_eq(cachedAppStats[key2].txBytes, 100);

  run_next_test();
});

add_test(function test_saveAppStatsWithDifferentDates() {
  var today = NetworkStatsService.cachedAppStatsDate;
  var tomorrow = new Date(today.getTime() + (24 * 60 * 60 * 1000));
  var key = 1 + 'wifi';

  NetworkStatsService.updateCachedAppStats(
    function (success, msg) {
      do_check_eq(success, true);

      do_check_eq(Object.keys(NetworkStatsService.cachedAppStats).length, 0);

      nssProxy.saveAppStats(1, Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
                            today.getTime(), 10, 20);

      nssProxy.saveAppStats(1, Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
                            today.getTime(), 10, 20);

      var saveAppStatsCb = {
        notify: function notify(success, message) {
          do_check_eq(success, true);

          var cachedAppStats = NetworkStatsService.cachedAppStats;
          var key = 2 + 'mobile';
          do_check_eq(Object.keys(cachedAppStats).length, 1);
          do_check_eq(cachedAppStats[key].appId, 2);
          do_check_eq(cachedAppStats[key].connectionType, 'mobile');
          do_check_eq(new Date(cachedAppStats[key].date).getTime() / 1000,
                      Math.floor(tomorrow.getTime() / 1000));
          do_check_eq(cachedAppStats[key].rxBytes, 30);
          do_check_eq(cachedAppStats[key].txBytes, 40);

          run_next_test();
        }
      };

      nssProxy.saveAppStats(2, Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
                            tomorrow.getTime(), 30, 40, saveAppStatsCb);
    }
  );
});

add_test(function test_saveAppStatsWithMaxCachedTraffic() {
  var timestamp = NetworkStatsService.cachedAppStatsDate.getTime();
  var maxtraffic = NetworkStatsService.maxCachedTraffic;

  NetworkStatsService.updateCachedAppStats(
    function (success, msg) {
      do_check_eq(success, true);

      var cachedAppStats = NetworkStatsService.cachedAppStats;
      do_check_eq(Object.keys(cachedAppStats).length, 0);

      nssProxy.saveAppStats(1, Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
                            timestamp, 10, 20);

      do_check_eq(Object.keys(cachedAppStats).length, 1);

      nssProxy.saveAppStats(1, Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
                            timestamp, maxtraffic, 20);

      do_check_eq(Object.keys(cachedAppStats).length, 0);

      run_next_test();
  });
});

function run_test() {
  do_get_profile();

  Cu.import("resource://gre/modules/NetworkStatsService.jsm");

  run_next_test();
}
