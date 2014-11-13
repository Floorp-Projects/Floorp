/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

// PIN is hardcoded as "0000" by default.
// See it here {B2G_HOME}/external/qemu/telephony/sim_card.c,
// in asimcard_create().
const TEST_DATA = [
  // Test passing no pin.
  {
    pin: "",
    newPin: "0000",
    newPinAgain: "1111",
    expectedError: {
      name: "emMmiError",
      additionalInformation: null
    }
  },
  // Test passing no newPin.
  {
    pin: "0000",
    newPin: "",
    newPinAgain: "",
    expectedError: {
      name: "emMmiError",
      additionalInformation: null
    }
  },
  // Test passing mismatched newPin.
  {
    pin: "0000",
    newPin: "0000",
    newPinAgain: "1111",
    expectedError: {
      name: "emMmiErrorMismatchPin",
      additionalInformation: null
    }
  },
  // Test passing invalid pin (< 4 digit).
  {
    pin: "000",
    newPin: "0000",
    newPinAgain: "0000",
    expectedError: {
      name: "emMmiErrorInvalidPin",
      additionalInformation: null
    }
  },
  // Test passing invalid newPin (> 8 digit).
  {
    pin: "0000",
    newPin: "000000000",
    newPinAgain: "000000000",
    expectedError: {
      name: "emMmiErrorInvalidPin",
      additionalInformation: null
    }
  },
  // Test passing incorrect pin.
  {
    pin: "1234",
    newPin: "0000",
    newPinAgain: "0000",
    expectedError: {
      name: "emMmiErrorBadPin",
      // The default pin retries is 3, failed once becomes to 2
      additionalInformation: 2
    }
  },
  // Test changing pin successfully (Reset the retries).
  {
    pin: "0000",
    newPin: "0000",
    newPinAgain: "0000"
  }
];

function testChangePin(aPin, aNewPin, aNewPinAgain, aExpectedError) {
  let MMI_CODE = "**04*" + aPin + "*" + aNewPin + "*" + aNewPinAgain + "#";
  log("Test " + MMI_CODE);

  return gSendMMI(MMI_CODE).then(aResult => {
    is(aResult.success, !aExpectedError, "check success");
    is(aResult.serviceCode, "scPin", "Check service code");

    if (aResult.success) {
      is(aResult.statusMessage, "smPinChanged", "Check status message");
      is(aResult.additionalInformation, undefined, "Check additional information");
    } else {
      is(aResult.statusMessage, aExpectedError.name, "Check name");
      is(aResult.additionalInformation, aExpectedError.additionalInformation,
         "Check additional information");
    }
  });
}

// Start test
startTest(function() {
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => testChangePin(data.pin,
                                               data.newPin,
                                               data.newPinAgain,
                                               data.expectedError));
  }

  return promise
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
