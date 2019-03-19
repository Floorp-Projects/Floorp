/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that sendMouseEvent dispatch events.
"use strict";

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
  // var tabParent = SpecialPowers.wrap(iframe)
  //                .frameLoader.tabParent;
  // and calling tabParent.getChildProcessOffset(offsetX, offsetY) if
  // tabParent was not null, but tabParent was always null.

  iframe.addEventListener("mozbrowserloadend", function onloadend(e) {
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
