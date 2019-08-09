/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that focus is restored to content page after closing the console. See Bug 588342.
const TEST_URI =
  "data:text/html;charset=utf-8,Test content focus after closing console";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Focus after console is opened");
  ok(isInputFocused(hud), "input node is focused after console is opened");

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    this.onFocus = new Promise(resolve => {
      content.addEventListener("focus", resolve, { once: true });
    });
  });

  info("Closing console");
  await closeConsole();

  const isFocused = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    {},
    async function() {
      await this.onFocus;
      return Services.focus.focusedWindow == content;
    }
  );
  ok(isFocused, "content document has focus after closing the console");
});
