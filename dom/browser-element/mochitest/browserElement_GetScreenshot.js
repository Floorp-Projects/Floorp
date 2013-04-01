/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the getScreenshot property for mozbrowser
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe1 = document.createElement('iframe');
  SpecialPowers.wrap(iframe1).mozbrowser = true;
  document.body.appendChild(iframe1);
  iframe1.src = 'data:text/html,<html>' +
    '<body style="background:green">hello</body></html>';

  var screenshotArrayBuffers = [];
  var numLoaded = 0;

  function screenshotTaken(screenshotArrayBuffer) {
    screenshotArrayBuffers.push(screenshotArrayBuffer);
    if (screenshotArrayBuffers.length === 1) {
      ok(true, 'Got initial non blank screenshot');
      iframe1.src = 'data:text/html,<html>' +
        '<body style="background:blue">hello</body></html>';

      // Wait until screenshotArrayBuffer !== screenshotArrayBuffers[0].
      waitForScreenshot(function(screenshotArrayBuffer) {
        var view1 = new Int8Array(screenshotArrayBuffer);
        var view2 = new Int8Array(screenshotArrayBuffers[0]);
        if (view1.length != view2.length) {
          return true;
        }

        for (var i = 0; i < view1.length; i++) {
          if (view1[i] != view2[i]) {
            return true;
          }
        }

        return false;
      });
    }
    else if (screenshotArrayBuffers.length === 2) {
      ok(true, 'Got updated screenshot after source page changed');
      SimpleTest.finish();
    }
  }

  // We continually take screenshots until we get one that we are
  // happy with.
  function waitForScreenshot(filter) {
    function gotScreenshotArrayBuffer() {
      // |this| is the FileReader whose result contains the screenshot as an
      // ArrayBuffer.

      if (filter(this.result)) {
        screenshotTaken(this.result);
        return;
      }
      if (--attempts === 0) {
        ok(false, 'Timed out waiting for correct screenshot');
        SimpleTest.finish();
      } else {
        setTimeout(function() {
          iframe1.getScreenshot(1000, 1000).onsuccess = getScreenshotArrayBuffer;
        }, 200);
      }
    }

    function getScreenshotArrayBuffer(e) {
      var fr = new FileReader();
      fr.onloadend = gotScreenshotArrayBuffer;
      fr.readAsArrayBuffer(e.target.result);
    }

    var attempts = 10;
    iframe1.getScreenshot(1000, 1000).onsuccess = getScreenshotArrayBuffer;
  }

  function iframeLoadedHandler() {
    numLoaded++;
    if (numLoaded === 2) {
      waitForScreenshot(function(screenshotArrayBuffer) {
        return screenshotArrayBuffer.byteLength != 0;
      });
    }
  }

  iframe1.addEventListener('mozbrowserloadend', iframeLoadedHandler);
}

addEventListener('testready', runTest);
