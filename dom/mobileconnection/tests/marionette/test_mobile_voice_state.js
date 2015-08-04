/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [{
    // Test state becomes to "unregistered"
    state: "unregistered",
    expected: {
      state: "notSearching",
      connected: false,
      emergencyCallsOnly: true,
      roaming: false,
      signalStrength: null,
      relSignalStrength: null,
      cell: null
    }
  }, {
    // Test state becomes to "searching"
    state: "searching",
    expected: {
      state: "searching",
      connected: false,
      emergencyCallsOnly: true,
      roaming: false,
      signalStrength: null,
      relSignalStrength: null,
      cell: null
    }
  }, {
    // Test state becomes to "denied"
    state: "denied",
    expected: {
      state: "denied",
      connected: false,
      emergencyCallsOnly: true,
      roaming: false,
      signalStrength: null,
      relSignalStrength: null,
      cell: null
    }
  }, {
    // Test state becomes to "roaming"
    state: "roaming",
    expected: {
      state: "registered",
      connected: true,
      emergencyCallsOnly: false,
      roaming: true,
      signalStrength: -99,
      relSignalStrength: 44,
      cell: {
        gsmLocationAreaCode: 0,
        gsmCellId: 0
      }
    }
  }, {
    // Reset state to default value.
    state: "home",
    expected: {
      state: "registered",
      connected: true,
      emergencyCallsOnly: false,
      roaming: false,
      signalStrength: -99,
      relSignalStrength: 44,
      cell: {
        gsmLocationAreaCode: 0,
        gsmCellId: 0
      }
    }
  }
];

function compareTo(aPrefix, aFrom, aTo) {
  for (let field in aTo) {
    let fullName = aPrefix + field;

    let lhs = aFrom[field];
    let rhs = aTo[field];
    ok(true, "lhs=" + JSON.stringify(lhs) + ", rhs=" + JSON.stringify(rhs));
    if (typeof rhs !== "object") {
      is(lhs, rhs, fullName);
    } else if (rhs) {
      ok(lhs, fullName);
      compareTo(fullName + ".", lhs, rhs);
    } else {
      is(lhs, null, fullName);
    }
  }
}

function verifyVoiceInfo(aExpected) {
  compareTo("voice.", mobileConnection.voice, aExpected);
}

/* Test Voice State Changed */
function testVoiceStateUpdate(aNewState, aExpected) {
  log("Test voice info with state='" + aNewState + "'");

  // Set emulator's lac/cid and wait for 'onvoicechange' event.
  return setEmulatorVoiceDataStateAndWait("voice", aNewState)
    .then(() => verifyVoiceInfo(aExpected));
}

startTestCommon(function() {
  log("Test initial voice connection info");

  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let entry = TEST_DATA[i];
    promise =
      promise.then(testVoiceStateUpdate.bind(null, entry.state, entry.expected));
  }

  return promise;
});
