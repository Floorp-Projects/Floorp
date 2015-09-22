/**
 * Adapted from https://dxr.mozilla.org/mozilla-central/rev/
 * 4e883591bb5dff021c108d3e30198a99547eed1e/layout/reftests/backgrounds/
 * delay-image-response.sjs
 */
"use strict";

// A 1x1 PNG image.
// Source: https://commons.wikimedia.org/wiki/File:1x1.png (Public Domain)
const IMAGE = atob("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
  "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII=");

// To avoid GC.
let timer = null;

function handleRequest(request, response) {
  let query = {};
  request.queryString.split("&").forEach(function(val) {
    let [name, value] = val.split("=");
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
  const nsITimer = Components.interfaces.nsITimer;

  timer = Components.classes["@mozilla.org/timer;1"].createInstance(nsITimer);
  timer.initWithCallback(function() {
    response.write(IMAGE);
    response.finish();
  }, query.delay, nsITimer.TYPE_ONE_SHOT);
}
