/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

// Start tests
startTestCommon(function() {
  return Promise.resolve()
    // TODO: Bug 1090811 - [B2G] support SET_CALL_WAITING and QUERY_CALL_WAITING
    // Currently emulator doesn't support RIL_REQUEST_QUERY_CALL_WAITING and
    // RIL_REQUEST_SET_CALL_WAITING, so we expect to get a 'RequestNotSupported'
    // error here.
    .then(() => setCallWaitingOption(true))
    .then(() => {
      ok(false, "setCallWaitingOption should not success");
    }, aError => {
      is(aError.name, "RequestNotSupported",
         "failed to setCallWaitingOption");
    })

    .then(() => getCallWaitingOption())
    .then(() => {
      ok(false, "getCallWaitingOption should not success");
    }, aError => {
      is(aError.name, "RequestNotSupported",
         "failed to getCallWaitingOption");
    });
});
