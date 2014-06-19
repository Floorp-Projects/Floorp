/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // [<pin>, <new pin>, <expected error>]

  // Test passing an invalid pin or newPin.
  [null, "0000", "InvalidPassword"],
  ["0000", null, "InvalidPassword"],
  [null, null, "InvalidPassword"],

  // Test passing mismatched newPin.
  ["000", "0000", "InvalidPassword"],
  ["00000", "1111", "InvalidPassword"],
  ["abcd", "efgh", "InvalidPassword"],

  // TODO: Bug 906603 - B2G RIL: Support Change Call Barring Password on Emulator.
  // Currently emulator doesn't support REQUEST_CHANGE_BARRING_PASSWORD, so we
  // expect to get a 'RequestNotSupported' error here.
  ["1234", "1234", "RequestNotSupported"]
];

function testChangeCallBarringPassword(aPin, aNewPin, aExpectedError) {
  log("Test changing call barring password to " + aPin + "/" + aNewPin);

  let options = {
    pin: aPin,
    newPin: aNewPin
  };
  return changeCallBarringPassword(options)
    .then(function resolve() {
      ok(!aExpectedError, "changeCallBarringPassword success");
    }, function reject(aError) {
      is(aError.name, aExpectedError, "failed to changeCallBarringPassword");
    });
}

// Start tests
startTestCommon(function() {
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise =
      promise.then(() => testChangeCallBarringPassword(data[0], data[1], data[2]));
  }
  return promise;
});
