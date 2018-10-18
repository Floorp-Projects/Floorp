/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the console does not steal the focus when reloading a page, if the focus
// is on the content page.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8>Focus test`;

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  info("Testing that messages disappear on a refresh if logs aren't persisted");
  const {jsterm} = await openNewTabAndConsole(TEST_URI);
  is(isJstermFocused(jsterm), true, "JsTerm is focused when opening the console");

  info("Put the focus on the content page");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => content.focus());
  await waitFor(() => isJstermFocused(jsterm) === false);

  info("Reload the page to check that JsTerm does not steal the content page focus");
  await refreshTab();
  is(isJstermFocused(jsterm), false,
    "JsTerm is still unfocused after reloading the page");
}
