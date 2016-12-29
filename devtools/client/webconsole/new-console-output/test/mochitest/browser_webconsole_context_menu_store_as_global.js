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
  console.log(["foo", window.bar, 2]);
</script>`;

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);

  let [msgWithText, msgWithObj, msgNested] =
    yield waitFor(() => findMessages(hud, "foo"));
  ok(msgWithText && msgWithObj && msgNested, "Three messages should have appeared");

  let text = msgWithText.querySelector(".objectBox-string");
  let objInMsgWithObj = msgWithObj.querySelector(".cm-variable");
  let textInMsgWithObj = msgWithObj.querySelector(".objectBox-string");

  // The third message has an object nested in an array, the array is therefore the top
  // object, the object is the nested object.
  let topObjInMsg = msgNested.querySelector(".objectBox-array > .cm-variable");
  let nestedObjInMsg = msgNested.querySelector(".objectBox-object > .cm-variable");

  info("Check store as global variable is disabled for text only messages");
  let menuPopup = yield openContextMenu(hud, text);
  let storeMenuItem = menuPopup.querySelector("#console-menu-store");
  ok(storeMenuItem.disabled, "store as global variable is disabled for text message");
  yield hideContextMenu(hud);

  info("Check store as global variable is disabled for text in complex messages");
  menuPopup = yield openContextMenu(hud, textInMsgWithObj);
  storeMenuItem = menuPopup.querySelector("#console-menu-store");
  ok(storeMenuItem.disabled,
    "store as global variable is disabled for text in complex message");
  yield hideContextMenu(hud);

  info("Check store as global variable is enabled for objects in complex messages");
  yield storeAsVariable(hud, objInMsgWithObj);

  is(hud.jsterm.getInputValue(), "temp0", "Input was set");

  let executedResult = yield hud.jsterm.execute();
  ok(executedResult.textContent.includes("{ baz: 1 }"),
     "Correct variable assigned into console");

  info("Check store as global variable is enabled for top object in nested messages");
  yield storeAsVariable(hud, topObjInMsg);

  is(hud.jsterm.getInputValue(), "temp1", "Input was set");

  executedResult = yield hud.jsterm.execute();
  ok(executedResult.textContent.includes("[ \"foo\", Object, 2 ]"),
     "Correct variable assigned into console " + executedResult.textContent);

  info("Check store as global variable is enabled for nested object in nested messages");
  yield storeAsVariable(hud, nestedObjInMsg);

  is(hud.jsterm.getInputValue(), "temp2", "Input was set");

  executedResult = yield hud.jsterm.execute();
  ok(executedResult.textContent.includes("{ baz: 1 }"),
     "Correct variable assigned into console " + executedResult.textContent);
});

function* storeAsVariable(hud, element) {
  info("Check store as global variable is enabled");
  let menuPopup = yield openContextMenu(hud, element);
  let storeMenuItem = menuPopup.querySelector("#console-menu-store");
  ok(!storeMenuItem.disabled,
    "store as global variable is enabled for object in complex message");

  info("Click on store as global variable");
  let onceInputSet = hud.jsterm.once("set-input-value");
  storeMenuItem.click();

  info("Wait for console input to be updated with the temp variable");
  yield onceInputSet;

  info("Wait for context menu to be hidden");
  yield hideContextMenu(hud);
}
