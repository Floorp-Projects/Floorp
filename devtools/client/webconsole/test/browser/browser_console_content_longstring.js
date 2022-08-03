/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that very long content strings can be expanded and collapsed in the
// Browser Console, and do not hang the browser.

"use strict";

const TEST_URI =
  "data:text/html,<!DOCTYPE html><meta charset=utf8>Test LongString hang";

const LONGSTRING = `foobar${"a".repeat(
  9000
)}foobaz${"abbababazomglolztest".repeat(100)}boom!`;

add_task(async function() {
  // Show the content messages
  await pushPref("devtools.browserconsole.contentMessages", true);
  // Enable Fission browser console to see the logged content object
  await pushPref("devtools.browsertoolbox.fission", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");

  await addTab(TEST_URI);

  info("Open the Browser Console");
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Log a longString");
  const onMessage = waitForMessageByType(
    hud,
    LONGSTRING.slice(0, 50),
    ".console-api"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [LONGSTRING], str => {
    content.console.log(str);
  });

  const { node } = await onMessage;
  const arrow = node.querySelector(".arrow");
  ok(arrow, "longString expand arrow is shown");

  info("wait for long string expansion");
  const onLongStringFullTextDisplayed = waitFor(() =>
    findConsoleAPIMessage(hud, LONGSTRING)
  );
  arrow.click();
  await onLongStringFullTextDisplayed;

  ok(true, "The full text of the longString is displayed");

  info("wait for long string collapse");
  const onLongStringCollapsed = waitFor(
    () => !findConsoleAPIMessage(hud, LONGSTRING)
  );
  arrow.click();
  await onLongStringCollapsed;

  ok(true, "The longString can be collapsed");
});
