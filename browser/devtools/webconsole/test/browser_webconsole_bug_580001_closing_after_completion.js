/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests to ensure that errors don't appear when the console is closed while a
// completion is being performed.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testClosingAfterCompletion);
  }, true);
}

function testClosingAfterCompletion(hud) {
  let inputNode = hud.jsterm.inputNode;

  let errorWhileClosing = false;
  function errorListener(evt) {
    errorWhileClosing = true;
  }

  browser.addEventListener("error", errorListener, false);

  // Focus the inputNode and perform the keycombo to close the WebConsole.
  inputNode.focus();

  gDevTools.once("toolbox-destroyed", function() {
    browser.removeEventListener("error", errorListener, false);
    is(errorWhileClosing, false, "no error while closing the WebConsole");
    finishTest();
  });

  if (Services.appinfo.OS == "Darwin") {
    EventUtils.synthesizeKey("i", { accelKey: true, altKey: true });
  } else {
    EventUtils.synthesizeKey("i", { accelKey: true, shiftKey: true });
  }
}

