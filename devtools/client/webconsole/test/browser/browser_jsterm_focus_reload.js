/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the console does not steal the focus when reloading a page, if the focus
// is on the content page.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8>Focus test`;

add_task(async function() {
  info("Testing that messages disappear on a refresh if logs aren't persisted");
  const hud = await openNewTabAndConsole(TEST_URI);
  is(isInputFocused(hud), true, "JsTerm is focused when opening the console");

  info("Put the focus on the content page");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => content.focus());
  await waitFor(() => isInputFocused(hud) === false);

  info(
    "Reload the page to check that JsTerm does not steal the content page focus"
  );
  await refreshTab();
  is(
    isInputFocused(hud),
    false,
    "JsTerm is still unfocused after reloading the page"
  );
});
