/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* globals getState, setState */
/* exported handleRequest */

"use strict";

const NUM_FILES = 3;

function _getUrl(request, counter) {
  const { scheme, host, path } = request;

  const index = Math.ceil(counter / 2);
  const showMap = false ;//(counter % 2) === 1
  const newPath = path.substr(0, path.lastIndexOf("/") + 1);
  const url = `${scheme}://${host}${newPath}/v${index}.bundle.js${showMap ? '.map' : ''}`;
  return url
}

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Content-Type", "text/javascript", false);

  // Redirect to a different file each time.
  let counter = (+getState("counter") || 1) % ( 2 * NUM_FILES + 1);

  const newUrl = _getUrl(request, counter);

  response.setStatusLine(request.httpVersion, 302, "Found");
  response.setHeader("Location", newUrl);
  setState("counter", "" + ++counter);
}
