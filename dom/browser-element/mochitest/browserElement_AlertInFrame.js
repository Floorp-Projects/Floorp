/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that alert works from inside an <iframe> inside an <iframe mozbrowser>.
"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");

  iframe.addEventListener("mozbrowsershowmodalprompt", function(e) {
    is(e.detail.message, "Hello");
    SimpleTest.finish();
  });

  iframe.src = "file_browserElement_AlertInFrame.html";
  document.body.appendChild(iframe);
}

addEventListener("testready", runTest);
