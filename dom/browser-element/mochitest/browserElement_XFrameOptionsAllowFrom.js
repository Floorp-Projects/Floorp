/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 690168 - Support Allow-From notation for X-Frame-Options header.
"use strict";

SimpleTest.waitForExplicitFinish();

var initialScreenshot = null;

function runTest() {
  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addPermission();
  var count = 0;

  var iframe = document.createElement('iframe');
  iframe.mozbrowser = true;
  iframe.height = '1000px';

  // The innermost page we load will fire an alert when it successfully loads.
  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    switch (e.detail.message) {
    case 'step 1':
      // Make the page wait for us to unblock it (which we do after we finish
      // taking the screenshot).
      e.preventDefault();

      iframe.getScreenshot().onsuccess = function(sshot) {
        if (initialScreenshot == null)
          initialScreenshot = sshot.target.result;
        e.detail.unblock();
      };
      break;
    case 'step 2':
      ok(false, 'cross origin page loaded');
      break;
    case 'finish':
      // The page has now attempted to load the X-Frame-Options page; take
      // another screenshot.
      iframe.getScreenshot().onsuccess = function(sshot) {
        is(sshot.target.result, initialScreenshot, "Screenshots should be identical");
        SimpleTest.finish();
      };
      break;
    }
  });

  document.body.appendChild(iframe);

  iframe.src = 'http://example.com/tests/dom/browser-element/mochitest/file_browserElement_XFrameOptionsAllowFrom.html';
}

runTest();
