/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const server = createTestHTTPServer();
const proxy = createTestHTTPServer();

proxy.identity.add("http", "localhost", server.identity.primaryPort);
proxy.registerPrefixHandler("/", (request, response) => {
  if (request.hasHeader("Proxy-Authorization")) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", true);
    response.write("ok, got proxy auth");
  } else {
    response.setStatusLine(
      request.httpVersion,
      407,
      "Proxy authentication required"
    );
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Proxy-Authenticate", 'Basic realm="foobar"', false);
    response.write("auth required");
  }
});

const SERVER_URL = `http://localhost:${server.identity.primaryPort}`;

// Test that `Proxy-Authorization` request header is not shown for in the headers panel
add_task(async function () {
  await pushPref("network.proxy.type", 1);
  await pushPref("network.proxy.http", "localhost");
  await pushPref("network.proxy.http_port", proxy.identity.primaryPort);
  await pushPref("network.proxy.allow_hijacking_localhost", true);

  // Wait for initial primary password dialog after opening the tab.
  const onDialog = TestUtils.topicObserved("common-dialog-loaded");

  const tab = await addTab(SERVER_URL, { waitForLoad: false });

  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "netmonitor",
  });
  info("Network monitor pane shown successfully.");

  const monitor = toolbox.getCurrentPanel();
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const [subject] = await onDialog;
  const dialog = subject.Dialog;

  ok(true, `Authentication dialog displayed`);

  info("Fill in login and password, and validate dialog");
  dialog.ui.loginTextbox.value = "user";
  dialog.ui.password1Textbox.value = "pass";

  const onDialogClosed = BrowserTestUtils.waitForEvent(
    window,
    "DOMModalDialogClosed"
  );
  dialog.ui.button0.click();
  await onDialogClosed;
  ok(true, "Dialog is closed");

  const requestEl = await waitFor(() =>
    document.querySelector(".request-list-item")
  );
  EventUtils.sendMouseEvent({ type: "mousedown" }, requestEl);

  await waitUntil(() => document.querySelector(".headers-overview"));

  const headersPanel = document.querySelector("#headers-panel");
  const headerIsFound = [
    ...headersPanel.querySelectorAll("tr .treeLabelCell .treeLabel"),
  ].some(headerEl => headerEl.innerText == "Proxy-Authorization");

  ok(
    !headerIsFound,
    "The `Proxy-Authorization` should not be displayed in the Headers panel"
  );
});
