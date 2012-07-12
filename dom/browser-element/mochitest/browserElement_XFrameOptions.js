/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 770239 - Test that we can load pages with X-Frame-Options: Deny inside
// <iframe mozbrowser>.
"use strict";

SimpleTest.waitForExplicitFinish();

function runTest() {
  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addToWhitelist();

  var iframe = document.createElement('iframe');
  iframe.mozbrowser = true;

  // The page we load will fire an alert when it successfully loads.
  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    ok(true, "Got alert");
    SimpleTest.finish();
  });

  document.body.appendChild(iframe);
  iframe.src = 'file_browserElement_XFrameOptions.sjs?DENY';
}

runTest();
