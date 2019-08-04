/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the accepting an autocompletion does not scroll the input.

const TEST_URI = `data:text/html;charset=utf-8,
  <script>
    /* Create a prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    window.foobar = Object.create(null, Object.getOwnPropertyDescriptors({
      item0: "value0",
      item1: "value1",
    }));
  </script>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm, ui } = hud;
  const { autocompletePopup: popup } = jsterm;

  info("Insert multiple new lines so the input overflows");
  const onPopUpOpen = popup.once("popup-opened");
  const lines = "\n".repeat(200);
  setInputValue(hud, lines);

  info("Fire the autocompletion popup");
  EventUtils.sendString("window.foobar.");

  await onPopUpOpen;
  const scrollableEl = ui.window.document.querySelector(".CodeMirror-scroll");

  ok(scrollableEl.scrollTop > 0, "The input overflows");
  const scrollTop = scrollableEl.scrollTop;

  info("Hit Enter to accept the autocompletion");
  const onPopupClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopupClose;

  ok(!popup.isOpen, "popup is not open after KEY_Enter");
  is(
    getInputValue(hud),
    lines + "window.foobar.item0",
    "completion was successful after KEY_Enter"
  );
  is(
    scrollableEl.scrollTop,
    scrollTop,
    "The scrolling position stayed the same when accepting the completion"
  );
});
