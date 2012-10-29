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

function test_mock_doc() {
  do_test_pending();
  let mockedDoc = mockDoc(null, TEST_URL, function(action, params) {
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

    let mockedDoc = mockDoc(null, TEST_URL, function(action, params) {
      do_check_eq(action, 'ready');
      controller.uninit();
      do_test_finished();
      run_next_test();
    });

    controller.init({pipe: mockPipe()});

    MinimalIDService.RP.watch(mockedDoc, {});
  });
}

function test_request_login() {
  do_test_pending();

  setup_test_identity("flan@food.gov", TEST_CERT, function() {
    let controller = SignInToWebsiteController;

    let mockedDoc = mockDoc(null, TEST_URL, call_sequentially(
      function(action, params) {
        do_check_eq(action, 'ready');
        do_check_eq(params, undefined);
      },
      function(action, params) {
        do_check_eq(action, 'login');
        do_check_eq(params, TEST_CERT);
        controller.uninit();
        do_test_finished();
        run_next_test();
      }
    ));

    controller.init({pipe: mockPipe()});
    MinimalIDService.RP.watch(mockedDoc, {});
    MinimalIDService.RP.request(mockedDoc.id, {});
  });
}

function test_request_logout() {
  do_test_pending();

  setup_test_identity("flan@food.gov", TEST_CERT, function() {
    let controller = SignInToWebsiteController;

    let mockedDoc = mockDoc(null, TEST_URL, call_sequentially(
      function(action, params) {
        do_check_eq(action, 'ready');
        do_check_eq(params, undefined);
      },
      function(action, params) {
        do_check_eq(action, 'logout');
        do_check_eq(params, undefined);
        controller.uninit();
        do_test_finished();
        run_next_test();
      }
    ));

    controller.init({pipe: mockPipe()});
    MinimalIDService.RP.watch(mockedDoc, {});
    MinimalIDService.RP.logout(mockedDoc.id, {});
  });
}

function test_request_login_logout() {
  do_test_pending();

  setup_test_identity("unagi@food.gov", TEST_CERT, function() {
    let controller = SignInToWebsiteController;

    let mockedDoc = mockDoc(null, TEST_URL, call_sequentially(
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
        do_test_finished();
        run_next_test();
      }
      /*
      ,function(action, params) {
        do_check_eq(action, 'ready');
        do_test_finished();
        run_next_test();
      }
       */
    ));

    controller.init({pipe: mockPipe()});
    MinimalIDService.RP.watch(mockedDoc, {});
    MinimalIDService.RP.request(mockedDoc.id, {});
    MinimalIDService.RP.logout(mockedDoc.id, {});
  });
}

let TESTS = [
  test_overall,
  test_mock_doc,
  test_watch,
  test_request_login,
  test_request_logout,
  test_request_login_logout
];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
