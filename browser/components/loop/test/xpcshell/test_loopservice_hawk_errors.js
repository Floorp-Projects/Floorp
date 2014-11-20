/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Unit tests for the error handling for hawkRequest via setError.
 *
 * hawkRequest calls setError itself for 401. Consumers need to report other
 * errors to setError themseleves.
 */

"use strict";

const { INVALID_AUTH_TOKEN } = Cu.import("resource:///modules/loop/MozLoopService.jsm");

/**
 * An HTTP request for /NNN responds with a request with a status of NNN.
 */
function errorRequestHandler(request, response) {
  let responseCode = request.path.substring(1);
  response.setStatusLine(null, responseCode, "Error");
  if (responseCode == 401) {
    response.write(JSON.stringify({
      code: parseInt(responseCode),
      errno: INVALID_AUTH_TOKEN,
      error: "INVALID_AUTH_TOKEN",
      message: "INVALID_AUTH_TOKEN",
    }));
  }
}

add_task(function* setup_server() {
  loopServer.registerPathHandler("/401", errorRequestHandler);
  loopServer.registerPathHandler("/404", errorRequestHandler);
  loopServer.registerPathHandler("/500", errorRequestHandler);
  loopServer.registerPathHandler("/503", errorRequestHandler);
});

add_task(function* error_offline() {
  Services.io.offline = true;
  yield MozLoopServiceInternal.hawkRequestInternal(LOOP_SESSION_TYPE.GUEST, "/offline", "GET").then(
    () => Assert.ok(false, "Should have rejected"),
    (error) => {
      MozLoopServiceInternal.setError("testing", error);
      Assert.strictEqual(MozLoopService.errors.size, 1, "Should be one error");

      // Network errors are converted to the "network" errorType.
      let err = MozLoopService.errors.get("network");
      Assert.strictEqual(err.code, null);
      Assert.strictEqual(err.friendlyMessage, getLoopString("could_not_connect"));
      Assert.strictEqual(err.friendlyDetails, getLoopString("check_internet_connection"));
      Assert.strictEqual(err.friendlyDetailsButtonLabel, getLoopString("retry_button"));
  });
  Services.io.offline = false;
});

add_task(cleanup_between_tests);

add_task(function* guest_401() {
  Services.prefs.setCharPref("loop.hawk-session-token", "guest");
  Services.prefs.setCharPref("loop.hawk-session-token.fxa", "fxa");
  yield MozLoopServiceInternal.hawkRequestInternal(LOOP_SESSION_TYPE.GUEST, "/401", "POST").then(
    () => Assert.ok(false, "Should have rejected"),
    (error) => {
      Assert.strictEqual(Services.prefs.getPrefType("loop.hawk-session-token"),
                         Services.prefs.PREF_INVALID,
                         "Guest session token should have been cleared");
      Assert.strictEqual(Services.prefs.getCharPref("loop.hawk-session-token.fxa"),
                         "fxa",
                         "FxA session token should NOT have been cleared");

      Assert.strictEqual(MozLoopService.errors.size, 1, "Should be one error");

      let err = MozLoopService.errors.get("registration");
      Assert.strictEqual(err.code, 401);
      Assert.strictEqual(err.friendlyMessage, getLoopString("session_expired_error_description"));
      Assert.equal(err.friendlyDetails, null);
      Assert.equal(err.friendlyDetailsButtonLabel, null);
  });
});

add_task(cleanup_between_tests);

add_task(function* fxa_401() {
  Services.prefs.setCharPref("loop.hawk-session-token", "guest");
  Services.prefs.setCharPref("loop.hawk-session-token.fxa", "fxa");
  yield MozLoopServiceInternal.hawkRequestInternal(LOOP_SESSION_TYPE.FXA, "/401", "POST").then(
    () => Assert.ok(false, "Should have rejected"),
    (error) => {
      Assert.strictEqual(Services.prefs.getCharPref("loop.hawk-session-token"),
                         "guest",
                         "Guest session token should NOT have been cleared");
      Assert.strictEqual(Services.prefs.getPrefType("loop.hawk-session-token.fxa"),
                         Services.prefs.PREF_INVALID,
                         "Fxa session token should have been cleared");
      Assert.strictEqual(MozLoopService.errors.size, 1, "Should be one error");

      let err = MozLoopService.errors.get("login");
      Assert.strictEqual(err.code, 401);
      Assert.strictEqual(err.friendlyMessage, getLoopString("could_not_authenticate"));
      Assert.strictEqual(err.friendlyDetails, getLoopString("password_changed_question"));
      Assert.strictEqual(err.friendlyDetailsButtonLabel, getLoopString("retry_button"));
  });
});

add_task(cleanup_between_tests);

add_task(function* error_404() {
  yield MozLoopServiceInternal.hawkRequestInternal(LOOP_SESSION_TYPE.GUEST, "/404", "GET").then(
    () => Assert.ok(false, "Should have rejected"),
    (error) => {
      MozLoopServiceInternal.setError("testing", error);
      Assert.strictEqual(MozLoopService.errors.size, 1, "Should be one error");

      let err = MozLoopService.errors.get("testing");
      Assert.strictEqual(err.code, 404);
      Assert.strictEqual(err.friendlyMessage, getLoopString("generic_failure_title"));
      Assert.equal(err.friendlyDetails, null);
      Assert.equal(err.friendlyDetailsButtonLabel, null);
  });
});

add_task(cleanup_between_tests);

add_task(function* error_500() {
  yield MozLoopServiceInternal.hawkRequestInternal(LOOP_SESSION_TYPE.GUEST, "/500", "GET").then(
    () => Assert.ok(false, "Should have rejected"),
    (error) => {
      MozLoopServiceInternal.setError("testing", error);
      Assert.strictEqual(MozLoopService.errors.size, 1, "Should be one error");

      let err = MozLoopService.errors.get("testing");
      Assert.strictEqual(err.code, 500);
      Assert.strictEqual(err.friendlyMessage, getLoopString("service_not_available"));
      Assert.strictEqual(err.friendlyDetails, getLoopString("try_again_later"));
      Assert.strictEqual(err.friendlyDetailsButtonLabel, getLoopString("retry_button"));
  });
});

add_task(cleanup_between_tests);

add_task(function* profile_500() {
  yield MozLoopServiceInternal.hawkRequestInternal(LOOP_SESSION_TYPE.GUEST, "/500", "GET").then(
    () => Assert.ok(false, "Should have rejected"),
    (error) => {
      MozLoopServiceInternal.setError("profile", error);
      Assert.strictEqual(MozLoopService.errors.size, 1, "Should be one error");

      let err = MozLoopService.errors.get("profile");
      Assert.strictEqual(err.code, 500);
      Assert.strictEqual(err.friendlyMessage, getLoopString("problem_accessing_account"));
      Assert.equal(err.friendlyDetails, null);
      Assert.equal(err.friendlyDetailsButtonLabel, null);
  });
});

add_task(cleanup_between_tests);

add_task(function* error_503() {
  yield MozLoopServiceInternal.hawkRequestInternal(LOOP_SESSION_TYPE.GUEST, "/503", "GET").then(
    () => Assert.ok(false, "Should have rejected"),
    (error) => {
      MozLoopServiceInternal.setError("testing", error);
      Assert.strictEqual(MozLoopService.errors.size, 1, "Should be one error");

      let err = MozLoopService.errors.get("testing");
      Assert.strictEqual(err.code, 503);
      Assert.strictEqual(err.friendlyMessage, getLoopString("service_not_available"));
      Assert.strictEqual(err.friendlyDetails, getLoopString("try_again_later"));
      Assert.strictEqual(err.friendlyDetailsButtonLabel, getLoopString("retry_button"));
  });
});

add_task(cleanup_between_tests);

function run_test() {
  setupFakeLoopServer();

  // Set the expiry time one hour in the future so that an error is shown when the guest session expires.
  MozLoopServiceInternal.expiryTimeSeconds = (Date.now() / 1000) + 3600;

  do_register_cleanup(() => {
    Services.prefs.clearUserPref("loop.hawk-session-token");
    Services.prefs.clearUserPref("loop.hawk-session-token.fxa");
    Services.prefs.clearUserPref("loop.urlsExpiryTimeSeconds");
    MozLoopService.errors.clear();
  });

  run_next_test();
}

function* cleanup_between_tests() {
  MozLoopService.errors.clear();
  Services.io.offline = false;
}
