/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

let networkManager =
  Cc["@mozilla.org/network/manager;1"].getService(Ci.nsINetworkManager);
ok(networkManager,
   "networkManager.constructor is " + networkManager.constructor);

function testInitialState() {
  return getSettings(SETTINGS_KEY_DATA_ENABLED)
    .then((enabled) => {
      is(enabled, false, "data should be off by default");
      is(networkManager.active, null,
         "networkManager.active should be null by default");
    });
}

function testActiveNetworkChangedBySwitchingDataCall(aDataCallEnabled) {
  log("Test active network by switching dataCallEnabled to " + aDataCallEnabled);

  let promises = [];
  promises.push(waitForObserverEvent(TOPIC_NETWORK_ACTIVE_CHANGED));
  promises.push(setSettings(SETTINGS_KEY_DATA_ENABLED, aDataCallEnabled));

  return Promise.all(promises).then(function(results) {
    let subject = results[0];

    if (aDataCallEnabled) {
      ok(subject instanceof Ci.nsINetworkInterface,
         "subject should be an instance of nsINetworkInterface");
      ok(subject instanceof Ci.nsIRilNetworkInterface,
         "subject should be an instance of nsIRILNetworkInterface");
      is(subject.type, Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
         "subject.type should be NETWORK_TYPE_MOBILE");
    }

    is(subject, networkManager.active,
       "subject should be equal with networkManager.active");
  });
}

// Start test
startTestBase(function() {
  return testInitialState()
    // Test active network changed by enabling data call.
    .then(() => testActiveNetworkChangedBySwitchingDataCall(true))
    // Test active network changed by disabling data call.
    .then(() => testActiveNetworkChangedBySwitchingDataCall(false));
});
