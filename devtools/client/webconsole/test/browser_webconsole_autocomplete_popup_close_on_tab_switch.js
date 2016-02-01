/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the autocomplete popup closes on switching tabs. See bug 900448.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>bug 900448 - autocomplete " +
                 "popup closes on tab switch";

add_task(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  let popup = hud.jsterm.autocompletePopup;
  let popupShown = onPopupShown(popup._panel);

  hud.jsterm.setInputValue("sc");
  EventUtils.synthesizeKey("r", {});

  yield popupShown;

  ok(!popup.isOpen, "Popup closes on tab switch");
});

function onPopupShown(panel) {
  let finished = promise.defer();

  panel.addEventListener("popupshown", function popupOpened() {
    panel.removeEventListener("popupshown", popupOpened, false);
    loadTab("data:text/html;charset=utf-8,<p>testing autocomplete closes")
      .then(finished.resolve);
  }, false);

  return finished.promise;
}
