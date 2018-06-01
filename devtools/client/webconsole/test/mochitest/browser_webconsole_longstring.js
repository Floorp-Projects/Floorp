/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that very long strings can be expanded and collapsed, and do not hang the browser.

"use strict";

const TEST_URI = "data:text/html,<meta charset=utf8>Test LongString hang";

const LONGSTRING =
  `foobar${"a".repeat(9000)}foobaz${"abbababazomglolztest".repeat(100)}boom!`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Log a longString");
  const onMessage = waitForMessage(hud, LONGSTRING.slice(0, 50));
  ContentTask.spawn(gBrowser.selectedBrowser, LONGSTRING, str => {
    content.console.log(str);
  });

  const {node} = await onMessage;
  const arrow = node.querySelector(".arrow");
  ok(arrow, "longString expand arrow is shown");

  info("wait for long string expansion");
  const onLongStringFullTextDisplayed = waitFor(() => findMessage(hud, LONGSTRING));
  arrow.click();
  await onLongStringFullTextDisplayed;

  ok(true, "The full text of the longString is displayed");

  info("wait for long string collapse");
  const onLongStringCollapsed = waitFor(() => !findMessage(hud, LONGSTRING));
  arrow.click();
  await onLongStringCollapsed;

  ok(true, "The longString can be collapsed");
});
