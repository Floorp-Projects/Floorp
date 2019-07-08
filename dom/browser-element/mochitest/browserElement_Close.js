/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that window.close() works.
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

  iframe.addEventListener("mozbrowserclose", function(e) {
    ok(true, "got mozbrowserclose event.");
    SimpleTest.finish();
  });

  iframe.src =
    "data:text/html,<html><body><script>window.close()</script></body></html>";
}

addEventListener("testready", runTest);
