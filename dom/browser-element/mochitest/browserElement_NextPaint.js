/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 808231 - Add mozbrowsernextpaint event.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;
  document.body.appendChild(iframe);

  // Add a first listener that we'll remove shortly after.
  iframe.addNextPaintListener(wrongListener);

  var gotFirstNextPaintEvent = false;
  iframe.addNextPaintListener(function () {
    ok(!gotFirstNextPaintEvent, 'got the first nextpaint event');

    // Make sure we're only called once.
    gotFirstNextPaintEvent = true;

    iframe.addNextPaintListener(function () {
      info('got the second nextpaint event');
      SimpleTest.finish();
    });

    // Force the iframe to repaint.
    SimpleTest.executeSoon(function () iframe.src += '#next');
  });

  // Remove the first listener to make sure it's not called.
  iframe.removeNextPaintListener(wrongListener);
  iframe.src = 'file_browserElement_NextPaint.html';
}

function wrongListener() {
  ok(false, 'first listener should have been removed');
}

addEventListener('testready', runTest);
