/* Any: copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/NetworkStatsDB.jsm");

const netStatsDb = new NetworkStatsDB();

function clearStore(store, callback) {
  netStatsDb.dbNewTxn(store, "readwrite", function(aTxn, aStore) {
    aStore.openCursor().onsuccess = function (event) {
      let cursor = event.target.result;
      if (cursor){
        cursor.delete();
        cursor.continue();
      }
    };
  }, callback);
}

add_test(function prepareDatabase() {
  // Clear whole database to avoid starting tests with unknown state
  // due to the previous tests.
  clearStore('net_stats_store', function() {
    clearStore('net_alarm', function() {
      run_next_test();
    });
  });
});

function filterTimestamp(date) {
  var sampleRate = netStatsDb.sampleRate;
  var offset = date.getTimezoneOffset() * 60 * 1000;
  return Math.floor((date.getTime() - offset) / sampleRate) * sampleRate;
}

function getNetworks() {
  return [{ id: '0', type: Ci.nsIDOMMozNetworkStatsManager.WIFI },
          { id: '1234', type: Ci.nsIDOMMozNetworkStatsManager.MOBILE }];
}

function compareNetworks(networkA, networkB) {
  return (networkA[0] == networkB[0] && networkA[1] == networkB[1]);
}

add_test(function test_sampleRate() {
  var sampleRate = netStatsDb.sampleRate;
  do_check_true(sampleRate > 0);
  netStatsDb.sampleRate = 0;
  sampleRate = netStatsDb.sampleRate;
  do_check_true(sampleRate > 0);

  run_next_test();
});

add_test(function test_maxStorageSamples() {
  var maxStorageSamples = netStatsDb.maxStorageSamples;
  do_check_true(maxStorageSamples > 0);
  netStatsDb.maxStorageSamples = 0;
  maxStorageSamples = netStatsDb.maxStorageSamples;
  do_check_true(maxStorageSamples > 0);

  run_next_test();
});

add_test(function test_fillResultSamples_emptyData() {
  var samples = 3;
  var data = [];
  var start = filterTimestamp(new Date());
  var sampleRate = netStatsDb.sampleRate;
  var end = start + (sampleRate * samples);
  netStatsDb.fillResultSamples(start, end, data);
  do_check_eq(data.length, samples + 1);

  var aux = start;
  var success = true;
  for (var i = 0; i <= samples; i++) {
    if (data[i].date.getTime() != aux || data[i].rxBytes != undefined || data[i].txBytes != undefined) {
      success = false;
      break;
    }
    aux += sampleRate;
  }
  do_check_true(success);

  run_next_test();
});

add_test(function test_fillResultSamples_noEmptyData() {
  var samples = 3;
  var sampleRate = netStatsDb.sampleRate;
  var start = filterTimestamp(new Date());
  var end = start + (sampleRate * samples);
  var data = [{date: new Date(start + sampleRate),
               rxBytes: 0,
               txBytes: 0}];
  netStatsDb.fillResultSamples(start, end, data);
  do_check_eq(data.length, samples + 1);

  var aux = start;
  var success = true;
  for (var i = 0; i <= samples; i++) {
    if (i == 1) {
      if (data[i].date.getTime() != aux || data[i].rxBytes != 0 || data[i].txBytes != 0) {
        success = false;
        break;
      }
    } else {
      if (data[i].date.getTime() != aux || data[i].rxBytes != undefined || data[i].txBytes != undefined) {
        success = false;
        break;
      }
    }
    aux += sampleRate;
  }
  do_check_true(success);

  run_next_test();
});

add_test(function test_clear() {
  var networks = getNetworks();
  netStatsDb.clearStats(networks, function (error, result) {
    do_check_eq(error, null);
    run_next_test();
  });
});

add_test(function test_clear_interface() {
  var networks = getNetworks();
  netStatsDb.clearInterfaceStats(networks[0], function (error, result) {
    do_check_eq(error, null);
    run_next_test();
  });
});

add_test(function test_internalSaveStats_singleSample() {
  var networks = getNetworks();

  var stats = { appId:         0,
                serviceType:   "",
                network:       [networks[0].id, networks[0].type],
                timestamp:     Date.now(),
                rxBytes:       0,
                txBytes:       0,
                rxSystemBytes: 1234,
                txSystemBytes: 1234,
                rxTotalBytes:  1234,
                txTotalBytes:  1234 };

  netStatsDb.dbNewTxn("net_stats_store", "readwrite", function(txn, store) {
    netStatsDb._saveStats(txn, store, stats);
  }, function(error, result) {
    do_check_eq(error, null);

    netStatsDb.logAllRecords(function(error, result) {
      do_check_eq(error, null);
      do_check_eq(result.length, 1);
      do_check_eq(result[0].appId, stats.appId);
      do_check_eq(result[0].serviceType, stats.serviceType);
      do_check_true(compareNetworks(result[0].network, stats.network));
      do_check_eq(result[0].timestamp, stats.timestamp);
      do_check_eq(result[0].rxBytes, stats.rxBytes);
      do_check_eq(result[0].txBytes, stats.txBytes);
      do_check_eq(result[0].rxSystemBytes, stats.rxSystemBytes);
      do_check_eq(result[0].txSystemBytes, stats.txSystemBytes);
      do_check_eq(result[0].rxTotalBytes, stats.rxTotalBytes);
      do_check_eq(result[0].txTotalBytes, stats.txTotalBytes);
      run_next_test();
    });
  });
});

add_test(function test_internalSaveStats_arraySamples() {
  clearStore('net_stats_store', function() {
    var networks = getNetworks();
    var network = [networks[0].id, networks[0].type];

    var samples = 2;
    var stats = [];
    for (var i = 0; i < samples; i++) {
      stats.push({ appId:         0,
                   serviceType:   "",
                   network:       network,
                   timestamp:     Date.now() + (10 * i),
                   rxBytes:       0,
                   txBytes:       0,
                   rxSystemBytes: 1234,
                   txSystemBytes: 1234,
                   rxTotalBytes:  1234,
                   txTotalBytes:  1234 });
    }

    netStatsDb.dbNewTxn("net_stats_store", "readwrite", function(txn, store) {
      netStatsDb._saveStats(txn, store, stats);
    }, function(error, result) {
      do_check_eq(error, null);

      netStatsDb.logAllRecords(function(error, result) {
        do_check_eq(error, null);

        do_check_eq(result.length, samples);
        var success = true;
        for (var i = 0; i < samples; i++) {
          if (result[i].appId != stats[i].appId ||
              result[i].serviceType != stats[i].serviceType ||
              !compareNetworks(result[i].network, stats[i].network) ||
              result[i].timestamp != stats[i].timestamp ||
              result[i].rxBytes != stats[i].rxBytes ||
              result[i].txBytes != stats[i].txBytes ||
              result[i].rxSystemBytes != stats[i].rxSystemBytes ||
              result[i].txSystemBytes != stats[i].txSystemBytes ||
              result[i].rxTotalBytes != stats[i].rxTotalBytes ||
              result[i].txTotalBytes != stats[i].txTotalBytes) {
            success = false;
            break;
          }
        }
        do_check_true(success);
        run_next_test();
      });
    });
  });
});

add_test(function test_internalRemoveOldStats() {
  clearStore('net_stats_store', function() {
    var networks = getNetworks();
    var network = [networks[0].id, networks[0].type];
    var samples = 10;
    var stats = [];
    for (var i = 0; i < samples - 1; i++) {
      stats.push({ appId:               0, serviceType: "",
                   network:       network, timestamp:     Date.now() + (10 * i),
                   rxBytes:             0, txBytes:       0,
                   rxSystemBytes:    1234, txSystemBytes: 1234,
                   rxTotalBytes:     1234, txTotalBytes:  1234 });
    }

    stats.push({ appId:               0, serviceType: "",
                 network:       network, timestamp:     Date.now() + (10 * samples),
                 rxBytes:             0, txBytes:       0,
                 rxSystemBytes:    1234, txSystemBytes: 1234,
                 rxTotalBytes:     1234, txTotalBytes:  1234 });

    netStatsDb.dbNewTxn("net_stats_store", "readwrite", function(txn, store) {
      netStatsDb._saveStats(txn, store, stats);
      var date = stats[stats.length - 1].timestamp
                 + (netStatsDb.sampleRate * netStatsDb.maxStorageSamples - 1) - 1;
      netStatsDb._removeOldStats(txn, store, 0, "", network, date);
    }, function(error, result) {
      do_check_eq(error, null);

      netStatsDb.logAllRecords(function(error, result) {
        do_check_eq(error, null);
        do_check_eq(result.length, 1);

        run_next_test();
      });
    });
  });
});

function processSamplesDiff(networks, lastStat, newStat, callback) {
  clearStore('net_stats_store', function() {
    netStatsDb.dbNewTxn("net_stats_store", "readwrite", function(txn, store) {
      netStatsDb._saveStats(txn, store, lastStat);
    }, function(error, result) {
      netStatsDb.dbNewTxn("net_stats_store", "readwrite", function(txn, store) {
        let request = store.index("network").openCursor(newStat.network, "prev");
        request.onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          do_check_neq(cursor, null);
          netStatsDb._processSamplesDiff(txn, store, cursor, newStat, true);
        };
      }, function(error, result) {
        do_check_eq(error, null);
        netStatsDb.logAllRecords(function(error, result) {
          do_check_eq(error, null);
          callback(result);
        });
      });
    });
  });
}

add_test(function test_processSamplesDiffSameSample() {
  var networks = getNetworks();
  var network = [networks[0].id, networks[0].type];

  var sampleRate = netStatsDb.sampleRate;
  var date = filterTimestamp(new Date());

  var lastStat = { appId:               0, serviceType:   "",
                   network:       network, timestamp:     date,
                   rxBytes:             0, txBytes:       0,
                   rxSystemBytes:    1234, txSystemBytes: 1234,
                   rxTotalBytes:     2234, txTotalBytes:  2234 };

  var newStat = { appId:               0, serviceType:   "",
                  network:       network, timestamp:     date,
                  rxBytes:             0, txBytes:       0,
                  rxSystemBytes:    2234, txSystemBytes: 2234,
                  rxTotalBytes:     2234, txTotalBytes:  2234 };

  processSamplesDiff(networks, lastStat, newStat, function(result) {
    do_check_eq(result.length, 1);
    do_check_eq(result[0].appId, newStat.appId);
    do_check_eq(result[0].serviceType, newStat.serviceType);
    do_check_true(compareNetworks(result[0].network, newStat.network));
    do_check_eq(result[0].timestamp, newStat.timestamp);
    do_check_eq(result[0].rxBytes, newStat.rxSystemBytes - lastStat.rxSystemBytes);
    do_check_eq(result[0].txBytes, newStat.txSystemBytes - lastStat.txSystemBytes);
    do_check_eq(result[0].rxTotalBytes, lastStat.rxTotalBytes + newStat.rxSystemBytes - lastStat.rxSystemBytes);
    do_check_eq(result[0].txTotalBytes, lastStat.txTotalBytes + newStat.txSystemBytes - lastStat.txSystemBytes);
    do_check_eq(result[0].rxSystemBytes, newStat.rxSystemBytes);
    do_check_eq(result[0].txSystemBytes, newStat.txSystemBytes);
    run_next_test();
  });
});

add_test(function test_processSamplesDiffNextSample() {
  var networks = getNetworks();
  var network = [networks[0].id, networks[0].type];

  var sampleRate = netStatsDb.sampleRate;
  var date = filterTimestamp(new Date());

  var lastStat = { appId:               0, serviceType:  "",
                   network:       network, timestamp:     date,
                   rxBytes:             0, txBytes:       0,
                   rxSystemBytes:    1234, txSystemBytes: 1234,
                   rxTotalBytes:     2234, txTotalBytes:  2234 };

  var newStat = { appId:               0, serviceType:  "",
                  network:       network, timestamp:     date + sampleRate,
                  rxBytes:             0, txBytes:       0,
                  rxSystemBytes:    1734, txSystemBytes: 1734,
                  rxTotalBytes:        0, txTotalBytes:  0 };

  processSamplesDiff(networks, lastStat, newStat, function(result) {
    do_check_eq(result.length, 2);
    do_check_eq(result[1].appId, newStat.appId);
    do_check_eq(result[1].serviceType, newStat.serviceType);
    do_check_true(compareNetworks(result[1].network, newStat.network));
    do_check_eq(result[1].timestamp, newStat.timestamp);
    do_check_eq(result[1].rxBytes, newStat.rxSystemBytes - lastStat.rxSystemBytes);
    do_check_eq(result[1].txBytes, newStat.txSystemBytes - lastStat.txSystemBytes);
    do_check_eq(result[1].rxSystemBytes, newStat.rxSystemBytes);
    do_check_eq(result[1].txSystemBytes, newStat.txSystemBytes);
    do_check_eq(result[1].rxTotalBytes, lastStat.rxTotalBytes + newStat.rxSystemBytes - lastStat.rxSystemBytes);
    do_check_eq(result[1].txTotalBytes, lastStat.txTotalBytes + newStat.txSystemBytes - lastStat.txSystemBytes);
    run_next_test();
  });
});

add_test(function test_processSamplesDiffSamplesLost() {
  var networks = getNetworks();
  var network = [networks[0].id, networks[0].type];
  var samples = 5;
  var sampleRate = netStatsDb.sampleRate;
  var date = filterTimestamp(new Date());
  var lastStat = { appId:              0, serviceType:  "",
                   network:      network, timestamp:     date,
                   rxBytes:            0, txBytes:       0,
                   rxSystemBytes:   1234, txSystemBytes: 1234,
                   rxTotalBytes:    2234, txTotalBytes:  2234};

  var newStat = { appId:               0, serviceType:  "",
                  network:       network, timestamp:     date + (sampleRate * samples),
                  rxBytes:             0, txBytes:       0,
                  rxSystemBytes:    2234, txSystemBytes: 2234,
                  rxTotalBytes:        0, txTotalBytes:  0 };

  processSamplesDiff(networks, lastStat, newStat, function(result) {
    do_check_eq(result.length, samples + 1);
    do_check_eq(result[0].appId, newStat.appId);
    do_check_eq(result[0].serviceType, newStat.serviceType);
    do_check_true(compareNetworks(result[samples].network, newStat.network));
    do_check_eq(result[samples].timestamp, newStat.timestamp);
    do_check_eq(result[samples].rxBytes, newStat.rxTotalBytes - lastStat.rxTotalBytes);
    do_check_eq(result[samples].txBytes, newStat.txTotalBytes - lastStat.txTotalBytes);
    do_check_eq(result[samples].rxSystemBytes, newStat.rxSystemBytes);
    do_check_eq(result[samples].txSystemBytes, newStat.txSystemBytes);
    do_check_eq(result[samples].rxTotalBytes, lastStat.rxTotalBytes + newStat.rxSystemBytes - lastStat.rxSystemBytes);
    do_check_eq(result[samples].txTotalBytes, lastStat.txTotalBytes + newStat.txSystemBytes - lastStat.txSystemBytes);
    run_next_test();
  });
});

add_test(function test_saveStats() {
  var networks = getNetworks();
  var network = [networks[0].id, networks[0].type];

  var stats = { appId:          0,
                serviceType:    "",
                networkId:      networks[0].id,
                networkType:    networks[0].type,
                date:           new Date(),
                rxBytes:        2234,
                txBytes:        2234,
                isAccumulative: true };

  clearStore('net_stats_store', function() {
    netStatsDb.saveStats(stats, function(error, result) {
      do_check_eq(error, null);
      netStatsDb.logAllRecords(function(error, result) {
        do_check_eq(error, null);
        do_check_eq(result.length, 1);
        do_check_eq(result[0].appId, stats.appId);
        do_check_eq(result[0].serviceType, stats.serviceType);
        do_check_true(compareNetworks(result[0].network, network));
        let timestamp = filterTimestamp(stats.date);
        do_check_eq(result[0].timestamp, timestamp);
        do_check_eq(result[0].rxBytes, stats.rxBytes);
        do_check_eq(result[0].txBytes, stats.txBytes);
        do_check_eq(result[0].rxSystemBytes, stats.rxBytes);
        do_check_eq(result[0].txSystemBytes, stats.txBytes);
        do_check_eq(result[0].rxTotalBytes, stats.rxBytes);
        do_check_eq(result[0].txTotalBytes, stats.txBytes);
        run_next_test();
      });
    });
  });
});

add_test(function test_saveAppStats() {
  var networks = getNetworks();
  var network = [networks[0].id, networks[0].type];

  var stats = { appId:          1,
                serviceType:    "",
                networkId:      networks[0].id,
                networkType:    networks[0].type,
                date:           new Date(),
                rxBytes:        2234,
                txBytes:        2234,
                isAccumulative: false };

  clearStore('net_stats_store', function() {
    netStatsDb.saveStats(stats, function(error, result) {
      do_check_eq(error, null);
      netStatsDb.logAllRecords(function(error, result) {
        do_check_eq(error, null);
        do_check_eq(result.length, 1);
        do_check_eq(result[0].appId, stats.appId);
        do_check_eq(result[0].serviceType, stats.serviceType);
        do_check_true(compareNetworks(result[0].network, network));
        let timestamp = filterTimestamp(stats.date);
        do_check_eq(result[0].timestamp, timestamp);
        do_check_eq(result[0].rxBytes, stats.rxBytes);
        do_check_eq(result[0].txBytes, stats.txBytes);
        do_check_eq(result[0].rxSystemBytes, 0);
        do_check_eq(result[0].txSystemBytes, 0);
        do_check_eq(result[0].rxTotalBytes, 0);
        do_check_eq(result[0].txTotalBytes, 0);
        run_next_test();
      });
    });
  });
});

add_test(function test_saveServiceStats() {
  var networks = getNetworks();
  var network = [networks[0].id, networks[0].type];

  var stats = { appId:          0,
                serviceType:    "FakeType",
                networkId:      networks[0].id,
                networkType:    networks[0].type,
                date:           new Date(),
                rxBytes:        2234,
                txBytes:        2234,
                isAccumulative: false };

  clearStore('net_stats_store', function() {
    netStatsDb.saveStats(stats, function(error, result) {
      do_check_eq(error, null);
      netStatsDb.logAllRecords(function(error, result) {
        do_check_eq(error, null);
        do_check_eq(result.length, 1);
        do_check_eq(result[0].appId, stats.appId);
        do_check_eq(result[0].serviceType, stats.serviceType);
        do_check_true(compareNetworks(result[0].network, network));
        let timestamp = filterTimestamp(stats.date);
        do_check_eq(result[0].timestamp, timestamp);
        do_check_eq(result[0].rxBytes, stats.rxBytes);
        do_check_eq(result[0].txBytes, stats.txBytes);
        do_check_eq(result[0].rxSystemBytes, 0);
        do_check_eq(result[0].txSystemBytes, 0);
        do_check_eq(result[0].rxTotalBytes, 0);
        do_check_eq(result[0].txTotalBytes, 0);
        run_next_test();
      });
    });
  });
});

function prepareFind(stats, callback) {
  clearStore('net_stats_store', function() {
    netStatsDb.dbNewTxn("net_stats_store", "readwrite", function(txn, store) {
      netStatsDb._saveStats(txn, store, stats);
    }, function(error, result) {
        callback(error, result);
    });
  });
}

add_test(function test_find () {
  var networks = getNetworks();
  var networkWifi = [networks[0].id, networks[0].type];
  var networkMobile = [networks[1].id, networks[1].type]; // Fake mobile interface
  var appId = 0;
  var serviceType = "";

  var samples = 5;
  var sampleRate = netStatsDb.sampleRate;
  var start = Date.now();
  var saveDate = filterTimestamp(new Date());
  var end = new Date(start + (sampleRate * (samples - 1)));
  start = new Date(start - sampleRate);
  var stats = [];
  for (var i = 0; i < samples; i++) {
    stats.push({ appId:               appId, serviceType:   serviceType,
                 network:       networkWifi, timestamp:     saveDate + (sampleRate * i),
                 rxBytes:                 0, txBytes:       10,
                 rxSystemBytes:           0, txSystemBytes: 0,
                 rxTotalBytes:            0, txTotalBytes:  0 });


    stats.push({ appId:                 appId, serviceType:   serviceType,
                 network:       networkMobile, timestamp:     saveDate + (sampleRate * i),
                 rxBytes:                   0, txBytes:       10,
                 rxSystemBytes:             0, txSystemBytes: 0,
                 rxTotalBytes:              0, txTotalBytes:  0 });
  }

  prepareFind(stats, function(error, result) {
    do_check_eq(error, null);
    netStatsDb.find(function (error, result) {
      do_check_eq(error, null);
      do_check_eq(result.serviceType, serviceType);
      do_check_eq(result.network.id, networks[0].id);
      do_check_eq(result.network.type, networks[0].type);
      do_check_eq(result.start.getTime(), start.getTime());
      do_check_eq(result.end.getTime(), end.getTime());
      do_check_eq(result.data.length, samples + 1);
      do_check_eq(result.data[0].rxBytes, null);
      do_check_eq(result.data[1].rxBytes, 0);
      do_check_eq(result.data[samples].rxBytes, 0);
      run_next_test();
    }, appId, serviceType, networks[0], start, end);
  });
});

add_test(function test_findAppStats () {
  var networks = getNetworks();
  var networkWifi = [networks[0].id, networks[0].type];
  var networkMobile = [networks[1].id, networks[1].type]; // Fake mobile interface
  var appId = 1;
  var serviceType = "";

  var samples = 5;
  var sampleRate = netStatsDb.sampleRate;
  var start = Date.now();
  var saveDate = filterTimestamp(new Date());
  var end = new Date(start + (sampleRate * (samples - 1)));
  start = new Date(start - sampleRate);
  var stats = [];
  for (var i = 0; i < samples; i++) {
    stats.push({ appId:              appId, serviceType:  serviceType,
                 network:      networkWifi, timestamp:    saveDate + (sampleRate * i),
                 rxBytes:                0, txBytes:      10,
                 rxTotalBytes:           0, txTotalBytes: 0 });

    stats.push({ appId:                appId, serviceType:  serviceType,
                 network:      networkMobile, timestamp:    saveDate + (sampleRate * i),
                 rxBytes:                  0, txBytes:      10,
                 rxTotalBytes:             0, txTotalBytes: 0 });
  }

  prepareFind(stats, function(error, result) {
    do_check_eq(error, null);
    netStatsDb.find(function (error, result) {
      do_check_eq(error, null);
      do_check_eq(result.serviceType, serviceType);
      do_check_eq(result.network.id, networks[0].id);
      do_check_eq(result.network.type, networks[0].type);
      do_check_eq(result.start.getTime(), start.getTime());
      do_check_eq(result.end.getTime(), end.getTime());
      do_check_eq(result.data.length, samples + 1);
      do_check_eq(result.data[0].rxBytes, null);
      do_check_eq(result.data[1].rxBytes, 0);
      do_check_eq(result.data[samples].rxBytes, 0);
      run_next_test();
    }, appId, serviceType, networks[0], start, end);
  });
});

add_test(function test_findServiceStats () {
  var networks = getNetworks();
  var networkWifi = [networks[0].id, networks[0].type];
  var networkMobile = [networks[1].id, networks[1].type]; // Fake mobile interface
  var appId = 0;
  var serviceType = "FakeType";

  var samples = 5;
  var sampleRate = netStatsDb.sampleRate;
  var start = Date.now();
  var saveDate = filterTimestamp(new Date());
  var end = new Date(start + (sampleRate * (samples - 1)));
  start = new Date(start - sampleRate);
  var stats = [];
  for (var i = 0; i < samples; i++) {
    stats.push({ appId:              appId, serviceType:  serviceType,
                 network:      networkWifi, timestamp:    saveDate + (sampleRate * i),
                 rxBytes:                0, txBytes:      10,
                 rxTotalBytes:           0, txTotalBytes: 0 });

    stats.push({ appId:                appId, serviceType:  serviceType,
                 network:      networkMobile, timestamp:    saveDate + (sampleRate * i),
                 rxBytes:                  0, txBytes:      10,
                 rxTotalBytes:             0, txTotalBytes: 0 });
  }

  prepareFind(stats, function(error, result) {
    do_check_eq(error, null);
    netStatsDb.find(function (error, result) {
      do_check_eq(error, null);
      do_check_eq(result.serviceType, serviceType);
      do_check_eq(result.network.id, networks[0].id);
      do_check_eq(result.network.type, networks[0].type);
      do_check_eq(result.start.getTime(), start.getTime());
      do_check_eq(result.end.getTime(), end.getTime());
      do_check_eq(result.data.length, samples + 1);
      do_check_eq(result.data[0].rxBytes, null);
      do_check_eq(result.data[1].rxBytes, 0);
      do_check_eq(result.data[samples].rxBytes, 0);
      run_next_test();
    }, appId, serviceType, networks[0], start, end);
  });
});

add_test(function test_saveMultipleAppStats () {
  var networks = getNetworks();
  var networkWifi = networks[0];
  var networkMobile = networks[1]; // Fake mobile interface

  var saveDate = filterTimestamp(new Date());
  var cached = Object.create(null);
  var serviceType = "FakeType";
  var wifiNetId = networkWifi.id + '' + networkWifi.type;
  var mobileNetId = networkMobile.id + '' + networkMobile.type;

  cached[0 + '' + serviceType + wifiNetId] = {
    appId:                    0, date:           new Date(),
    networkId:   networkWifi.id, networkType:    networkWifi.type,
    rxBytes:                  0, txBytes:        10,
    serviceType:    serviceType, isAccumulative: false
  };

  cached[0 + '' + serviceType + mobileNetId] = {
    appId:                    0, date:           new Date(),
    networkId: networkMobile.id, networkType:    networkMobile.type,
    rxBytes:                  0, txBytes:        10,
    serviceType:    serviceType, isAccumulative: false
  };

  cached[1 + '' + wifiNetId] = {
    appId:                    1, date:           new Date(),
    networkId:   networkWifi.id, networkType:    networkWifi.type,
    rxBytes:                  0, txBytes:        10,
    serviceType:             "", isAccumulative: false
  };

  cached[1 + '' + mobileNetId] = {
    appId:                    1, date:           new Date(),
    networkId: networkMobile.id, networkType:    networkMobile.type,
    rxBytes:                  0, txBytes:        10,
    serviceType:             "", isAccumulative: false
  };

  cached[2 + '' + wifiNetId] = {
    appId:                    2, date:           new Date(),
    networkId:   networkWifi.id, networkType:    networkWifi.type,
    rxBytes:                  0, txBytes:        10,
    serviceType:             "", isAccumulative: false
  };

  cached[2 + '' + mobileNetId] = {
    appId:                    2, date:           new Date(),
    networkId: networkMobile.id, networkType:    networkMobile.type,
    rxBytes:                  0, txBytes:        10,
    serviceType:             "", isAccumulative: false
  };

  let keys = Object.keys(cached);
  let index = 0;

  networks.push(networkMobile);

  clearStore('net_stats_store', function() {
    netStatsDb.saveStats(cached[keys[index]],
      function callback(error, result) {
        do_check_eq(error, null);

        if (index == keys.length - 1) {
          netStatsDb.logAllRecords(function(error, result) {
            do_check_eq(error, null);
            do_check_eq(result.length, 6);
            do_check_eq(result[0].serviceType, serviceType);
            do_check_eq(result[3].appId, 1);
            do_check_true(compareNetworks(result[0].network, [networkWifi.id, networkWifi.type]));
            do_check_eq(result[0].rxBytes, 0);
            do_check_eq(result[0].txBytes, 10);
            run_next_test();
          });
         return;
        }

        index += 1;
        netStatsDb.saveStats(cached[keys[index]], callback);
    });
  });
});

var networkWifi = '00';
var networkMobile = '11';

var examplePageURL = "http://example.com/index.html";
var exampleManifestURL = "http://example.com/manifest.webapp";

var testPageURL = "http://test.com/index.html";
var testManifestURL = "http://test.com/manifest.webapp";

var alarms = [{ id:                null,
                networkId:         networkWifi,
                absoluteThreshold: 10000,
                relativeThreshold: 10000,
                data:              {foo: "something"},
                pageURL:           examplePageURL,
                manifestURL:       exampleManifestURL },
              { id:                null,
                networkId:         networkWifi,
                absoluteThreshold: 1000,
                relativeThreshold: 1000,
                data:              {foo: "else"},
                pageURL:           examplePageURL,
                manifestURL:       exampleManifestURL },
              { id:                null,
                networkId:         networkMobile,
                absoluteThreshold: 100,
                relativeThreshold: 100,
                data:              {foo: "to"},
                pageURL:           examplePageURL,
                manifestURL:       exampleManifestURL },
              { id:                null,
                networkId:         networkMobile,
                absoluteThreshold: 10,
                relativeThreshold: 10,
                data:              {foo: "test"},
                pageURL:           testPageURL,
                manifestURL:       testManifestURL }];

var alarmsDbId = 1;

add_test(function test_addAlarm() {
  // Add alarms[0] -> DB: [ alarms[0] (id: 1) ]
  // Check the insertion is OK.
  netStatsDb.addAlarm(alarms[0], function(error, result) {
    do_check_eq(error, null);
    alarmsDbId = result;
    netStatsDb.getAlarms(Ci.nsINetworkInterface.NETWORK_TYPE_WIFI, exampleManifestURL, function(error, result) {
      do_check_eq(error, null);
      do_check_eq(result.length, 1);
      do_check_eq(result[0].id, alarmsDbId);
      do_check_eq(result[0].networkId, alarms[0].networkId);
      do_check_eq(result[0].absoluteThreshold, alarms[0].absoluteThreshold);
      do_check_eq(result[0].relativeThreshold, alarms[0].relativeThreshold);
      do_check_eq(result[0].data.foo, alarms[0].data.foo);
      run_next_test();
    });
  });
});

add_test(function test_getFirstAlarm() {
  // Add alarms[1] -> DB: [ alarms[0] (id: 1), alarms[1] (id: 2) ]
  // Check first alarm is alarms[1] because threshold is lower.
  alarmsDbId += 1;
  netStatsDb.addAlarm(alarms[1], function (error, result) {
    do_check_eq(error, null);
    do_check_eq(result, alarmsDbId);
    netStatsDb.getFirstAlarm(networkWifi, function(error, result) {
      do_check_eq(error, null);
      do_check_eq(result.id, alarmsDbId);
      do_check_eq(result.networkId, alarms[1].networkId);
      do_check_eq(result.absoluteThreshold, alarms[1].absoluteThreshold);
      do_check_eq(result.relativeThreshold, alarms[1].relativeThreshold);
      do_check_eq(result.data.foo, alarms[1].data.foo);
      do_check_eq(result.pageURL, alarms[1].pageURL);
      do_check_eq(result.manifestURL, alarms[1].manifestURL);
      run_next_test();
    });
  });
});

add_test(function test_removeAlarm() {
  // Remove alarms[1] (id: 2) -> DB: [ alarms[0] (id: 1) ]
  // Check get first return alarms[0].
  netStatsDb.removeAlarm(alarmsDbId, alarms[0].manifestURL, function (error, result) {
    do_check_eq(error, null);
    netStatsDb.getFirstAlarm(networkWifi, function(error, result) {
      do_check_eq(error, null);
      do_check_eq(result.id, alarmsDbId - 1);
      do_check_eq(result.networkId, alarms[0].networkId);
      do_check_eq(result.absoluteThreshold, alarms[0].absoluteThreshold);
      do_check_eq(result.relativeThreshold, alarms[0].relativeThreshold);
      do_check_eq(result.data.foo, alarms[0].data.foo);
      do_check_eq(result.pageURL, alarms[0].pageURL);
      do_check_eq(result.manifestURL, alarms[0].manifestURL);
      run_next_test();
    });
  });
});

add_test(function test_removeAppAlarm() {
  // Remove alarms[0] (id: 1) -> DB: [ ]
  netStatsDb.removeAlarm(alarmsDbId - 1, alarms[0].manifestURL, function (error, result) {
    do_check_eq(error, null);
    netStatsDb.getAlarms(networkWifi, exampleManifestURL, function(error, result) {
      do_check_eq(error, null);
      do_check_eq(result.length, 0);
      run_next_test();
    });
  });
});

add_test(function test_getAlarms() {
  // Add all alarms -> DB: [ alarms[0] (id: 3),
  //                         alarms[1] (id: 4),
  //                         alarms[2] (id: 5),
  //                         alarms[3] (id: 6) ]
  // Check that getAlarms for wifi returns 2 alarms.
  // Check that getAlarms for all connections returns 3 alarms.

  var callback = function () {
    netStatsDb.getAlarms(networkWifi, exampleManifestURL, function(error, result) {
      do_check_eq(error, null);
      do_check_eq(result.length, 2);
      netStatsDb.getAlarms(null, exampleManifestURL, function(error, result) {
        do_check_eq(error, null);
        do_check_eq(result.length, 3);
        run_next_test();
      });
    });
  };

  var index = 0;

  var addFunction = function () {
    alarmsDbId += 1;
    netStatsDb.addAlarm(alarms[index], function (error, result) {
      do_check_eq(error, null);
      index += 1;
      do_check_eq(result, alarmsDbId);
      if (index >= alarms.length) {
        callback();
        return;
      }
      addFunction();
    });
  };

  addFunction();
});

add_test(function test_removeAppAllAlarms() {
  // Remove all alarms for exampleManifestURL -> DB: [ alarms[3] (id: 6) ]
  netStatsDb.removeAlarms(exampleManifestURL, function (error, result) {
    do_check_eq(error, null);
    netStatsDb.getAlarms(null, exampleManifestURL, function(error, result) {
      do_check_eq(error, null);
      do_check_eq(result.length, 0);
      netStatsDb.getAlarms(null, testManifestURL, function(error, result) {
        do_check_eq(error, null);
        do_check_eq(result.length, 1);
        run_next_test();
      });
    });
  });
});

add_test(function test_updateAlarm() {
  // Update alarms[3] (id: 6) -> DB: [ alarms[3]* (id: 6) ]

  var updatedAlarm = alarms[1];
  updatedAlarm.id = alarmsDbId;
  updatedAlarm.threshold = 10;

  netStatsDb.updateAlarm(updatedAlarm, function (error, result) {
    do_check_eq(error, null);
    netStatsDb.getFirstAlarm(networkWifi, function(error, result) {
      do_check_eq(error, null);
      do_check_eq(result.id, updatedAlarm.id);
      do_check_eq(result.networkId, updatedAlarm.networkId);
      do_check_eq(result.absoluteThreshold, updatedAlarm.absoluteThreshold);
      do_check_eq(result.relativeThreshold, updatedAlarm.relativeThreshold);
      do_check_eq(result.data.foo, updatedAlarm.data.foo);
      do_check_eq(result.pageURL, updatedAlarm.pageURL);
      do_check_eq(result.manifestURL, updatedAlarm.manifestURL);
      run_next_test();
    });
  });
});

function run_test() {
  do_get_profile();
  run_next_test();
}
