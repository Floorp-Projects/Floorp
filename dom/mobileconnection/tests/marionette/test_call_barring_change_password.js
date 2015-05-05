/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testChangeCallBarringPassword(aExpectedError, aOptions) {
  log("Test changing call barring password from " +
      aOptions.pin + " to " + aOptions.newPin);

  return changeCallBarringPassword(aOptions)
    .then(function resolve() {
      ok(!aExpectedError, "changeCallBarringPassword success");
    }, function reject(aError) {
      is(aError.name, aExpectedError, "failed to changeCallBarringPassword");
    });
}

// Start tests
startTestCommon(function() {
  return Promise.resolve()

    // According to TS.22.004 clause 5.2, the password should consist of four
    // digits in the range 0000 to 9999.

    // Test passing an invalid pin.
    .then(() => testChangeCallBarringPassword("InvalidPassword", {
      pin: null,
      newPin: "0000"
    }))

    .then(() => testChangeCallBarringPassword("InvalidPassword", {
      pin: "000",
      newPin: "0000"
    }))

    .then(() => testChangeCallBarringPassword("InvalidPassword", {
      pin: "00000",
      newPin: "0000"
    }))

    .then(() => testChangeCallBarringPassword("InvalidPassword", {
      pin: "abcd",
      newPin: "0000"
    }))

    // Test passing an invalid newPin
    .then(() => testChangeCallBarringPassword("InvalidPassword", {
      pin: "0000",
      newPin: null
    }))

    .then(() => testChangeCallBarringPassword("InvalidPassword", {
      pin: "0000",
      newPin: "000"
    }))

    .then(() => testChangeCallBarringPassword("InvalidPassword", {
      pin: "0000",
      newPin: "00000"
    }))

    .then(() => testChangeCallBarringPassword("InvalidPassword", {
      pin: "0000",
      newPin: "abcd"
    }))

    // Test passing an incorrect password, where the default password is "0000".
    .then(() => testChangeCallBarringPassword("IncorrectPassword", {
      pin: "1111",
      newPin: "2222"
    }))

    // Sucessful
    .then(() => testChangeCallBarringPassword(null, {
      pin: "0000",
      newPin: "2222"
    }))

    .then(() => testChangeCallBarringPassword(null, {
      pin: "2222",
      newPin: "0000"
    }));
});
