"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-block-action.html";
const TIMEOUT = "TIMEOUT";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  ok(hud, "web console opened");

  const filter = "test-block-action-style.css";
  const blockCommand = `:block ${filter}`;
  const unblockCommand = `:unblock ${filter}`;

  info("Before blocking");
  await tryFetching();
  const resp1 = await waitFor(() => findConsoleAPIMessage(hud, "successful"));
  ok(resp1, "the request was not blocked");
  info(`Execute the :block command and try to do execute a network request`);
  await executeAndWaitForMessageByType(
    hud,
    blockCommand,
    "are now blocked",
    ".console-api"
  );
  await tryFetching();

  const resp2 = await waitFor(() => findConsoleAPIMessage(hud, "blocked"));
  ok(resp2, "the request was blocked as expected");

  info("Open netmonitor check the blocked filter is registered in its state");
  const { panelWin } = await openNetMonitor();
  const nmStore = panelWin.store;
  nmStore.dispatch(panelWin.actions.toggleRequestBlockingPanel());
  //await waitForTime(1e7);
  // wait until the blockedUrls property is populated
  await waitFor(() => !!nmStore.getState().requestBlocking.blockedUrls.length);
  const netMonitorState1 = nmStore.getState();
  is(
    netMonitorState1.requestBlocking.blockedUrls[0].url,
    filter,
    "blocked request shows up in netmonitor state"
  );

  info("Switch back to the console");
  await hud.toolbox.selectTool("webconsole");

  // :unblock
  await executeAndWaitForMessageByType(
    hud,
    unblockCommand,
    "Removed blocking",
    ".console-api"
  );
  info("After unblocking");

  const netMonitorState2 = nmStore.getState();
  is(
    netMonitorState2.requestBlocking.blockedUrls.length,
    0,
    "unblocked request should not be in netmonitor state"
  );

  await tryFetching();

  const resp3 = await waitFor(() => findConsoleAPIMessage(hud, "successful"));
  ok(resp3, "the request was not blocked");
});

async function tryFetching() {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [TIMEOUT], async function(
    timeoutStr
  ) {
    const win = content.wrappedJSObject;
    const FETCH_URI =
      "https://example.com/browser/devtools/client/webconsole/" +
      "test/browser/test-block-action-style.css";
    const timeout = new Promise(res =>
      win.setTimeout(() => res(timeoutStr), 1000)
    );
    const fetchPromise = win.fetch(FETCH_URI);

    try {
      const resp = await Promise.race([fetchPromise, timeout]);
      if (typeof resp === "object") {
        // Request Promise
        win.console.log("the request was successful");
      } else if (resp === timeoutStr) {
        // Timeout
        win.console.log("the request was blocked");
      } else {
        win.console.error("Unkown response");
      }
    } catch {
      // NetworkError
      win.console.log("the request was blocked");
    }
  });
}
