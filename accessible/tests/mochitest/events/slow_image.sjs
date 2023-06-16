/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

// stolen from file_blocked_script.sjs
function setGlobalState(data, key) {
  let x = {
    data,
    QueryInterface: ChromeUtils.generateQI([]),
  };
  x.wrappedJSObject = x;
  setObjectState(key, x);
}

function getGlobalState(key) {
  var data;
  getObjectState(key, function (x) {
    data = x && x.wrappedJSObject.data;
  });
  return data;
}

function handleRequest(request, response) {
  if (request.queryString == "complete") {
    // Unblock the previous request.
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "application/json", false);
    response.write("true"); // the payload doesn't matter.

    let blockedResponse = getGlobalState("a11y-image");
    if (blockedResponse) {
      blockedResponse.setStatusLine(request.httpVersion, 200, "OK");
      blockedResponse.setHeader("Cache-Control", "no-cache", false);
      blockedResponse.setHeader("Content-Type", "image/png", false);
      blockedResponse.write(IMG_BYTES);
      blockedResponse.finish();

      setGlobalState(undefined, "a11y-image");
    }
  } else {
    // Getting the image
    response.processAsync();
    // Store the response in the global state
    setGlobalState(response, "a11y-image");
  }
}
