/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test FxA logins with Loop.
 */

"use strict";

const BASE_URL = Services.prefs.getCharPref("loop.server");

function* checkFxA401() {
  let err = MozLoopService.errors.get("login");
  is(err.code, 401, "Check error code");
  is(err.friendlyMessage, getLoopString("could_not_authenticate"),
     "Check friendlyMessage");
  is(err.friendlyDetails, getLoopString("password_changed_question"),
     "Check friendlyDetails");
  is(err.friendlyDetailsButtonLabel, getLoopString("retry_button"),
     "Check friendlyDetailsButtonLabel");
  let loopButton = document.getElementById("loop-button");
  is(loopButton.getAttribute("state"), "error",
     "state of loop button should be error after a 401 with login");

  let loopPanel = document.getElementById("loop-notification-panel");
  yield loadLoopPanel();
  let loopDoc = document.getElementById("loop-panel-iframe").contentDocument;
  is(loopDoc.querySelector(".alert-error .message").textContent,
     getLoopString("could_not_authenticate"),
     "Check error bar message");
  is(loopDoc.querySelector(".details-error .details").textContent,
     getLoopString("password_changed_question"),
     "Check error bar details message");
  is(loopDoc.querySelector(".details-error .detailsButton").textContent,
     getLoopString("retry_button"),
     "Check error bar details button");
  loopPanel.hidePopup();
}

add_task(function* setup() {
  Services.prefs.setBoolPref("loop.gettingStarted.seen", true);
  MozLoopServiceInternal.mocks.pushHandler = mockPushHandler;
  // Normally the same pushUrl would be registered but we change it in the test
  // to be able to check for success on the second registration.

  registerCleanupFunction(function* () {
    info("cleanup time");
    yield promiseDeletedOAuthParams(BASE_URL);
    Services.prefs.clearUserPref("loop.gettingStarted.seen");
    MozLoopServiceInternal.mocks.pushHandler = undefined;

    yield resetFxA();
    Services.prefs.clearUserPref(MozLoopServiceInternal.getSessionTokenPrefName(LOOP_SESSION_TYPE.GUEST));
  });
});

add_task(function* checkOAuthParams() {
  let params = {
    client_id: "client_id",
    content_uri: BASE_URL + "/content",
    oauth_uri: BASE_URL + "/oauth",
    profile_uri: BASE_URL + "/profile",
    state: "state"
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);
  let client = yield MozLoopServiceInternal.promiseFxAOAuthClient();
  for (let key of Object.keys(params)) {
    is(client.parameters[key], params[key], "Check " + key + " was passed to the OAuth client");
  }
  let prefName = MozLoopServiceInternal.getSessionTokenPrefName(LOOP_SESSION_TYPE.FXA);
  let padding = "X".repeat(HAWK_TOKEN_LENGTH - params.client_id.length);
  is(Services.prefs.getCharPref(prefName), params.client_id + padding, "Check FxA hawk token");
});

add_task(function* basicAuthorization() {
  let result = yield MozLoopServiceInternal.promiseFxAOAuthAuthorization();
  is(result.code, "code1", "Check code");
  is(result.state, "state", "Check state");
});

add_task(function* sameOAuthClientForTwoCalls() {
  yield resetFxA();
  let client1 = yield MozLoopServiceInternal.promiseFxAOAuthClient();
  let client2 = yield MozLoopServiceInternal.promiseFxAOAuthClient();
  is(client1, client2, "The same client should be returned");
});

add_task(function* paramsInvalid() {
  yield resetFxA();
  // Delete the params so an empty object is returned.
  yield promiseDeletedOAuthParams(BASE_URL);
  let result = null;
  let loginPromise = MozLoopService.logInToFxA();
  let caught = false;
  yield loginPromise.catch(() => {
    ok(true, "The login promise should be rejected due to invalid params");
    caught = true;
  });
  ok(caught, "Should have caught the rejection");
  is(result, null, "No token data should be returned");
});

add_task(function* params_no_hawk_session() {
  yield resetFxA();
  let params = {
    client_id: "client_id",
    content_uri: BASE_URL + "/content",
    oauth_uri: BASE_URL + "/oauth",
    profile_uri: BASE_URL + "/profile",
    state: "state",
    test_error: "params_no_hawk"
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);

  let loginPromise = MozLoopService.logInToFxA();
  let caught = false;
  yield loginPromise.catch(() => {
    ok(true, "The login promise should be rejected due to a lack of a hawk session");
    caught = true;
  });
  ok(caught, "Should have caught the rejection");
  let prefName = MozLoopServiceInternal.getSessionTokenPrefName(LOOP_SESSION_TYPE.FXA);
  is(Services.prefs.getPrefType(prefName),
     Services.prefs.PREF_INVALID,
     "Check FxA hawk token is not set");
});

add_task(function* params_nonJSON() {
  let loopServerUrl = Services.prefs.getCharPref("loop.server");
  Services.prefs.setCharPref("loop.server", "https://localhost:3000/invalid");
  try {
    // Reset after changing the server so a new HawkClient is created
    yield resetFxA();

    let loginPromise = MozLoopService.logInToFxA();
    let caught = false;
    yield loginPromise.catch(() => {
      ok(true, "The login promise should be rejected due to non-JSON params");
      caught = true;
    });
    ok(caught, "Should have caught the rejection");
  } catch (err) {
    throw err;
  } finally {
    Services.prefs.setCharPref("loop.server", loopServerUrl);
  }
});

add_task(function* invalidState() {
  yield resetFxA();
  let params = {
    client_id: "client_id",
    content_uri: BASE_URL + "/content",
    oauth_uri: BASE_URL + "/oauth",
    profile_uri: BASE_URL + "/profile",
    state: "invalid_state"
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);
  let loginPromise = MozLoopService.logInToFxA();
  yield loginPromise.catch((error) => {
    ok(error, "The login promise should be rejected due to invalid state");
  });
});

add_task(function* basicRegistrationWithoutSession() {
  yield resetFxA();
  yield promiseDeletedOAuthParams(BASE_URL);
  let caught = false;
  yield MozLoopServiceInternal.promiseFxAOAuthToken("code1", "state").catch((error) => {
    caught = true;
    is(error.code, 401, "Should have returned a 401");
  });
  ok(caught, "Should have caught the error requesting /token without a hawk session");
  yield checkFxA401();
});

add_task(function* basicRegistration() {
  let params = {
    client_id: "client_id",
    content_uri: BASE_URL + "/content",
    oauth_uri: BASE_URL + "/oauth",
    profile_uri: BASE_URL + "/profile",
    state: "state"
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);
  yield resetFxA();
  // Create a fake FxA hawk session token
  const fxASessionPref = MozLoopServiceInternal.getSessionTokenPrefName(LOOP_SESSION_TYPE.FXA);
  Services.prefs.setCharPref(fxASessionPref, "X".repeat(HAWK_TOKEN_LENGTH));

  let tokenData = yield MozLoopServiceInternal.promiseFxAOAuthToken("code1", "state");
  is(tokenData.access_token, "code1_access_token", "Check access_token");
  is(tokenData.scope, "profile", "Check scope");
  is(tokenData.token_type, "bearer", "Check token_type");
});

add_task(function* registrationWithInvalidState() {
  yield resetFxA();
  let params = {
    client_id: "client_id",
    content_uri: BASE_URL + "/content",
    oauth_uri: BASE_URL + "/oauth",
    profile_uri: BASE_URL + "/profile",
    state: "invalid_state"
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);

  // Create a fake FxA hawk session token
  const fxASessionPref = MozLoopServiceInternal.getSessionTokenPrefName(LOOP_SESSION_TYPE.FXA);
  Services.prefs.setCharPref(fxASessionPref, "X".repeat(HAWK_TOKEN_LENGTH));

  let tokenPromise = MozLoopServiceInternal.promiseFxAOAuthToken("code1", "state");
  yield tokenPromise.then(body => {
    ok(false, "Promise should have rejected");
  },
  error => {
    is(error.code, 400, "Check error code");
    checkFxAOAuthTokenData(null);
    is(MozLoopService.userProfile, null, "Profile should be empty after invalid login");
  });
});

add_task(function* registrationWith401() {
  yield resetFxA();
  let params = {
    client_id: "client_id",
    content_uri: BASE_URL + "/content",
    oauth_uri: BASE_URL + "/oauth",
    profile_uri: BASE_URL + "/profile",
    state: "state",
    test_error: "token_401"
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);

  let tokenPromise = MozLoopServiceInternal.promiseFxAOAuthToken("code1", "state");
  yield tokenPromise.then(body => {
    ok(false, "Promise should have rejected");
  },
  error => {
    is(error.code, 401, "Check error code");
    checkFxAOAuthTokenData(null);
    is(MozLoopService.userProfile, null, "Profile should be empty after invalid login");
  });

  yield checkFxA401();

  // Make the server no longer return a 401
  delete params.test_error;
  yield promiseOAuthParamsSetup(BASE_URL, params);

  // Create a fake FxA hawk session token
  const fxASessionPref = MozLoopServiceInternal.getSessionTokenPrefName(LOOP_SESSION_TYPE.FXA);
  Services.prefs.setCharPref(fxASessionPref, "X".repeat(HAWK_TOKEN_LENGTH));

  let tokenData = yield MozLoopServiceInternal.promiseFxAOAuthToken("code1", "state");
  is(tokenData.access_token, "code1_access_token", "Check access_token");
  is(tokenData.scope, "profile", "Check scope");
  is(tokenData.token_type, "bearer", "Check token_type");

  // Try again with the retry function
  let err = MozLoopService.errors.get("login");
  // Catch the clearError notification first then the "login" one
  let statusChangedPromise = promiseObserverNotified("loop-status-changed").then(
    () => promiseObserverNotified("loop-status-changed", "login")
  );

  info("going to retry");
  yield err.friendlyDetailsButtonCallback();
  yield statusChangedPromise;
  ok(!MozLoopService.errors.get("login"), "Shouldn't have a login error after");
});

add_task(function* basicAuthorizationAndRegistration() {
  yield resetFxA();
  let params = {
    client_id: "client_id",
    content_uri: BASE_URL + "/content",
    oauth_uri: BASE_URL + "/oauth",
    profile_uri: BASE_URL + "/profile",
    state: "state"
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);

  info("registering");
  mockPushHandler.registrationPushURL = "https://localhost/pushUrl/guest";
  yield MozLoopService.promiseRegisteredWithServers();

  let statusChangedPromise = promiseObserverNotified("loop-status-changed");
  yield loadLoopPanel({stayOnline: true});
  yield statusChangedPromise;
  let loopDoc = document.getElementById("loop-panel-iframe").contentDocument;
  let visibleEmail = loopDoc.getElementsByClassName("user-identity")[0];
  is(visibleEmail.textContent, "Guest", "Guest should be displayed on the panel when not logged in");
  is(MozLoopService.userProfile, null, "profile should be null before log-in");
  let loopButton = document.getElementById("loop-button");
  is(loopButton.getAttribute("state"), "", "state of loop button should be empty when not logged in");

  info("Login");
  statusChangedPromise = promiseObserverNotified("loop-status-changed", "login");
  let tokenData = yield MozLoopService.logInToFxA();
  yield statusChangedPromise;
  is(tokenData.access_token, "code1_access_token", "Check access_token");
  is(tokenData.scope, "profile", "Check scope");
  is(tokenData.token_type, "bearer", "Check token_type");

  is(MozLoopService.userProfile.email, "test@example.com", "email should exist in the profile data");
  is(MozLoopService.userProfile.uid, "1234abcd", "uid should exist in the profile data");
  is(visibleEmail.textContent, "test@example.com", "the email should be correct on the panel");
  is(loopButton.getAttribute("state"), "active", "state of loop button should be active when logged in");

  let registrationResponse = yield promiseOAuthGetRegistration(BASE_URL);
  is(registrationResponse.response.simplePushURLs.calls, "https://localhost/pushUrl/fxa-calls",
     "Check registered push URL");
  is(registrationResponse.response.simplePushURLs.rooms, "https://localhost/pushUrl/fxa-rooms",
     "Check registered push URL");

  let loopPanel = document.getElementById("loop-notification-panel");
  loopPanel.hidePopup();
  statusChangedPromise = promiseObserverNotified("loop-status-changed");
  yield loadLoopPanel({stayOnline: true});
  yield statusChangedPromise;
  is(loopButton.getAttribute("state"), "", "state of loop button should return to empty after panel is opened");
  loopPanel.hidePopup();

  info("logout");
  yield MozLoopService.logOutFromFxA();
  checkLoggedOutState();
  registrationResponse = yield promiseOAuthGetRegistration(BASE_URL);
  is(registrationResponse.response, null,
     "Check registration was deleted on the server");
  is(visibleEmail.textContent, "Guest", "Guest should be displayed on the panel again after logout");
  is(MozLoopService.userProfile, null, "userProfile should be null after logout");
});

add_task(function* loginWithParams401() {
  yield resetFxA();
  let params = {
    client_id: "client_id",
    content_uri: BASE_URL + "/content",
    oauth_uri: BASE_URL + "/oauth",
    profile_uri: BASE_URL + "/profile",
    state: "state",
    test_error: "params_401"
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);
  yield MozLoopService.promiseRegisteredWithServers();

  let loginPromise = MozLoopService.logInToFxA();
  yield loginPromise.then(tokenData => {
    ok(false, "Promise should have rejected");
  },
  error => {
    is(error.code, 401, "Check error code");
    checkFxAOAuthTokenData(null);
  });

  yield checkFxA401();
});

add_task(function* logoutWithIncorrectPushURL() {
  yield resetFxA();
  let pushURL = "http://www.example.com/";
  // Create a fake FxA hawk session token
  const fxASessionPref = MozLoopServiceInternal.getSessionTokenPrefName(LOOP_SESSION_TYPE.FXA);
  Services.prefs.setCharPref(fxASessionPref, "X".repeat(HAWK_TOKEN_LENGTH));
  yield MozLoopServiceInternal.registerWithLoopServer(LOOP_SESSION_TYPE.FXA, "calls", pushURL);
  let registrationResponse = yield promiseOAuthGetRegistration(BASE_URL);
  is(registrationResponse.response.simplePushURLs.calls, pushURL, "Check registered push URL");
  MozLoopServiceInternal.pushURLs.get(LOOP_SESSION_TYPE.FXA).calls = "http://www.example.com/invalid";
  let caught = false;
  yield MozLoopService.logOutFromFxA().catch((error) => {
    caught = true;
  });
  ok(caught, "Should have caught an error logging out with a mismatched push URL");
  checkLoggedOutState();
  registrationResponse = yield promiseOAuthGetRegistration(BASE_URL);
  is(registrationResponse.response.simplePushURLs.calls, pushURL, "Check registered push URL wasn't deleted");
});

add_task(function* logoutWithNoPushURL() {
  yield resetFxA();
  let pushURL = "http://www.example.com/";
  // Create a fake FxA hawk session token
  const fxASessionPref = MozLoopServiceInternal.getSessionTokenPrefName(LOOP_SESSION_TYPE.FXA);
  Services.prefs.setCharPref(fxASessionPref, "X".repeat(HAWK_TOKEN_LENGTH));

  yield MozLoopServiceInternal.registerWithLoopServer(LOOP_SESSION_TYPE.FXA, "calls", pushURL);
  let registrationResponse = yield promiseOAuthGetRegistration(BASE_URL);
  is(registrationResponse.response.simplePushURLs.calls, pushURL, "Check registered push URL");
  MozLoopServiceInternal.pushURLs.delete(LOOP_SESSION_TYPE.FXA);
  yield MozLoopService.logOutFromFxA();
  checkLoggedOutState();
  registrationResponse = yield promiseOAuthGetRegistration(BASE_URL);
  is(registrationResponse.response.simplePushURLs.calls, pushURL, "Check registered push URL wasn't deleted");
});

add_task(function* loginWithRegistration401() {
  yield resetFxA();
  let params = {
    client_id: "client_id",
    content_uri: BASE_URL + "/content",
    oauth_uri: BASE_URL + "/oauth",
    profile_uri: BASE_URL + "/profile",
    state: "state",
    test_error: "token_401"
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);

  let loginPromise = MozLoopService.logInToFxA();
  yield loginPromise.then(tokenData => {
    ok(false, "Promise should have rejected");
  },
  error => {
    is(error.code, 401, "Check error code");
    checkFxAOAuthTokenData(null);
  });

  yield checkFxA401();
});

add_task(function* openFxASettings() {
  yield resetFxA();

  // Since the default b-c window has a blank tab, open a new non-blank tab to
  // force switchToTabHavingURI to open a new tab instead of reusing the current
  // blank tab.
  gBrowser.selectedTab = gBrowser.addTab(BASE_URL);

  let params = {
    client_id: "client_id",
    content_uri: BASE_URL + "/content",
    oauth_uri: BASE_URL + "/oauth",
    profile_uri: BASE_URL + "/profile",
    state: "state",
    test_error: "token_401"
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);

  yield new Promise((resolve, reject) => {
    let progressListener = {
      onLocationChange: function onLocationChange(aBrowser) {
        if (aBrowser.currentURI.spec == BASE_URL) {
          // Ignore the changes from the addTab above.
          return;
        }
        gBrowser.removeTabsProgressListener(progressListener);
        let contentURI = Services.io.newURI(params.content_uri, null, null);
        is(aBrowser.currentURI.spec, Services.io.newURI("/settings", null, contentURI).spec,
           "Check settings tab URL");
        resolve();
      }
    };
    gBrowser.addTabsProgressListener(progressListener);

    MozLoopService.openFxASettings();
  });

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeTab(gBrowser.tabs[1]);
  }
});
