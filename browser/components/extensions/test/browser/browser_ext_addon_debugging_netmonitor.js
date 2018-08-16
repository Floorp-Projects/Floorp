/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

ChromeUtils.defineModuleGetter(this, "BrowserToolboxProcess",
                               "resource://devtools/client/framework/ToolboxProcess.jsm");

async function setupToolboxProcessTest(toolboxProcessScript) {
  // Enable addon debugging.
  await SpecialPowers.pushPrefEnv({
    "set": [
      // Force enabling of addons debugging
      ["devtools.chrome.enabled", true],
      ["devtools.debugger.remote-enabled", true],
      // Disable security prompt
      ["devtools.debugger.prompt-connection", false],
      // Enable Browser toolbox test script execution via env variable
      ["devtools.browser-toolbox.allow-unsafe-script", true],
    ],
  });

  let env = Cc["@mozilla.org/process/environment;1"]
              .getService(Ci.nsIEnvironment);
  env.set("MOZ_TOOLBOX_TEST_SCRIPT", `(${toolboxProcessScript})();`);
  registerCleanupFunction(() => {
    env.set("MOZ_TOOLBOX_TEST_SCRIPT", "");
  });
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
      browser.test.log("Addon Debugging Netmonitor panel collected requests: " +
                       JSON.stringify(requests));
      browser.test.assertEq(1, requests.length, "Got one request logged");
      browser.test.assertEq("GET", requests[0].method, "Got a GET request");
      browser.test.assertEq(expectedURL, requests[0].url, "Got the expected request url");

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
        gecko: {id: EXTENSION_ID},
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  // Be careful, this JS function is going to be executed in the addon toolbox,
  // which lives in another process. So do not try to use any scope variable!
  const toolboxProcessScript = async function() {
    /* eslint-disable no-undef */
    async function waitFor(condition) {
      while (!condition()) {
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        await new Promise(done => window.setTimeout(done, 1000));
      }
    }

    const console = await toolbox.selectTool("webconsole");
    const {hud} = console;
    const {jsterm} = hud;

    const netmonitor = await toolbox.selectTool("netmonitor");

    const expectedURL = "http://mochi.test:8888/?test_netmonitor=1";

    // Call a function defined in the target extension to make it
    // fetch from an expected http url.
    await jsterm.execute(`doFetchHTTPRequest("${expectedURL}");`);

    await waitFor(() => {
      return !netmonitor.panelWin.document.querySelector(".request-list-empty-notice");
    });

    let {store} = netmonitor.panelWin;

    // NOTE: we need to filter the requests to the ones that we expect until
    // the network monitor is not yet filtering out the requests that are not
    // coming from an extension window or a descendent of an extension window,
    // in both oop and non-oop extension mode (filed as Bug 1442621).
    function filterRequest(request) {
      return request.url === expectedURL;
    }

    let requests;

    await waitFor(() => {
      requests = Array.from(store.getState().requests.requests.values())
                      .filter(filterRequest);

      return requests.length > 0;
    });

    // Call a function defined in the target extension to make assertions
    // on the network requests collected by the netmonitor panel.
    await jsterm.execute(`testNetworkRequestReceived(${JSON.stringify(requests)});`);

    await toolbox.destroy();
    /* eslint-enable no-undef */
  };

  await setupToolboxProcessTest(toolboxProcessScript);
  const browserToolboxProcess = new BrowserToolboxProcess({
    addonID: EXTENSION_ID,
  });

  let onToolboxClose = browserToolboxProcess.once("close");
  await extension.awaitFinish("netmonitor_request_logged");
  await onToolboxClose;

  info("Addon Toolbox closed");

  await extension.unload();
});
