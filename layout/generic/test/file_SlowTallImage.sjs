"use strict";

let { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

// A tall 1x1000 black png.
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAPoAQMAAAAleAYdAAAABlBMVEUAAAD///+l2Z/dAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAAF0lEQVQ4jWNgGAWjYBSMglEwCkbBUAcAB9AAASBs/t4AAAAASUVORK5CYII="
);

// Cargo-culted from file_SlowImage.sjs
function handleRequest(request, response) {
  response.processAsync();
  response.setHeader("Content-Type", "image/png");
  let delay = request.queryString.indexOf("slow") >= 0 ? 600 : 200;
  setTimeout(function () {
    response.write(IMG_BYTES);
    response.finish();
  }, delay);
}
