/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that scroll event bubbles up.
"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();
browserElementTestHelpers.allowTopLevelDataURINavigation();

function runTest() {
  var iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");
  document.body.appendChild(iframe);

  iframe.addEventListener("mozbrowserscroll", function(e) {
    ok(true, "got mozbrowserscroll event.");
    ok(e.detail, "event.detail is not null.");
    is(Math.round(e.detail.top), 4000, "top position is correct.");
    is(Math.round(e.detail.left), 4000, "left position is correct.");
    SimpleTest.finish();
  });

  // We need a viewport meta tag to allow us to scroll to (4000, 4000). Without
  // the viewport meta tag, we shrink the (5000, 5000) content so that we can't
  // have enough space to scroll to the point in the layout viewport.
  iframe.src =
    "data:text/html,<html><meta name='viewport' content='width=device-width,minimum-scale=1,initial-scale=1'><body style='min-height: 5000px; min-width: 5000px;'></body><script>window.scrollTo(4000, 4000);</script></html>";
}

addEventListener("testready", runTest);
