"use strict";

function handleRequest(request, response) {
  let match;

  // Allow the caller to drive how authentication is processed via the query.
  // Eg, http://localhost:8888/authenticate.sjs?user=foo&realm=bar
  // The extra ? allows the user/pass/realm checks to succeed if the name is
  // at the beginning of the query string.
  let query = new URLSearchParams(request.queryString);

  let expected_user = query.get("user");
  let expected_pass = query.get("pass");
  let realm = query.get("realm");

  // Look for an authentication header, if any, in the request.
  //
  // EG: Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
  //
  // This test only supports Basic auth. The value sent by the client is
  // "username:password", obscured with base64 encoding.

  let actual_user = "",
    actual_pass = "",
    authHeader;
  if (request.hasHeader("Authorization")) {
    authHeader = request.getHeader("Authorization");
    match = /Basic (.+)/.exec(authHeader);
    if (match.length != 2) {
      throw new Error("Couldn't parse auth header: " + authHeader);
    }
    // Decode base64 to string
    let userpass = atob(match[1]);
    match = /(.*):(.*)/.exec(userpass);
    if (match.length != 3) {
      throw new Error("Couldn't decode auth header: " + userpass);
    }
    actual_user = match[1];
    actual_pass = match[2];
  }

  // Don't request authentication if the credentials we got were what we
  // expected.
  let requestAuth =
    expected_user != actual_user || expected_pass != actual_pass;

  if (requestAuth) {
    response.setStatusLine("1.0", 401, "Authentication required");
    response.setHeader("WWW-Authenticate", 'basic realm="' + realm + '"', true);
    response.write("Authentication required");
  } else {
    response.setStatusLine("1.0", 200, "OK");
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader(
      "Content-Disposition",
      "attachment; filename=dummy-file.html"
    );
    response.setHeader("Content-Type", "text/html");
    response.write("<p id='success'>SUCCESS</p>");
  }
}
