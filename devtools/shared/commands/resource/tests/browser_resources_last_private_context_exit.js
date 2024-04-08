/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Verify that LAST_PRIVATE_CONTEXT_EXIT fires when closing the last opened private window

"use strict";

const NON_PRIVATE_TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>Not private";
const PRIVATE_TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html>Test in private windows`;

add_task(async function () {
  await pushPref("devtools.browsertoolbox.scope", "everything");
  const { commands } = await initMultiProcessResourceCommand();
  const { resourceCommand } = commands;

  const availableResources = [];
  await resourceCommand.watchResources(
    [resourceCommand.TYPES.LAST_PRIVATE_CONTEXT_EXIT],
    {
      onAvailable(resources) {
        availableResources.push(resources);
      },
    }
  );
  is(
    availableResources.length,
    0,
    "We do not get any LAST_PRIVATE_CONTEXT_EXIT after initialization"
  );

  await addTab(NON_PRIVATE_TEST_URI);

  info("Open a new private window and select the new tab opened in it");
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  ok(PrivateBrowsingUtils.isWindowPrivate(privateWindow), "window is private");
  const privateBrowser = privateWindow.gBrowser;
  privateBrowser.selectedTab = BrowserTestUtils.addTab(
    privateBrowser,
    PRIVATE_TEST_URI
  );

  info("private tab opened");
  ok(
    PrivateBrowsingUtils.isBrowserPrivate(privateBrowser.selectedBrowser),
    "tab window is private"
  );

  info("Open a second tab in the private window");
  await addTab(PRIVATE_TEST_URI, { window: privateWindow });

  // Let a chance to an unexpected async event to be fired
  await wait(1000);

  is(
    availableResources.length,
    0,
    "We do not get any LAST_PRIVATE_CONTEXT_EXIT when opening a private window"
  );

  info("Open a second private browsing window");
  const secondPrivateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  info("Close the second private window");
  secondPrivateWindow.BrowserCommands.tryToCloseWindow();

  // Let a chance to an unexpected async event to be fired
  await wait(1000);

  is(
    availableResources.length,
    0,
    "We do not get any LAST_PRIVATE_CONTEXT_EXIT when closing the second private window only"
  );

  info(
    "close the private window and check if LAST_PRIVATE_CONTEXT_EXIT resource is sent"
  );
  privateWindow.BrowserCommands.tryToCloseWindow();

  info("Wait for LAST_PRIVATE_CONTEXT_EXIT");
  await waitFor(() => availableResources.length == 1);
  is(
    availableResources.length,
    1,
    "We get one LAST_PRIVATE_CONTEXT_EXIT when closing the last opened private window"
  );

  await commands.destroy();
});
