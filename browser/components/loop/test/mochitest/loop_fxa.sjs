/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This is a mock server that implements the FxA endpoints on the Loop server.
 */

"use strict";

const REQUIRED_PARAMS = ["client_id", "content_uri", "oauth_uri", "profile_uri", "state"];
const HAWK_TOKEN_LENGTH = 64;

Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.importGlobalProperties(["URL"]);

/**
 * Entry point for HTTP requests.
 */
function handleRequest(request, response) {
  // Convert the query string to a path with a placeholder base of example.com
  let url = new URL(request.queryString.replace(/%3F.*/,""), "http://www.example.com");
  dump("loop_fxa.sjs request for: " + url.pathname + "\n");
  switch (url.pathname) {
    case "/setup_params": // Test-only
      setup_params(request, response);
      return;
    case "/fxa-oauth/params":
      params(request, response);
      return;
    case "/" + encodeURIComponent("/oauth/authorization"):
      oauth_authorization(request, response);
      return;
    case "/fxa-oauth/token":
      token(request, response);
      return;
    case "/registration":
      if (request.method == "DELETE") {
        delete_registration(request, response);
      } else {
        registration(request, response);
      }
      return;
    case "/get_registration": // Test-only
      get_registration(request, response);
      return;
    case "/profile/profile":
      profile(request, response);
      return;
  }
  response.setStatusLine(request.httpVersion, 404, "Not Found");
}

/**
 * POST /setup_params
 * DELETE /setup_params
 *
 * Test-only endpoint to setup the /fxa-oauth/params response.
 *
 * For a POST the X-Params header should contain a JSON object with keys to set for /fxa-oauth/params.
 * A DELETE request will delete the stored parameters and should be run in a cleanup function to
 * avoid interfering with subsequent tests.
 */
function setup_params(request, response) {
  response.setHeader("Content-Type", "text/plain", false);
  if (request.method == "DELETE") {
    setSharedState("/fxa-oauth/params", "");
    setSharedState("/registration", "");
    response.write("Params deleted");
    return;
  }
  let params = JSON.parse(request.getHeader("X-Params"));
  if (!params) {
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
    return;
  }
  setSharedState("/fxa-oauth/params", JSON.stringify(params));
  response.write("Params updated");
}

/**
 * POST /fxa-oauth/params endpoint
 *
 * Fetch OAuth parameters used to start the OAuth flow in the browser.
 * Parameters: None
 * Response: JSON containing an object of oauth parameters.
 */
function params(request, response) {
  if (request.method != "POST") {
    response.setStatusLine(request.httpVersion, 405, "Method Not Allowed");
    response.setHeader("Allow", "POST", false);

    // Add a button to make a POST request to make this endpoint easier to debug in the browser.
    response.write("<form method=POST><button type=submit>POST</button></form>");
    return;
  }

  let params = JSON.parse(getSharedState("/fxa-oauth/params") || "{}");

  if (params.test_error && params.test_error == "params_401") {
    response.setStatusLine(request.httpVersion, 401, "Unauthorized");
    response.write("401 Unauthorized");
    return;
  }

  // Warn if required parameters are missing.
  for (let paramName of REQUIRED_PARAMS) {
    if (!(paramName in params)) {
      dump("Warning: " + paramName + " is a required parameter\n");
    }
  }

  // Save the result so we have the effective `state` value.
  setSharedState("/fxa-oauth/params", JSON.stringify(params));
  response.setHeader("Content-Type", "application/json; charset=utf-8", false);

  let client_id = params.client_id || "";
  // Pad the client_id with "X" until the token length to simulate a token
  let padding = "X".repeat(HAWK_TOKEN_LENGTH - client_id.length);
  if (params.test_error !== "params_no_hawk") {
    response.setHeader("Hawk-Session-Token", client_id + padding, false);
  }

  response.write(JSON.stringify(params, null, 2));
}

/**
 * GET /oauth/authorization endpoint for the test params.
 *
 * Redirect to a test page that uses WebChannel to complete the web flow.
 */
function oauth_authorization(request, response) {
  response.setStatusLine(request.httpVersion, 302, "Found");
  response.setHeader("Location", "browser_fxa_oauth.html");
}

/**
 * POST /fxa-oauth/token
 *
 * Validate the state parameter with the server session state and if it matches, exchange the code
 * for an OAuth Token.
 * Parameters: code & state as JSON in the POST body.
 * Response: JSON containing an object of OAuth token information.
 */
function token(request, response) {
  let params = JSON.parse(getSharedState("/fxa-oauth/params") || "{}");

  if (params.test_error && params.test_error == "token_401") {
    response.setStatusLine(request.httpVersion, 401, "Unauthorized");
    response.write("401 Unauthorized");
    return;
  }

  if (!request.hasHeader("Authorization") ||
        !request.getHeader("Authorization").startsWith("Hawk")) {
    response.setStatusLine(request.httpVersion, 401, "Missing Hawk");
    response.write("401 Missing Hawk Authorization header");
    return;
  }

  let body = NetUtil.readInputStreamToString(request.bodyInputStream,
                                             request.bodyInputStream.available());
  let payload = JSON.parse(body);
  if (!params.state || params.state !== payload.state) {
    response.setStatusLine(request.httpVersion, 400, "State mismatch");
    response.write("State mismatch");
    return;
  }

  let tokenData = {
    access_token: payload.code + "_access_token",
    scope: "profile",
    token_type: "bearer",
  };
  response.setHeader("Content-Type", "application/json; charset=utf-8", false);
  response.write(JSON.stringify(tokenData, null, 2));
}

/**
 * GET /profile
 *
 */
function profile(request, response) {
  response.setHeader("Content-Type", "application/json; charset=utf-8", false);
  let profile = {
    email: "test@example.com",
    uid: "1234abcd",
  };
  response.write(JSON.stringify(profile, null, 2));
}

/**
 * POST /registration
 *
 * Mock Loop registration endpoint. Hawk Authorization headers are expected only for FxA sessions.
 */
function registration(request, response) {
  let body = NetUtil.readInputStreamToString(request.bodyInputStream,
                                             request.bodyInputStream.available());
  let payload = JSON.parse(body);
  if (payload.simplePushURL == "https://localhost/pushUrl/fxa" &&
       (!request.hasHeader("Authorization") ||
        !request.getHeader("Authorization").startsWith("Hawk"))) {
    response.setStatusLine(request.httpVersion, 401, "Missing Hawk");
    response.write("401 Missing Hawk Authorization header");
    return;
  }
  setSharedState("/registration", body);
}

/**
 * DELETE /registration
 *
 * Hawk Authorization headers are required.
 */
function delete_registration(request, response) {
  if (!request.hasHeader("Authorization") ||
      !request.getHeader("Authorization").startsWith("Hawk")) {
    response.setStatusLine(request.httpVersion, 401, "Missing Hawk");
    response.write("401 Missing Hawk Authorization header");
    return;
  }

  // Do some query string munging due to the SJS file using a base with a trailing "?"
  // making the path become a query parameter. This is because we aren't actually
  // registering endpoints at the root of the hostname e.g. /registration.
  let url = new URL(request.queryString.replace(/%3F.*/,""), "http://www.example.com");
  let registration = JSON.parse(getSharedState("/registration"));
  if (registration.simplePushURL == url.searchParams.get("simplePushURL")) {
    setSharedState("/registration", "");
  } else {
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
  }
}

/**
 * GET /get_registration
 *
 * Used for testing purposes to check if registration succeeded by returning the POST body.
 */
function get_registration(request, response) {
  response.setHeader("Content-Type", "application/json; charset=utf-8", false);
  response.write(getSharedState("/registration"));
}
