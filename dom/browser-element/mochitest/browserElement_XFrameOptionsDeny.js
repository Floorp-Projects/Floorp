/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 770239 - Test that X-Frame-Options will correctly block a page inside a
// subframe of <iframe mozbrowser>.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var initialScreenshotArrayBuffer;

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
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');

  // Our child will create two iframes, so make sure this iframe is big enough
  // to show both of them without scrolling, so taking a screenshot gets both
  // frames.
  iframe.height = '1000px';

  var step1, stepfinish;
  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    switch (e.detail.message) {
    case 'step 1':
      step1 = SpecialPowers.snapshotWindow(iframe.contentWindow);
      break;
    case 'step 2':
      // The page has now attempted to load the X-Frame-Options page; take
      // another screenshot.
      stepfinish = SpecialPowers.snapshotWindow(iframe.contentWindow);
      ok(step1.toDataURL() == stepfinish.toDataURL(), "Screenshots should be identical");
      SimpleTest.finish();
      break;
    }
  });

  document.body.appendChild(iframe);

  // Load this page from a different origin than ourselves.  This page will, in
  // turn, load a child from mochi.test:8888, our origin, with X-Frame-Options:
  // SAMEORIGIN.  That load should be denied.
  iframe.src = 'http://example.com/tests/dom/browser-element/mochitest/file_browserElement_XFrameOptionsDeny.html';
}

addEventListener('testready', runTest);
