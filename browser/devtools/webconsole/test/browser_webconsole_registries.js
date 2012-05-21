/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the HUD service keeps an accurate registry of all the Web Console
// instances.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testRegistries, false);
}

function testRegistries() {
  browser.removeEventListener("DOMContentLoaded", testRegistries, false);

  openConsole();

  let hud = HUDService.getHudByWindow(content);
  ok(hud, "we have a HUD");
  ok(HUDService.hudReferences[hud.hudId], "we have a HUD in hudReferences");

  let windowID = WebConsoleUtils.getOuterWindowId(content);
  is(HUDService.windowIds[windowID], hud.hudId, "windowIds are working");

  finishTest();
}

