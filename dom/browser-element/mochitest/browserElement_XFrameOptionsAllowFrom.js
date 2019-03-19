/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 690168 - Support Allow-From notation for X-Frame-Options header.
"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var initialScreenshotArrayBuffer = null;

function arrayBuffersEqual(a, b) {
  var x = new Int8Array(a);
  var y = new Int8Array(b);
  if (x.length != y.length) {
    return false;
  }

  for (var i = 0; i < x.length; i++) {
    if (x[i] != y[i]) {
      return false;
    }
  }

  return true;
}

function runTest() {
  var iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");
  iframe.height = "1000px";

  var step1, stepfinish;
  // The innermost page we load will fire an alert when it successfully loads.
  iframe.addEventListener("mozbrowsershowmodalprompt", function(e) {
    switch (e.detail.message) {
    case "step 1":
      step1 = SpecialPowers.snapshotWindow(iframe.contentWindow);
      break;
    case "step 2":
      ok(false, "cross origin page loaded");
      break;
    case "finish":
      // The page has now attempted to load the X-Frame-Options page; take
      // another screenshot.
      stepfinish = SpecialPowers.snapshotWindow(iframe.contentWindow);
      ok(step1.toDataURL() == stepfinish.toDataURL(), "Screenshots should be identical");
      SimpleTest.finish();
    }
  });

  document.body.appendChild(iframe);

  iframe.src = "http://example.com/tests/dom/browser-element/mochitest/file_browserElement_XFrameOptionsAllowFrom.html";
}

addEventListener("testready", runTest);
