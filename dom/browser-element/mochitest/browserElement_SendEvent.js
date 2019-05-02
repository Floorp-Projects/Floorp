/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that sendMouseEvent dispatch events.
"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

function runTest() {
  var iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");
  document.body.appendChild(iframe);
  var x = 10;
  var y = 10;
  // This test used to try to transform the coordinates from child
  // to parent coordinate space by first calling
  // iframe.getBoundingClientRect();
  // to refresh offsets and then calling
  // var remoteTab = SpecialPowers.wrap(iframe)
  //                .frameLoader.remoteTab;
  // and calling remoteTab.getChildProcessOffset(offsetX, offsetY) if
  // remoteTab was not null, but remoteTab was always null.

  iframe.addEventListener("mozbrowserloadend", function onloadend(e) {
    // Ensure we lay out the iframe before sending mouse events.
    iframe.getBoundingClientRect();
    iframe.sendMouseEvent("mousedown", x, y, 0, 1, 0);
  });

  iframe.addEventListener("mozbrowserlocationchange", function onlocchange(e) {
    var a = document.createElement("a");
    a.href = e.detail.url;

    switch (a.hash) {
      case "#mousedown":
        ok(true, "Receive a mousedown event.");
        iframe.sendMouseEvent("mousemove", x, y, 0, 0, 0);
        break;
      case "#mousemove":
        ok(true, "Receive a mousemove event.");
        iframe.sendMouseEvent("mouseup", x, y, 0, 1, 0);
        break;
      case "#mouseup":
        ok(true, "Receive a mouseup event.");
        break;
      case "#click":
        ok(true, "Receive a click event.");
        iframe.removeEventListener("mozbrowserlocationchange", onlocchange);
        SimpleTest.finish();
        break;
    }
  });

  iframe.src = "file_browserElement_SendEvent.html";
}

addEventListener("testready", runTest);
