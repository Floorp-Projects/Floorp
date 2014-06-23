/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

// Emulator uses rssi = 7 as default value.
const DEFAULT_RSSI = 7;

const TEST_DATA = [
  // All invalid case.
  {
    input: {
      rxlev: 99,
      rsrp: 65535,
      rssnr: 65535
    },
    expect: {
      signalStrength: null,
      relSignalStrength: null
    }
  },
  // Valid rxlev with max value.
  {
    input: {
      rxlev: 63,
      rsrp: 65535,
      rssnr: 65535
    },
    expect: {
      signalStrength: -48,
      relSignalStrength: 100
    }
  },
  // Valid rxlev.
  {
    input: {
      rxlev: 12,
      rsrp: 65535,
      rssnr: 65535
    },
    expect: {
      signalStrength: -99,
      relSignalStrength: 100
    }
  },
  // Valid rxlev with min value.
  {
    input: {
      rxlev: 0,
      rsrp: 65535,
      rssnr: 65535
    },
    expect: {
      signalStrength: -111,
      relSignalStrength: 0
    }
  }
];

function testInitialSignalStrengthInfo() {
  log("Test initial signal strength info");

  let voice = mobileConnection.voice;
  // Android emulator initializes the signal strength to -99 dBm
  is(voice.signalStrength, -99, "check voice.signalStrength");
  is(voice.relSignalStrength, 44, "check voice.relSignalStrength");
}

function testLteSignalStrength(aInput, aExpect) {
  log("Test setting LTE signal strength to " + JSON.stringify(aInput));

  return setEmulatorLteSignalStrengthAndWait(aInput.rxlev, aInput.rsrp, aInput.rssnr)
    .then(() => {
      let voice = mobileConnection.voice;
      is(voice.signalStrength, aExpect.signalStrength,
         "check voice.signalStrength");
      is(voice.relSignalStrength, aExpect.relSignalStrength,
         "check voice.relSignalStrength");
    });
}

// Start tests
startTestCommon(function() {
  // Test initial status
  testInitialSignalStrengthInfo();

  // Test Unsolicited Signal Strength Events for LTE
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => testLteSignalStrength(data.input,
                                                       data.expect));
  }

  // Reset Signal Strength Info to default
  return promise.then(() => setEmulatorGsmSignalStrengthAndWait(DEFAULT_RSSI));
});
