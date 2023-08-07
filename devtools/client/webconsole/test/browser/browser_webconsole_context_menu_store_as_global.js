/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the "Store as global variable" menu item works even if the page
// has a global variable conflicts with extra bindings.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html><script>
  /* Verify a conflicting global doesn't break the feature. */
  var _self = "wrong value";

  window.bar = { baz: 1 };
  console.log("foo");
  console.log("foo", window.bar);
  window.array = ["foo", window.bar, 2];
  console.log(window.array);
  window.longString = "foo" + "a".repeat(1e4);
  console.log(window.longString);
  window.symbol = Symbol();
  console.log("foo", window.symbol);
</script>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const messages = await waitFor(() => findConsoleAPIMessages(hud, "foo"));
  is(messages.length, 5, "Five messages should have appeared");
  const [msgWithText, msgWithObj, msgNested, msgLongStr, msgSymbol] = messages;
  let varIdx = 0;

  info("Check store as global variable is disabled for text only messages");
  await storeAsVariable(hud, msgWithText, "string");

  info(
    "Check store as global variable is disabled for text in complex messages"
  );
  await storeAsVariable(hud, msgWithObj, "string");

  info(
    "Check store as global variable is enabled for objects in complex messages"
  );
  await storeAsVariable(hud, msgWithObj, "object", varIdx++, "window.bar");

  info(
    "Check store as global variable is enabled for top object in nested messages"
  );
  await storeAsVariable(hud, msgNested, "array", varIdx++, "window.array");

  info(
    "Check store as global variable is enabled for nested object in nested messages"
  );
  await storeAsVariable(hud, msgNested, "object", varIdx++, "window.bar");

  info("Check store as global variable is enabled for long strings");
  await storeAsVariable(
    hud,
    msgLongStr,
    "string",
    varIdx++,
    "window.longString"
  );

  info("Check store as global variable is enabled for symbols");
  await storeAsVariable(hud, msgSymbol, "symbol", varIdx++, "window.symbol");

  info(
    "Check store as global variable is enabled for invisible-to-debugger objects"
  );
  const onMessageInvisible = waitForMessageByType(hud, "foo", ".console-api");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const obj = Cu.Sandbox(Cu.getObjectPrincipal(content), {
      invisibleToDebugger: true,
    });
    content.wrappedJSObject.invisibleToDebugger = obj;
    content.console.log("foo", obj);
  });
  const msgInvisible = (await onMessageInvisible).node;
  await storeAsVariable(
    hud,
    msgInvisible,
    "object",
    varIdx++,
    "window.invisibleToDebugger"
  );
});

async function storeAsVariable(hud, msg, type, varIdx, equalTo) {
  // Refresh the reference to the message, as it may have been scrolled out of existence.
  msg = await findMessageVirtualizedById({
    hud,
    messageId: msg.getAttribute("data-message-id"),
  });
  const element = msg.querySelector(".objectBox-" + type);
  const menuPopup = await openContextMenu(hud, element);
  const storeMenuItem = menuPopup.querySelector("#console-menu-store");

  if (varIdx == null) {
    ok(storeMenuItem.disabled, "store as global variable is disabled");
    await hideContextMenu(hud);
    return;
  }

  ok(!storeMenuItem.disabled, "store as global variable is enabled");

  info("Click on store as global variable");
  const onceInputSet = hud.jsterm.once("set-input-value");
  menuPopup.activateItem(storeMenuItem);

  info("Wait for console input to be updated with the temp variable");
  await onceInputSet;

  info("Wait for context menu to be hidden");
  await hideContextMenu(hud);

  is(getInputValue(hud), "temp" + varIdx, "Input was set");

  await executeAndWaitForResultMessage(
    hud,
    `temp${varIdx} === ${equalTo}`,
    true
  );
  ok(true, "Correct variable assigned into console.");
}
