// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*=============================================================================
  Test cases
=============================================================================*/

function test() {
  runTests();
}

function setUp() {
  yield addTab("about:blank");
}

function testState(aState) {
  let bcastValue = document.getElementById("bcast_preciseInput").getAttribute("input");
  is(bcastValue, aState, "bcast attribute is " + aState);

  if (aState == "precise") {
    ok(InputSourceHelper.isPrecise, "InputSourceHelper");
    let uri = Util.makeURI("chrome://browser/content/cursor.css");
    ok(!StyleSheetSvc.sheetRegistered(uri, Ci.nsIStyleSheetService.AGENT_SHEET), "cursor stylesheet registered");
  } else {
    ok(!InputSourceHelper.isPrecise, "InputSourceHelper");
    let uri = Util.makeURI("chrome://browser/content/cursor.css");
    ok(StyleSheetSvc.sheetRegistered(uri, Ci.nsIStyleSheetService.AGENT_SHEET), "cursor stylesheet registered");
  }
}

function sendMouseMoves() {
  let utils = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                      .getInterface(Components.interfaces.nsIDOMWindowUtils);
  for (let deg = 0; deg < 180; deg++) {
    let coord = Math.sin((deg * Math.PI)/180) * 750;
    utils.sendMouseEventToWindow("mousemove", coord, coord, 2, 1, 0, true,
                                  1, Ci.nsIDOMMouseEvent.MOZ_SOURCE_MOUSE);
  }
}

function sendTouchStart() {
  EventUtils.synthesizeTouchAtPoint(100, 100, { type: "touchstart" }, window);
}

function sendTouchMove() {
  EventUtils.synthesizeTouchAtPoint(100, 100, { type: "touchmove" }, window);
}

function sendTouchEnd() {
  EventUtils.synthesizeTouchAtPoint(100, 100, { type: "touchend" }, window);
}

gTests.push({
  desc: "precise/imprecise input switcher",
  setUp: setUp,
  run: function () {
    sendMouseMoves();
    testState("precise");
    sendTouchStart();
    testState("imprecise");
    sendMouseMoves();
    testState("imprecise");
    sendTouchMove();
    testState("imprecise");
    sendTouchEnd();
    testState("imprecise");
    sendMouseMoves();
    testState("precise");
  }
});

