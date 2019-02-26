/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that navigating the page closes the autocomplete popup.

const TEST_URI = `data:text/html;charset=utf-8,
<head>
  <script>
    /* Create a prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    window.foo = Object.create(null, Object.getOwnPropertyDescriptors({
      item0: "value0",
      item1: "value1",
    }));
  </script>
</head>
<body>Test autocomplete close on content navigation</body>`;

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const { jsterm } = await openNewTabAndConsole(TEST_URI);
  info("web console opened");

  const { autocompletePopup: popup } = jsterm;

  const onPopUpOpen = popup.once("popup-opened");

  info("wait for completion: window.foo.");
  jsterm.setInputValue("window.foo");
  EventUtils.sendString(".");

  await onPopUpOpen;

  ok(popup.isOpen, "popup is open");
  ok(popup.itemCount, "popup has items");

  info("reload the page to close the popup");
  const onPopupClose = popup.once("popup-closed");
  await refreshTab();
  await onPopupClose;

  ok(!popup.isOpen, "popup is not open after reloading the page");
  is(jsterm.getInputValue(), "window.foo.", "completion was cancelled");
  ok(!getJsTermCompletionValue(jsterm), "completeNode is empty");
}
