/* Any: copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/ResourceStatsDB.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const db = new ResourceStatsDB();

// Components.
const wifiComponent = "wifi:0";
const mobileComponent = "mobile:1";
const cpuComponent = "cpu:0";
const gpsComponent = "gps:0";

// List of available components.
const networkComponents = [wifiComponent, mobileComponent];
const powerComponents = [cpuComponent, gpsComponent];
const offset = (new Date()).getTimezoneOffset() * 60 * 1000;

// Clear store.
function clearStore(store, callback) {
  db._dbNewTxn(store, "readwrite", function(aTxn, aStore) {
    aStore.openCursor().onsuccess = function (event) {
      let cursor = event.target.result;
      if (cursor){
        cursor.delete();
        cursor.continue();
      }
    };
  }, callback);
}

// Clear all stores to avoid starting tests with unknown state.
add_test(function prepareDatabase() {
  clearStore('power_stats_store', function() {
    clearStore('network_stats_store', function() {
      clearStore('alarm_store', function() {
        run_next_test();
      });
    });
  });
});

// Dump data saved in a store.
function dumpStore(store, callback) {
  db._dbNewTxn(store, "readonly", function(aTxn, aStore) {
    aStore.mozGetAll().onsuccess = function onsuccess(event) {
      aTxn.result = event.target.result;
    };
  }, callback);
}

// Check sampleRate is unchangeable.
add_test(function test_sampleRate() {
  var sampleRate = db.sampleRate;
  do_check_true(sampleRate > 0);

  db.sampleRate = 0;
  sampleRate = db.sampleRate;
  do_check_true(sampleRate > 0);

  run_next_test();
});

// Test maxStorageAge is unchangeable.
add_test(function test_maxStorageAge() {
  var maxStorageAge = db.maxStorageAge;
  do_check_true(maxStorageAge > 0);

  db.maxStorageAge = 0;
  maxStorageAge = db.maxStorageAge;
  do_check_true(maxStorageAge > 0);

  run_next_test();
});

// Normalize timestamp to sampleRate precision.
function normalizeTime(aTimeStamp) {
  var sampleRate = db.sampleRate;

  return Math.floor((aTimeStamp - offset) / sampleRate) * sampleRate;
}

// Generte record as input for saveNetworkStats() as well as the expected
// result when calling getStats().
function generateNetworkRecord(aAppId, aServiceType, aComponents) {
  var result = [];
  var componentStats = {};
  var receivedBytes;
  var sentBytes;
  var totalReceivedBytes = 0;
  var totalSentBytes = 0;

  aComponents.forEach(function(comp) {
    // Step 1: generate random value for receivedBytes and sentBytes.
    receivedBytes = Math.floor(Math.random() * 1000);
    sentBytes = Math.floor(Math.random() * 1000);
    totalReceivedBytes += receivedBytes;
    totalSentBytes += sentBytes;

    // Step 2: add stats to record.componentStats.
    componentStats[comp] = {
      receivedBytes: receivedBytes,
      sentBytes: sentBytes
    };

    // Step 3: generate expected results.
    result.push({
      appId: aAppId,
      serviceType: aServiceType,
      component: comp,
      receivedBytes: receivedBytes,
      sentBytes: sentBytes
    });
  });

  // Step 4: generate expected total stats.
  result.push({
    appId: aAppId,
    serviceType: aServiceType,
    component: "",
    receivedBytes: totalReceivedBytes,
    sentBytes: totalSentBytes
  });

  // Step 5: get record.
  var record = { appId: aAppId,
                 serviceType: aServiceType,
                 componentStats: componentStats };

  return { record: record, result: result };
}

// Generte record as input for savePowerStats() as well as the expected
// result when calling getStats().
function generatePowerRecord(aAppId, aServiceType, aComponents) {
  var result = [];
  var componentStats = {};
  var consumedPower;
  var totalConsumedPower = 0;

  aComponents.forEach(function(comp) {
    // Step 1: generate random value for consumedPower.
    consumedPower = Math.floor(Math.random() * 1000);
    totalConsumedPower += consumedPower;

    // Step 2: add stats to record.componentStats.
    componentStats[comp] = consumedPower;

    // Step 3: generate expected results.
    result.push({
      appId: aAppId,
      serviceType: aServiceType,
      component: comp,
      consumedPower: consumedPower
    });
  });

  // Step 4: generate expected total stats.
  result.push({
    appId: aAppId,
    serviceType: aServiceType,
    component: "",
    consumedPower: totalConsumedPower
  });

  // Step 5: get record.
  var record = { appId: aAppId,
                 serviceType: aServiceType,
                 componentStats: componentStats };

  return { record: record, result: result };
}

// Compare stats saved in network store with expected results.
function checkNetworkStatsStore(aExpectedResult, aDumpResult, aTimestamp) {
  // Step 1: a quick check for the length of arrays first.
  do_check_eq(aExpectedResult.length, aDumpResult.length);

  // Step 2: create a map array for search by receivedBytes.
  var mapArray = aExpectedResult.map(function(e) {return e.receivedBytes;});

  // Step 3: compare each element to make sure both array are equal.
  var index;
  var target;

  aDumpResult.forEach(function(stats) {
    index = 0;
    // Find the first equal receivedBytes since index.
    while ((index = mapArray.indexOf(stats.receivedBytes, index)) > -1) {
      // Compare all attributes.
      target = aExpectedResult[index];
      if (target.appId != stats.appId ||
          target.serviceType != stats.serviceType ||
          target.component != stats.component ||
          target.sentBytes != stats.sentBytes ||
          aTimestamp != stats.timestamp) {
        index += 1;
        continue;
      } else {
        // If found, remove that element from aExpectedResult and mapArray.
        aExpectedResult.splice(index, 1);
        mapArray.splice(index, 1);
        break;
      }
    }
    do_check_neq(index, -1);
  });
  run_next_test();
}

// Compare stats saved in power store with expected results.
function checkPowerStatsStore(aExpectedResult, aDumpResult, aTimestamp) {
  // Step 1: a quick check for the length of arrays first.
  do_check_eq(aExpectedResult.length, aDumpResult.length);

  // Step 2: create a map array for search by consumedPower.
  var mapArray = aExpectedResult.map(function(e) {return e.consumedPower;});

  // Step 3: compare each element to make sure both array are equal.
  var index;
  var target;

  aDumpResult.forEach(function(stats) {
    index = 0;
    // Find the first equal consumedPower since index.
    while ((index = mapArray.indexOf(stats.consumedPower, index)) > -1) {
      // Compare all attributes.
      target = aExpectedResult[index];
      if (target.appId != stats.appId ||
          target.serviceType != stats.serviceType ||
          target.component != stats.component ||
          aTimestamp != stats.timestamp) {
        index += 1;
        continue;
      } else {
        // If found, remove that element from aExpectedResult and mapArray.
        aExpectedResult.splice(index, 1);
        mapArray.splice(index, 1);
        break;
      }
    }
    do_check_neq(index, -1);
  });
  run_next_test();
}

// Prepare network store for testing.
function prepareNetworkStatsStore(recordArray, timestamp, callback) {
  // Step 1: clear store.
  clearStore("network_stats_store", function() {
    // Step 2: save record to store.
    db.saveNetworkStats(recordArray, timestamp, callback);
  });
}

// Prepare power store for testing.
function preparePowerStatsStore(recordArray, timestamp, callback) {
  // Step 1: clear store.
  clearStore("power_stats_store", function() {
    // Step 2: save record to store.
    db.savePowerStats(recordArray, timestamp, callback);
  });
}

// Test saveNetworkStats.
add_test(function test_saveNetworkStats() {
  var appId = 1;
  var serviceType = "";

  // Step 1: generate data saved to store.
  var { record: record, result: expectedResult } =
    generateNetworkRecord(appId, serviceType, networkComponents);
  var recordArray = [record];
  var timestamp = Date.now();

  // Step 2: save recordArray to network store.
  prepareNetworkStatsStore(recordArray, timestamp, function(error, callback) {
    // Step 3: check if the function call succeed.
    do_check_eq(error, null);
    // Step 4: dump store for comparison.
    dumpStore("network_stats_store", function(error, result) {
      do_check_eq(error, null);
      checkNetworkStatsStore(expectedResult, result, normalizeTime(timestamp));
    });
  });
});

// Test savePowerStats.
add_test(function test_savePowerStats() {
  var appId = 1;
  var serviceType = "";

  // Step 1: generate data saved to store.
  var { record: record, result: expectedResult } =
    generatePowerRecord(appId, serviceType, powerComponents);
  var recordArray = [record];
  var timestamp = Date.now();

  // Step 2: save recordArray to power store.
  preparePowerStatsStore(recordArray, timestamp, function(error, callback) {
    // Step 3: check if the function call succeed.
    do_check_eq(error, null);
    // Step 4: dump store for comparison.
    dumpStore("power_stats_store", function(error, result) {
      do_check_eq(error, null);
      checkPowerStatsStore(expectedResult, result, normalizeTime(timestamp));
    });
  });
});

// Test getting network stats via getStats.
add_test(function test_getNetworkStats() {
  var appId = 0;
  var manifestURL = "";
  var serviceType = "";
  var component = "";

  // Step 1: generate data saved to store.
  var { record: record, result: result } =
    generateNetworkRecord(appId, serviceType, networkComponents);
  var recordArray = [record];
  var expectedStats = result[result.length - 1]; // Check total stats only.
  var timestamp = Date.now();
  var end = normalizeTime(timestamp) + offset;
  var start = end;

  // Step 2: save record and prepare network store.
  prepareNetworkStatsStore(recordArray, timestamp, function(error, callback) {
    // Step 3: get network stats.
    db.getStats("network", manifestURL, serviceType, component, start, end,
      function(error, result) {
      do_check_eq(error, null);

      // Step 4: verify result.
      do_check_eq(result.type, "network");
      do_check_eq(result.manifestURL, manifestURL);
      do_check_eq(result.serviceType, serviceType);
      do_check_eq(result.component, component);
      do_check_eq(result.start, start);
      do_check_eq(result.end, end);
      do_check_eq(result.sampleRate, db.sampleRate);
      do_check_true(Array.isArray(result.statsData));
      do_check_eq(result.statsData.length, 1);
      var stats = result.statsData[0];
      do_check_eq(stats.receivedBytes, expectedStats.receivedBytes);
      do_check_eq(stats.sentBytes, expectedStats.sentBytes);

      run_next_test(); // If success, run next test.
    });
  });
});

// Test getting power stats via getStats.
add_test(function test_getPowerStats() {
  var appId = 0;
  var manifestURL = "";
  var serviceType = "";
  var component = "";

  // Step 1: generate data saved to store.
  var { record: record, result: result } =
    generatePowerRecord(appId, serviceType, powerComponents);
  var recordArray = [record];
  var expectedStats = result[result.length - 1]; // Check total stats only.
  var timestamp = Date.now();
  var end = normalizeTime(timestamp) + offset;
  var start = end;

  // Step 2: save record and prepare power store.
  preparePowerStatsStore(recordArray, timestamp, function(error, callback) {
    // Step 3: get power stats.
    db.getStats("power", manifestURL, serviceType, component, start, end,
      function(error, result) {
      do_check_eq(error, null);

      // Step 4: verify result
      do_check_eq(result.type, "power");
      do_check_eq(result.manifestURL, manifestURL);
      do_check_eq(result.serviceType, serviceType);
      do_check_eq(result.component, component);
      do_check_eq(result.start, start);
      do_check_eq(result.end, end);
      do_check_eq(result.sampleRate, db.sampleRate);
      do_check_true(Array.isArray(result.statsData));
      do_check_eq(result.statsData.length, 1);
      var stats = result.statsData[0];
      do_check_eq(stats.consumedPower, expectedStats.consumedPower);

      run_next_test(); // If success, run next test.
    });
  });
});

// Test deleting network stats via clearStats.
add_test(function test_clearNetworkStats() {
  var appId = 0;
  var manifestURL = "";
  var serviceType = "";
  var component = "";

  // Step 1: genrate data saved to network store.
  var { record: record, result: result } =
    generateNetworkRecord(appId, serviceType, networkComponents);
  var recordArray = [record];
  var timestamp = Date.now();
  var end = normalizeTime(timestamp) + offset;
  var start = end;

  // Step 2: save record and prepare network store.
  prepareNetworkStatsStore(recordArray, timestamp, function(error, callback) {
    // Step 3: clear network stats.
    db.clearStats("network", appId, serviceType, component, start, end,
      function(error, result) {
      do_check_eq(error, null);

      // Step 4: check if the stats is deleted.
      db.getStats("network", manifestURL, serviceType, component, start, end,
        function(error, result) {
        do_check_eq(result.statsData.length, 0);
        run_next_test();
      });
    });
  });
});

// Test deleting power stats via clearStats.
add_test(function test_clearPowerStats() {
  var appId = 0;
  var manifestURL = "";
  var serviceType = "";
  var component = "";

  // Step 1: genrate data saved to power store.
  var { record: record, result: result } =
    generatePowerRecord(appId, serviceType, powerComponents);
  var recordArray = [record];
  var timestamp = Date.now();
  var end = normalizeTime(timestamp) + offset;
  var start = end;

  // Step 2: save record and prepare power store.
  preparePowerStatsStore(recordArray, timestamp, function(error, callback) {
    // Step 3: clear power stats.
    db.clearStats("power", appId, serviceType, component, start, end,
      function(error, result) {
      do_check_eq(error, null);

      // Step 4: check if the stats is deleted.
      db.getStats("power", manifestURL, serviceType, component, start, end,
        function(error, result) {
        do_check_eq(result.statsData.length, 0);
        run_next_test();
      });
    });
  });
});

// Test clearing all network stats.
add_test(function test_clearAllNetworkStats() {
  db.clearAllStats("network", function(error, result) {
    do_check_eq(error, null);
    run_next_test();
  });
});

// Test clearing all power stats.
add_test(function test_clearAllPowerStats() {
  db.clearAllStats("power", function(error, result) {
    do_check_eq(error, null);
    run_next_test();
  });
});

// Test getting network components.
add_test(function test_getNetworkComponents() {
  var appId = 0;
  var serviceType = "";

  // Step 1: generate data saved to store.
  var { record: record, result: expectedResult } =
    generateNetworkRecord(appId, serviceType, networkComponents);
  var recordArray = [record];
  var timestamp = Date.now();

  // Step 2: save recordArray to network store.
  prepareNetworkStatsStore(recordArray, timestamp, function(error, callback) {
    // Step 3: call getComponents.
    db.getComponents("network", function(error, result) {
      do_check_eq(error, null);
      do_check_true(Array.isArray(result));
      do_check_eq(result.length, networkComponents.length);

      // Check each element in result array is an element of networkComponents.
      result.forEach(function(component) {
        do_check_true(networkComponents.indexOf(component) > -1);
      });

      run_next_test(); // If success, run next test.
    });
  });
});

// Test getting power components.
add_test(function test_getPowerComponents() {
  var appId = 0;
  var serviceType = "";

  // Step 1: generate data saved to store.
  var { record: record, result: expectedResult } =
    generatePowerRecord(appId, serviceType, powerComponents);
  var recordArray = [record];
  var timestamp = Date.now();

  // Step 2: save recordArray to power store.
  preparePowerStatsStore(recordArray, timestamp, function(error, callback) {
    // Step 3: call getComponents.
    db.getComponents("power", function(error, result) {
      do_check_eq(error, null);
      do_check_true(Array.isArray(result));
      do_check_eq(result.length, powerComponents.length);

      // Check each element in result array is an element of powerComponents.
      result.forEach(function(component) {
        do_check_true(powerComponents.indexOf(component) > -1);
      });

      run_next_test(); // If success, run next test.
    });
  });
});

// Generate alarm object for addAlarm().
function generateAlarmObject(aType, aManifestURL, aServiceType, aComponent) {
  let alarm = {
    type: aType,
    component: aComponent,
    serviceType: aServiceType,
    manifestURL: aManifestURL,
    threshold: Math.floor(Math.random() * 1000),
    startTime: Math.floor(Math.random() * 1000),
    data: null
  };

  return alarm;
}

// Test adding a network alarm.
add_test(function test_addNetowrkAlarm() {
  var manifestURL = "";
  var serviceType = "";

  // Step 1: generate a network alarm.
  var alarm =
    generateAlarmObject("network", manifestURL, serviceType, wifiComponent);

  // Step 2: clear store.
  clearStore("alarm_store", function() {
    // Step 3: save the alarm to store.
    db.addAlarm(alarm, function(error, result) {
      // Step 4: check if the function call succeed.
      do_check_eq(error, null);
      do_check_true(result > -1);
      let alarmId = result;

      // Step 5: dump store for comparison.
      dumpStore("alarm_store", function(error, result) {
        do_check_eq(error, null);
        do_check_true(Array.isArray(result));
        do_check_true(result.length == 1);
        do_check_eq(result[0].type, alarm.type);
        do_check_eq(result[0].manifestURL, alarm.manifestURL);
        do_check_eq(result[0].serviceType, alarm.serviceType);
        do_check_eq(result[0].component, alarm.component);
        do_check_eq(result[0].threshold, alarm.threshold);
        do_check_eq(result[0].startTime, alarm.startTime);
        do_check_eq(result[0].data, alarm.data);
        do_check_eq(result[0].alarmId, alarmId);

        run_next_test(); // If success, run next test.
      });
    });
  });
});

// Test adding a power alarm.
add_test(function test_addPowerAlarm() {
  var manifestURL = "";
  var serviceType = "";

  // Step 1: generate a power alarm.
  var alarm =
    generateAlarmObject("power", manifestURL, serviceType, cpuComponent);

  // Step 2: clear store.
  clearStore("alarm_store", function() {
    // Step 3: save the alarm to store.
    db.addAlarm(alarm, function(error, result) {
      // Step 4: check if the function call succeed.
      do_check_eq(error, null);
      do_check_true(result > -1);
      let alarmId = result;

      // Step 5: dump store for comparison.
      dumpStore("alarm_store", function(error, result) {
        do_check_eq(error, null);
        do_check_true(Array.isArray(result));
        do_check_true(result.length == 1);
        do_check_eq(result[0].type, alarm.type);
        do_check_eq(result[0].manifestURL, alarm.manifestURL);
        do_check_eq(result[0].serviceType, alarm.serviceType);
        do_check_eq(result[0].component, alarm.component);
        do_check_eq(result[0].threshold, alarm.threshold);
        do_check_eq(result[0].startTime, alarm.startTime);
        do_check_eq(result[0].data, alarm.data);
        do_check_eq(result[0].alarmId, alarmId);

        run_next_test(); // If success, run next test.
      });
    });
  });
});

// Add multiple alarms to store and record the obtained alarmId in each
// alarm object.
function addAlarmsToStore(alarms, index, callback) {
  var alarm = alarms[index++];
  if (index < alarms.length) {
    db.addAlarm(alarm, function(error, result) {
      alarm.alarmId = result;
      addAlarmsToStore(alarms, index, callback);
    });
  } else {
    db.addAlarm(alarm, function(error, result) {
      alarm.alarmId = result;
      callback(error, result);
    });
  }
}

// Prepare alarm store for testging.
function prepareAlarmStore(alarms, callback) {
  // Step 1: clear srore.
  clearStore("alarm_store", function() {
    // Step 2: save alarms to store one by one.
    addAlarmsToStore(alarms, 0, callback);
  });
}

// Compare alrams returned by getAlarms().
function compareAlarms(aExpectedResult, aResult) {
  // Step 1: a quick check for the length of arrays first.
  do_check_eq(aExpectedResult.length, aResult.length);

  // Step 2: create a map array for search by threshold.
  var mapArray = aExpectedResult.map(function(e) {return e.threshold;});

  // Step 3: compare each element to make sure both array are equal.
  var index;
  var target;

  aResult.forEach(function(alarm) {
    index = 0;
    // Find the first equal receivedBytes since index.
    while ((index = mapArray.indexOf(alarm.threshold, index)) > -1) {
      // Compare all attributes.
      target = aExpectedResult[index];
      if (target.alarmId != alarm.alarmId ||
          target.type != alarm.type ||
          target.manifestURL != alarm.manifestURL ||
          target.serviceType != alarm.serviceType ||
          target.component != alarm.component ||
          target.data != alarm.data) {
        index += 1;
        continue;
      } else {
        // If found, remove that element from aExpectedResult and mapArray.
        aExpectedResult.splice(index, 1);
        mapArray.splice(index, 1);
        break;
      }
    }
    do_check_neq(index, -1);
  });
  run_next_test();
}

// Test getting designate network alarms from store.
add_test(function test_getNetworkAlarms() {
  var manifestURL = "";
  var serviceType = "";
  var alarms = [];

  // Step 1: generate two network alarms using same parameters.
  alarms.push(generateAlarmObject("network", manifestURL, serviceType,
                                  wifiComponent));
  alarms.push(generateAlarmObject("network", manifestURL, serviceType,
                                  wifiComponent));

  // Step 2: generate another network alarm using different parameters.
  alarms.push(generateAlarmObject("network", manifestURL, serviceType,
                                  mobileComponent));

  // Step 3: clear alarm store and save new alarms to store.
  prepareAlarmStore(alarms, function(error, result) {
    // Step 4: call getAlarms.
    let options = {
      manifestURL: manifestURL,
      serviceType: serviceType,
      component: wifiComponent
    };
    db.getAlarms("network", options, function(error, result) {
      // Step 5: check if the function call succeed.
      do_check_eq(error, null);

      // Step 6: check results.
      // The last element in alarms array is not our expected result,
      // so pop that out first.
      alarms.pop();
      compareAlarms(alarms, result);
    });
  });
});

// Test getting designate power alarms from store.
add_test(function test_getPowerAlarms() {
  var manifestURL = "";
  var serviceType = "";
  var alarms = [];

  // Step 1: generate two power alarms using same parameters.
  alarms.push(generateAlarmObject("power", manifestURL, serviceType,
                                  cpuComponent));
  alarms.push(generateAlarmObject("power", manifestURL, serviceType,
                                  cpuComponent));

  // Step 2: generate another power alarm using different parameters.
  alarms.push(generateAlarmObject("power", manifestURL, serviceType,
                                  gpsComponent));

  // Step 3: clear alarm store and save new alarms to store.
  prepareAlarmStore(alarms, function(error, result) {
    // Step 4: call getAlarms.
    let options = {
      manifestURL: manifestURL,
      serviceType: serviceType,
      component: cpuComponent
    };
    db.getAlarms("power", options, function(error, result) {
      // Step 5: check if the function call succeed.
      do_check_eq(error, null);

      // Step 6: check results.
      // The last element in alarms array is not our expected result,
      // so pop that out first.
      alarms.pop();
      compareAlarms(alarms, result);
    });
  });
});

// Test getting all network alarms from store.
add_test(function test_getAllNetworkAlarms() {
  var manifestURL = "";
  var serviceType = "";
  var alarms = [];

  // Step 1: generate two network alarms.
  alarms.push(generateAlarmObject("network", manifestURL, serviceType,
                                  wifiComponent));
  alarms.push(generateAlarmObject("network", manifestURL, serviceType,
                                  mobileComponent));

  // Step 2: generate another power alarm.
  alarms.push(generateAlarmObject("power", manifestURL, serviceType,
                                  cpuComponent));

  // Step 3: clear alarm store and save new alarms to store.
  prepareAlarmStore(alarms, function(error, result) {
    // Step 4: call getAlarms.
    let options = null;
    db.getAlarms("network", options, function(error, result) {
      // Step 5: check if the function call succeed.
      do_check_eq(error, null);

      // Step 6: check results.
      // The last element in alarms array is not our expected result,
      // so pop that out first.
      alarms.pop();
      compareAlarms(alarms, result);
    });
  });
});

// Test getting all power alarms from store.
add_test(function test_getAllPowerAlarms() {
  var manifestURL = "";
  var serviceType = "";
  var alarms = [];

  // Step 1: generate two power alarms.
  alarms.push(generateAlarmObject("power", manifestURL, serviceType,
                                  cpuComponent));
  alarms.push(generateAlarmObject("power", manifestURL, serviceType,
                                  gpsComponent));

  // Step 2: generate another network alarm.
  alarms.push(generateAlarmObject("network", manifestURL, serviceType,
                                  wifiComponent));

  // Step 3: clear alarm store and save new alarms to store.
  prepareAlarmStore(alarms, function(error, result) {
    // Step 4: call getAlarms.
    let options = null;
    db.getAlarms("power", options, function(error, result) {
      // Step 5: check if the function call succeed.
      do_check_eq(error, null);

      // Step 6: check results.
      // The last element in alarms array is not our expected result,
      // so pop that out first.
      alarms.pop();
      compareAlarms(alarms, result);
    });
  });
});

// Test removing designate network alarm from store.
add_test(function test_removeNetworkAlarm() {
  var manifestURL = "";
  var serviceType = "";
  var alarms = [];

  // Step 1: generate one network alarm.
  alarms.push(generateAlarmObject("network", manifestURL, serviceType,
                                  wifiComponent));

  // Step 2: clear alarm store and save new alarms to store.
  prepareAlarmStore(alarms, function(error, result) {
    var alarmId = result;
    // Step 3: remove the alarm.
    db.removeAlarm("network", alarmId, function(error, result) {
      // Step 4: check if the function call succeed.
      do_check_eq(result, true);

      // Step 5: dump store to check if the alarm is removed.
      dumpStore("alarm_store", function(error, result) {
        do_check_eq(error, null);
        do_check_true(Array.isArray(result));
        do_check_true(result.length === 0);

        run_next_test(); // If success, run next test.
      });
    });
  });
});

// Test removing designate power alarm from store.
add_test(function test_removePowerAlarm() {
  var manifestURL = "";
  var serviceType = "";
  var alarms = [];

  // Step 1: generate one power alarm.
  alarms.push(generateAlarmObject("power", manifestURL, serviceType,
                                  cpuComponent));

  // Step 2: clear alarm store and save new alarms to store.
  prepareAlarmStore(alarms, function(error, result) {
    var alarmId = result;
    // Step 3: remove the alarm.
    db.removeAlarm("power", alarmId, function(error, result) {
      // Step 4: check if the function call succeed.
      do_check_eq(result, true);

      // Step 5: dump store to check if the alarm is removed.
      dumpStore("alarm_store", function(error, result) {
        do_check_eq(error, null);
        do_check_true(Array.isArray(result));
        do_check_true(result.length === 0);

        run_next_test(); // If success, run next test.
      });
    });
  });
});

// Test removing designate network alarm from store.
add_test(function test_removeAllNetworkAlarms() {
  var manifestURL = "";
  var serviceType = "";
  var alarms = [];

  // Step 1: generate two network alarms.
  alarms.push(generateAlarmObject("network", manifestURL, serviceType,
                                  wifiComponent));
  alarms.push(generateAlarmObject("network", manifestURL, serviceType,
                                  mobileComponent));

  // Step 2: generate another power alarm.
  alarms.push(generateAlarmObject("power", manifestURL, serviceType,
                                  cpuComponent));

  // Step 3: clear alarm store and save new alarms to store.
  prepareAlarmStore(alarms, function(error, result) {
    // Step 4: remove all network alarms.
    db.removeAllAlarms("network", function(error, result) {
      // Step 5: check if the function call succeed.
      do_check_eq(error, null);

      // Step 6: dump store for comparison.
      // Because the power alarm should not be removed, so it would be the
      // only result returned by dumpStore.
      var alarm = alarms.pop(); // The expected result.
      dumpStore("alarm_store", function(error, result) {
        do_check_eq(error, null);
        do_check_true(Array.isArray(result));
        do_check_true(result.length == 1);
        do_check_eq(result[0].type, alarm.type);
        do_check_eq(result[0].manifestURL, alarm.manifestURL);
        do_check_eq(result[0].serviceType, alarm.serviceType);
        do_check_eq(result[0].component, alarm.component);
        do_check_eq(result[0].threshold, alarm.threshold);
        do_check_eq(result[0].startTime, alarm.startTime);
        do_check_eq(result[0].data, alarm.data);
        do_check_eq(result[0].alarmId, alarm.alarmId);

        run_next_test(); // If success, run next test.
      });
    });
  });
});

// Test removing designate power alarm from store.
add_test(function test_removeAllPowerAlarms() {
  var manifestURL = "";
  var serviceType = "";
  var alarms = [];

  // Step 1: generate two power alarms.
  alarms.push(generateAlarmObject("power", manifestURL, serviceType,
                                  cpuComponent));
  alarms.push(generateAlarmObject("power", manifestURL, serviceType,
                                  gpsComponent));

  // Step 2: generate another network alarm.
  alarms.push(generateAlarmObject("network", manifestURL, serviceType,
                                  wifiComponent));

  // Step 3: clear alarm store and save new alarms to store.
  prepareAlarmStore(alarms, function(error, result) {
    // Step 4: remove all power alarms.
    db.removeAllAlarms("power", function(error, result) {
      // Step 5: check if the function call succeed.
      do_check_eq(error, null);

      // Step 6: dump store for comparison.
      // Because the network alarm should not be removed, so it would be the
      // only result returned by dumpStore.
      var alarm = alarms.pop(); // The expected result.
      dumpStore("alarm_store", function(error, result) {
        do_check_eq(error, null);
        do_check_true(Array.isArray(result));
        do_check_true(result.length == 1);
        do_check_eq(result[0].type, alarm.type);
        do_check_eq(result[0].manifestURL, alarm.manifestURL);
        do_check_eq(result[0].serviceType, alarm.serviceType);
        do_check_eq(result[0].component, alarm.component);
        do_check_eq(result[0].threshold, alarm.threshold);
        do_check_eq(result[0].startTime, alarm.startTime);
        do_check_eq(result[0].data, alarm.data);
        do_check_eq(result[0].alarmId, alarm.alarmId);

        run_next_test(); // If success, run next test.
      });
    });
  });
});

function run_test() {
  do_get_profile();
  run_next_test();
}
