/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the autocomplete popup closes on switching tabs. See bug 900448.

const TEST_URI = "data:text/html;charset=utf-8,<p>bug 900448 - autocomplete popup closes on tab switch";

let popup = null;

registerCleanupFunction(function() {
  popup = null;
});

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(HUD) {
  popup = HUD.jsterm.autocompletePopup;

  popup._panel.addEventListener("popupshown", function popupOpened() {
    popup._panel.removeEventListener("popupshown", popupOpened, false);
    addTab("data:text/html;charset=utf-8,<p>testing autocomplete closes");
    gBrowser.selectedBrowser.addEventListener("load", tab2Loaded, true);
  }, false);

  HUD.jsterm.setInputValue("sc");
  EventUtils.synthesizeKey("r", {});
}

function tab2Loaded() {
  gBrowser.selectedBrowser.removeEventListener("load", tab2Loaded, true);
  ok(!popup.isOpen, "Popup closes on tab switch");
  gBrowser.removeCurrentTab();
  finishTest();
}
