/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that scroll event bubbles up.
"use strict";

SimpleTest.waitForExplicitFinish();

function runTest() {
  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addToWhitelist();

  var iframe = document.createElement('iframe');
  iframe.mozbrowser = true;
  document.body.appendChild(iframe);

  iframe.addEventListener("mozbrowserscroll", function(e) {
    ok(true, "got mozbrowserscroll event.");
    ok(e.detail, "event.detail is not null.");
    ok(e.detail.top === 4000, "top position is correct.");
    ok(e.detail.left === 4000, "left position is correct.");
    SimpleTest.finish();
  });

  iframe.src = "data:text/html,<html><body style='min-height: 5000px; min-width: 5000px;'></body><script>window.scrollTo(4000, 4000);</script></html>";
}

runTest();
