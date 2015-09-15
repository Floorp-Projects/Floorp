/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for b2g/components/SignInToWebsite.jsm

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "MinimalIDService",
                                  "resource://gre/modules/identity/MinimalIdentity.jsm",
                                  "IdentityService");

XPCOMUtils.defineLazyModuleGetter(this, "SignInToWebsiteController",
                                  "resource://gre/modules/SignInToWebsite.jsm",
                                  "SignInToWebsiteController");

Cu.import("resource://gre/modules/identity/LogUtils.jsm");

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["test_signintowebsite"].concat(aMessageArgs));
}

function test_overall() {
  do_check_neq(MinimalIDService, null);
  run_next_test();
}

function objectContains(object, subset) {
  let objectKeys = Object.keys(object);
  let subsetKeys = Object.keys(subset);

  // can't have fewer keys than the subset
  if (objectKeys.length < subsetKeys.length) {
    return false;
  }

  let key;
  let success = true;
  if (subsetKeys.length > 0) {
    for (let i=0; i<subsetKeys.length; i++) {
      key = subsetKeys[i];

      // key exists in the source object
      if (typeof object[key] === 'undefined') {
        success = false;
        break;
      }

      // recursively check object values
      else if (typeof subset[key] === 'object') {
        if (typeof object[key] !== 'object') {
          success = false;
          break;
        }
        if (! objectContains(object[key], subset[key])) {
          success = false;
          break;
        }
      }

      else if (object[key] !== subset[key]) {
        success = false;
        break;
      }
    }
  }

  return success;
}

function test_object_contains() {
  do_test_pending();

  let someObj = {
    pies: 42,
    green: "spam",
    flan: {yes: "please"}
  };
  let otherObj = {
    pies: 42,
    flan: {yes: "please"}
  };
  do_check_true(objectContains(someObj, otherObj));
  do_test_finished();
  run_next_test();
}

function test_mock_doc() {
  do_test_pending();
  let mockedDoc = mockDoc({loggedInUser: null}, function(action, params) {
    do_check_eq(action, 'coffee');
    do_test_finished();
    run_next_test();
  });

  // A smoke test to ensure that mockedDoc is functioning correctly.
  // There is presently no doCoffee method in Persona.
  mockedDoc.doCoffee();
}

function test_watch() {
  do_test_pending();

  setup_test_identity("pie@food.gov", TEST_CERT, function() {
    let controller = SignInToWebsiteController;

    let mockedDoc = mockDoc({loggedInUser: null}, function(action, params) {
      do_check_eq(action, 'ready');
      controller.uninit();
      MinimalIDService.RP.unwatch(mockedDoc.id);
      do_test_finished();
      run_next_test();
    });

    controller.init({pipe: mockReceivingPipe()});
    MinimalIDService.RP.watch(mockedDoc, {});
  });
}

function test_request_login() {
  do_test_pending();

  setup_test_identity("flan@food.gov", TEST_CERT, function() {
    let controller = SignInToWebsiteController;

    let mockedDoc = mockDoc({loggedInUser: null}, call_sequentially(
      function(action, params) {
        do_check_eq(action, 'ready');
        do_check_eq(params, undefined);
      },
      function(action, params) {
        do_check_eq(action, 'login');
        do_check_eq(params, TEST_CERT);
        controller.uninit();
        MinimalIDService.RP.unwatch(mockedDoc.id);
        do_test_finished();
        run_next_test();
      }
    ));

    controller.init({pipe: mockReceivingPipe()});
    MinimalIDService.RP.watch(mockedDoc, {});
    MinimalIDService.RP.request(mockedDoc.id, {});
  });
}

function test_request_logout() {
  do_test_pending();

  setup_test_identity("flan@food.gov", TEST_CERT, function() {
    let controller = SignInToWebsiteController;

    let mockedDoc = mockDoc({loggedInUser: null}, call_sequentially(
      function(action, params) {
        do_check_eq(action, 'ready');
        do_check_eq(params, undefined);
      },
      function(action, params) {
        do_check_eq(action, 'logout');
        do_check_eq(params, undefined);
        controller.uninit();
        MinimalIDService.RP.unwatch(mockedDoc.id);
        do_test_finished();
        run_next_test();
      }
    ));

    controller.init({pipe: mockReceivingPipe()});
    MinimalIDService.RP.watch(mockedDoc, {});
    MinimalIDService.RP.logout(mockedDoc.id, {});
  });
}

function test_request_login_logout() {
  do_test_pending();

  setup_test_identity("unagi@food.gov", TEST_CERT, function() {
    let controller = SignInToWebsiteController;

    let mockedDoc = mockDoc({loggedInUser: null}, call_sequentially(
      function(action, params) {
        do_check_eq(action, 'ready');
        do_check_eq(params, undefined);
      },
      function(action, params) {
        do_check_eq(action, 'login');
        do_check_eq(params, TEST_CERT);
      },
      function(action, params) {
        do_check_eq(action, 'logout');
        do_check_eq(params, undefined);
        controller.uninit();
        MinimalIDService.RP.unwatch(mockedDoc.id);
        do_test_finished();
        run_next_test();
      }
    ));

    controller.init({pipe: mockReceivingPipe()});
    MinimalIDService.RP.watch(mockedDoc, {});
    MinimalIDService.RP.request(mockedDoc.id, {});
    MinimalIDService.RP.logout(mockedDoc.id, {});
  });
}

function test_logout_everywhere() {
  do_test_pending();
  let logouts = 0;

  setup_test_identity("fugu@food.gov", TEST_CERT, function() {
    let controller = SignInToWebsiteController;

    let mockedDoc1 = mockDoc({loggedInUser: null}, call_sequentially(
      function(action, params) {
        do_check_eq(action, 'ready');
      },
      function(action, params) {
        do_check_eq(action, 'login');
      },
      function(action, params) {
        // Result of logout from doc2.
        // We don't know what order the logouts will occur in.
        do_check_eq(action, 'logout');
        if (++logouts === 2) {
          do_test_finished();
          run_next_test();
        }
      }
    ));

    let mockedDoc2 = mockDoc({loggedInUser: null}, call_sequentially(
      function(action, params) {
        do_check_eq(action, 'ready');
      },
      function(action, params) {
        do_check_eq(action, 'login');
      },
      function(action, params) {
        do_check_eq(action, 'logout');
        if (++logouts === 2) {
          do_test_finished();
          run_next_test();
        }
      }
    ));

    controller.init({pipe: mockReceivingPipe()});
    MinimalIDService.RP.watch(mockedDoc1, {});
    MinimalIDService.RP.request(mockedDoc1.id, {});

    MinimalIDService.RP.watch(mockedDoc2, {});
    MinimalIDService.RP.request(mockedDoc2.id, {});

    // Logs out of both docs because they share the
    // same origin.
    MinimalIDService.RP.logout(mockedDoc2.id, {});
  });
}

function test_options_pass_through() {
  do_test_pending();

  // An meaningless structure for testing that RP messages preserve
  // objects and their parameters as they are passed back and forth.
  let randomMixedParams = {
    loggedInUser: "juanita@mozilla.com",
    forceAuthentication: true,
    forceIssuer: "foo.com",
    someThing: {
      name: "Pertelote",
      legs: 4,
      nested: {bee: "Eric", remaining: "1/2"}
      }
    };

  let mockedDoc = mockDoc(randomMixedParams, function(action, params) {});

  function pipeOtherEnd(rpOptions, gaiaOptions) {
    // Ensure that every time we receive a message, our mixed
    // random params are contained in that message
    do_check_true(objectContains(rpOptions, randomMixedParams));

    switch (gaiaOptions.message) {
      case "identity-delegate-watch":
        MinimalIDService.RP.request(mockedDoc.id, {});
        break;
      case "identity-delegate-request":
        MinimalIDService.RP.logout(mockedDoc.id, {});
        break;
      case "identity-delegate-logout":
        do_test_finished();
        controller.uninit();
        MinimalIDService.RP.unwatch(mockedDoc.id);
        run_next_test();
        break;
    }
  }

  let controller = SignInToWebsiteController;
  controller.init({pipe: mockSendingPipe(pipeOtherEnd)});

  MinimalIDService.RP.watch(mockedDoc, {});
}

var TESTS = [
  test_overall,
  test_mock_doc,
  test_object_contains,

  test_watch,
  test_request_login,
  test_request_logout,
  test_request_login_logout,
  test_logout_everywhere,

  test_options_pass_through
];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
