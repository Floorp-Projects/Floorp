/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function check(aLongName, aShortName) {
  let network = mobileConnection.voice.network;

  is(network.longName, aLongName, "network.longName");
  is(network.shortName, aShortName, "network.shortName");
  is(network.mcc, "310", "network.mcc");
  is(network.mnc, "260", "network.mnc");
}

function test(aLongName, aShortName) {
  log("Testing '" + aLongName + "', '" + aShortName + "':");

  return setEmulatorOperatorNamesAndWait("home", aLongName, aShortName,
                                         null, null, true, false)
    .then(() => check(aLongName, aShortName));
}

startTestCommon(function() {
  return getEmulatorOperatorNames()
    .then(function(aOperators) {
      return Promise.resolve()

        .then(() => test("Mozilla", "B2G"))
        .then(() => test("Mozilla", ""))
        .then(() => test("", "B2G"))
        .then(() => test("", ""))

        // Reset back to initial values.
        .then(() => test(aOperators[0].longName, aOperators[0].shortName));
    });
});
