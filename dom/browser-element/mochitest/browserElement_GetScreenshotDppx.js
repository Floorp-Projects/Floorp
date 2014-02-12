/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the getScreenshot property for mozbrowser
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var dppxPref = 'layout.css.devPixelsPerPx';
  var cssPixelWidth = 600;
  var cssPixelHeight = 400;

  var iframe1 = document.createElement('iframe');
  iframe1.setAttribute('width', cssPixelWidth);
  iframe1.setAttribute('height', cssPixelHeight);
  SpecialPowers.wrap(iframe1).mozbrowser = true;

  iframe1.src = 'data:text/html,<html><body>hello</body></html>';
  document.body.appendChild(iframe1);

  var images = [];

  function screenshotTaken(image) {
    images.push(image);
    if (images.length === 1) {
      ok(true, 'Got initial non blank screenshot');

      if (image.width !== cssPixelWidth || image.height !== cssPixelHeight) {
        ok(false, 'The pixel width of the image received is not correct');
        SimpleTest.finish();
        return;
      }
      ok(true, 'The pixel width of the image received is correct');

      SpecialPowers.pushPrefEnv(
        {'set': [['layout.css.devPixelsPerPx', 2]]}, takeScreenshot);
    }
    else if (images.length === 2) {
      ok(true, 'Got updated screenshot after source page changed');

      if (image.width !== cssPixelWidth * 2 ||
          image.height !== cssPixelHeight * 2) {
        ok(false, 'The pixel width of the 2dppx image received is not correct');
        SimpleTest.finish();
        return;
      }
      ok(true, 'The pixel width of the 2dppx image received is correct');
      SimpleTest.finish();
    }
  }

  function takeScreenshot() {
    function gotImage(e) {
      // |this| is the Image.

      URL.revokeObjectURL(this.src);

      if (e.type === 'error' || !this.width || !this.height) {
        tryAgain();

        return;
      }

      screenshotTaken(this);
    }

    function tryAgain() {
      if (--attempts === 0) {
        ok(false, 'Timed out waiting for correct screenshot');
        SimpleTest.finish();
      } else {
        setTimeout(function() {
          iframe1.getScreenshot(cssPixelWidth, cssPixelHeight).onsuccess =
            getScreenshotImageData;
        }, 200);
      }
    }

    function getScreenshotImageData(e) {
      var blob = e.target.result;
      if (blob.size === 0) {
        tryAgain();

        return;
      }

      var img = new Image();
      img.src = URL.createObjectURL(blob);
      img.onload = img.onerror = gotImage;
    }

    var attempts = 10;
    iframe1.getScreenshot(cssPixelWidth, cssPixelHeight).onsuccess =
      getScreenshotImageData;
  }

  function iframeLoadedHandler() {
    SpecialPowers.pushPrefEnv(
      {'set': [['layout.css.devPixelsPerPx', 1]]}, takeScreenshot);
  }

  iframe1.addEventListener('mozbrowserloadend', iframeLoadedHandler);
}

addEventListener('testready', runTest);
