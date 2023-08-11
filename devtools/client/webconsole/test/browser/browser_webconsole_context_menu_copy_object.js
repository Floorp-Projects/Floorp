/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the "Copy object" menu item of the webconsole is enabled only when
// clicking on messages that are associated with an object actor.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html><script>
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
  /* Verify that the conflicting binding on user code doesn't break the
   * functionality. */
  function copy() { alert("user-defined function is called"); }
</script>`;
const copyObjectMenuItemId = "#console-menu-copy-object";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const [msgWithText, msgWithObj, msgNested] = await waitFor(() =>
    findConsoleAPIMessages(hud, "foo")
  );
  ok(
    msgWithText && msgWithObj && msgNested,
    "Three messages should have appeared"
  );

  const [groupMsgObj] = await waitFor(() =>
    findMessagePartsByType(hud, {
      text: "group",
      typeSelector: ".console-api",
      partSelector: ".message-body",
    })
  );
  const [collapsedGroupMsgObj] = await waitFor(() =>
    findMessagePartsByType(hud, {
      text: "collapsed",
      typeSelector: ".console-api",
      partSelector: ".message-body",
    })
  );
  const [numberMsgObj] = await waitFor(() =>
    findMessagePartsByType(hud, {
      text: `532`,
      typeSelector: ".console-api",
      partSelector: ".message-body",
    })
  );
  const [trueMsgObj] = await waitFor(() =>
    findMessagePartsByType(hud, {
      text: `true`,
      typeSelector: ".console-api",
      partSelector: ".message-body",
    })
  );
  const [falseMsgObj] = await waitFor(() =>
    findMessagePartsByType(hud, {
      text: `false`,
      typeSelector: ".console-api",
      partSelector: ".message-body",
    })
  );
  const [undefinedMsgObj] = await waitFor(() =>
    findMessagePartsByType(hud, {
      text: `undefined`,
      typeSelector: ".console-api",
      partSelector: ".message-body",
    })
  );
  const [nullMsgObj] = await waitFor(() =>
    findMessagePartsByType(hud, {
      text: `null`,
      typeSelector: ".console-api",
      partSelector: ".message-body",
    })
  );
  ok(nullMsgObj, "One message with null value should have appeared");

  const text = msgWithText.querySelector(".objectBox-string");
  const objInMsgWithObj = msgWithObj.querySelector(".objectBox-object");
  const textInMsgWithObj = msgWithObj.querySelector(".objectBox-string");

  // The third message has an object nested in an array, the array is therefore the top
  // object, the object is the nested object.
  const topObjInMsg = msgNested.querySelector(".objectBox-array");
  const nestedObjInMsg = msgNested.querySelector(".objectBox-object");

  const consoleMessages = await waitFor(() =>
    findMessagePartsByType(hud, {
      text: 'console.log("foo");',
      typeSelector: ".console-api",
      partSelector: ".message-location",
    })
  );
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
  const menuPopup = await openContextMenu(hud, element);
  const copyObjectMenuItem = menuPopup.querySelector(copyObjectMenuItemId);
  ok(
    !copyObjectMenuItem.disabled,
    "`Copy object` is enabled for object in complex message"
  );
  is(
    copyObjectMenuItem.getAttribute("accesskey"),
    "o",
    "`Copy object` has the right accesskey"
  );

  const validatorFn = data => {
    const prettifiedMessage = prettyPrintMessage(expectedMessage, objectInput);
    return data === prettifiedMessage;
  };

  info("Activate item `Copy object`");
  await waitForClipboardPromise(
    () => menuPopup.activateItem(copyObjectMenuItem),
    validatorFn
  );
}

async function testCopyObjectMenuItemDisabled(hud, element) {
  const menuPopup = await openContextMenu(hud, element);
  const copyObjectMenuItem = menuPopup.querySelector(copyObjectMenuItemId);
  ok(
    copyObjectMenuItem.disabled,
    `"Copy object" is disabled for messages
    with no variables/objects`
  );
  await hideContextMenu(hud);
}

function prettyPrintMessage(message, isObject) {
  return isObject ? JSON.stringify(JSON.parse(message), null, 2) : message;
}
