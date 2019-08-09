/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that filters don't affect navigation markers.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,
  <p>Web Console test for navigation marker filtering.</p>
  <script>console.log("hello " + "world");</script>`;

add_task(async function() {
  // Enable persist log
  await pushPref("devtools.webconsole.persistlog", true);

  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(() => findMessage(hud, "hello world"));

  info("Reload the page");
  const onInitMessage = waitForMessage(hud, "hello world");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.location.reload();
  });
  await onInitMessage;

  // Wait for the navigation message to be displayed.
  await waitFor(() => findMessage(hud, "Navigated to"));

  info("disable all filters and set a text filter that doesn't match anything");
  await setFilterState(hud, {
    error: false,
    warn: false,
    log: false,
    info: false,
    text: "qwqwqwqwqwqw",
  });

  await waitFor(() => !findMessage(hud, "hello world"));
  ok(
    findMessage(hud, "Navigated to"),
    "The navigation marker is still visible"
  );
});
