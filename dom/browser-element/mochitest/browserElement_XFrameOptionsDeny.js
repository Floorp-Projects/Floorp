/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 770239 - Test that X-Frame-Options will correctly block a page inside a
// subframe of <iframe mozbrowser>.
"use strict";

SimpleTest.waitForExplicitFinish();

var initialScreenshot;

function runTest() {
  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addToWhitelist();

  var iframe = document.createElement('iframe');
  iframe.mozbrowser = true;

  // Our child will create two iframes, so make sure this iframe is big enough
  // to show both of them without scrolling, so taking a screenshot gets both
  // frames.
  iframe.height = '1000px';

  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    switch (e.detail.message) {
    case 'step 1':
      // Make the page wait for us to unblock it (which we do after we finish
      // taking the screenshot).
      e.preventDefault();

      iframe.getScreenshot().onsuccess = function(sshot) {
        initialScreenshot = sshot.target.result;
        e.detail.unblock();
      };
      break;
    case 'step 2':
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

  // Load this page from a different origin than ourselves.  This page will, in
  // turn, load a child from mochi.test:8888, our origin, with X-Frame-Options:
  // SAMEORIGIN.  That load should be denied.
  iframe.src = 'http://example.com/tests/dom/browser-element/mochitest/file_browserElement_XFrameOptionsDeny.html';
}

runTest();
