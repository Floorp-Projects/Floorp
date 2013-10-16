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

function notifyPrecise()
{
  Services.obs.notifyObservers(null, "metro_precise_input", null);
}

function notifyImprecise()
{
  Services.obs.notifyObservers(null, "metro_imprecise_input", null);
}

gTests.push({
  desc: "precise/imprecise input switcher",
  setUp: setUp,
  run: function () {
    notifyPrecise();
    testState("precise");
    notifyImprecise();
    testState("imprecise");
    notifyPrecise();
    testState("precise");
    notifyImprecise();
    testState("imprecise");
  }
});

