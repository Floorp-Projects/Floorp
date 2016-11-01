/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testSetCallingLineIdRestriction(aMode, aExpectedErrorMsg) {
  log("Test setting calling line id restriction mode to " + aMode);

  return setClir(aMode)
    .then(function resolve() {
      ok(!aExpectedErrorMsg, "setCallingLineIdRestriction success");
    }, function reject(aError) {
      is(aError.name, aExpectedErrorMsg,
         "failed to setCallingLineIdRestriction");
    });
}

// TODO: Bug 999374 - [B2G] [Emulator] support setClir and getClir.
// Currently emulator doesn't support REQUEST_GET_CLIR, so we expect to
// get a 'RequestNotSupported' error here.
function testGetCallingLineIdRestriction() {
  log("Test getting calling line id restriction mode");

  return getClir()
    .then(function resolve() {
      ok(false, "getCallingLineIdRestriction should not success");
    }, function reject(aError) {
      is(aError.name, "RequestNotSupported",
         "failed to getCallingLineIdRestriction");
    });
}

// Start tests
startTestCommon(function() {
  return Promise.resolve()
    // Test set calling line id restriction mode.
    // TODO: Bug 999374 - [B2G] [Emulator] support setClir and getClir.
    // Currently emulator doesn't support REQUEST_SET_CLIR, so we expect to get
    // a 'RequestNotSupported' error here.
    .then(() => testSetCallingLineIdRestriction(
                  MozMobileConnection.CLIR_DEFAULT, "RequestNotSupported"))
    .then(() => testSetCallingLineIdRestriction(
                  MozMobileConnection.CLIR_INVOCATION, "RequestNotSupported"))
    .then(() => testSetCallingLineIdRestriction(
                  MozMobileConnection.CLIR_SUPPRESSION, "RequestNotSupported"))

    // Test passing an invalid mode.
    .then(() => testSetCallingLineIdRestriction(10 /* Invalid mode */,
                                                "InvalidParameter"))

    // Test get calling line id restriction mode.
    .then(() => testGetCallingLineIdRestriction());
});
