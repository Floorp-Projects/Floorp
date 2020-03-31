/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

const { DevToolsClient } = require("devtools/client/devtools-client");
const { DevToolsServer } = require("devtools/server/devtools-server");
const { gDevTools } = require("devtools/client/framework/devtools");
const { Toolbox } = require("devtools/client/framework/toolbox");

async function setupToolboxTest(extensionId) {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  await client.connect();
  const addonFront = await client.mainRoot.getAddon({ id: extensionId });
  const target = await addonFront.getTarget();
  const toolbox = await gDevTools.showToolbox(
    target,
    null,
    Toolbox.HostType.WINDOW
  );

  async function waitFor(condition) {
    while (!condition()) {
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(done => window.setTimeout(done, 1000));
    }
  }

  const consoleFront = await toolbox.target.getFront("console");

  const netmonitor = await toolbox.selectTool("netmonitor");

  const expectedURL = "http://mochi.test:8888/?test_netmonitor=1";

  // Call a function defined in the target extension to make it
  // fetch from an expected http url.
  await consoleFront.evaluateJSAsync(`doFetchHTTPRequest("${expectedURL}");`);

  await waitFor(() => {
    return !netmonitor.panelWin.document.querySelector(
      ".request-list-empty-notice"
    );
  });

  let { store } = netmonitor.panelWin;

  // NOTE: we need to filter the requests to the ones that we expect until
  // the network monitor is not yet filtering out the requests that are not
  // coming from an extension window or a descendent of an extension window,
  // in both oop and non-oop extension mode (filed as Bug 1442621).
  function filterRequest(request) {
    return request.url === expectedURL;
  }

  let requests;

  await waitFor(() => {
    requests = Array.from(store.getState().requests.requests.values()).filter(
      filterRequest
    );

    return !!requests.length;
  });

  // Call a function defined in the target extension to make assertions
  // on the network requests collected by the netmonitor panel.
  await consoleFront.evaluateJSAsync(
    `testNetworkRequestReceived(${JSON.stringify(requests)});`
  );

  const onToolboxClosed = gDevTools.once("toolbox-destroyed");
  await toolbox.destroy();
  return onToolboxClosed;
}

add_task(async function test_addon_debugging_netmonitor_panel() {
  const EXTENSION_ID = "test-monitor-panel@mozilla";

  function background() {
    let expectedURL;
    window.doFetchHTTPRequest = async function(urlToFetch) {
      expectedURL = urlToFetch;
      await fetch(urlToFetch);
    };
    window.testNetworkRequestReceived = async function(requests) {
      browser.test.log(
        "Addon Debugging Netmonitor panel collected requests: " +
          JSON.stringify(requests)
      );
      browser.test.assertEq(1, requests.length, "Got one request logged");
      browser.test.assertEq("GET", requests[0].method, "Got a GET request");
      browser.test.assertEq(
        expectedURL,
        requests[0].url,
        "Got the expected request url"
      );

      browser.test.notifyPass("netmonitor_request_logged");
    };
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    useAddonManager: "temporary",
    manifest: {
      permissions: ["http://mochi.test/"],
      applications: {
        gecko: { id: EXTENSION_ID },
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  const onToolboxClose = setupToolboxTest(EXTENSION_ID);
  await Promise.all([
    extension.awaitFinish("netmonitor_request_logged"),
    onToolboxClose,
  ]);

  info("Addon Toolbox closed");

  await extension.unload();
});
