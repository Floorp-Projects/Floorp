/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AboutServiceWorkers",
  "resource://gre/modules/AboutServiceWorkers.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gServiceWorkerManager",
                                   "@mozilla.org/serviceworkers/manager;1",
                                   "nsIServiceWorkerManager");

const CHROME_MSG = "mozAboutServiceWorkersChromeEvent";

const ORIGINAL_SENDRESULT = AboutServiceWorkers.sendResult;
const ORIGINAL_SENDERROR = AboutServiceWorkers.sendError;

do_get_profile();

var mockSendResult = (aId, aResult) => {
  let msg = {
    id: aId,
    result: aResult
  };
  Services.obs.notifyObservers({wrappedJSObject: msg}, CHROME_MSG, null);
};

var mockSendError = (aId, aError) => {
  let msg = {
    id: aId,
    result: aError
  };
  Services.obs.notifyObservers({wrappedJSObject: msg}, CHROME_MSG, null);
};

function attachMocks() {
  AboutServiceWorkers.sendResult = mockSendResult;
  AboutServiceWorkers.sendError = mockSendError;
}

function restoreMocks() {
  AboutServiceWorkers.sendResult = ORIGINAL_SENDRESULT;
  AboutServiceWorkers.sendError = ORIGINAL_SENDERROR;
}

do_register_cleanup(restoreMocks);

function run_test() {
  run_next_test();
}

/**
 * "init" tests
 */
[
// Pref disabled, no registrations
{
  prefEnabled: false,
  expectedMessage: {
    id: Date.now(),
    result: {
      enabled: false,
      registrations: []
    }
  }
},
// Pref enabled, no registrations
{
  prefEnabled: true,
  expectedMessage: {
    id: Date.now(),
    result: {
      enabled: true,
      registrations: []
    }
  }
}].forEach(test => {
  add_test(function() {
    Services.prefs.setBoolPref("dom.serviceWorkers.enabled", test.prefEnabled);

    let id = test.expectedMessage.id;

    function onMessage(subject, topic, data) {
      let message = subject.wrappedJSObject;
      let expected = test.expectedMessage;

      do_check_true(message.id, "Message should have id");
      do_check_eq(message.id, test.expectedMessage.id,
                  "Id should be the expected one");
      do_check_eq(message.result.enabled, expected.result.enabled,
                  "Pref should be disabled");
      do_check_true(message.result.registrations, "Registrations should exist");
      do_check_eq(message.result.registrations.length,
                  expected.result.registrations.length,
                  "Registrations length should be the expected one");

      Services.obs.removeObserver(onMessage, CHROME_MSG);

      run_next_test();
    }

    Services.obs.addObserver(onMessage, CHROME_MSG, false);

    attachMocks();

    AboutServiceWorkers.handleEvent({ detail: {
      id: id,
      name: "init"
    }});
  });
});

/**
 * ServiceWorkerManager tests.
 */

// We cannot register a sw via ServiceWorkerManager cause chrome
// registrations are not allowed.
// All we can do for now is to test the interface of the swm.
add_test(function test_swm() {
  do_check_true(gServiceWorkerManager, "SWM exists");
  do_check_true(gServiceWorkerManager.getAllRegistrations,
                "SWM.getAllRegistrations exists");
  do_check_true(typeof gServiceWorkerManager.getAllRegistrations == "function",
                "SWM.getAllRegistrations is a function");
  do_check_true(gServiceWorkerManager.propagateSoftUpdate,
                "SWM.propagateSoftUpdate exists");
  do_check_true(typeof gServiceWorkerManager.propagateSoftUpdate == "function",

                "SWM.propagateSoftUpdate is a function");
  do_check_true(gServiceWorkerManager.propagateUnregister,
                "SWM.propagateUnregister exists");
  do_check_true(typeof gServiceWorkerManager.propagateUnregister == "function",
                "SWM.propagateUnregister exists");

  run_next_test();
});
