"use strict";

function handleRequest(request, response) {
  let match;
  let requestAuth = true;

  // Allow the caller to drive how authentication is processed via the query.
  // Eg, http://localhost:8888/authenticate.sjs?user=foo&realm=bar
  // The extra ? allows the user/pass/realm checks to succeed if the name is
  // at the beginning of the query string.
  let query = "?" + request.queryString;

  let expected_user = "test",
    expected_pass = "testpass",
    realm = "mochitest";

  // user=xxx
  match = /[^_]user=([^&]*)/.exec(query);
  if (match) {
    expected_user = match[1];
  }

  // pass=xxx
  match = /[^_]pass=([^&]*)/.exec(query);
  if (match) {
    expected_pass = match[1];
  }

  // realm=xxx
  match = /[^_]realm=([^&]*)/.exec(query);
  if (match) {
    realm = match[1];
  }

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
    /* eslint-disable-next-line no-use-before-define */
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
  if (expected_user == actual_user && expected_pass == actual_pass) {
    requestAuth = false;
  }

  if (requestAuth) {
    response.setStatusLine("1.0", 401, "Authentication required");
    response.setHeader("WWW-Authenticate", 'basic realm="' + realm + '"', true);
  } else {
    response.setStatusLine("1.0", 200, "OK");
  }

  response.setHeader("Content-Type", "application/xhtml+xml", false);
  response.write("<html xmlns='http://www.w3.org/1999/xhtml'>");
  response.write(
    "<p>Login: <span id='ok'>" +
      (requestAuth ? "FAIL" : "PASS") +
      "</span></p>\n"
  );
  response.write("<p>Auth: <span id='auth'>" + authHeader + "</span></p>\n");
  response.write("<p>User: <span id='user'>" + actual_user + "</span></p>\n");
  response.write("<p>Pass: <span id='pass'>" + actual_pass + "</span></p>\n");
  response.write("</html>");
}
