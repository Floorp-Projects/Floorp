/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>test for bug 642615 & 994134</p>";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);
const WebConsoleUtils = require("devtools/client/webconsole/utils").Utils;
const stringToCopy = "foobazbarBug642615";

add_task(async function() {
  await pushPref("devtools.selfxss.count", 0);

  const {jsterm} = await openNewTabAndConsole(TEST_URI);
  jsterm.clearOutput();
  ok(!jsterm.completeNode.value, "no completeNode.value");

  jsterm.setInputValue("doc");

  info("wait for completion value after typing 'docu'");
  let onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("u");
  await onAutocompleteUpdated;

  const completionValue = jsterm.completeNode.value;

  // Arguments: expected, setup.
  await waitForClipboardPromise(() =>
    clipboardHelper.copyString(stringToCopy), stringToCopy);

  await testSelfXss(jsterm);

  jsterm.setInputValue("docu");
  info("wait for completion update after clipboard paste");
  updateEditUIVisibility();
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  goDoCommand("cmd_paste");

  await onAutocompleteUpdated;

  ok(!jsterm.completeNode.value, "no completion value after paste");

  info("wait for completion update after undo");
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");

  goDoCommand("cmd_undo");

  await onAutocompleteUpdated;

  is(jsterm.completeNode.value, completionValue, "same completeNode.value after undo");

  info("wait for completion update after clipboard paste (ctrl-v)");
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");

  EventUtils.synthesizeKey("v", {accelKey: true});

  await onAutocompleteUpdated;
  ok(!jsterm.completeNode.value, "no completion value after paste (ctrl-v)");
});

// Self xss prevention tests (bug 994134)
async function testSelfXss(jsterm) {
  info("Self-xss paste tests");
  WebConsoleUtils.usageCount = 0;
  is(WebConsoleUtils.usageCount, 0, "Test for usage count getter");
  // Input some commands to check if usage counting is working
  for (let i = 0; i <= 3; i++) {
    jsterm.setInputValue(i);
    jsterm.execute();
  }
  is(WebConsoleUtils.usageCount, 4, "Usage count incremented");
  WebConsoleUtils.usageCount = 0;
  updateEditUIVisibility();

  const oldVal = jsterm.getInputValue();
  goDoCommand("cmd_paste");

  const notificationbox =
    jsterm.hud.document.getElementById("webconsole-notificationbox");
  const notification = notificationbox.querySelector(".notification");
  is(notification.getAttribute("data-key"), "selfxss-notification",
    "Self-xss notification shown");
  is(oldVal, jsterm.getInputValue(), "Paste blocked by self-xss prevention");

  // Allow pasting
  jsterm.setInputValue("allow pasting");
  const evt = document.createEvent("KeyboardEvent");
  evt.initKeyEvent("keyup", true, true, window,
                    0, 0, 0, 0,
                    0, " ".charCodeAt(0));
  jsterm.inputNode.dispatchEvent(evt);
  jsterm.setInputValue("");
  goDoCommand("cmd_paste");
  is(stringToCopy, jsterm.getInputValue(), "Paste works");
}
