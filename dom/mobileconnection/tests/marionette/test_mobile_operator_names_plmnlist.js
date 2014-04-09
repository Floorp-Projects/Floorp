/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function check(aLongName, aShortName, aMcc, aMnc) {
  let network = mobileConnection.voice.network;

  is(network.longName, aLongName, "network.longName");
  is(network.shortName, aShortName, "network.shortName");
  is(network.mcc, aMcc, "network.mcc");
  is(network.mnc, aMnc, "network.mnc");
}

function test(aLongName, aShortName, aMcc, aMnc, aExpectedLongName,
              aExpectedShortName) {
  log("Testing mcc = " + aMcc + ", mnc = " + aMnc + ":");

  let promises = [];
  promises.push(waitForManagerEvent("voicechange"));
  promises.push(setEmulatorOperatorNames("home", aLongName, aShortName, aMcc, aMnc));
  return Promise.all(promises)
    .then(() => check(aExpectedLongName, aExpectedShortName, aMcc, aMnc));
}

startTestCommon(function() {
  return getEmulatorOperatorNames()
    .then(function(aOperators) {
      let {longName: longName, shortName: shortName} = aOperators[0];
      let {mcc: mcc, mnc: mnc} = mobileConnection.voice.network;
      return Promise.resolve()

        .then(() => test(longName, shortName, "123", "456", longName, shortName))
        .then(() => test(longName, shortName, "310", "070", "AT&T", ""))

        // Reset back to initial values.
        .then(() => test(longName, shortName, mcc, mnc, longName, shortName));
    });
});
