/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 770239 - Test that we can load pages with X-Frame-Options: Deny inside
// <iframe mozbrowser>.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;

  // The page we load will fire an alert when it successfully loads.
  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    ok(true, "Got alert");
    SimpleTest.finish();
  });

  document.body.appendChild(iframe);
  iframe.src = 'file_browserElement_XFrameOptions.sjs?DENY';
}

addEventListener('testready', runTest);
