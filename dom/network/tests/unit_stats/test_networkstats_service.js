/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

function getNetworks(callback) {
  NetworkStatsService._db.getAvailableNetworks(function onGetNetworks(aError, aResult) {
    callback(aError, aResult);
  });
}

add_test(function test_clearDB() {
  getNetworks(function onGetNetworks(error, result) {
    do_check_eq(error, null);
    var networks = result;
    NetworkStatsService._db.clearStats(networks, function onDBCleared(error, result) {
      do_check_eq(result, null);
      run_next_test();
    });
  });
});

function getNetworkId(callback) {
  getNetworks(function onGetNetworks(error, result) {
    do_check_eq(error, null);
    var netId = NetworkStatsService.getNetworkId(result[0].id, result[0].type);
    callback(null, netId);
  });
}

add_test(function test_networkStatsAvailable_ok() {
  getNetworkId(function onGetId(error, result) {
    do_check_eq(error, null);
    var netId = result;
    NetworkStatsService.networkStatsAvailable(function (success, msg) {
      do_check_eq(success, true);
      run_next_test();
    }, netId, true, 1234, 4321, new Date());
  });
});

add_test(function test_networkStatsAvailable_failure() {
  getNetworkId(function onGetId(error, result) {
    do_check_eq(error, null);
    var netId = result;
    NetworkStatsService.networkStatsAvailable(function (success, msg) {
      do_check_eq(success, false);
      run_next_test();
    }, netId, false, 1234, 4321, new Date());
  });
});

add_test(function test_update_invalidNetwork() {
  NetworkStatsService.update(-1, function (success, msg) {
    do_check_eq(success, false);
    do_check_eq(msg, "Invalid network -1");
    run_next_test();
  });
});

add_test(function test_update() {
  getNetworkId(function onGetId(error, result) {
    do_check_eq(error, null);
    var netId = result;
    NetworkStatsService.update(netId, function (success, msg) {
      do_check_eq(success, true);
      run_next_test();
    });
  });
});

add_test(function test_updateQueueIndex() {
  NetworkStatsService.updateQueue = [{netId: 0, callbacks: null},
                                     {netId: 1, callbacks: null},
                                     {netId: 2, callbacks: null},
                                     {netId: 3, callbacks: null},
                                     {netId: 4, callbacks: null}];
  var index = NetworkStatsService.updateQueueIndex(3);
  do_check_eq(index, 3);
  index = NetworkStatsService.updateQueueIndex(10);
  do_check_eq(index, -1);

  NetworkStatsService.updateQueue = [];
  run_next_test();
});

add_test(function test_updateAllStats() {
  NetworkStatsService.updateAllStats(function(success, msg) {
    do_check_eq(success, true);
    run_next_test();
  });
});

add_test(function test_updateStats_ok() {
  getNetworkId(function onGetId(error, result) {
    do_check_eq(error, null);
    var netId = result;
    NetworkStatsService.updateStats(netId, function(success, msg){
      do_check_eq(success, true);
      run_next_test();
    });
  });
});

add_test(function test_updateStats_failure() {
  NetworkStatsService.updateStats(-1, function(success, msg){
    do_check_eq(success, false);
    run_next_test();
  });
});

add_test(function test_queue() {
  // Fill networks with fake network interfaces
  // to enable netd async requests
  var network = {id: "1234", type: Ci.nsIDOMMozNetworkStatsManager.MOBILE};
  var netId1 = NetworkStatsService.getNetworkId(network.id, network.type);
  NetworkStatsService._networks[netId1] = { network: network,
                                            interfaceName: "net1" };

  network = {id: "5678", type: Ci.nsIDOMMozNetworkStatsManager.MOBILE};
  var netId2 = NetworkStatsService.getNetworkId(network.id, network.type);
  NetworkStatsService._networks[netId2] = { network: network,
                                            interfaceName: "net2" };

  NetworkStatsService.updateStats(netId1);
  NetworkStatsService.updateStats(netId2);
  do_check_eq(NetworkStatsService.updateQueue.length, 2);
  do_check_eq(NetworkStatsService.updateQueue[0].callbacks.length, 1);

  var callback = function(success, msg) {
    return;
  };

  NetworkStatsService.updateStats(netId1, callback);
  NetworkStatsService.updateStats(netId2, callback);

  do_check_eq(NetworkStatsService.updateQueue.length, 2);
  do_check_eq(NetworkStatsService.updateQueue[0].callbacks.length, 2);
  do_check_eq(NetworkStatsService.updateQueue[0].callbacks[0], null);
  do_check_neq(NetworkStatsService.updateQueue[0].callbacks[1], null);

  // Clear queue because in test environment requests for mobile networks
  // can not be handled.
  NetworkStatsService.updateQueue =  [];
  run_next_test();
});

var wifiId = '00';

add_test(function test_updateThreshold() {
  let alarm = { networkId: wifiId, threshold: 10000 };

  NetworkStatsService._updateThreshold(alarm, function onSet(error, threshold){
    do_check_eq(error, null);
    do_check_neq(threshold.systemThreshold, undefined);
    do_check_neq(threshold.absoluteThreshold, undefined);
    run_next_test();
  });
});

var testPageURL = "http://test.com";
var testManifestURL = "http://test.com/manifest.webapp";

add_test(function test_setAlarm() {
  let alarm = { id: null,
                networkId: wifiId,
                threshold: 10000,
                absoluteThreshold: null,
                alarmStart: null,
                alarmEnd: null,
                data: null,
                pageURL: testPageURL,
                manifestURL: testManifestURL };

  NetworkStatsService._setAlarm(alarm, function onSet(error, result) {
    do_check_eq(result, 1);
    run_next_test();
  });
});

add_test(function test_setAlarm_invalid_threshold() {
  let alarm = { id: null,
                networkId: wifiId,
                threshold: -10000,
                absoluteThreshold: null,
                alarmStart: null,
                alarmEnd: null,
                data: null,
                pageURL: testPageURL,
                manifestURL: testManifestURL };

  NetworkStatsService._setAlarm(alarm, function onSet(error, result) {
    do_check_eq(error, "InvalidStateError");
    run_next_test();
  });
});

add_test(function test_fireAlarm() {
  // Add a fake alarm into database.
  let alarm = { id: null,
                networkId: wifiId,
                threshold: 10000,
                absoluteThreshold: null,
                alarmStart: null,
                alarmEnd: null,
                data: null,
                pageURL: testPageURL,
                manifestURL: testManifestURL };

  NetworkStatsService._db.addAlarm(alarm, function addSuccessCb(error, newId) {
    NetworkStatsService._db.getAlarms(Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
                                      testManifestURL, function onGet(error, result) {
      do_check_eq(error, null);
      do_check_eq(result.length, 1);

      // Result of getAlarms is based on expected child's data format, so
      // some changes are needed to be able to use it.
      result[0].networkId = wifiId;
      result[0].pageURL = testPageURL;
      result[0].manifestURL = testManifestURL;

      NetworkStatsService._fireAlarm(result[0], false);
      NetworkStatsService._db.getAlarms(Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
                                        testManifestURL, function onGet(error, result) {
        do_check_eq(error, undefined);
        do_check_eq(result.length, 0);
        run_next_test();
      });
    });
  });
});

function run_test() {
  do_get_profile();

  Cu.import("resource://gre/modules/NetworkStatsService.jsm");
  run_next_test();
}
