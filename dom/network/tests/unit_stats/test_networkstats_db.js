/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/NetworkStatsDB.jsm");

const netStatsDb = new NetworkStatsDB(this);

function filterTimestamp(date) {
  var sampleRate = netStatsDb.sampleRate;
  var offset = date.getTimezoneOffset() * 60 * 1000;
  return Math.floor((date.getTime() - offset) / sampleRate) * sampleRate;
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
  netStatsDb.clear(function (error, result) {
    do_check_eq(error, null);
    run_next_test();
  });
});

add_test(function test_internalSaveStats_singleSample() {
  var stats = {connectionType: "wifi",
               timestamp:      Date.now(),
               rxBytes:        0,
               txBytes:        0,
               rxTotalBytes:   1234,
               txTotalBytes:   1234};

  netStatsDb.dbNewTxn("readwrite", function(txn, store) {
    netStatsDb._saveStats(txn, store, stats);
  }, function(error, result) {
    do_check_eq(error, null);

    netStatsDb.logAllRecords(function(error, result) {
      do_check_eq(error, null);
      do_check_eq(result.length, 1);
      do_check_eq(result[0].connectionType, stats.connectionType);
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
  netStatsDb.clear(function (error, result) {
    do_check_eq(error, null);

    var samples = 2;
    var stats = [];
    for (var i = 0; i < samples; i++) {
      stats.push({connectionType: "wifi",
                  timestamp:      Date.now() + (10 * i),
                  rxBytes:        0,
                  txBytes:        0,
                  rxTotalBytes:   1234,
                  txTotalBytes:   1234});
    }

    netStatsDb.dbNewTxn("readwrite", function(txn, store) {
      netStatsDb._saveStats(txn, store, stats);
    }, function(error, result) {
      do_check_eq(error, null);

      netStatsDb.logAllRecords(function(error, result) {
        do_check_eq(error, null);
        do_check_eq(result.length, samples);

        var success = true;
        for (var i = 0; i < samples; i++) {
          if (result[i].connectionType != stats[i].connectionType ||
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
  netStatsDb.clear(function (error, result) {
    do_check_eq(error, null);

    var samples = 10;
    var stats = [];
    for (var i = 0; i < samples - 1; i++) {
      stats.push({connectionType: "wifi", timestamp:    Date.now() + (10 * i),
                  rxBytes:             0, txBytes:      0,
                  rxTotalBytes:     1234, txTotalBytes: 1234});
    }

    stats.push({connectionType: "wifi", timestamp:    Date.now() + (10 * samples),
                  rxBytes:           0, txBytes:      0,
                  rxTotalBytes:   1234, txTotalBytes: 1234});

    netStatsDb.dbNewTxn("readwrite", function(txn, store) {
      netStatsDb._saveStats(txn, store, stats);
      var date = stats[stats.length -1].timestamp
                 + (netStatsDb.sampleRate * netStatsDb.maxStorageSamples - 1) - 1;
      netStatsDb._removeOldStats(txn, store, "wifi", date);
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

function processSamplesDiff(lastStat, newStat, callback) {
  netStatsDb.clear(function (error, result){
    do_check_eq(error, null);
    netStatsDb.dbNewTxn("readwrite", function(txn, store) {
      netStatsDb._saveStats(txn, store, lastStat);
    }, function(error, result) {
      netStatsDb.dbNewTxn("readwrite", function(txn, store) {
        let request = store.index("connectionType").openCursor(newStat.connectionType, "prev");
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
  var sampleRate = netStatsDb.sampleRate;
  var date = filterTimestamp(new Date());
  var lastStat = {connectionType: "wifi", timestamp:    date,
                  rxBytes:             0, txBytes:      0,
                  rxTotalBytes:     1234, txTotalBytes: 1234};

  var newStat = {connectionType: "wifi", timestamp:    date,
                 rxBytes:               0, txBytes:      0,
                 rxTotalBytes:       2234, txTotalBytes: 2234};

  processSamplesDiff(lastStat, newStat, function(result) {
    do_check_eq(result.length, 1);
    do_check_eq(result[0].connectionType, newStat.connectionType);
    do_check_eq(result[0].timestamp, newStat.timestamp);
    do_check_eq(result[0].rxBytes, newStat.rxTotalBytes - lastStat.rxTotalBytes);
    do_check_eq(result[0].txBytes, newStat.txTotalBytes - lastStat.txTotalBytes);
    do_check_eq(result[0].rxTotalBytes, newStat.rxTotalBytes);
    do_check_eq(result[0].txTotalBytes, newStat.txTotalBytes);
    run_next_test();
  });
});

add_test(function test_processSamplesDiffNextSample() {
  var sampleRate = netStatsDb.sampleRate;
  var date = filterTimestamp(new Date());
  var lastStat = {connectionType: "wifi", timestamp:    date,
                  rxBytes:             0, txBytes:      0,
                  rxTotalBytes:     1234, txTotalBytes: 1234};

  var newStat = {connectionType: "wifi", timestamp:    date + sampleRate,
                 rxBytes:               0, txBytes:      0,
                 rxTotalBytes:        500, txTotalBytes: 500};

  processSamplesDiff(lastStat, newStat, function(result) {
    do_check_eq(result.length, 2);
    do_check_eq(result[1].connectionType, newStat.connectionType);
    do_check_eq(result[1].timestamp, newStat.timestamp);
    do_check_eq(result[1].rxBytes, newStat.rxTotalBytes);
    do_check_eq(result[1].txBytes, newStat.txTotalBytes);
    do_check_eq(result[1].rxTotalBytes, newStat.rxTotalBytes);
    do_check_eq(result[1].txTotalBytes, newStat.txTotalBytes);
    run_next_test();
  });
});

add_test(function test_processSamplesDiffSamplesLost() {
  var samples = 5;
  var sampleRate = netStatsDb.sampleRate;
  var date = filterTimestamp(new Date());
  var lastStat = {connectionType: "wifi", timestamp:    date,
                  rxBytes:             0, txBytes:      0,
                  rxTotalBytes:     1234, txTotalBytes: 1234};

  var newStat = {connectionType: "wifi", timestamp:    date + (sampleRate * samples),
                 rxBytes:               0, txBytes:      0,
                 rxTotalBytes:      2234, txTotalBytes: 2234};

  processSamplesDiff(lastStat, newStat, function(result) {
    do_check_eq(result.length, samples + 1);
    do_check_eq(result[samples].connectionType, newStat.connectionType);
    do_check_eq(result[samples].timestamp, newStat.timestamp);
    do_check_eq(result[samples].rxBytes, newStat.rxTotalBytes - lastStat.rxTotalBytes);
    do_check_eq(result[samples].txBytes, newStat.txTotalBytes - lastStat.txTotalBytes);
    do_check_eq(result[samples].rxTotalBytes, newStat.rxTotalBytes);
    do_check_eq(result[samples].txTotalBytes, newStat.txTotalBytes);
    run_next_test();
  });
});

add_test(function test_saveStats() {
  var stats = { connectionType: "wifi",
                date:           new Date(),
                rxBytes:        2234,
                txBytes:        2234};

  netStatsDb.clear(function (error, result) {
    do_check_eq(error, null);
    netStatsDb.saveStats(stats, function(error, result) {
      do_check_eq(error, null);
      netStatsDb.logAllRecords(function(error, result) {
        do_check_eq(error, null);
        do_check_eq(result.length, 1);
        do_check_eq(result[0].connectionType, stats.connectionType);
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

function prepareFind(stats, callback) {
  netStatsDb.clear(function (error, result) {
    do_check_eq(error, null);
    netStatsDb.dbNewTxn("readwrite", function(txn, store) {
      netStatsDb._saveStats(txn, store, stats);
    }, function(error, result) {
        callback(error, result);
    });
  });
}

add_test(function test_find () {
  var samples = 5;
  var sampleRate = netStatsDb.sampleRate;
  var start = Date.now();
  var saveDate = filterTimestamp(new Date());
  var end = new Date(start + (sampleRate * (samples - 1)));
  start = new Date(start - sampleRate);
  var stats = [];
  for (var i = 0; i < samples; i++) {i
    stats.push({connectionType: "wifi", timestamp:    saveDate + (sampleRate * i),
                rxBytes:             0, txBytes:      10,
                rxTotalBytes:        0, txTotalBytes: 0});

    stats.push({connectionType: "mobile", timestamp:  saveDate + (sampleRate * i),
                rxBytes:             0, txBytes:      10,
                rxTotalBytes:        0, txTotalBytes: 0});
  }

  prepareFind(stats, function(error, result) {
    do_check_eq(error, null);
    netStatsDb.find(function (error, result) {
      do_check_eq(error, null);
      do_check_eq(result.connectionType, "wifi");
      do_check_eq(result.start.getTime(), start.getTime());
      do_check_eq(result.end.getTime(), end.getTime());
      do_check_eq(result.data.length, samples + 1);
      do_check_eq(result.data[0].rxBytes, null);
      do_check_eq(result.data[1].rxBytes, 0);
      do_check_eq(result.data[samples].rxBytes, 0);

      netStatsDb.findAll(function (error, result) {
        do_check_eq(error, null);
        do_check_eq(result.connectionType, null);
        do_check_eq(result.start.getTime(), start.getTime());
        do_check_eq(result.end.getTime(), end.getTime());
        do_check_eq(result.data.length, samples + 1);
        do_check_eq(result.data[0].rxBytes, null);
        do_check_eq(result.data[1].rxBytes, 0);
        do_check_eq(result.data[1].txBytes, 20);
        do_check_eq(result.data[samples].rxBytes, 0);
        run_next_test();
      }, {start: start, end: end});
    }, {start: start, end: end, connectionType: "wifi"});
  });
});

function run_test() {
  do_get_profile();

  var idbManager = Cc["@mozilla.org/dom/indexeddb/manager;1"].
                   getService(Ci.nsIIndexedDatabaseManager);
  idbManager.initWindowless(this);

  run_next_test();
}
