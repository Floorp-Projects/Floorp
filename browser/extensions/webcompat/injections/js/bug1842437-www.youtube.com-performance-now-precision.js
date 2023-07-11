/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1842437 - When attempting to go back on youtube.com, the content remains the same
 *
 * If consecutive session history entries had history.state.entryTime set to same value,
 * back button doesn't work as expected. The entryTime value is coming from performance.now()
 * and modifying its return value slightly to make sure two close consecutive calls don't
 * get the same result helped with resolving the issue.
 */

/* globals exportFunction */

console.info(
  "performance.now precision has been modified for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1756970 for details."
);

const origPerf = performance.wrappedJSObject;
const origNow = origPerf.now;

let counter = 0;
let previousVal = 0;

Object.defineProperty(window.performance.wrappedJSObject, "now", {
  value: exportFunction(function () {
    let originalVal = origNow.call(origPerf);
    if (originalVal === previousVal) {
      originalVal += 0.00000003 * ++counter;
    } else {
      previousVal = originalVal;
      counter = 0;
    }
    return originalVal;
  }, window),
});
