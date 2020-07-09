/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for redirection.

const REDIRECT_PAGE = `${URL_ROOT}sjs_redirection.sjs`;
const CUSTOM_USER_AGENT = "Mozilla/5.0 (Test Device) Firefox/74.0";

addRDMTask(
  null,
  async () => {
    reloadOnUAChange(true);

    registerCleanupFunction(() => {
      reloadOnUAChange(false);
    });

    const tab = await addTab("http://example.com/");
    const browser = tab.linkedBrowser;

    const { ui } = await openRDM(tab);
    await waitForDeviceAndViewportState(ui);

    info("Change the user agent");
    await changeUserAgentInput(ui, CUSTOM_USER_AGENT);
    await testUserAgent(ui, CUSTOM_USER_AGENT);

    info("Open network monitor");
    const monitor = await openNetworkMonitor(tab);
    const {
      connector: monitorConnector,
      store: monitorStore,
    } = monitor.panelWin;

    info("Load a page which redirects");
    load(browser, REDIRECT_PAGE);

    info("Wait until getting all requests");
    await waitUntil(
      () => monitorStore.getState().requests.requests.length === 2
    );

    info("Check the user agent for each requests");
    for (const { id, url } of monitorStore.getState().requests.requests) {
      const userAgent = await getUserAgentRequestHeader(
        monitorStore,
        monitorConnector,
        id
      );
      is(userAgent, CUSTOM_USER_AGENT, `Sent user agent is correct for ${url}`);
    }

    await closeRDM(tab);
    await removeTab(tab);
  },
  { onlyPrefAndTask: true }
);

async function openNetworkMonitor(tab) {
  const target = await TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, "netmonitor");
  const monitor = toolbox.getCurrentPanel();
  return monitor;
}

async function getUserAgentRequestHeader(store, connector, id) {
  connector.requestData(id, "requestHeaders");

  let headers = null;
  await waitUntil(() => {
    const request = store.getState().requests.requests.find(r => r.id === id);
    if (request.requestHeaders) {
      headers = request.requestHeaders.headers;
    }
    return headers;
  });

  const userAgentHeader = headers.find(header => header.name === "User-Agent");
  return userAgentHeader.value;
}
