/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the "Store as global variable" menu item of the webconsole is enabled only when
// clicking on messages that are associated with an object actor.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<script>
  window.bar = { baz: 1 };
  console.log("foo");
  console.log("foo", window.bar);
  window.array = ["foo", window.bar, 2];
  console.log(window.array);
  window.longString = "foo" + "a".repeat(1e4);
  console.log(window.longString);
</script>`;

add_task(async function() {
  let hud = await openNewTabAndConsole(TEST_URI);

  let messages = await waitFor(() => findMessages(hud, "foo"));
  is(messages.length, 4, "Four messages should have appeared");
  let [msgWithText, msgWithObj, msgNested, msgLongStr] = messages;
  let varIdx = 0;

  info("Check store as global variable is disabled for text only messages");
  await storeAsVariable(hud, msgWithText, "string");

  info("Check store as global variable is disabled for text in complex messages");
  await storeAsVariable(hud, msgWithObj, "string");

  info("Check store as global variable is enabled for objects in complex messages");
  await storeAsVariable(hud, msgWithObj, "object", varIdx++, "window.bar");

  info("Check store as global variable is enabled for top object in nested messages");
  await storeAsVariable(hud, msgNested, "array", varIdx++, "window.array");

  info("Check store as global variable is enabled for nested object in nested messages");
  await storeAsVariable(hud, msgNested, "object", varIdx++, "window.bar");

  info("Check store as global variable is enabled for long strings");
  await storeAsVariable(hud, msgLongStr, "string", varIdx++, "window.longString");

  info("Check store as global variable is enabled for invisible-to-debugger objects");
  let onMessageInvisible = waitForMessage(hud, "foo");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    let obj = Cu.Sandbox(Cu.getObjectPrincipal(content), {invisibleToDebugger: true});
    content.wrappedJSObject.invisibleToDebugger = obj;
    content.console.log("foo", obj);
  });
  let msgInvisible = (await onMessageInvisible).node;
  await storeAsVariable(hud, msgInvisible, "object", varIdx++, "window.invisibleToDebugger");
});

async function storeAsVariable(hud, msg, type, varIdx, equalTo) {
  let element = msg.querySelector(".objectBox-" + type);
  let menuPopup = await openContextMenu(hud, element);
  let storeMenuItem = menuPopup.querySelector("#console-menu-store");

  if (varIdx == null) {
    ok(storeMenuItem.disabled, "store as global variable is disabled");
    await hideContextMenu(hud);
    return;
  }

  ok(!storeMenuItem.disabled, "store as global variable is enabled");

  info("Click on store as global variable");
  let onceInputSet = hud.jsterm.once("set-input-value");
  storeMenuItem.click();

  info("Wait for console input to be updated with the temp variable");
  await onceInputSet;

  info("Wait for context menu to be hidden");
  await hideContextMenu(hud);

  is(hud.jsterm.getInputValue(), "temp" + varIdx, "Input was set");

  let equal = await hud.jsterm.requestEvaluation("temp" + varIdx + " === " + equalTo);
  is(equal.result, true, "Correct variable assigned into console.");
}
