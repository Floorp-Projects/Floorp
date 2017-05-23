/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* globals getState, setState */
/* exported handleRequest */

"use strict";

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");
  response.setHeader("Access-Control-Allow-Origin", "*", false);

  // Redirect to a different file each time.
  let counter = 1 + +getState("counter");

  let index = request.path.lastIndexOf("/");
  let newPath = request.path.substr(0, index + 1) +
      "code_bundle_reload_" + counter + ".js";
  let newUrl = request.scheme + "://" + request.host + newPath;

  response.setStatusLine(request.httpVersion, 302, "Found");
  response.setHeader("Location", newUrl);
  setState("counter", "" + counter);
}
