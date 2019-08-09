/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that a console.timeStamp() does not print anything in the console

"use strict";

const TEST_URI = "data:text/html,<meta charset=utf8>";

add_task(async function() {
  // We open the console and an empty tab, as we only want to evaluate something.
  const hud = await openNewTabAndConsole(TEST_URI);
  // We execute `console.timeStamp('test')` from the console input.
  execute(hud, "console.timeStamp('test')");
  info(`Checking size`);
  await waitFor(() => findMessages(hud, "").length == 2);
  const [first, second] = findMessages(hud, "").map(message =>
    message.textContent.trim()
  );
  info(`Checking first message`);
  is(first, "console.timeStamp('test')", "First message has expected text");
  info(`Checking second message`);
  is(second, "undefined", "Second message has expected text");
});
