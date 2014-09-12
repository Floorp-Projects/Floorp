/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the server mocking FxA integration endpoints on the Loop server.
 */

"use strict";

const BASE_URL = "http://mochi.test:8888/browser/browser/components/loop/test/mochitest/loop_fxa.sjs?";

registerCleanupFunction(function* () {
  yield promiseDeletedOAuthParams(BASE_URL);
});

add_task(function* required_setup_params() {
  let params = {
    client_id: "my_client_id",
    content_uri: "https://example.com/content/",
    oauth_uri: "https://example.com/oauth/",
    profile_uri: "https://example.com/profile/",
    state: "my_state",
  };
  let request = yield promiseOAuthParamsSetup(BASE_URL, params);
  is(request.status, 200, "Check /setup_params status");
  request = yield promiseParams();
  is(request.status, 200, "Check /fxa-oauth/params status");
  for (let param of Object.keys(params)) {
    is(request.response[param], params[param], "Check /fxa-oauth/params " + param);
  }
});

add_task(function* optional_setup_params() {
  let params = {
    action: "signin",
    client_id: "my_client_id",
    content_uri: "https://example.com/content/",
    oauth_uri: "https://example.com/oauth/",
    profile_uri: "https://example.com/profile/",
    scope: "profile",
    state: "my_state",
  };
  let request = yield promiseOAuthParamsSetup(BASE_URL, params);
  is(request.status, 200, "Check /setup_params status");
  request = yield promiseParams();
  is(request.status, 200, "Check /fxa-oauth/params status");
  for (let param of Object.keys(params)) {
    is(request.response[param], params[param], "Check /fxa-oauth/params " + param);
  }
});

add_task(function* delete_setup_params() {
  yield promiseDeletedOAuthParams(BASE_URL);
  let request = yield promiseParams();
  is(Object.keys(request.response).length, 0, "Params should have been deleted");
});

// Begin /fxa-oauth/token tests

add_task(function* token_request() {
  let params = {
    client_id: "my_client_id",
    content_uri: "https://example.com/content/",
    oauth_uri: "https://example.com/oauth/",
    profile_uri: "https://example.com/profile/",
    state: "my_state",
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);

  let request = yield promiseToken("my_code", params.state);
  ise(request.status, 200, "Check token response status");
  ise(request.response.access_token, "my_code_access_token", "Check access_token");
  ise(request.response.scope, "profile", "Check scope");
  ise(request.response.token_type, "bearer", "Check token_type");
});

add_task(function* token_request_invalid_state() {
  let params = {
    client_id: "my_client_id",
    content_uri: "https://example.com/content/",
    oauth_uri: "https://example.com/oauth/",
    profile_uri: "https://example.com/profile/",
    state: "my_invalid_state",
  };
  yield promiseOAuthParamsSetup(BASE_URL, params);
  let request = yield promiseToken("my_code", "my_state");
  ise(request.status, 400, "Check token response status");
  ise(request.response, null, "Check token response body");
});


// Helper methods

function promiseParams() {
  let deferred = Promise.defer();
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
              createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("POST", BASE_URL + "/fxa-oauth/params", true);
  xhr.responseType = "json";
  xhr.addEventListener("load", () => {
    info("/fxa-oauth/params response:\n" + JSON.stringify(xhr.response, null, 4));
    deferred.resolve(xhr);
  });
  xhr.addEventListener("error", deferred.reject);
  xhr.send();

  return deferred.promise;
}

function promiseToken(code, state) {
  let deferred = Promise.defer();
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
              createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("POST", BASE_URL + "/fxa-oauth/token", true);
  xhr.setRequestHeader("Authorization", "Hawk ...");
  xhr.responseType = "json";
  xhr.addEventListener("load", () => {
    info("/fxa-oauth/token response:\n" + JSON.stringify(xhr.response, null, 4));
    deferred.resolve(xhr);
  });
  xhr.addEventListener("error", deferred.reject);
  let payload = {
    code: code,
    state: state,
  };
  xhr.send(JSON.stringify(payload, null, 4));

  return deferred.promise;
}
