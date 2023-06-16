/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the requests that are blocked by extenstions show correctly.
 */
add_task(async function () {
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
            "https://example.com/browser/devtools/client/netmonitor/test/request_0",
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

  const { tab, monitor } = await initNetMonitor(HTTPS_SINGLE_GET_URL, {
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

  await reloadBrowser();

  await Promise.all([
    waitForNetworkEventsToComplete,
    waitForRequestsToRender,
    waitForReload,
  ]);

  // Find the request list item that matches the blocked url
  const request = document.querySelector(
    "td.requests-list-url[title*=request_0]"
  ).parentNode;

  info("Assert the blocked request");
  ok(
    !!request.querySelector(".requests-list-status .status-code-blocked-icon"),
    "The request blocked status icon is visible"
  );

  is(
    request.querySelector(".requests-list-status .requests-list-status-code")
      .title,
    "Blocked",
    "The request status title is 'Blocked'"
  );

  is(
    request.querySelector(".requests-list-type").innerText,
    "",
    "The request shows no type"
  );

  is(
    request.querySelector(".requests-list-transferred").innerText,
    `Blocked By ${extensionName}`,
    "The request shows the blocking extension name"
  );

  is(
    request.querySelector(".requests-list-size").innerText,
    "",
    "The request shows no size"
  );

  await teardown(monitor);
  info(`Unloading the ${extensionName} extension`);
  await extension.unload();
});
