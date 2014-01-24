/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "mobile_header.js";

function setEmulatorDataState(state) {
  emulatorHelper.sendCommand("gsm data " + state);
}

function waitForDataChangeEvent(callback) {
  mobileConnection.addEventListener("datachange", function ondatachange() {
    mobileConnection.removeEventListener("datachange", ondatachange);

    if (callback && typeof callback === "function") {
      callback();
    }
  });
}

/* Test Initial Connection Info */
taskHelper.push(function testInitialDataInfo() {
  log("Test initial data connection info");

  let data = mobileConnection.data;
  // |data.connected| reports true only if the "default" data connection is
  // established.
  is(data.connected, false, "check data.connected");
  is(data.state, "registered", "check data.state");
  is(data.emergencyCallsOnly, false, "check data.emergencyCallsOnly");
  is(data.roaming, false, "check data.roaming");
  // Android emulator initializes the signal strength to -99 dBm
  is(data.signalStrength, -99, "check data.signalStrength");
  is(data.relSignalStrength, 44, "check data.relSignalStrength");

  let cell = data.cell;
  ok(data.cell, "location available");
  // Initial LAC/CID. Android emulator initializes both value to
  // 0xffff/0xffffffff.
  is(cell.gsmLocationAreaCode, 65535, "check data.cell.gsmLocationAreaCode");
  is(cell.gsmCellId, 268435455, "check data.cell.gsmCellId");
  is(cell.cdmaBaseStationId, -1, "check data.cell.cdmaBaseStationId");
  is(cell.cdmaBaseStationLatitude, -2147483648,
     "check data.cell.cdmaBaseStationLatitude");
  is(cell.cdmaBaseStationLongitude, -2147483648,
     "check data.cell.cdmaBaseStationLongitude");
  is(cell.cdmaSystemId, -1, "check data.cell.cdmaSystemId");
  is(cell.cdmaNetworkId, -1, "check data.cell.cdmaNetworkId");

  taskHelper.runNext();
});

/* Test Data State Changed */
taskHelper.push(function testDataStateUpdate() {
  // Set emulator's lac/cid and wait for 'ondatachange' event.
  function doTestDataState(state, expect, callback) {
    log("Test data info with state='" + state + "'");

    waitForDataChangeEvent(function() {
      let data = mobileConnection.data;
      is(data.state, expect.state, "check data.state");
      is(data.connected, expect.connected, "check data.connected");
      is(data.emergencyCallsOnly, expect.emergencyCallsOnly,
         "check data.emergencyCallsOnly");
      is(data.roaming, expect.roaming, "check data.roaming");
      is(data.signalStrength, expect.signalStrength,
         "check data.signalStrength");
      is(data.relSignalStrength, expect.relSignalStrength,
         "check data.relSignalStrength");

      let cell = data.cell;
      if (!expect.cell) {
        ok(!cell, "check data.cell");
      } else {
        is(cell.gsmLocationAreaCode, expect.cell.gsmLocationAreaCode,
           "check data.cell.gsmLocationAreaCode");
        is(cell.gsmCellId, expect.cell.gsmCellId, "check data.cell.gsmCellId");
        is(cell.cdmaBaseStationId, -1, "check data.cell.cdmaBaseStationId");
        is(cell.cdmaBaseStationLatitude, -2147483648,
           "check data.cell.cdmaBaseStationLatitude");
        is(cell.cdmaBaseStationLongitude, -2147483648,
           "check data.cell.cdmaBaseStationLongitude");
        is(cell.cdmaSystemId, -1, "check data.cell.cdmaSystemId");
        is(cell.cdmaNetworkId, -1, "check data.cell.cdmaNetworkId");
      }

      if (callback && typeof callback === "function") {
        callback();
      }
    });

    setEmulatorDataState(state);
  }

  let testData = [
    // Test state becomes to "unregistered"
    {state: "unregistered",
     expect: {
      state: "notSearching",
      connected: false,
      emergencyCallsOnly: true,
      roaming: false,
      signalStrength: null,
      relSignalStrength: null,
      cell: null
    }},
    // Test state becomes to "searching"
    {state: "searching",
     expect: {
      state: "searching",
      connected: false,
      emergencyCallsOnly: true,
      roaming: false,
      signalStrength: null,
      relSignalStrength: null,
      cell: null
    }},
    // Test state becomes to "denied"
    {state: "denied",
     expect: {
      state: "denied",
      connected: false,
      emergencyCallsOnly: true,
      roaming: false,
      signalStrength: null,
      relSignalStrength: null,
      cell: null
    }},
    // Test state becomes to "roaming"
    // Set emulator's data state to "roaming" won't change the operator's
    // long_name/short_name/mcc/mnc, so that the |data.roaming| will still
    // report false. Please see bug 787967.
    {state: "roaming",
     expect: {
      state: "registered",
      connected: false,
      emergencyCallsOnly: false,
      roaming: false,
      signalStrength: -99,
      relSignalStrength: 44,
      cell: {
        gsmLocationAreaCode: 65535,
        gsmCellId: 268435455
    }}},
    // Reset state to default value.
    {state: "home",
     expect: {
      state: "registered",
      connected: false,
      emergencyCallsOnly: false,
      roaming: false,
      signalStrength: -99,
      relSignalStrength: 44,
      cell: {
        gsmLocationAreaCode: 65535,
        gsmCellId: 268435455
    }}}
  ];

  // Run all test data.
  (function do_call() {
    let next = testData.shift();
    if (!next) {
      taskHelper.runNext();
      return;
    }
    doTestDataState(next.state, next.expect, do_call);
  })();
});

// Start test
taskHelper.runNext();
