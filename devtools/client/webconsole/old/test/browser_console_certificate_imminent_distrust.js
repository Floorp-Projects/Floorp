/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests handling of certificates that will be imminently distrusted, and thus
// should emit a warning to the console.
//
// This test requires a cert to be created in build/pgo/certs.
//
// Change directories to build/pgo/certs:
//  cd build/pgo/certs
//
//  certutil -S -d . -n "imminently_distrusted" -s "CN=Imminently Distrusted End Entity" -c "pgo temporary ca" -t "P,," -k rsa -g 2048 -Z SHA256 -m 1519140221 -v 120 -8 "imminently-distrusted.example.com"
//


const TEST_URI = "data:text/html;charset=utf8,Browser Console imminent " +
                 "distrust warnings test";
const TEST_URI_PATH = "/browser/devtools/client/webconsole/old/test/" +
                      "test-certificate-messages.html";

var gWebconsoleTests = [
  {url: "https://sha256ee.example.com" + TEST_URI_PATH,
   name: "Imminent distrust warnings appropriately not present",
   warning: [], nowarning: ["Upcoming_Distrust_Actions"]},
  {url: "https://imminently-distrusted.example.com" +
          TEST_URI_PATH,
   name: "Imminent distrust warning displayed successfully",
   warning: ["Upcoming_Distrust_Actions"], nowarning: []},
];
const TRIGGER_MSG = "If you haven't seen ssl warnings yet, you won't";

var gHud = undefined, gContentBrowser;
var gCurrentTest;

function test() {
  registerCleanupFunction(function () {
    gHud = gContentBrowser = null;
  });

  loadTab(TEST_URI).then(({browser}) => {
    gContentBrowser = browser;

    let opened = waitForBrowserConsole();

    let hud = HUDService.getBrowserConsole();
    ok(!hud, "browser console is not open");

    HUDService.toggleBrowserConsole();

    opened.then(function (hud) {
      ok(hud, "browser console opened");
      runTestLoop(hud);
    });
  });
}

function runTestLoop(theHud) {
  gCurrentTest = gWebconsoleTests.shift();
  if (!gCurrentTest) {
    finishTest();
    return;
  }
  if (!gHud) {
    gHud = theHud;
  }
  gHud.jsterm.clearOutput();
  BrowserTestUtils.browserLoaded(gContentBrowser).then(onLoad);
  if (gCurrentTest.pref) {
    SpecialPowers.pushPrefEnv({"set": gCurrentTest.pref},
      function () {
        BrowserTestUtils.loadURI(gBrowser.selectedBrowser, gCurrentTest.url);
      });
  } else {
    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, gCurrentTest.url);
  }
}

function onLoad() {
  waitForSuccess({
    name: gCurrentTest.name,
    validator: function () {
      if (gHud.outputNode.textContent.includes(TRIGGER_MSG)) {
        for (let warning of gCurrentTest.warning) {
          if (!gHud.outputNode.textContent.includes(warning)) {
            return false;
          }
        }
        for (let nowarning of gCurrentTest.nowarning) {
          if (gHud.outputNode.textContent.includes(nowarning)) {
            return false;
          }
        }
        return true;
      }
    }
  }).then(runTestLoop);
}
