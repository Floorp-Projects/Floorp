/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the getScreenshot property for mozbrowser
"use strict";

SimpleTest.waitForExplicitFinish();

function runTest() {

  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addToWhitelist();

  var iframe1 = document.createElement('iframe');
  iframe1.mozbrowser = true;
  document.body.appendChild(iframe1);
  iframe1.src = 'data:text/html,<html>' +
    '<body style="background:green">hello</body></html>';

  var screenshots = [];
  var numLoaded = 0;

  function screenshotTaken(screenshot) {
    screenshots.push(screenshot);
    if (screenshots.length === 1) {
      ok(true, 'Got initial non blank screenshot');
      iframe1.src = 'data:text/html,<html>' +
        '<body style="background:blue">hello</body></html>';
      waitForScreenshot(function(screenshot) {
        return screenshot !== screenshots[0];
      });
    }
    else if (screenshots.length === 2) {
      ok(true, 'Got updated screenshot after source page changed');
      SimpleTest.finish();
    }
  }

  // We continually take screenshots until we get one that we are
  // happy with
  function waitForScreenshot(filter) {

    function screenshotLoaded(e) {
      if (filter(e.target.result)) {
        screenshotTaken(e.target.result);
        return;
      }
      if (--attempts === 0) {
        ok(false, 'Timed out waiting for correct screenshot');
        SimpleTest.finish();
      } else {
        content.document.defaultView.setTimeout(function() {
          iframe1.getScreenshot().onsuccess = screenshotLoaded;
        }, 200);
      }
    }

    var attempts = 10;
    iframe1.getScreenshot().onsuccess = screenshotLoaded;
  }

  function iframeLoadedHandler() {
    numLoaded++;
    if (numLoaded === 2) {
      waitForScreenshot(function(screenshot) {
        return screenshot !== 'data:,';
      });
    }
  }

  iframe1.addEventListener('mozbrowserloadend', iframeLoadedHandler);
}

addEventListener('load', function() { SimpleTest.executeSoon(runTest); });


