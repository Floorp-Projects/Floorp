/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 770239 - Load an X-Frame-Options: SAMEORIGIN page inside an <iframe>
// inside <iframe mozbrowser>.  The two iframes will have the same origin, but
// this page will be of a different origin.  The load should succeed.

"use strict";

SimpleTest.waitForExplicitFinish();

function runTest() {
  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addToWhitelist();

  var iframe = document.createElement('iframe');
  iframe.mozbrowser = true;

  // The innermost page we load will fire an alert when it successfully loads.
  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    ok(true, "Got alert");
    SimpleTest.finish();
  });

  document.body.appendChild(iframe);
  iframe.src = 'http://example.com/tests/dom/browser-element/mochitest/file_browserElement_XFrameOptionsSameOrigin.html';
}

runTest();
