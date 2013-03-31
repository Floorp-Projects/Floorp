/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 690168 - Support Allow-From notation for X-Frame-Options header.
"use strict";

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
  var count = 0;

  var iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;
  iframe.height = '1000px';

  // The innermost page we load will fire an alert when it successfully loads.
  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    switch (e.detail.message) {
    case 'step 1':
      // Make the page wait for us to unblock it (which we do after we finish
      // taking the screenshot).
      e.preventDefault();

      iframe.getScreenshot(1000, 1000).onsuccess = function(sshot) {
        var fr = new FileReader();
        fr.onloadend = function() {
          if (initialScreenshotArrayBuffer == null)
            initialScreenshotArrayBuffer = fr.result;
          e.detail.unblock();
        };
        fr.readAsArrayBuffer(sshot.target.result);
      };
      break;
    case 'step 2':
      ok(false, 'cross origin page loaded');
      break;
    case 'finish':
      // The page has now attempted to load the X-Frame-Options page; take
      // another screenshot.
      iframe.getScreenshot(1000, 1000).onsuccess = function(sshot) {
        var fr = new FileReader();
        fr.onloadend = function() {
          ok(arrayBuffersEqual(fr.result, initialScreenshotArrayBuffer),
             "Screenshots should be identical");
          SimpleTest.finish();
        };
        fr.readAsArrayBuffer(sshot.target.result);
      };
      break;
    }
  });

  document.body.appendChild(iframe);

  iframe.src = 'http://example.com/tests/dom/browser-element/mochitest/file_browserElement_XFrameOptionsAllowFrom.html';
}

addEventListener('testready', runTest);
