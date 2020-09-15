/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the requests that are blocked by extenstions show correctly.
 */
add_task(async function() {
  const extensionName = "Test Blocker";
  info(`Start loading the ${extensionName} extension...`);
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: extensionName,
      permissions: ["*://example.com/", "webRequest", "webRequestBlocking"],
    },
    useAddonManager: "temporary",
    background() {
      // eslint-disable-next-line no-undef
      browser.webRequest.onBeforeRequest.addListener(
        () => {
          return {
            cancel: true,
          };
        },
        {
          urls: [
            "http://example.com/browser/devtools/client/netmonitor/test/request_0",
          ],
        },
        ["blocking"]
      );
      // eslint-disable-next-line no-undef
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  const { tab, monitor } = await initNetMonitor(SINGLE_GET_URL, {
    requestCount: 2,
  });

  const { document } = monitor.panelWin;

  info("Starting test... ");

  const waitForNetworkEventsToComplete = waitForNetworkEvents(monitor, 2);
  const waitForRequestsToRender = waitForDOM(
    document,
    ".requests-list-row-group"
  );
  const waitForReload = BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  tab.linkedBrowser.reload();

  await Promise.all([
    waitForNetworkEventsToComplete,
    waitForRequestsToRender,
    waitForReload,
  ]);

  const requests = document.querySelectorAll(".request-list-item");

  info("Assert the blocked request");
  ok(
    !!requests[1].querySelector(
      ".requests-list-status .status-code-blocked-icon"
    ),
    "The request blocked status icon is visible"
  );
  is(
    requests[1].querySelector(
      ".requests-list-status .requests-list-status-code"
    ).title,
    "Blocked",
    "The request status title is 'Blocked'"
  );
  is(
    requests[1].querySelector(".requests-list-type").innerText,
    "",
    "The request shows no type"
  );
  is(
    requests[1].querySelector(".requests-list-transferred").innerText,
    `Blocked By ${extensionName}`,
    "The request shows the blocking extension name"
  );
  is(
    requests[1].querySelector(".requests-list-size").innerText,
    "",
    "The request shows no size"
  );

  await teardown(monitor);
  info(`Unloading the ${extensionName} extension`);
  await extension.unload();
});
