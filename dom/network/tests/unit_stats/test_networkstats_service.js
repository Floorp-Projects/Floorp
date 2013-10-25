/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

add_test(function test_clearDB() {
  var networks = NetworkStatsService.availableNetworks();
  NetworkStatsService._db.clearStats(networks, function onDBCleared(error, result) {
    do_check_eq(result, null);
    run_next_test();
  });
});

function getNetworkId() {
  var network = (NetworkStatsService.availableNetworks())[0];
  return NetworkStatsService.getNetworkId(network.id, network.type);
}

add_test(function test_networkStatsAvailable_ok() {
  var netId = getNetworkId();
  NetworkStatsService.networkStatsAvailable(function (success, msg) {
    do_check_eq(success, true);
    run_next_test();
  }, netId, true, 1234, 4321, new Date());
});

add_test(function test_networkStatsAvailable_failure() {
  var netId = getNetworkId();
  NetworkStatsService.networkStatsAvailable(function (success, msg) {
    do_check_eq(success, false);
    run_next_test();
  }, netId, false, 1234, 4321, new Date());
});

add_test(function test_update_invalidNetwork() {
  NetworkStatsService.update(-1, function (success, msg) {
    do_check_eq(success, false);
    do_check_eq(msg, "Invalid network -1");
    run_next_test();
  });
});

add_test(function test_update() {
  var netId = getNetworkId();
  NetworkStatsService.update(netId, function (success, msg) {
    do_check_eq(success, true);
    run_next_test();
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
  var netId = getNetworkId();
  NetworkStatsService.updateStats(netId, function(success, msg){
    do_check_eq(success, true);
    run_next_test();
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

  run_next_test();
});

function run_test() {
  do_get_profile();

  Cu.import("resource://gre/modules/NetworkStatsService.jsm");
  run_next_test();
}
