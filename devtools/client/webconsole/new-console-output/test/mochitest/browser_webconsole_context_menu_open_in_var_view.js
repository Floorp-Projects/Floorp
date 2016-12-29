/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the Open in Variables View menu item of the webconsole is enabled only when
// clicking on messages that can be opened in the variables view.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<script>
  console.log("foo");
  console.log("foo", window);
</script>`;

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);

  let [msgWithText, msgWithObj] = yield waitFor(() => findMessages(hud, "foo"));
  ok(msgWithText && msgWithObj, "Two messages should have appeared");

  let text = msgWithText.querySelector(".objectBox-string");
  let objInMsgWithObj = msgWithObj.querySelector(".cm-variable");
  let textInMsgWithObj = msgWithObj.querySelector(".objectBox-string");

  info("Check open in variables view is disabled for text only messages");
  let menuPopup = yield openContextMenu(hud, text);
  let openMenuItem = menuPopup.querySelector("#console-menu-open");
  ok(openMenuItem.disabled, "open in variables view is disabled for text message");
  yield hideContextMenu(hud);

  info("Check open in variables view is enabled for objects in complex messages");
  menuPopup = yield openContextMenu(hud, objInMsgWithObj);
  openMenuItem = menuPopup.querySelector("#console-menu-open");
  ok(!openMenuItem.disabled,
    "open in variables view is enabled for object in complex message");
  yield hideContextMenu(hud);

  info("Check open in variables view is disabled for text in complex messages");
  menuPopup = yield openContextMenu(hud, textInMsgWithObj);
  openMenuItem = menuPopup.querySelector("#console-menu-open");
  ok(openMenuItem.disabled,
    "open in variables view is disabled for text in complex message");
  yield hideContextMenu(hud);
});
