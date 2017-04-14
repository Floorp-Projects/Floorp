/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsMgmtService",
  "resource://gre/modules/FxAccountsMgmtService.jsm",
  "FxAccountsMgmtService");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsManager",
  "resource://gre/modules/FxAccountsManager.jsm");

// At end of test, restore original state
const ORIGINAL_AUTH_URI = Services.prefs.getCharPref("identity.fxaccounts.auth.uri");
var { SystemAppProxy } = Cu.import("resource://gre/modules/FxAccountsMgmtService.jsm", {});
const ORIGINAL_SENDCUSTOM = SystemAppProxy._sendCustomEvent;
do_register_cleanup(function() {
  Services.prefs.setCharPref("identity.fxaccounts.auth.uri", ORIGINAL_AUTH_URI);
  SystemAppProxy._sendCustomEvent = ORIGINAL_SENDCUSTOM;
  Services.prefs.clearUserPref("identity.fxaccounts.skipDeviceRegistration");
});

// Make profile available so that fxaccounts can store user data
do_get_profile();

// Mock the system app proxy; make message passing possible
var mockSendCustomEvent = function(aEventName, aMsg) {
  Services.obs.notifyObservers({wrappedJSObject: aMsg}, aEventName, null);
};

function run_test() {
  run_next_test();
}

add_task(function test_overall() {
  // FxA device registration throws from this context
  Services.prefs.setBoolPref("identity.fxaccounts.skipDeviceRegistration", true);

  do_check_neq(FxAccountsMgmtService, null);
});

// Check that invalid email capitalization is corrected on signIn.
// https://github.com/mozilla/fxa-auth-server/blob/master/docs/api.md#post-v1accountlogin
add_test(function test_invalidEmailCase_signIn() {
  do_test_pending();
  let clientEmail = "greta.garbo@gmail.com";
  let canonicalEmail = "Greta.Garbo@gmail.COM";
  let attempts = 0;

  function writeResp(response, msg) {
    if (typeof msg === "object") {
      msg = JSON.stringify(msg);
    }
    response.bodyOutputStream.write(msg, msg.length);
  }

  // Mock of the fxa accounts auth server, reproducing the behavior of
  // /account/login when email capitalization is incorrect on signIn.
  let server = httpd_setup({
    "/account/login": function(request, response) {
      response.setHeader("Content-Type", "application/json");
      attempts += 1;

      // Ensure we don't get in an endless loop
      if (attempts > 2) {
        response.setStatusLine(request.httpVersion, 429, "Sorry, you had your chance");
        writeResp(response, {});
        return;
      }

      let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      let jsonBody = JSON.parse(body);
      let email = jsonBody.email;

      // The second time through, the accounts client will call the api with
      // the correct email capitalization.
      if (email == canonicalEmail) {
        response.setStatusLine(request.httpVersion, 200, "Yay");
        writeResp(response, {
          uid: "your-uid",
          sessionToken: "your-sessionToken",
          keyFetchToken: "your-keyFetchToken",
          verified: true,
          authAt: 1392144866,
        });
        return;
      }

      // If the client has the wrong case on the email, we return a 400, with
      // the capitalization of the email as saved in the accounts database.
      response.setStatusLine(request.httpVersion, 400, "Incorrect email case");
      writeResp(response, {
        code: 400,
        errno: 120,
        error: "Incorrect email case",
        email: canonicalEmail,
      });
      return;
    },
  });

  // Point the FxAccountsClient's hawk rest request client to the mock server
  Services.prefs.setCharPref("identity.fxaccounts.auth.uri", server.baseURI);

  // FxA device registration throws from this context
  Services.prefs.setBoolPref("identity.fxaccounts.skipDeviceRegistration", true);

  // Receive a mozFxAccountsChromeEvent message
  function onMessage(subject, topic, data) {
    let message = subject.wrappedJSObject;

    switch (message.id) {
      // When we signed in as "Greta.Garbo", the server should have told us
      // that the proper capitalization is really "greta.garbo".  Call
      // getAccounts to get the signed-in user and ensure that the
      // capitalization is correct.
      case "signIn":
        FxAccountsMgmtService.handleEvent({
          detail: {
            id: "getAccounts",
            data: {
              method: "getAccounts",
            }
          }
        });
        break;

      // Having initially signed in as "Greta.Garbo", getAccounts should show
      // us that the signed-in user has the properly-capitalized email,
      // "greta.garbo".
      case "getAccounts":
        Services.obs.removeObserver(onMessage, "mozFxAccountsChromeEvent");

        do_check_eq(message.data.email, canonicalEmail);

        do_test_finished();
        server.stop(run_next_test);
        break;

      // We should not receive any other mozFxAccountsChromeEvent messages
      default:
        do_throw("wat!");
        break;
    }
  }

  Services.obs.addObserver(onMessage, "mozFxAccountsChromeEvent");

  SystemAppProxy._sendCustomEvent = mockSendCustomEvent;

  // Trigger signIn using an email with incorrect capitalization
  FxAccountsMgmtService.handleEvent({
    detail: {
      id: "signIn",
      data: {
        method: "signIn",
        email: clientEmail,
        password: "123456",
      },
    },
  });
});

add_test(function testHandleGetAssertionError_defaultCase() {
  do_test_pending();

  // FxA device registration throws from this context
  Services.prefs.setBoolPref("identity.fxaccounts.skipDeviceRegistration", true);

  FxAccountsManager.getAssertion(null).then(
    success => {
      // getAssertion should throw with invalid audience
      ok(false);
    },
    reason => {
      equal("INVALID_AUDIENCE", reason.error);
      do_test_finished();
      run_next_test();
    }
  )
});

// End of tests
// Utility functions follow

function httpd_setup (handlers, port=-1) {
  let server = new HttpServer();
  for (let path in handlers) {
    server.registerPathHandler(path, handlers[path]);
  }
  try {
    server.start(port);
  } catch (ex) {
    dump("ERROR starting server on port " + port + ".  Already a process listening?");
    do_throw(ex);
  }

  // Set the base URI for convenience.
  let i = server.identity;
  server.baseURI = i.primaryScheme + "://" + i.primaryHost + ":" + i.primaryPort;

  return server;
}
