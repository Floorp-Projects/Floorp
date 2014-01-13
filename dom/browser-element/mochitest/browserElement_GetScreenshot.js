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

  var screenshotImageDatas = [];
  var numLoaded = 0;

  function screenshotTaken(screenshotImageData) {
    screenshotImageDatas.push(screenshotImageData);
    if (screenshotImageDatas.length === 1) {
      ok(true, 'Got initial non blank screenshot');

      var view = screenshotImageData.data;
      if (view[3] !== 255) {
        ok(false, 'The first pixel of initial screenshot is not opaque');
        SimpleTest.finish();
        return;
      }
      ok(true, 'Verified the first pixel of initial screenshot is opaque');

      iframe1.src = 'data:text/html,<html>' +
        '<body style="background:transparent">hello</body></html>';

      // Wait until screenshotImageData !== screenshotImageDatas[0].
      waitForScreenshot(function(screenshotImageData) {
        var view1 = screenshotImageData.data;
        var view2 = screenshotImageDatas[0].data;

        if (view1.length != view2.length) {
          return true;
        }

        for (var i = 0; i < view1.length; i++) {
          if (view1[i] != view2[i]) {
            return true;
          }
        }

        return false;
      }, 'image/png');
    }
    else if (screenshotImageDatas.length === 2) {
      ok(true, 'Got updated screenshot after source page changed');

      var view = screenshotImageData.data;
      if (view[3] !== 0) {
        // The case here will always fail when oop'd on Firefox Desktop,
        // but not on B2G Emulator
        // See https://bugzil.la/878003#c20

        var isB2G = (navigator.platform === '');
        info('navigator.platform: ' + navigator.platform);
        if (!isB2G && browserElementTestHelpers.getOOPByDefaultPref()) {
          todo(false, 'The first pixel of updated screenshot is not transparent');
        } else {
          ok(false, 'The first pixel of updated screenshot is not transparent');
        }
        SimpleTest.finish();
        return;
      }

      ok(true, 'Verified the first pixel of updated screenshot is transparent');
      SimpleTest.finish();
    }
  }

  // We continually take screenshots until we get one that we are
  // happy with.
  function waitForScreenshot(filter, mimeType) {
    function gotImage(e) {
      // |this| is the Image.

      URL.revokeObjectURL(this.src);

      if (e.type === 'error' || !this.width || !this.height) {
        tryAgain();

        return;
      }

      var canvas = document.createElement('canvas');
      canvas.width = canvas.height = 1000;
      var ctx = canvas.getContext('2d');
      ctx.drawImage(this, 0, 0);
      var imageData = ctx.getImageData(0, 0, 1000, 1000);

      if (filter(imageData)) {
        screenshotTaken(imageData);
        return;
      }
      tryAgain();
    }

    function tryAgain() {
      if (--attempts === 0) {
        ok(false, 'Timed out waiting for correct screenshot');
        SimpleTest.finish();
      } else {
        setTimeout(function() {
          iframe1.getScreenshot(1000, 1000, mimeType).onsuccess =
            getScreenshotImageData;
        }, 200);
      }
    }

    function getScreenshotImageData(e) {
      var blob = e.target.result;
      if (blob.type !== mimeType) {
        ok(false, 'MIME type of screenshot taken incorrect');
        SimpleTest.finish();
      }

      if (blob.size === 0) {
        tryAgain();

        return;
      }

      var img = new Image();
      img.src = URL.createObjectURL(blob);
      img.onload = img.onerror = gotImage;
    }

    var attempts = 10;
    iframe1.getScreenshot(1000, 1000, mimeType).onsuccess =
      getScreenshotImageData;
  }

  function iframeLoadedHandler() {
    numLoaded++;
    if (numLoaded === 2) {
      waitForScreenshot(function(screenshotImageData) {
        return true;
      }, 'image/jpeg');
    }
  }

  iframe1.addEventListener('mozbrowserloadend', iframeLoadedHandler);
}

addEventListener('testready', runTest);
