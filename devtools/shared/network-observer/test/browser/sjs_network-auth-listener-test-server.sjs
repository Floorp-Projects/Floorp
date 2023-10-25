/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function handleRequest(request, response) {
  let body;

  // Expect guest/guest as correct credentials, but `btoa` is unavailable in sjs
  //   "Z3Vlc3Q6Z3Vlc3Q=" == btoa("guest:guest")
  const expectedHeader = "Basic Z3Vlc3Q6Z3Vlc3Q=";

  // correct login credentials provided
  if (
    request.hasHeader("Authorization") &&
    request.getHeader("Authorization") == expectedHeader
  ) {
    response.setStatusLine(request.httpVersion, 200, "OK, authorized");
    response.setHeader("Content-Type", "text", false);

    body = "success";
  } else {
    // incorrect credentials
    response.setStatusLine(request.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
    response.setHeader("Content-Type", "text", false);

    body = "failed";
  }
  response.bodyOutputStream.write(body, body.length);
}
