/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {
    action: MozMobileConnection.CALL_FORWARD_ACTION_REGISTRATION,
    reason: MozMobileConnection.CALL_FORWARD_REASON_MOBILE_BUSY,
    number: "0912345678",
    timeSeconds: 5,
    // Currently gecko only support ICC_SERVICE_CLASS_VOICE.
    serviceClass: MozMobileConnection.ICC_SERVICE_CLASS_VOICE
  }, {
    action: MozMobileConnection.CALL_FORWARD_ACTION_ENABLE,
    reason: MozMobileConnection.CALL_FORWARD_REASON_NO_REPLY,
    number: "+886912345678",
    timeSeconds: 20,
    // Currently gecko only support ICC_SERVICE_CLASS_VOICE.
    serviceClass: MozMobileConnection.ICC_SERVICE_CLASS_VOICE
  }
];

function testSetCallForwardingOption(aOptions) {
  log("Test setting call forwarding to " + JSON.stringify(aOptions));

  let promises = [];

  // Check cfstatechange event.
  promises.push(waitForManagerEvent("cfstatechange").then(function(aEvent) {
    is(aEvent.action, aOptions.action, "check action");
    is(aEvent.reason, aOptions.reason, "check reason");
    is(aEvent.number, aOptions.number, "check number");
    is(aEvent.timeSeconds, aOptions.timeSeconds, "check timeSeconds");
    is(aEvent.serviceClass, aOptions.serviceClass, "check serviceClass");
  }));
  // Check DOMRequest's result.
  promises.push(setCallForwardingOption(aOptions).then(null, (aError) => {
    ok(false, "got '" + aError.name + "' error");
  }));

  return Promise.all(promises);
}

function testGetCallForwardingOption(aReason, aExpectedResult) {
  log("Test getting call forwarding for " + aReason);

  return getCallForwardingOption(aReason)
    .then(function resolve(aResults) {
      is(Array.isArray(aResults), true, "results should be an array");

      for (let i = 0; i < aResults.length; i++) {
        let result = aResults[i];

        // Only need to check the result containing the serviceClass that we are
        // interesting in.
        if (!(result.serviceClass & aExpectedResult.serviceClass)) {
          continue;
        }

        let expectedActive =
          aExpectedResult.action === MozMobileConnection.CALL_FORWARD_ACTION_ENABLE ||
          aExpectedResult.action === MozMobileConnection.CALL_FORWARD_ACTION_REGISTRATION;
        is(result.active, expectedActive, "check active");
        is(result.reason, aExpectedResult.reason, "check reason");
        is(result.number, aExpectedResult.number, "check number");
        is(result.timeSeconds, aExpectedResult.timeSeconds, "check timeSeconds");
      }
    }, function reject(aError) {
      ok(false, "got '" + aError.name + "' error");
    });
}

function clearAllCallForwardingSettings() {
  log("Clear all call forwarding settings");

  let promise = Promise.resolve();
  for (let reason = MozMobileConnection.CALL_FORWARD_REASON_UNCONDITIONAL;
       reason <= MozMobileConnection.CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING;
       reason++) {
    let options = {
      reason: reason,
      action: MozMobileConnection.CALL_FORWARD_ACTION_ERASURE
    };
    promise =
      promise.then(() => setCallForwardingOption(options).then(null, () => {}));
  }
  return promise;
}

// Start tests
startTestCommon(function() {
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => testSetCallForwardingOption(data))
                     .then(() => testGetCallForwardingOption(data.reason, data));
  }
  // reset call forwarding settings.
  return promise.then(null, () => {})
    .then(() => clearAllCallForwardingSettings());
});

