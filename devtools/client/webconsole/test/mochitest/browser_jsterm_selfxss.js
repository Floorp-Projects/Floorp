/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>Test self-XSS protection</p>";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);
const WebConsoleUtils = require("devtools/client/webconsole/utils").Utils;
const stringToCopy = "EvilCommand";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTest();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTest();
});

async function performTest() {
  await pushPref("devtools.selfxss.count", 0);
  const {jsterm} = await openNewTabAndConsole(TEST_URI);
  const {document} = jsterm.hud;

  info("Self-xss paste tests");
  WebConsoleUtils.usageCount = 0;
  is(WebConsoleUtils.usageCount, 0, "Test for usage count getter");

  // Input some commands to check if usage counting is working
  for (let i = 0; i <= 3; i++) {
    jsterm.setInputValue(i.toString());
    jsterm.execute();
  }
  is(WebConsoleUtils.usageCount, 4, "Usage count incremented");
  WebConsoleUtils.usageCount = 0;

  info(`Copy "${stringToCopy}" in clipboard`);
  await waitForClipboardPromise(() =>
    clipboardHelper.copyString(stringToCopy), stringToCopy);
  goDoCommand("cmd_paste");

  const notificationbox = document.getElementById("webconsole-notificationbox");
  const notification = notificationbox.querySelector(".notification");
  is(notification.getAttribute("data-key"), "selfxss-notification",
    "Self-xss notification shown");
  is(jsterm.getInputValue(), "", "Paste blocked by self-xss prevention");

  // Allow pasting
  const allowToken = "allow pasting";
  for (const char of allowToken) {
    EventUtils.sendString(char);
  }

  jsterm.setInputValue("");
  goDoCommand("cmd_paste");
  is(jsterm.getInputValue(), stringToCopy, "Paste works");
}
