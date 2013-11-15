/* Any: copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/NetworkStatsDB.jsm");

const netStatsDb = new NetworkStatsDB();

function clearWholeDB(callback) {
  netStatsDb.dbNewTxn("readwrite", function(aTxn, aStore) {
    aStore.delete();
  }, callback);
}

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

  var stats = { appId:        0,
                network:      [networks[0].id, networks[0].type],
                timestamp:    Date.now(),
                rxBytes:      0,
                txBytes:      0,
                rxTotalBytes: 1234,
                txTotalBytes: 1234 };

  netStatsDb.dbNewTxn("readwrite", function(txn, store) {
    netStatsDb._saveStats(txn, store, stats);
  }, function(error, result) {
    do_check_eq(error, null);

    netStatsDb.logAllRecords(function(error, result) {
      do_check_eq(error, null);
      do_check_eq(result.length, 1);
      do_check_eq(result[0].appId, stats.appId);
      do_check_true(compareNetworks(result[0].network, stats.network));
      do_check_eq(result[0].timestamp, stats.timestamp);
      do_check_eq(result[0].rxBytes, stats.rxBytes);
      do_check_eq(result[0].txBytes, stats.txBytes);
      do_check_eq(result[0].rxTotalBytes, stats.rxTotalBytes);
      do_check_eq(result[0].txTotalBytes, stats.txTotalBytes);
      run_next_test();
    });
  });
});

add_test(function test_internalSaveStats_arraySamples() {
  var networks = getNetworks();

  netStatsDb.clearStats(networks, function (error, result) {
    do_check_eq(error, null);

    var network = [networks[0].id, networks[0].type];

    var samples = 2;
    var stats = [];
    for (var i = 0; i < samples; i++) {
      stats.push({ appId:        0,
                   network:      network,
                   timestamp:    Date.now() + (10 * i),
                   rxBytes:      0,
                   txBytes:      0,
                   rxTotalBytes: 1234,
                   txTotalBytes: 1234 });
    }

    netStatsDb.dbNewTxn("readwrite", function(txn, store) {
      netStatsDb._saveStats(txn, store, stats);
    }, function(error, result) {
      do_check_eq(error, null);

      netStatsDb.logAllRecords(function(error, result) {
        do_check_eq(error, null);

        // Result has one sample more than samples because clear inserts
        // an empty sample to keep totalBytes synchronized with netd counters
        result.shift();
        do_check_eq(result.length, samples);

        var success = true;
        for (var i = 1; i < samples; i++) {
          if (result[i].appId != stats[i].appId ||
              !compareNetworks(result[i].network, stats[i].network) ||
              result[i].timestamp != stats[i].timestamp ||
              result[i].rxBytes != stats[i].rxBytes ||
              result[i].txBytes != stats[i].txBytes ||
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
  var networks = getNetworks();

  netStatsDb.clearStats(networks, function (error, result) {
    do_check_eq(error, null);

    var network = [networks[0].id, networks[0].type];
    var samples = 10;
    var stats = [];
    for (var i = 0; i < samples - 1; i++) {
      stats.push({ appId:              0,
                   network:      network, timestamp:    Date.now() + (10 * i),
                   rxBytes:            0, txBytes:      0,
                   rxTotalBytes:    1234, txTotalBytes: 1234 });
    }

    stats.push({ appId:              0,
                 network:      network, timestamp:    Date.now() + (10 * samples),
                 rxBytes:            0, txBytes:      0,
                 rxTotalBytes:    1234, txTotalBytes: 1234 });

    netStatsDb.dbNewTxn("readwrite", function(txn, store) {
      netStatsDb._saveStats(txn, store, stats);
      var date = stats[stats.length - 1].timestamp
                 + (netStatsDb.sampleRate * netStatsDb.maxStorageSamples - 1) - 1;
      netStatsDb._removeOldStats(txn, store, 0, network, date);
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
  netStatsDb.clearStats(networks, function (error, result){
    do_check_eq(error, null);
    netStatsDb.dbNewTxn("readwrite", function(txn, store) {
      netStatsDb._saveStats(txn, store, lastStat);
    }, function(error, result) {
      netStatsDb.dbNewTxn("readwrite", function(txn, store) {
        let request = store.index("network").openCursor(newStat.network, "prev");
        request.onsuccess = function onsuccess(event) {
          let cursor = event.target.result;
          do_check_neq(cursor, null);
          netStatsDb._processSamplesDiff(txn, store, cursor, newStat);
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

  var lastStat = { appId:              0,
                   network:      network, timestamp:    date,
                   rxBytes:            0, txBytes:      0,
                   rxTotalBytes:    1234, txTotalBytes: 1234 };

  var newStat = { appId:              0,
                  network:      network, timestamp:    date,
                  rxBytes:            0, txBytes:      0,
                  rxTotalBytes:    2234, txTotalBytes: 2234 };

  processSamplesDiff(networks, lastStat, newStat, function(result) {
    do_check_eq(result.length, 1);
    do_check_eq(result[0].appId, newStat.appId);
    do_check_true(compareNetworks(result[0].network, newStat.network));
    do_check_eq(result[0].timestamp, newStat.timestamp);
    do_check_eq(result[0].rxBytes, newStat.rxTotalBytes - lastStat.rxTotalBytes);
    do_check_eq(result[0].txBytes, newStat.txTotalBytes - lastStat.txTotalBytes);
    do_check_eq(result[0].rxTotalBytes, newStat.rxTotalBytes);
    do_check_eq(result[0].txTotalBytes, newStat.txTotalBytes);
    run_next_test();
  });
});

add_test(function test_processSamplesDiffNextSample() {
  var networks = getNetworks();
  var network = [networks[0].id, networks[0].type];

  var sampleRate = netStatsDb.sampleRate;
  var date = filterTimestamp(new Date());

  var lastStat = { appId:              0,
                   network:      network, timestamp:    date,
                   rxBytes:            0, txBytes:      0,
                   rxTotalBytes:    1234, txTotalBytes: 1234 };

  var newStat = { appId:              0,
                  network:      network, timestamp:    date + sampleRate,
                  rxBytes:            0, txBytes:      0,
                  rxTotalBytes:     500, txTotalBytes: 500 };

  processSamplesDiff(networks, lastStat, newStat, function(result) {
    do_check_eq(result.length, 2);
    do_check_eq(result[1].appId, newStat.appId);
    do_check_true(compareNetworks(result[1].network, newStat.network));
    do_check_eq(result[1].timestamp, newStat.timestamp);
    do_check_eq(result[1].rxBytes, newStat.rxTotalBytes);
    do_check_eq(result[1].txBytes, newStat.txTotalBytes);
    do_check_eq(result[1].rxTotalBytes, newStat.rxTotalBytes);
    do_check_eq(result[1].txTotalBytes, newStat.txTotalBytes);
    run_next_test();
  });
});

add_test(function test_processSamplesDiffSamplesLost() {
  var networks = getNetworks();
  var network = [networks[0].id, networks[0].type];
  var samples = 5;
  var sampleRate = netStatsDb.sampleRate;
  var date = filterTimestamp(new Date());
  var lastStat = { appId:             0,
                   network:     network, timestamp:    date,
                   rxBytes:           0, txBytes:      0,
                   rxTotalBytes:   1234, txTotalBytes: 1234 };

  var newStat = { appId:              0,
                  network:      network, timestamp:    date + (sampleRate * samples),
                  rxBytes:            0, txBytes:      0,
                  rxTotalBytes:    2234, txTotalBytes: 2234 };

  processSamplesDiff(networks, lastStat, newStat, function(result) {
    do_check_eq(result.length, samples + 1);
    do_check_eq(result[0].appId, newStat.appId);
    do_check_true(compareNetworks(result[samples].network, newStat.network));
    do_check_eq(result[samples].timestamp, newStat.timestamp);
    do_check_eq(result[samples].rxBytes, newStat.rxTotalBytes - lastStat.rxTotalBytes);
    do_check_eq(result[samples].txBytes, newStat.txTotalBytes - lastStat.txTotalBytes);
    do_check_eq(result[samples].rxTotalBytes, newStat.rxTotalBytes);
    do_check_eq(result[samples].txTotalBytes, newStat.txTotalBytes);
    run_next_test();
  });
});

add_test(function test_saveStats() {
  var networks = getNetworks();
  var network = [networks[0].id, networks[0].type];

  var stats = { appId:       0,
                networkId:   networks[0].id,
                networkType: networks[0].type,
                date:        new Date(),
                rxBytes:     2234,
                txBytes:     2234};

  netStatsDb.clearStats(networks, function (error, result) {
    do_check_eq(error, null);
    netStatsDb.saveStats(stats, function(error, result) {
      do_check_eq(error, null);
      netStatsDb.logAllRecords(function(error, result) {
        do_check_eq(error, null);
        do_check_eq(result.length, 1);
        do_check_eq(result[0].appId, stats.appId);
        do_check_true(compareNetworks(result[0].network, network));
        let timestamp = filterTimestamp(stats.date);
        do_check_eq(result[0].timestamp, timestamp);
        do_check_eq(result[0].rxBytes, 0);
        do_check_eq(result[0].txBytes, 0);
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

  var stats = { appId:       1,
                networkId:   networks[0].id,
                networkType: networks[0].type,
                date:        new Date(),
                rxBytes:     2234,
                txBytes:     2234};

  netStatsDb.clearStats(networks, function (error, result) {
    do_check_eq(error, null);
    netStatsDb.saveStats(stats, function(error, result) {
      do_check_eq(error, null);
      netStatsDb.logAllRecords(function(error, result) {
        do_check_eq(error, null);
        // The clear function clears all records of the datbase but
        // inserts a new element for each [appId, connectionId, connectionType]
        // record to keep the track of rxTotalBytes / txTotalBytes.
        // So at this point, we have two records, one for the appId 0 used in
        // past tests and the new one for appId 1
        do_check_eq(result.length, 2);
        do_check_eq(result[1].appId, stats.appId);
        do_check_true(compareNetworks(result[1].network, network));
        let timestamp = filterTimestamp(stats.date);
        do_check_eq(result[1].timestamp, timestamp);
        do_check_eq(result[1].rxBytes, stats.rxBytes);
        do_check_eq(result[1].txBytes, stats.txBytes);
        do_check_eq(result[1].rxTotalBytes, 0);
        do_check_eq(result[1].txTotalBytes, 0);
        run_next_test();
      });
    });
  });
});

function prepareFind(network, stats, callback) {
  netStatsDb.clearStats(network, function (error, result) {
    do_check_eq(error, null);
    netStatsDb.dbNewTxn("readwrite", function(txn, store) {
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

  var samples = 5;
  var sampleRate = netStatsDb.sampleRate;
  var start = Date.now();
  var saveDate = filterTimestamp(new Date());
  var end = new Date(start + (sampleRate * (samples - 1)));
  start = new Date(start - sampleRate);
  var stats = [];
  for (var i = 0; i < samples; i++) {
    stats.push({ appId:              appId,
                 network:      networkWifi, timestamp:    saveDate + (sampleRate * i),
                 rxBytes:                0, txBytes:      10,
                 rxTotalBytes:           0, txTotalBytes: 0 });

    stats.push({ appId:                appId,
                 network:      networkMobile, timestamp:    saveDate + (sampleRate * i),
                 rxBytes:                  0, txBytes:      10,
                 rxTotalBytes:             0, txTotalBytes: 0 });
  }

  prepareFind(networks[0], stats, function(error, result) {
    do_check_eq(error, null);
    netStatsDb.find(function (error, result) {
      do_check_eq(error, null);
      do_check_eq(result.network.id, networks[0].id);
      do_check_eq(result.network.type, networks[0].type);
      do_check_eq(result.start.getTime(), start.getTime());
      do_check_eq(result.end.getTime(), end.getTime());
      do_check_eq(result.data.length, samples + 1);
      do_check_eq(result.data[0].rxBytes, null);
      do_check_eq(result.data[1].rxBytes, 0);
      do_check_eq(result.data[samples].rxBytes, 0);
      run_next_test();
    }, networks[0], start, end, appId);
  });
});

add_test(function test_findAppStats () {
  var networks = getNetworks();
  var networkWifi = [networks[0].id, networks[0].type];
  var networkMobile = [networks[1].id, networks[1].type]; // Fake mobile interface

  var samples = 5;
  var sampleRate = netStatsDb.sampleRate;
  var start = Date.now();
  var saveDate = filterTimestamp(new Date());
  var end = new Date(start + (sampleRate * (samples - 1)));
  start = new Date(start - sampleRate);
  var stats = [];
  for (var i = 0; i < samples; i++) {
    stats.push({ appId:                  1,
                 network:      networkWifi, timestamp:    saveDate + (sampleRate * i),
                 rxBytes:                0, txBytes:      10,
                 rxTotalBytes:           0, txTotalBytes: 0 });

    stats.push({ appId:                    1,
                 network:      networkMobile, timestamp:    saveDate + (sampleRate * i),
                 rxBytes:                  0, txBytes:      10,
                 rxTotalBytes:             0, txTotalBytes: 0 });
  }

  prepareFind(networks[0], stats, function(error, result) {
    do_check_eq(error, null);
    netStatsDb.find(function (error, result) {
      do_check_eq(error, null);
      do_check_eq(result.network.id, networks[0].id);
      do_check_eq(result.network.type, networks[0].type);
      do_check_eq(result.start.getTime(), start.getTime());
      do_check_eq(result.end.getTime(), end.getTime());
      do_check_eq(result.data.length, samples + 1);
      do_check_eq(result.data[0].rxBytes, null);
      do_check_eq(result.data[1].rxBytes, 0);
      do_check_eq(result.data[samples].rxBytes, 0);
      run_next_test();
    }, networks[0], start, end, 1);
  });
});

add_test(function test_saveMultipleAppStats () {
  var networks = getNetworks();
  var networkWifi = networks[0];
  var networkMobile = networks[1]; // Fake mobile interface

  var saveDate = filterTimestamp(new Date());
  var cached = Object.create(null);

  cached['1wifi'] = { appId:                      1, date:           new Date(),
                      networkId:     networkWifi.id, networkType: networkWifi.type,
                      rxBytes:                    0, txBytes:      10 };

  cached['1mobile'] = { appId:                    1, date:           new Date(),
                        networkId: networkMobile.id, networkType: networkMobile.type,
                        rxBytes:                  0, txBytes:      10 };

  cached['2wifi'] = { appId:                      2, date:           new Date(),
                      networkId:     networkWifi.id, networkType: networkWifi.type,
                      rxBytes:                    0, txBytes:      10 };

  cached['2mobile'] = { appId:                    2, date:           new Date(),
                        networkId: networkMobile.id, networkType: networkMobile.type,
                        rxBytes:                  0, txBytes:      10 };

  let keys = Object.keys(cached);
  let index = 0;

  networks.push(networkMobile);
  netStatsDb.clearStats(networks, function (error, result) {
    do_check_eq(error, null);
    netStatsDb.saveStats(cached[keys[index]],
      function callback(error, result) {
        do_check_eq(error, null);

        if (index == keys.length - 1) {
          netStatsDb.logAllRecords(function(error, result) {
          // Again, result has two samples more than expected samples because
          // clear inserts one empty sample for each network to keep totalBytes
          // synchronized with netd counters. so the first two samples have to
          // be discarted.
          result.shift();
          result.shift();

          do_check_eq(error, null);
          do_check_eq(result.length, 4);
          do_check_eq(result[0].appId, 1);
          do_check_true(compareNetworks(result[0].network,[networkWifi.id, networkWifi.type]));
          do_check_eq(result[0].rxBytes, 0);
          do_check_eq(result[0].txBytes, 10);
          run_next_test();
          });
        }

        index += 1;
        netStatsDb.saveStats(cached[keys[index]], callback);
    });
  });
});

function run_test() {
  do_get_profile();

  // Clear whole database to avoid start tests with unknown state
  // due to previous tests.
  clearWholeDB(function(){
    run_next_test();
  });
}
