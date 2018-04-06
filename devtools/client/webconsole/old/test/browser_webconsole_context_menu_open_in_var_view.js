/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the "Open in Variables View" context menu item is enabled
// only for objects.

"use strict";

const TEST_URI = `data:text/html,<script>
  console.log("foo");
  console.log("foo", window);
</script>`;

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      count: 2,
      text: /foo/
    }],
  });

  let [msgWithText, msgWithObj] = [...result.matched];
  ok(msgWithText && msgWithObj, "Two messages should have appeared");

  let contextMenu = hud.iframeWindow.document
                                    .getElementById("output-contextmenu");
  let openInVarViewItem = contextMenu.querySelector("#menu_openInVarView");
  let obj = msgWithObj.querySelector(".cm-variable");
  let text = msgWithText.querySelector(".console-string");

  yield waitForContextMenu(contextMenu, obj, () => {
    ok(openInVarViewItem.disabled === false, "The \"Open In Variables View\" " +
      "context menu item should be available for objects");
  }, () => {
    ok(openInVarViewItem.disabled === true, "The \"Open In Variables View\" " +
      "context menu item should be disabled on popup hiding");
  });

  yield waitForContextMenu(contextMenu, text, () => {
    ok(openInVarViewItem.disabled === true, "The \"Open In Variables View\" " +
      "context menu item should be disabled for texts");
  });
});
