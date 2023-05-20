/**
 * Adapted from https://searchfox.org/mozilla-central/source/layout/reftests/backgrounds/delay-image-response.sjs
 */
"use strict";

// A 1x1 PNG image.
// Source: https://commons.wikimedia.org/wiki/File:1x1.png (Public Domain)
const IMAGE = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
    "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII="
);

// To avoid GC.
let timer = null;

function handleRequest(request, response) {
  const query = {};
  request.queryString.split("&").forEach(function (val) {
    const [name, value] = val.split("=");
    query[name] = unescape(value);
  });

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "image/png", false);

  // If there is no delay, we write the image and leave.
  if (!("delay" in query)) {
    response.write(IMAGE);
    return;
  }

  // If there is a delay, we create a timer which, when it fires, will write
  // image and leave.
  response.processAsync();
  const nsITimer = Ci.nsITimer;

  timer = Cc["@mozilla.org/timer;1"].createInstance(nsITimer);
  timer.initWithCallback(
    function () {
      response.write(IMAGE);
      response.finish();
    },
    query.delay,
    nsITimer.TYPE_ONE_SHOT
  );
}
