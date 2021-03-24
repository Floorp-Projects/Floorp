/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const TEST_URI = `data:text/html;charset=utf8,<script>console.group('hello')</script>`;

add_task(async function() {
  // Enable persist logs
  await pushPref("devtools.webconsole.persistlog", true);

  info(
    "Open the console and wait for the console.group message to be rendered"
  );
  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(() => findMessage(hud, "hello", ".startGroup"));

  info("Refresh tab several times and check for correct message indentation");
  for (let i = 0; i < 5; i++) {
    await refreshTabAndCheckIndent(hud);
  }
});

async function refreshTabAndCheckIndent(hud) {
  const onMessage = waitForMessage(hud, "hello", ".startGroup");
  await refreshTab();
  const { node } = await onMessage;

  is(
    node.querySelector(".indent").getAttribute("data-indent"),
    "0",
    "The message has the expected indent"
  );
}
