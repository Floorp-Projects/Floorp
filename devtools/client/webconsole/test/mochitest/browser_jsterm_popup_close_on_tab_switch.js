/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the autocomplete popup closes on switching tabs. See bug 900448.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>bug 900448 - autocomplete " +
                 "popup closes on tab switch";
const TEST_URI_NAVIGATE = "data:text/html;charset=utf-8,<p>testing autocomplete closes";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const popup = hud.jsterm.autocompletePopup;
  const popupShown = once(popup, "popup-opened");

  hud.jsterm.setInputValue("sc");
  EventUtils.sendString("r");

  await popupShown;

  await addTab(TEST_URI_NAVIGATE);

  ok(!popup.isOpen, "Popup closes on tab switch");
});
