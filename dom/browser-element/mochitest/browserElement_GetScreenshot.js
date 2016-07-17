/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the getScreenshot property for mozbrowser
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

function runTest() {
  var iframe1 = document.createElement('iframe');
  iframe1.setAttribute('mozbrowser', 'true');

  iframe1.src = 'data:text/html,<html>' +
    '<body style="background:green">hello</body></html>';
  document.body.appendChild(iframe1);

  var screenshotImageDatas = [];

  function screenshotTaken(aScreenshotImageData) {
    screenshotImageDatas.push(aScreenshotImageData);

    if (screenshotImageDatas.length === 1) {
      ok(true, 'Got initial non blank screenshot');

      var view = aScreenshotImageData.data;
      if (view[3] !== 255) {
        ok(false, 'The first pixel of initial screenshot is not opaque');
        SimpleTest.finish();
        return;
      }
      ok(true, 'Verified the first pixel of initial screenshot is opaque');

      iframe1.src = 'data:text/html,<html>' +
        '<body style="background:transparent">hello</body></html>';

      iframe1.addEventListener('mozbrowserloadend', ()=>takeScreenshot('image/png'));

    }
    else if (screenshotImageDatas.length === 2) {
      ok(true, 'Got updated screenshot after source page changed');

      var view = aScreenshotImageData.data;
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
  function takeScreenshot(mimeType) {
    function gotImage(e) {
      // |this| is the Image.

      URL.revokeObjectURL(this.src);

      if (e.type === 'error' || !this.width || !this.height) {
        ok(false, "load image error");
        SimpleTest.finish();
        return;
      }

      var canvas = document.createElement('canvas');
      canvas.width = canvas.height = 1000;
      var ctx = canvas.getContext('2d');
      ctx.drawImage(this, 0, 0);
      var imageData = ctx.getImageData(0, 0, 1000, 1000);

      screenshotTaken(imageData);
    }

    function getScreenshotImageData(e) {
      var blob = e.target.result;
      if (blob.type !== mimeType) {
        ok(false, 'MIME type of screenshot taken incorrect');
        SimpleTest.finish();
      }

      if (blob.size === 0) {
        ok(false, "get screenshot image error");
        SimpleTest.finish();
      }

      var img = new Image();
      img.src = URL.createObjectURL(blob);
      img.onload = img.onerror = gotImage;
    }

    iframe1.getScreenshot(1000, 1000, mimeType).onsuccess =
      getScreenshotImageData;
  }

  function iframeLoadedHandler(e) {
    iframe1.removeEventListener('mozbrowserloadend', iframeLoadedHandler);
    takeScreenshot('image/jpeg');
  }

  iframe1.addEventListener('mozbrowserloadend', iframeLoadedHandler);
}

addEventListener('testready', runTest);
