/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 597103. Check that closing the console on an unfocused window does not trigger
// any error.

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function() {
  const tab1 = await addTab(TEST_URI, { window });

  info("Open a second window");
  const win2 = await BrowserTestUtils.openNewBrowserWindow();

  info("Add a test tab in the second window");
  const tab2 = await addTab(TEST_URI, { window: win2 });
  win2.gBrowser.selectedTab = tab2;

  info("Open console in tabs located in different windows");
  await openConsole(tab1);
  await openConsole(tab2);

  info(
    "Close toolboxes in tabs located in different windows, one of them not focused"
  );
  await gDevTools.closeToolboxForTab(tab1);
  await gDevTools.closeToolboxForTab(tab2);

  info("Close the second window");
  win2.close();

  info("Close the test tab in the first window");
  window.gBrowser.removeTab(tab1);

  ok(true, "No error was triggered during the test");
});
