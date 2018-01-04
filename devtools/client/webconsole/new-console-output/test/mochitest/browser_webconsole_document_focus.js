/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that focus is restored to content page after closing the console. See Bug 588342.
const TEST_URI = "data:text/html;charset=utf-8,Test content focus after closing console";

add_task(async function () {
  let hud = await openNewTabAndConsole(TEST_URI);

  let inputNode = hud.jsterm.inputNode;
  info("Focus after console is opened");
  ok(hasFocus(inputNode), "input node is focused after console is opened");

  info("Closing console");
  await closeConsole();
  const isFocused = await ContentTask.spawn(gBrowser.selectedBrowser, { }, function () {
    const cmp = "@mozilla.org/focus-manager;1";
    const fm = Components.classes[cmp].getService(Components.interfaces.nsIFocusManager);
    return fm.focusedWindow == content;
  });
  ok(isFocused, "content document has focus after closing the console");
});
