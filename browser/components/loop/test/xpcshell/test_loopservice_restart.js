/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FAKE_FXA_TOKEN_DATA = JSON.stringify({
  "token_type": "bearer",
  "access_token": "1bad3e44b12f77a88fe09f016f6a37c42e40f974bc7a8b432bb0d2f0e37e1752",
  "scope": "profile"
});
const FAKE_FXA_PROFILE = JSON.stringify({
  "email": "test@example.com",
  "uid": "999999994d9f4b08a2cbfc0999999999",
  "avatar": null
});
const LOOP_FXA_TOKEN_PREF = "loop.fxa_oauth.tokendata";
const LOOP_FXA_PROFILE_PREF = "loop.fxa_oauth.profile";
const LOOP_CREATED_ROOM_PREF = "loop.createdRoom";
const LOOP_INITIAL_DELAY_PREF = "loop.initialDelay";

/**
 * This file is to test restart+reauth.
 */

add_task(function* test_initialize_with_no_guest_rooms_and_no_auth_token() {
  // Set time to be 2 seconds in the past.
  var nowSeconds = Date.now() / 1000;
  Services.prefs.setBoolPref(LOOP_CREATED_ROOM_PREF, false);
  Services.prefs.clearUserPref(LOOP_FXA_TOKEN_PREF);

  yield MozLoopService.initialize().then((msg) => {
    Assert.equal(msg, "registration not needed", "Initialize should not register when the " +
                                                 "URLs are expired and there are no auth tokens");
  }, (error) => {
    Assert.ok(false, error, "should have resolved the promise that initialize returned");
  });
});

add_task(function* test_initialize_with_created_room_and_no_auth_token() {
  Services.prefs.setBoolPref(LOOP_CREATED_ROOM_PREF, true);
  Services.prefs.clearUserPref(LOOP_FXA_TOKEN_PREF);

  loopServer.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
  });

  yield MozLoopService.initialize().then((msg) => {
    Assert.equal(msg, "initialized without FxA status", "Initialize should register as a " +
                                                     "guest when no auth tokens but expired URLs");
  }, (error) => {
    Assert.ok(false, error, "should have resolved the promise that initialize returned");
  });
});

add_task(function* test_initialize_with_invalid_fxa_token() {
  Services.prefs.setCharPref(LOOP_FXA_PROFILE_PREF, FAKE_FXA_PROFILE);
  Services.prefs.setCharPref(LOOP_FXA_TOKEN_PREF, FAKE_FXA_TOKEN_DATA);

  // Only need to implement the FxA registration because the previous
  // test registered as a guest.
  loopServer.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 401, "Unauthorized");
    response.write(JSON.stringify({
      code: 401,
      errno: 110,
      error: "Unauthorized",
      message: "Unknown credentials",
    }));
  });

  yield MozLoopService.initialize().then(() => {
    Assert.ok(false, "Initializing with an invalid token should reject the promise");
  },
  (error) => {
    Assert.equal(MozLoopServiceInternal.pushHandler.registrationPushURL, kEndPointUrl, "Push URL should match");
    Assert.equal(Services.prefs.getCharPref(LOOP_FXA_TOKEN_PREF), "",
                 "FXA pref should be cleared if token was invalid");
    Assert.equal(Services.prefs.getCharPref(LOOP_FXA_PROFILE_PREF), "",
                 "FXA profile pref should be cleared if token was invalid");
    Assert.ok(MozLoopServiceInternal.errors.has("login"),
              "Initialization error should have been reported to UI");
    Assert.ok(MozLoopServiceInternal.errors.has("login"));
    Assert.ok(MozLoopServiceInternal.errors.get("login").friendlyDetailsButtonCallback,
              "Check that there is a retry callback");
  });
});

add_task(function* test_initialize_with_fxa_token() {
  Services.prefs.setCharPref(LOOP_FXA_PROFILE_PREF, FAKE_FXA_PROFILE);
  Services.prefs.setCharPref(LOOP_FXA_TOKEN_PREF, FAKE_FXA_TOKEN_DATA);

  MozLoopService.errors.clear();

  loopServer.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
  });

  yield MozLoopService.initialize().then(() => {
    Assert.equal(Services.prefs.getCharPref(LOOP_FXA_TOKEN_PREF), FAKE_FXA_TOKEN_DATA,
                 "FXA pref should still be set after initialization");
    Assert.equal(Services.prefs.getCharPref(LOOP_FXA_PROFILE_PREF), FAKE_FXA_PROFILE,
                 "FXA profile should still be set after initialization");
    Assert.ok(!MozLoopServiceInternal.errors.has("login"), "Initialization error should not exist");
  });
});

function run_test() {
  setupFakeLoopServer();
  // Note, this is just used to speed up the test.
  Services.prefs.setIntPref(LOOP_INITIAL_DELAY_PREF, 0);
  MozLoopServiceInternal.mocks.pushHandler = mockPushHandler;
  mockPushHandler.registrationPushURL = kEndPointUrl;

  do_register_cleanup(function() {
    MozLoopServiceInternal.mocks.pushHandler = undefined;
    Services.prefs.clearUserPref(LOOP_INITIAL_DELAY_PREF);
    Services.prefs.clearUserPref(LOOP_FXA_TOKEN_PREF);
    Services.prefs.clearUserPref(LOOP_FXA_PROFILE_PREF);
    Services.prefs.clearUserPref(LOOP_CREATED_ROOM_PREF);
  });

  run_next_test();
}
