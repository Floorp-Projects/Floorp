/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

// TODO: Bug 999369 - B2G Emulator: Support voice privacy.
// Currently emulator doesn't support
// REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE and run as gsm mode by default,
// so we expect to get a 'RequestNotSupported' error here.
function testSetVoicePrivacyMode(aEnabled) {
  log("Test setting voice privacy mode to " + aEnabled);

  return setVoicePrivacyMode(aEnabled)
    .then(function resolve() {
      ok(false, "setVoicePrivacyMode should not success");
    }, function reject(aError) {
      is(aError.name, "RequestNotSupported", "failed to setVoicePrivacyMode");
    });
}

// TODO: Bug 999369 - B2G Emulator: Support voice privacy.
// Currently emulator doesn't support
// RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE and run as gsm mode by
// default, so we expect to get a 'RequestNotSupported' error here.
function testGetVoicePrivacyMode() {
  log("Test getting voice privacy mode");

  return getVoicePrivacyMode()
    .then(function resolve() {
      ok(false, "getVoicePrivacyMode should not success");
    }, function reject(aError) {
      is(aError.name, "RequestNotSupported", "failed to getVoicePrivacyMode");
    });
}

// Start tests
startTestCommon(function() {
  return Promise.resolve()
    .then(() => testSetVoicePrivacyMode(true))
    .then(() => testGetVoicePrivacyMode());
});
