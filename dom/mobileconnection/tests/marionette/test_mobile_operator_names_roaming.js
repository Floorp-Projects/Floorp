/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function check(aLongName, aShortName, aRoaming) {
  let voice = mobileConnection.voice;
  let network = voice.network;

  is(network.longName, aLongName, "network.longName");
  is(network.shortName, aShortName, "network.shortName");
  is(voice.roaming, aRoaming, "voice.roaming");
}

// See bug 797972 - B2G RIL: False roaming situation
//
// Steps to test:
// 1. set roaming operator names
// 2. set emulator roaming
// 3. wait for onvoicechange event and test passing conditions
// 4. set emulator roaming back to false
// 5. wait for onvoicechange event again and callback
function test(aLongName, aShortName, aRoaming) {
  log("Testing roaming check '" + aLongName + "', '" + aShortName + "':");

  return Promise.resolve()

    // We should not have voicechange here, but, yes, we do.
    .then(() => setEmulatorOperatorNamesAndWait("roaming", aLongName, aShortName,
                                                null, null, true, false))

    .then(() => setEmulatorVoiceDataStateAndWait("voice", "roaming"))
    .then(() => check(aLongName, aShortName, aRoaming))
    .then(() => setEmulatorVoiceDataStateAndWait("voice", "home"));
}

startTestCommon(function() {
  return getEmulatorOperatorNames()
    .then(function(aOperators) {
      let {longName: longName, shortName: shortName} = aOperators[0];

      return Promise.resolve()

        // If Either long name or short name of current registered operator
        // matches SPN("Android"), then the `roaming` attribute should be set
        // to false.
        .then(() => test(longName,               shortName,               false))
        .then(() => test(longName,               shortName.toLowerCase(), false))
        .then(() => test(longName,               "Bar",                   false))
        .then(() => test(longName.toLowerCase(), shortName,               false))
        .then(() => test(longName.toLowerCase(), shortName.toLowerCase(), false))
        .then(() => test(longName.toLowerCase(), "Bar",                   false))
        .then(() => test("Foo",                  shortName,               false))
        .then(() => test("Foo",                  shortName.toLowerCase(), false))
        .then(() => test("Foo",                  "Bar",                   true))

        // Reset roaming operator.
        .then(() => setEmulatorOperatorNamesAndWait("roaming",
                                                    aOperators[1].longName,
                                                    aOperators[1].shortName));
    });
});
