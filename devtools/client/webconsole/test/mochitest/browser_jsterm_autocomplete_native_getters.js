/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that native getters (e.g. document.body) autocompletes in the web console.
// See Bug 651501.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,Test document.body autocompletion";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm, ui } = hud;

  const { autocompletePopup: popup } = jsterm;

  ok(!popup.isOpen, "popup is not open");
  const onPopupOpen = popup.once("popup-opened");

  setInputValue(hud, "document.body");
  EventUtils.sendString(".");

  await onPopupOpen;

  ok(popup.isOpen, "popup is open");
  const cacheMatches = ui.wrapper.getStore().getState().autocomplete.cache
    .matches;
  is(popup.itemCount, cacheMatches.length, "popup.itemCount is correct");
  ok(
    cacheMatches.includes("addEventListener"),
    "addEventListener is in the list of suggestions"
  );
  ok(cacheMatches.includes("bgColor"), "bgColor is in the list of suggestions");
  ok(
    cacheMatches.includes("ATTRIBUTE_NODE"),
    "ATTRIBUTE_NODE is in the list of suggestions"
  );

  const onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Escape");

  await onPopupClose;

  ok(!popup.isOpen, "popup is not open");
  const onAutoCompleteUpdated = jsterm.once("autocomplete-updated");
  const inputStr = "document.b";
  setInputValue(hud, inputStr);
  EventUtils.sendString("o");

  await onAutoCompleteUpdated;
  checkInputCompletionValue(hud, "dy", "autocomplete shows document.body");
});
