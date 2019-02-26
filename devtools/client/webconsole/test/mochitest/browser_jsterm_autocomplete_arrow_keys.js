/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<head><script>
    /* Create a prototype-less object so popup does not contain native
     * Object prototype properties.
     */
    window.foo = Object.create(null, Object.getOwnPropertyDescriptors({
      aa: "a",
      bb: "b",
    }));
  </script></head><body>Autocomplete text navigation key usage test</body>`;

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
  const { autocompletePopup: popup } = jsterm;

  const checkInput = (expected, assertionInfo) =>
    checkJsTermValueAndCursor(jsterm, expected, assertionInfo);

  let onPopUpOpen = popup.once("popup-opened");
  jsterm.setInputValue("window.foo");
  EventUtils.sendString(".");
  await onPopUpOpen;

  info("Trigger autocomplete popup opening");
  // checkInput is asserting the cursor position with the "|" char.
  checkInput("window.foo.|");
  is(popup.isOpen, true, "popup is open");
  checkJsTermCompletionValue(jsterm, "           aa", "completeNode has expected value");

  info("Test that arrow left closes the popup and clears complete node");
  let onPopUpClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  await onPopUpClose;
  checkInput("window.foo|.");
  is(popup.isOpen, false, "popup is closed");
  checkJsTermCompletionValue(jsterm, "", "completeNode is empty");

  info("Trigger autocomplete popup opening again");
  onPopUpOpen = popup.once("popup-opened");
  jsterm.setInputValue("window.foo");
  EventUtils.sendString(".");
  await onPopUpOpen;

  checkInput("window.foo.|");
  is(popup.isOpen, true, "popup is open");
  checkJsTermCompletionValue(jsterm, "           aa", "completeNode has expected value");

  info("Test that arrow right selects selected autocomplete item");
  onPopUpClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_ArrowRight");
  await onPopUpClose;
  checkInput("window.foo.aa|");
  is(popup.isOpen, false, "popup is closed");
  checkJsTermCompletionValue(jsterm, "", "completeNode is empty");

  info("Test that Ctrl/Cmd + Left removes complete node");
  await setInputValueForAutocompletion(jsterm, "window.foo.a");
  const prefix = jsterm.getInputValue().replace(/[\S]/g, " ");
  checkJsTermCompletionValue(jsterm, prefix + "a", "completeNode has expected value");

  const isOSX = Services.appinfo.OS == "Darwin";
  EventUtils.synthesizeKey("KEY_ArrowLeft", {
    [isOSX ? "metaKey" : "ctrlKey"]: true,
  });
  checkJsTermCompletionValue(jsterm, "",
    "completeNode was cleared after Ctrl/Cmd + left");

  info("Test that Ctrl/Cmd + Right closes the popup if there's text after cursor");
  jsterm.setInputValue(".");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  onPopUpOpen = popup.once("popup-opened");
  EventUtils.sendString("win");
  await onPopUpOpen;
  ok(popup.isOpen, "popup is open");

  onPopUpClose = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_ArrowRight", {
    [isOSX ? "metaKey" : "ctrlKey"]: true,
  });
  await onPopUpClose;
  is(jsterm.getInputValue(), "win.", "input value wasn't modified");
}
