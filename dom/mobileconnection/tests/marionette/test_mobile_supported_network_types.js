/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function getSupportedNetworkTypesFromSystemProperties(aClientId) {
  let key = "ro.moz.ril." + aClientId + ".network_types";

  return runEmulatorShellCmdSafe(["getprop", key])
    .then(function resolve(aResults) {
      let result = aResults[0];
      if (!result || result === "") {
        // Copied from GECKO_SUPPORTED_NETWORK_TYPES_DEFAULT in
        // dom/system/gonk/ril_consts.js.
        result = "wcdma,gsm";
      }
      return result.split(",");
    });
}

// Start test
startTestCommon(function() {
  return Promise.resolve()
    // Get SupportedNetworkTypes from system properties.
    .then(() => getSupportedNetworkTypesFromSystemProperties(0))
    .then((testData) => {
      let supportedNetworkTypes = mobileConnection.supportedNetworkTypes;
      ok(Array.isArray(supportedNetworkTypes),
         "supportedNetworkTypes should be an array");

      is(testData.length, supportedNetworkTypes.length);
      for (let i = 0; i < testData.length; i++) {
        ok(supportedNetworkTypes.indexOf(testData[i]) >= 0,
           "Should support '" + testData[i] + "'");
      }
    });
});
