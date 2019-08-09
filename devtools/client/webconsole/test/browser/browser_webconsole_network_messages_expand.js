/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "https://example.com/browser/devtools/client/webconsole/" + "test/browser/";
const TEST_URI = TEST_PATH + TEST_FILE;

requestLongerTimeout(2);

pushPref("devtools.webconsole.filter.net", false);
pushPref("devtools.webconsole.filter.netxhr", true);

const tabs = [
  {
    id: "headers",
    testEmpty: testEmptyHeaders,
    testContent: testHeaders,
  },
  {
    id: "cookies",
    testEmpty: testEmptyCookies,
    testContent: testCookies,
  },
  {
    id: "params",
    testEmpty: testEmptyParams,
    testContent: testParams,
  },
  {
    id: "response",
    testEmpty: testEmptyResponse,
    testContent: testResponse,
  },
  {
    id: "timings",
    testEmpty: testEmptyTimings,
    testContent: testTimings,
  },
  {
    id: "stack-trace",
    testEmpty: testEmptyStackTrace,
    testContent: testStackTrace,
  },
  {
    id: "security",
    testEmpty: testEmptySecurity,
    testContent: testSecurity,
  },
];

/**
 * Main test for checking HTTP logs in the Console panel.
 */
add_task(async function task() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const currentTab = gBrowser.selectedTab;
  const target = await TargetFactory.forTab(currentTab);

  // Execute XHR and expand it after all network
  // update events are received. Consequently,
  // check out content of all (HTTP details) tabs.
  await openRequestAfterUpdates(target, hud);

  // Test proper UI update when request is opened.
  // For every tab (with HTTP details):
  // 1. Execute long-time request
  // 2. Expand the net log before the request finishes (set default tab)
  // 3. Check the default tab empty content
  // 4. Wait till the request finishes
  // 5. Check content of all tabs
  for (const tab of tabs) {
    await openRequestBeforeUpdates(target, hud, tab);
  }
});

async function openRequestAfterUpdates(target, hud) {
  const toolbox = gDevTools.getToolbox(target);

  const xhrUrl = TEST_PATH + "sjs_slow-response-test-server.sjs";
  const onMessage = waitForMessage(hud, xhrUrl);
  const onRequestUpdates = waitForRequestUpdates(hud);
  const onPayloadReady = waitForPayloadReady(hud);

  // Fire an XHR POST request.
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.testXhrPostSlowResponse();
  });

  const { node: messageNode } = await onMessage;
  ok(messageNode, "Network message found.");

  await onRequestUpdates;

  // Expand network log
  await expandXhrMessage(messageNode);

  const toggleButtonNode = messageNode.querySelector(".sidebar-toggle");
  ok(!toggleButtonNode, "Sidebar toggle button shouldn't be shown");

  await onPayloadReady;
  await testNetworkMessage(toolbox, messageNode);
}

async function openRequestBeforeUpdates(target, hud, tab) {
  const toolbox = gDevTools.getToolbox(target);

  hud.ui.clearOutput(true);

  const xhrUrl = TEST_PATH + "sjs_slow-response-test-server.sjs";
  const onMessage = waitForMessage(hud, xhrUrl);
  const onRequestUpdates = waitForRequestUpdates(hud);
  const onPayloadReady = waitForPayloadReady(hud);

  // Fire an XHR POST request.
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.testXhrPostSlowResponse();
  });
  const { node: messageNode } = await onMessage;
  ok(messageNode, "Network message found.");

  // Set the default panel.
  const state = hud.ui.wrapper.getStore().getState();
  state.ui.networkMessageActiveTabId = tab.id;

  // Expand network log
  await expandXhrMessage(messageNode);

  // Except the security tab. It isn't available till the
  // "securityInfo" packet type is received, so doesn't
  // fit this part of the test.
  if (tab.id != "security") {
    // Make sure the current tab is the expected one.
    const currentTab = messageNode.querySelector(`#${tab.id}-tab`);
    is(
      currentTab.getAttribute("aria-selected"),
      "true",
      "The correct tab is selected"
    );

    // The tab should be empty now.
    tab.testEmpty(messageNode);
  }

  // Wait till all updates and payload are received.
  await onRequestUpdates;
  await onPayloadReady;

  // Test content of the default tab.
  await tab.testContent(messageNode);

  // Test all tabs in the network log.
  await testNetworkMessage(toolbox, messageNode);
}

// Panel testing helpers

async function testNetworkMessage(toolbox, messageNode) {
  await testStatusInfo(messageNode);
  await testHeaders(messageNode);
  await testCookies(messageNode);
  await testParams(messageNode);
  await testResponse(messageNode);
  await testTimings(messageNode);
  await testStackTrace(messageNode);
  await testSecurity(messageNode);
  await waitForLazyRequests(toolbox);
}

// Status Info

function testStatusInfo(messageNode) {
  const statusInfo = messageNode.querySelector(".status-info");
  ok(statusInfo, "Status info is not empty");
}

// Headers

function testEmptyHeaders(messageNode) {
  const emptyNotice = messageNode.querySelector("#headers-panel .empty-notice");
  ok(emptyNotice, "Headers tab is empty");
}

async function testHeaders(messageNode) {
  const headersTab = messageNode.querySelector("#headers-tab");
  ok(headersTab, "Headers tab is available");

  // Select Headers tab and check the content.
  headersTab.click();
  await waitFor(() =>
    messageNode.querySelector("#headers-panel .headers-overview")
  );
}

// Cookies

function testEmptyCookies(messageNode) {
  const emptyNotice = messageNode.querySelector("#cookies-panel .empty-notice");
  ok(emptyNotice, "Cookies tab is empty");
}

async function testCookies(messageNode) {
  const cookiesTab = messageNode.querySelector("#cookies-tab");
  ok(cookiesTab, "Cookies tab is available");

  // Select tab and check the content.
  cookiesTab.click();
  await waitFor(() =>
    messageNode.querySelector("#cookies-panel .treeValueCell")
  );
}

// Params

function testEmptyParams(messageNode) {
  const emptyNotice = messageNode.querySelector("#params-panel .empty-notice");
  ok(emptyNotice, "Params tab is empty");
}

async function testParams(messageNode) {
  const paramsTab = messageNode.querySelector("#params-tab");
  ok(paramsTab, "Params tab is available");

  // Select Params tab and check the content. CodeMirror initialization
  // is delayed to prevent UI freeze, so wait for a little while.
  paramsTab.click();
  const paramsPanel = messageNode.querySelector("#params-panel");
  await waitForSourceEditor(paramsPanel);
  const paramsContent = messageNode.querySelector(
    "#params-panel .panel-container .CodeMirror"
  );
  ok(paramsContent, "Params content is available");
  ok(
    paramsContent.textContent.includes("Hello world!"),
    "Post body is correct"
  );
}

// Response

function testEmptyResponse(messageNode) {
  const panel = messageNode.querySelector("#response-panel .tab-panel");
  is(panel.textContent, "", "Cookies tab is empty");
}

async function testResponse(messageNode) {
  const responseTab = messageNode.querySelector("#response-tab");
  ok(responseTab, "Response tab is available");

  // Select Response tab and check the content. CodeMirror initialization
  // is delayed, so again wait for a little while.
  responseTab.click();
  const responsePanel = messageNode.querySelector("#response-panel");
  await waitForSourceEditor(responsePanel);
  const responseContent = messageNode.querySelector(
    "#response-panel .editor-row-container .CodeMirror"
  );
  ok(responseContent, "Response content is available");
  ok(responseContent.textContent, "Response text is available");
}

// Timings

function testEmptyTimings(messageNode) {
  const panel = messageNode.querySelector("#timings-panel .tab-panel");
  is(panel.textContent, "", "Timings tab is empty");
}

async function testTimings(messageNode) {
  const timingsTab = messageNode.querySelector("#timings-tab");
  ok(timingsTab, "Timings tab is available");

  // Select Timings tab and check the content.
  timingsTab.click();
  await waitFor(() =>
    messageNode.querySelector(
      "#timings-panel .timings-container .timings-label"
    )
  );
  const timingsContent = messageNode.querySelector(
    "#timings-panel .timings-container .timings-label"
  );
  ok(timingsContent, "Timings content is available");
  ok(timingsContent.textContent, "Timings text is available");
}

// Stack Trace

function testEmptyStackTrace(messageNode) {
  const panel = messageNode.querySelector("#stack-trace-panel .stack-trace");
  is(panel.textContent, "", "StackTrace tab is empty");
}

async function testStackTrace(messageNode) {
  const stackTraceTab = messageNode.querySelector("#stack-trace-tab");
  ok(stackTraceTab, "StackTrace tab is available");

  // Select Timings tab and check the content.
  stackTraceTab.click();
  await waitFor(() =>
    messageNode.querySelector("#stack-trace-panel .frame-link")
  );
}

// Security

function testEmptySecurity(messageNode) {
  const panel = messageNode.querySelector("#security-panel .tab-panel");
  is(panel.textContent, "", "Security tab is empty");
}

async function testSecurity(messageNode) {
  const securityTab = messageNode.querySelector("#security-tab");
  ok(securityTab, "Security tab is available");

  // Select Timings tab and check the content.
  securityTab.click();
  await waitFor(() =>
    messageNode.querySelector("#security-panel .treeTable .treeRow")
  );
}

// Waiting helpers

async function waitForPayloadReady(hud) {
  return hud.ui.once("network-request-payload-ready");
}

async function waitForSourceEditor(panel) {
  return waitUntil(() => {
    return !!panel.querySelector(".CodeMirror");
  });
}

async function waitForRequestUpdates(hud) {
  return hud.ui.once("network-message-updated");
}

function expandXhrMessage(node) {
  info(
    "Click on XHR message and wait for the network detail panel to be displayed"
  );
  node.querySelector(".url").click();
  return waitFor(() => node.querySelector(".network-info"));
}

/**
 * Wait until all lazily fetch requests in netmonitor get finished.
 * Otherwise test will be shutdown too early and cause failure.
 */
async function waitForLazyRequests(toolbox) {
  const { ui } = toolbox.getCurrentPanel().hud;
  const proxy = ui.proxy;
  return waitUntil(() => {
    return !proxy.networkDataProvider.lazyRequestData.size;
  });
}
