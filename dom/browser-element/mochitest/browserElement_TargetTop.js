/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 771273 - Check that window.open(url, '_top') works properly with <iframe
// mozbrowser>.
"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");

  iframe.addEventListener("mozbrowseropenwindow", function(e) {
    ok(false, "Not expecting an openwindow event.");
  });

  iframe.addEventListener("mozbrowserlocationchange", function(e) {
    if (/file_browserElement_TargetTop.html\?2$/.test(e.detail.url)) {
      ok(true, "Got the locationchange we were looking for.");
      SimpleTest.finish();
    }
  });

  document.body.appendChild(iframe);
  iframe.src = "file_browserElement_TargetTop.html";
}

addEventListener("testready", runTest);
