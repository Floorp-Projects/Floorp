/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "mobile_header.js";

/* Emulator command for GSM/UMTS signal strength */
function setEmulatorGsmSignalStrength(rssi) {
  emulatorHelper.sendCommand("gsm signal " + rssi);
}

/* Emulator command for LTE signal strength */
function setEmulatorLteSignalStrength(rxlev, rsrp, rssnr) {
  let lteSignal = rxlev + " " + rsrp + " " + rssnr;
  emulatorHelper.sendCommand("gsm lte_signal " + lteSignal);
}

function waitForVoiceChangeEvent(callback) {
  mobileConnection.addEventListener("voicechange", function onvoicechange() {
    mobileConnection.removeEventListener("voicechange", onvoicechange);

    if (callback && typeof callback === "function") {
      callback();
    }
  });
}

/* Test Initial Signal Strength Info */
taskHelper.push(function testInitialSignalStrengthInfo() {
  log("Test initial signal strength info");

  let voice = mobileConnection.voice;
  // Android emulator initializes the signal strength to -99 dBm
  is(voice.signalStrength, -99, "check voice.signalStrength");
  is(voice.relSignalStrength, 44, "check voice.relSignalStrength");

  taskHelper.runNext();
});

/* Test Unsolicited Signal Strength Events for LTE */
taskHelper.push(function testLteSignalStrength() {
  // Set emulator's LTE signal strength and wait for 'onvoicechange' event.
  function doTestLteSignalStrength(input, expect, callback) {
    log("Test LTE signal info with data : " + JSON.stringify(input));

    waitForVoiceChangeEvent(function() {
      let voice = mobileConnection.voice;
      is(voice.signalStrength, expect.signalStrength,
         "check voice.signalStrength");
      is(voice.relSignalStrength, expect.relSignalStrength,
         "check voice.relSignalStrength");

      if (callback && typeof callback === "function") {
        callback();
      }
    });

    setEmulatorLteSignalStrength(input.rxlev, input.rsrp, input.rssnr);
  }

  let testData = [
    // All invalid case.
    {input: {
      rxlev: 99,
      rsrp: 65535,
      rssnr: 65535},
     expect: {
      signalStrength: null,
      relSignalStrength: null}
    },
    // Valid rxlev with max value.
    {input: {
      rxlev: 63,
      rsrp: 65535,
      rssnr: 65535},
     expect: {
      signalStrength: -48,
      relSignalStrength: 100}
    },
    // Valid rxlev.
    {input: {
      rxlev: 12,
      rsrp: 65535,
      rssnr: 65535},
     expect: {
      signalStrength: -99,
      relSignalStrength: 100}
    },
    // Valid rxlev with min value.
    {input: {
      rxlev: 0,
      rsrp: 65535,
      rssnr: 65535},
     expect: {
      signalStrength: -111,
      relSignalStrength: 0}
    }
  ];

  // Run all test data.
  (function do_call() {
    let next = testData.shift();
    if (!next) {
      taskHelper.runNext();
      return;
    }
    doTestLteSignalStrength(next.input, next.expect, do_call);
  })();
});

/* Reset Signal Strength Info to default, and finsih the test */
taskHelper.push(function testResetSignalStrengthInfo() {
  // Reset emulator's signal strength and wait for 'onvoicechange' event.
  function doResetSignalStrength(rssi) {
    waitForVoiceChangeEvent(function() {
      let voice = mobileConnection.voice;
      is(voice.signalStrength, -99, "check voice.signalStrength");
      is(voice.relSignalStrength, 44, "check voice.relSignalStrength");

      taskHelper.runNext();
    });

    setEmulatorGsmSignalStrength(rssi);
  }

  // Emulator uses rssi = 7 as default value, and we need to reset it after
  // finishing test in case other test cases need those values for testing.
  doResetSignalStrength(7);
});

// Start test
taskHelper.runNext();
