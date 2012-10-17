"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "MinimalIDService",
                                  "resource://gre/modules/identity/MinimalIdentity.jsm",
                                  "IdentityService");

XPCOMUtils.defineLazyModuleGetter(this, "SignInToWebsite",
                                  "resource://gre/b2g/components/SignInToWebsite.jsm",
                                  "SignInToWebsite");

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
  let mockedDoc = mock_doc(null, TEST_URL, function(action, params) {
    do_check_eq(action, 'coffee');
    do_test_finished();
    run_next_test();
  });

  mockedDoc.doCoffee();
}

// XXX bug 800085 complete these tests - mock the gaia content object
// so we can round trip through the SignInToWebsiteController

let TESTS = [
  test_overall,
  test_mock_doc,
];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
