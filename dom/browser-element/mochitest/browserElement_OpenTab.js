/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1144015 - test middle/ctrl/cmd-click on a link.

"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  let iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");
  document.body.appendChild(iframe);

  let x = 2;
  let y = 2;
  // This test used to try to transform the coordinates from child
  // to parent coordinate space by first calling
  // iframe.getBoundingClientRect();
  // to refresh offsets and then calling
  // var remoteTab = SpecialPowers.wrap(iframe)
  //                .frameLoader.remoteTab;
  // and calling remoteTab.getChildProcessOffset(offsetX, offsetY) if
  // remoteTab was not null, but remoteTab was always null.

  let sendCtrlClick = () => {
    let nsIDOMWindowUtils = SpecialPowers.Ci.nsIDOMWindowUtils;
    let mod = nsIDOMWindowUtils.MODIFIER_META |
              nsIDOMWindowUtils.MODIFIER_CONTROL;
    iframe.sendMouseEvent("mousedown", x, y, 0, 1, mod);
    iframe.sendMouseEvent("mouseup", x, y, 0, 1, mod);
  };

  let onCtrlClick = e => {
    is(e.detail.url, "http://example.com/", "URL matches");
    iframe.removeEventListener("mozbrowseropentab", onCtrlClick);
    iframe.addEventListener("mozbrowseropentab", onMiddleClick);
    sendMiddleClick();
  };

  let sendMiddleClick = () => {
    iframe.sendMouseEvent("mousedown", x, y, 1, 1, 0);
    iframe.sendMouseEvent("mouseup", x, y, 1, 1, 0);
  };

  let onMiddleClick = e => {
    is(e.detail.url, "http://example.com/", "URL matches");
    iframe.removeEventListener("mozbrowseropentab", onMiddleClick);
    SimpleTest.finish();
  };

  iframe.addEventListener("mozbrowserloadend", e => {
    iframe.addEventListener("mozbrowseropentab", onCtrlClick);
    sendCtrlClick();
  });


  iframe.src = 'data:text/html,<body style="margin:0"><a href="http://example.com"><span>click here</span></a></body>';
}

addEventListener("testready", runTest);
