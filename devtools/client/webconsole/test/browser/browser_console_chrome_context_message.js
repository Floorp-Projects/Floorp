/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that console API calls in the content page appear in the browser console.

"use strict";

add_task(async function() {
  // Needed for the execute() function below
  await pushPref("security.allow_parent_unrestricted_js_loads", true);

  // Show the content messages
  await pushPref("devtools.browserconsole.contentMessages", true);

  const hud = await BrowserConsoleManager.toggleBrowserConsole();
  await clearOutput(hud);
  await openNewTabAndConsole(
    `data:text/html,<script>console.log("hello from content")</script>`
  );

  const expectedMessages = [
    `Cu.reportError`, // bug 1561930
  ];

  execute(hud, `Cu.reportError("Cu.reportError");`); // bug 1561930
  info("Wait for expected message are shown on browser console");
  await waitFor(() =>
    expectedMessages.every(expectedMessage => findMessage(hud, expectedMessage))
  );
  await waitFor(() => findMessage(hud, "hello from content"));

  ok(true, "Expected messages are displayed in the browser console");

  info("Uncheck the Show content messages checkbox");
  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-contentMessages"
  );
  await waitFor(() => !findMessage(hud, "hello from content"));

  info("Check the expected messages are still visiable in the browser console");
  for (const expectedMessage of expectedMessages) {
    ok(
      findMessage(hud, expectedMessage),
      `"${expectedMessage}" should be still visible`
    );
  }

  // Wait until the sourcemap worker is fully initialized
  await waitForSourceMapWorker(hud);
});

function waitForSourceMapWorker(hud) {
  const { targetList } = hud;
  return new Promise(resolve => {
    const onAvailable = ({ targetFront }) => {
      if (
        targetFront.url.endsWith("devtools/client/shared/source-map/worker.js")
      ) {
        targetList.unwatchTargets([targetList.TYPES.WORKER], onAvailable);
        resolve();
      }
    };
    targetList.watchTargets([targetList.TYPES.WORKER], onAvailable);
  });
}
