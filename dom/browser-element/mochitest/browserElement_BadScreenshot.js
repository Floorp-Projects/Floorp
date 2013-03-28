/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 800170 - Test that we get errors when we pass bad arguments to
// mozbrowser's getScreenshot.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe;
var numPendingTests = 0;

// Call iframe.getScreenshot with the given args.  If expectSuccess is true, we
// expect the screenshot's onsuccess handler to fire.  Otherwise, we expect
// getScreenshot() to throw an exception.
function checkScreenshotResult(expectSuccess, args) {
  var req;
  try {
    req = iframe.getScreenshot.apply(iframe, args);
  }
  catch(e) {
    ok(!expectSuccess, "getScreenshot(" + JSON.stringify(args) + ") threw an exception.");
    return;
  }

  numPendingTests++;
  req.onsuccess = function() {
    ok(expectSuccess, "getScreenshot(" + JSON.stringify(args) + ") succeeded.");
    numPendingTests--;
    if (numPendingTests == 0) {
      SimpleTest.finish();
    }
  };

  // We never expect to see onerror.
  req.onerror = function() {
    ok(false, "getScreenshot(" + JSON.stringify(args) + ") ran onerror.");
    numPendingTests--;
    if (numPendingTests == 0) {
      SimpleTest.finish();
    }
  };
}

function runTest() {
  iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;
  document.body.appendChild(iframe);
  iframe.src = 'data:text/html,<html>' +
    '<body style="background:green">hello</body></html>';

  iframe.addEventListener('mozbrowserfirstpaint', function() {
    // This one should succeed.
    checkScreenshotResult(true, [100, 100]);

    // These should fail.
    checkScreenshotResult(false, []);
    checkScreenshotResult(false, [100]);
    checkScreenshotResult(false, ['a', 100]);
    checkScreenshotResult(false, [100, 'a']);
    checkScreenshotResult(false, [-1, 100]);
    checkScreenshotResult(false, [100, -1]);

    if (numPendingTests == 0) {
      SimpleTest.finish();
    }
  });
}

addEventListener('testready', runTest);
