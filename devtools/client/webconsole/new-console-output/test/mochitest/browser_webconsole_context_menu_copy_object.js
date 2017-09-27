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

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);

  let [msgWithText, msgWithObj, msgNested] =
    yield waitFor(() => findMessages(hud, "foo"));
  ok(msgWithText && msgWithObj && msgNested, "Three messages should have appeared");

  let [groupMsgObj] = yield waitFor(() => findMessages(hud, "group", ".message-body"));
  let [collapsedGroupMsgObj] = yield waitFor(() =>
    findMessages(hud, "collapsed", ".message-body"));
  let [numberMsgObj] = yield waitFor(() => findMessages(hud, `532`, ".message-body"));
  let [trueMsgObj] = yield waitFor(() => findMessages(hud, `true`, ".message-body"));
  let [falseMsgObj] = yield waitFor(() => findMessages(hud, `false`, ".message-body"));
  let [undefinedMsgObj] = yield waitFor(() => findMessages(hud, `undefined`,
    ".message-body"));
  let [nullMsgObj] = yield waitFor(() => findMessages(hud, `null`, ".message-body"));
  ok(nullMsgObj, "One message with null value should have appeared");

  let text = msgWithText.querySelector(".objectBox-string");
  let objInMsgWithObj = msgWithObj.querySelector(".objectBox-object");
  let textInMsgWithObj = msgWithObj.querySelector(".objectBox-string");

  // The third message has an object nested in an array, the array is therefore the top
  // object, the object is the nested object.
  let topObjInMsg = msgNested.querySelector(".objectBox-array");
  let nestedObjInMsg = msgNested.querySelector(".objectBox-object");

  let consoleMessages = yield waitFor(() => findMessages(hud, "console.log(\"foo\");",
    ".message-location"));
  yield testCopyObjectMenuItemDisabled(hud, consoleMessages[0]);

  info(`Check "Copy object" is enabled for text only messages
    thus copying the text`);
  yield testCopyObject(hud, text, `foo`, false);

  info(`Check "Copy object" is enabled for text in complex messages
   thus copying the text`);
  yield testCopyObject(hud, textInMsgWithObj, `foo`, false);

  info("Check `Copy object` is enabled for objects in complex messages");
  yield testCopyObject(hud, objInMsgWithObj, `{"baz":1}`, true);

  info("Check `Copy object` is enabled for top object in nested messages");
  yield testCopyObject(hud, topObjInMsg, `["foo",{"baz":1},2]`, true);

  info("Check `Copy object` is enabled for nested object in nested messages");
  yield testCopyObject(hud, nestedObjInMsg, `{"baz":1}`, true);

  info("Check `Copy object` is disabled on `console.group('group')` messages");
  yield testCopyObjectMenuItemDisabled(hud, groupMsgObj);

  info(`Check "Copy object" is disabled in "console.groupCollapsed('collapsed')"
    messages`);
  yield testCopyObjectMenuItemDisabled(hud, collapsedGroupMsgObj);

  // Check for primitive objects
  info("Check `Copy object` is enabled for numbers");
  yield testCopyObject(hud, numberMsgObj, `532`, false);

  info("Check `Copy object` is enabled for booleans");
  yield testCopyObject(hud, trueMsgObj, `true`, false);
  yield testCopyObject(hud, falseMsgObj, `false`, false);

  info("Check `Copy object` is enabled for undefined and null");
  yield testCopyObject(hud, undefinedMsgObj, `undefined`, false);
  yield testCopyObject(hud, nullMsgObj, `null`, false);
});

function* testCopyObject(hud, element, expectedMessage, objectInput) {
  info("Check `Copy object` is enabled");
  let menuPopup = yield openContextMenu(hud, element);
  let copyObjectMenuItem = menuPopup.querySelector(copyObjectMenuItemId);
  ok(!copyObjectMenuItem.disabled,
    "`Copy object` is enabled for object in complex message");

  const validatorFn = data => {
    let prettifiedMessage = prettyPrintMessage(expectedMessage, objectInput);
    return data === prettifiedMessage;
  };

  info("Click on `Copy object`");
  yield waitForClipboardPromise(() => copyObjectMenuItem.click(), validatorFn);

  info("`Copy object` by using the access-key O");
  menuPopup = yield openContextMenu(hud, element);
  yield waitForClipboardPromise(() => synthesizeKeyShortcut("O"), validatorFn);
}

function* testCopyObjectMenuItemDisabled(hud, element) {
  let menuPopup = yield openContextMenu(hud, element);
  let copyObjectMenuItem = menuPopup.querySelector(copyObjectMenuItemId);
  ok(copyObjectMenuItem.disabled, `"Copy object" is disabled for messages
    with no variables/objects`);
  yield hideContextMenu(hud);
}

function prettyPrintMessage(message, isObject) {
  return isObject ? JSON.stringify(JSON.parse(message), null, 2) : message;
}
