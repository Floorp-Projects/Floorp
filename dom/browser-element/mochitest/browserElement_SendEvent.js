/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that sendMouseEvent dispatch events.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement("iframe");
  SpecialPowers.wrap(iframe).mozbrowser = true;
  document.body.appendChild(iframe);

  iframe.addEventListener("mozbrowserloadend", function onloadend(e) {
    iframe.sendMouseEvent("mousedown", 10, 10, 0, 1, 0);
  });

  iframe.addEventListener("mozbrowserlocationchange", function onlocchange(e) {
    var a = document.createElement("a");
    a.href = e.detail;

    switch (a.hash) {
      case "#mousedown":
        ok(true, "Receive a mousedown event.");
        iframe.sendMouseEvent("mousemove", 10, 10, 0, 0, 0);
        break;
      case "#mousemove":
        ok(true, "Receive a mousemove event.");
        iframe.sendMouseEvent("mouseup", 10, 10, 0, 1, 0);
        break;
      case "#mouseup":
        ok(true, "Receive a mouseup event.");
        break;
      case "#click":
        ok(true, "Receive a click event.");
        if (SpecialPowers.getIntPref("dom.w3c_touch_events.enabled") != 0) {
          iframe.sendTouchEvent("touchstart", [1], [10], [10], [2], [2],
                                [20], [0.5], 1, 0);
        } else {
          SimpleTest.finish();
        }
        break;
      case "#touchstart":
        ok(true, "Receive a touchstart event.");
        iframe.sendTouchEvent("touchmove", [1], [10], [10], [2], [2],
                              [20], [0.5], 1, 0);
      case "#touchmove":
        ok(true, "Receive a touchmove event.");
        iframe.sendTouchEvent("touchend", [1], [10], [10], [2], [2],
                              [20], [0.5], 1, 0);
        break;
      case "#touchend":
        ok(true, "Receive a touchend event.");
        iframe.sendTouchEvent("touchcancel", [1], [10], [10], [2], [2],
                              [20], [0.5], 1, 0);
        SimpleTest.finish();
        break;
    }
  });

  iframe.src = "data:text/html,<html><body>" +
               "<button>send[Mouse|Touch]Event</button>" +
               "</body><script>" +
               "function changeHash(e) {" +
               "  document.location.hash = e.type;" +
               "};" +
               "window.addEventListener('mousedown', changeHash);" +
               "window.addEventListener('mousemove', changeHash);" +
               "window.addEventListener('mouseup', changeHash);" +
               "window.addEventListener('click', changeHash, true);" +
               "window.addEventListener('touchstart', changeHash);" +
               "window.addEventListener('touchmove', changeHash);" +
               "window.addEventListener('touchend', changeHash);" +
               "window.addEventListener('touchcancel', changeHash);" +
               "</script></html>";

}

addEventListener('testready', runTest);
