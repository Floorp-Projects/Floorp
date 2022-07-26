/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const key = "store_header";
function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Access-Control-Allow-Origin", "https://example.com");
  response.setHeader("Access-Control-Allow-Credentials", "true");

  if (request.queryString === "getstate") {
    response.write(getSharedState(key));
  } else if (request.queryString === "checkheader") {
    if (request.hasHeader("Cookie")) {
      setSharedState(key, "hasCookie");
    } else {
      setSharedState(key, "noCookie");
    }
  } else {
    // This is the first request which sets the cookie
  }
}
