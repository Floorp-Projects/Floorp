/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that pasting clipboard content into input with middle-click works.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,Web Console test paste on middle-click`;

add_task(async function() {
  await pushPref("devtools.selfxss.count", 5);

  // Enable pasting with middle-click.
  await pushPref("middlemouse.paste", true);

  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;

  info("Set clipboard content");
  const clipboardContent = "test clipboard content";
  setClipboardText(clipboardContent);

  info("Middle-click on the console input");
  const node = jsterm.node;

  EventUtils.synthesizeMouse(node, 30, 10, { button: 1 }, hud.iframeWindow);
  is(
    getInputValue(hud),
    clipboardContent,
    "clipboard content was pasted in the console input"
  );
});

function setClipboardText(text) {
  const helper = SpecialPowers.Cc[
    "@mozilla.org/widget/clipboardhelper;1"
  ].getService(SpecialPowers.Ci.nsIClipboardHelper);
  helper.copyString(text);
}
