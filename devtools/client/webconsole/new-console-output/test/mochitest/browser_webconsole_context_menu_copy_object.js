/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the "Copy object" menu item of the webconsole is enabled only when
// clicking on messages that are associated with an object actor.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<script>
  window.bar = { baz: 1 };
  console.log("foo");
  console.log("foo", window.bar);
  console.log(["foo", window.bar, 2]);
  console.group("group");
  console.groupCollapsed("collapsed");
  console.groupEnd();
  console.log(532);
  console.log(true);
  console.log(false);
  console.log(undefined);
  console.log(null);
</script>`;
const copyObjectMenuItemId = "#console-menu-copy-object";

add_task(async function() {
  let hud = await openNewTabAndConsole(TEST_URI);

  let [msgWithText, msgWithObj, msgNested] =
    await waitFor(() => findMessages(hud, "foo"));
  ok(msgWithText && msgWithObj && msgNested, "Three messages should have appeared");

  let [groupMsgObj] = await waitFor(() => findMessages(hud, "group", ".message-body"));
  let [collapsedGroupMsgObj] = await waitFor(() =>
    findMessages(hud, "collapsed", ".message-body"));
  let [numberMsgObj] = await waitFor(() => findMessages(hud, `532`, ".message-body"));
  let [trueMsgObj] = await waitFor(() => findMessages(hud, `true`, ".message-body"));
  let [falseMsgObj] = await waitFor(() => findMessages(hud, `false`, ".message-body"));
  let [undefinedMsgObj] = await waitFor(() => findMessages(hud, `undefined`,
    ".message-body"));
  let [nullMsgObj] = await waitFor(() => findMessages(hud, `null`, ".message-body"));
  ok(nullMsgObj, "One message with null value should have appeared");

  let text = msgWithText.querySelector(".objectBox-string");
  let objInMsgWithObj = msgWithObj.querySelector(".objectBox-object");
  let textInMsgWithObj = msgWithObj.querySelector(".objectBox-string");

  // The third message has an object nested in an array, the array is therefore the top
  // object, the object is the nested object.
  let topObjInMsg = msgNested.querySelector(".objectBox-array");
  let nestedObjInMsg = msgNested.querySelector(".objectBox-object");

  let consoleMessages = await waitFor(() => findMessages(hud, "console.log(\"foo\");",
    ".message-location"));
  await testCopyObjectMenuItemDisabled(hud, consoleMessages[0]);

  info(`Check "Copy object" is enabled for text only messages
    thus copying the text`);
  await testCopyObject(hud, text, `foo`, false);

  info(`Check "Copy object" is enabled for text in complex messages
   thus copying the text`);
  await testCopyObject(hud, textInMsgWithObj, `foo`, false);

  info("Check `Copy object` is enabled for objects in complex messages");
  await testCopyObject(hud, objInMsgWithObj, `{"baz":1}`, true);

  info("Check `Copy object` is enabled for top object in nested messages");
  await testCopyObject(hud, topObjInMsg, `["foo",{"baz":1},2]`, true);

  info("Check `Copy object` is enabled for nested object in nested messages");
  await testCopyObject(hud, nestedObjInMsg, `{"baz":1}`, true);

  info("Check `Copy object` is disabled on `console.group('group')` messages");
  await testCopyObjectMenuItemDisabled(hud, groupMsgObj);

  info(`Check "Copy object" is disabled in "console.groupCollapsed('collapsed')"
    messages`);
  await testCopyObjectMenuItemDisabled(hud, collapsedGroupMsgObj);

  // Check for primitive objects
  info("Check `Copy object` is enabled for numbers");
  await testCopyObject(hud, numberMsgObj, `532`, false);

  info("Check `Copy object` is enabled for booleans");
  await testCopyObject(hud, trueMsgObj, `true`, false);
  await testCopyObject(hud, falseMsgObj, `false`, false);

  info("Check `Copy object` is enabled for undefined and null");
  await testCopyObject(hud, undefinedMsgObj, `undefined`, false);
  await testCopyObject(hud, nullMsgObj, `null`, false);
});

async function testCopyObject(hud, element, expectedMessage, objectInput) {
  info("Check `Copy object` is enabled");
  let menuPopup = await openContextMenu(hud, element);
  let copyObjectMenuItem = menuPopup.querySelector(copyObjectMenuItemId);
  ok(!copyObjectMenuItem.disabled,
    "`Copy object` is enabled for object in complex message");

  const validatorFn = data => {
    let prettifiedMessage = prettyPrintMessage(expectedMessage, objectInput);
    return data === prettifiedMessage;
  };

  info("Click on `Copy object`");
  await waitForClipboardPromise(() => copyObjectMenuItem.click(), validatorFn);

  info("`Copy object` by using the access-key O");
  menuPopup = await openContextMenu(hud, element);
  await waitForClipboardPromise(() => synthesizeKeyShortcut("O"), validatorFn);
}

async function testCopyObjectMenuItemDisabled(hud, element) {
  let menuPopup = await openContextMenu(hud, element);
  let copyObjectMenuItem = menuPopup.querySelector(copyObjectMenuItemId);
  ok(copyObjectMenuItem.disabled, `"Copy object" is disabled for messages
    with no variables/objects`);
  await hideContextMenu(hud);
}

function prettyPrintMessage(message, isObject) {
  return isObject ? JSON.stringify(JSON.parse(message), null, 2) : message;
}
