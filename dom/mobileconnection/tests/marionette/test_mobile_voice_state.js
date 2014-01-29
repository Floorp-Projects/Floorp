/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "mobile_header.js";

function setEmulatorVoiceState(state) {
  emulatorHelper.sendCommand("gsm voice " + state);
}

function waitForVoiceChangeEvent(callback) {
  mobileConnection.addEventListener("voicechange", function onvoicechange() {
    mobileConnection.removeEventListener("voicechange", onvoicechange);

    if (callback && typeof callback === "function") {
      callback();
    }
  });
}

/* Test Initial Connection Info */
taskHelper.push(function testInitialVoiceInfo() {
  log("Test initial voice connection info");

  let voice = mobileConnection.voice;
  is(voice.state, "registered", "check voice.state");
  is(voice.connected, true, "check voice.connected");
  is(voice.emergencyCallsOnly, false, "check voice.emergencyCallsOnly");
  is(voice.roaming, false, "check voice.roaming");
  // Android emulator initializes the signal strength to -99 dBm
  is(voice.signalStrength, -99, "check voice.signalStrength");
  is(voice.relSignalStrength, 44, "check voice.relSignalStrength");

  let cell = voice.cell;
  ok(cell, "location available");
  // Initial LAC/CID. Android emulator initializes both value to
  // 0xffff/0xffffffff.
  is(cell.gsmLocationAreaCode, 65535, "check voice.cell.gsmLocationAreaCode");
  is(cell.gsmCellId, 268435455, "check voice.cell.gsmCellId");
  is(cell.cdmaBaseStationId, -1, "check voice.cell.cdmaBaseStationId");
  is(cell.cdmaBaseStationLatitude, -2147483648,
     "check voice.cell.cdmaBaseStationLatitude");
  is(cell.cdmaBaseStationLongitude, -2147483648,
     "check voice.cell.cdmaBaseStationLongitude");
  is(cell.cdmaSystemId, -1, "check voice.cell.cdmaSystemId");
  is(cell.cdmaNetworkId, -1, "check voice.cell.cdmaNetworkId");

  taskHelper.runNext();
});

/* Test Voice State Changed */
taskHelper.push(function testVoiceStateUpdate() {
  // Set emulator's lac/cid and wait for 'onvoicechange' event.
  function doTestVoiceState(state, expect, callback) {
    log("Test voice info with state='" + state + "'");

    waitForVoiceChangeEvent(function() {
      let voice = mobileConnection.voice;
      is(voice.state, expect.state, "check voice.state");
      is(voice.connected, expect.connected, "check voice.connected");
      is(voice.emergencyCallsOnly, expect.emergencyCallsOnly,
         "check voice.emergencyCallsOnly");
      is(voice.roaming, expect.roaming, "check voice.roaming");
      is(voice.signalStrength, expect.signalStrength,
         "check voice.signalStrength");
      is(voice.relSignalStrength, expect.relSignalStrength,
         "check voice.relSignalStrength");

      let cell = voice.cell;
      if (!expect.cell) {
        ok(!cell, "check voice.cell");
      } else {
        is(cell.gsmLocationAreaCode, expect.cell.gsmLocationAreaCode,
           "check voice.cell.gsmLocationAreaCode");
        is(cell.gsmCellId, expect.cell.gsmCellId, "check voice.cell.gsmCellId");
        is(cell.cdmaBaseStationId, -1, "check voice.cell.cdmaBaseStationId");
        is(cell.cdmaBaseStationLatitude, -2147483648,
           "check voice.cell.cdmaBaseStationLatitude");
        is(cell.cdmaBaseStationLongitude, -2147483648,
           "check voice.cell.cdmaBaseStationLongitude");
        is(cell.cdmaSystemId, -1, "check voice.cell.cdmaSystemId");
        is(cell.cdmaNetworkId, -1, "check voice.cell.cdmaNetworkId");
      }

      if (callback && typeof callback === "function") {
        callback();
      }
    });

    setEmulatorVoiceState(state);
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
    {state: "roaming",
     expect: {
      state: "registered",
      connected: true,
      emergencyCallsOnly: false,
      roaming: true,
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
      connected: true,
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
    doTestVoiceState(next.state, next.expect, do_call);
  })();
});

// Start test
taskHelper.runNext();
