/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env worker */

"use strict";

function wait(ms) {
  return new Promise(resolve => {
    setTimeout(resolve, ms);
  });
}

// Wait for one second to switch from installing to installed.
self.addEventListener("install", function (event) {
  event.waitUntil(wait(1000));
});
