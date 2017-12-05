/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests handling of certificates issued by Symantec. If such
// certificates have a notBefore before 1 June 2016, and are not
// issued by an Apple or Google intermediate, they should emit a
// warning to the console.
//
// This test required two certs to be created in build/pgo/certs:
// 1. A new trusted root. This should theoretically be built with certutil, but
//    because it needs to have a perfectly-matching Subject, this wasn't
//    (currently) practical.
// 2. An affected certificate from before the cutoff
//
// Change directories to build/pgo/certs:
//  cd build/pgo/certs
//
// Figure out the months-warp-factor for the cutoff, first. We'll use this later.
//
//   monthsSince=$(( ( $(date -u +"%s") - $(date -u -d "2016-06-01 00:00:00" +"%s") ) / (60*60*24*30) + 1 ))
//
// Constructing the root with certutil should look like this:
//   certutil -S -s "C=US,O=GeoTrust Inc.,CN=GeoTrust Universal CA" -t "C,," -x -m 1 -w -${monthsSince} -v 120 -n "symantecRoot" -Z SHA256 -g 2048 -2 -d .
//   (export) certutil -L -d . -n "symantecRoot" -a -o symantecRoot.ca
//
// Unfortunately, certutil reorders the RDNs so that C doesn't come first.
// Instead, we'll use one of the precisely-created certificates from the xpcshell
// tests: security/manager/ssl/tests/unit/test_symantec_apple_google/test-ca.pem
//
// We'll need to cheat and make a pkcs12 file to import to get the key.
//   openssl pkcs12 -export -out symantecRoot.p12 -inkey ../../../security/manager/ssl/tests/unit/test_symantec_apple_google/default-ee.key -in ../../../security/manager/ssl/tests/unit/test_symantec_apple_google/test-ca.pem
//   certutil -A -d . -n "symantecRoot" -t "C,," -a -i ../../../security/manager/ssl/tests/unit/test_symantec_apple_google/test-ca.pem
//   pk12util -d . -i symantecRoot.p12
//
// With that in hand, we can generate a keypair for the test site:
//   certutil -S -d . -n "symantec_affected" -s "CN=symantec-not-whitelisted-before-cutoff.example.com" -c "symantecRoot" -t "P,," -k rsa -g 2048 -Z SHA256 -m 8939454 -w -${monthsSince} -v 120 -8 "symantec-not-whitelisted-before-cutoff.example.com"
//
// Finally, copy in that key as a .ca file:
// (NOTE: files ended in .ca are added as trusted roots by the mochitest harness)
//   cp ../../../security/manager/ssl/tests/unit/test_symantec_apple_google/test-ca.pem symantecRoot.ca


const TEST_URI = "data:text/html;charset=utf8,Browser Console imminent " +
                 "distrust warnings test";
const TEST_URI_PATH = "/browser/devtools/client/webconsole/test/" +
                      "test-certificate-messages.html";

var gWebconsoleTests = [
  {url: "https://sha256ee.example.com" + TEST_URI_PATH,
   name: "Imminent distrust warnings appropriately not present",
   warning: [], nowarning: ["Upcoming_Distrust_Actions"]},
  {url: "https://symantec-not-whitelisted-before-cutoff.example.com" +
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
      if (gHud.outputNode.textContent.indexOf(TRIGGER_MSG) >= 0) {
        for (let warning of gCurrentTest.warning) {
          if (gHud.outputNode.textContent.indexOf(warning) < 0) {
            return false;
          }
        }
        for (let nowarning of gCurrentTest.nowarning) {
          if (gHud.outputNode.textContent.indexOf(nowarning) >= 0) {
            return false;
          }
        }
        return true;
      }
    }
  }).then(runTestLoop);
}
