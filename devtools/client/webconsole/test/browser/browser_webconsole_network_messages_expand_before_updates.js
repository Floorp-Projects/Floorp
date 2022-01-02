/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "https://example.com/browser/devtools/client/webconsole/test/browser/";
const TEST_URI = TEST_PATH + TEST_FILE;

requestLongerTimeout(4);

pushPref("devtools.webconsole.filter.net", false);
pushPref("devtools.webconsole.filter.netxhr", true);

// Update waitFor default interval (10ms) to avoid test timeouts.
// The test often times out on waitFor statements use a 50ms interval instead.
waitFor.overrideIntervalForTestFile = 50;

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
    id: "request",
    testEmpty: testEmptyRequest,
    testContent: testRequest,
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

  // Test proper UI update when request is opened.
  // For every tab (with HTTP details):
  // 1. Execute long-time request
  // 2. Expand the net log before the request finishes (set default tab)
  // 3. Check the default tab empty content
  // 4. Wait till the request finishes
  // 5. Check content of all tabs
  for (const tab of tabs) {
    info(`Test "${tab.id}" panel`);
    await openRequestBeforeUpdates(hud, tab);
  }
});

async function openRequestBeforeUpdates(hud, tab) {
  const toolbox = hud.toolbox;

  await clearOutput(hud);

  const xhrUrl = TEST_PATH + "sjs_slow-response-test-server.sjs";
  const onMessage = waitForMessage(hud, xhrUrl);

  // Fire an XHR POST request.
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.testXhrPostSlowResponse();
  });
  info(`Wait for ${xhrUrl} message`);
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

    if (tab.testEmpty) {
      info("Test that the tab is empty");
      tab.testEmpty(messageNode);
    }
  }

  info("Test content of the default tab");
  await tab.testContent(messageNode);

  info("Test all tabs in the network log");
  await testNetworkMessage(toolbox, messageNode);
}

// Panel testing helpers

async function testNetworkMessage(toolbox, messageNode) {
  await testStatusInfo(messageNode);
  await testHeaders(messageNode);
  await testCookies(messageNode);
  await testRequest(messageNode);
  await testResponse(messageNode);
  await testTimings(messageNode);
  await testStackTrace(messageNode);
  await testSecurity(messageNode);
  await waitForLazyRequests(toolbox);
}

// Status Info
async function testStatusInfo(messageNode) {
  const statusInfo = await waitFor(() =>
    messageNode.querySelector(".status-info")
  );
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
  await waitFor(
    () => messageNode.querySelector("#headers-panel .headers-overview"),
    "Wait for .header-overview to be rendered"
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
  await waitFor(
    () => messageNode.querySelector("#cookies-panel .treeValueCell"),
    "Wait for .treeValueCell to be rendered"
  );
}

// Request
function testEmptyRequest(messageNode) {
  const emptyNotice = messageNode.querySelector("#request-panel .empty-notice");
  ok(emptyNotice, "Request tab is empty");
}

async function testRequest(messageNode) {
  const requestTab = messageNode.querySelector("#request-tab");
  ok(requestTab, "Request tab is available");

  // Select Request tab and check the content. CodeMirror initialization
  // is delayed to prevent UI freeze, so wait for a little while.
  requestTab.click();
  const requestPanel = messageNode.querySelector("#request-panel");
  await waitForSourceEditor(requestPanel);
  const requestContent = requestPanel.querySelector(
    ".panel-container .CodeMirror"
  );
  ok(requestContent, "Request content is available");
  ok(
    requestContent.textContent.includes("Hello world!"),
    "Request POST body is correct"
  );
}

// Response
function testEmptyResponse(messageNode) {
  const panel = messageNode.querySelector("#response-panel .tab-panel");
  is(
    panel.textContent,
    "No response data available for this request",
    "Cookies tab is empty"
  );
}

async function testResponse(messageNode) {
  const responseTab = messageNode.querySelector("#response-tab");
  ok(responseTab, "Response tab is available");

  // Select Response tab and check the content. CodeMirror initialization
  // is delayed, so again wait for a little while.
  responseTab.click();
  const responsePanel = messageNode.querySelector("#response-panel");
  const responsePayloadHeader = await waitFor(() =>
    responsePanel.querySelector(".data-header")
  );
  // Expand the header if it wasn't yet.
  if (responsePayloadHeader.getAttribute("aria-expanded") === "false") {
    responsePayloadHeader.click();
  }
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
  is(panel.textContent, "No timings for this request", "Timings tab is empty");
}

async function testTimings(messageNode) {
  const timingsTab = messageNode.querySelector("#timings-tab");
  ok(timingsTab, "Timings tab is available");

  // Select Timings tab and check the content.
  timingsTab.click();
  const timingsContent = await waitFor(() =>
    messageNode.querySelector(
      "#timings-panel .timings-container .timings-label",
      "Wait for .timings-label to be rendered"
    )
  );
  ok(timingsContent, "Timings content is available");
  ok(timingsContent.textContent, "Timings text is available");
}

// Stack Trace
function testEmptyStackTrace(messageNode) {
  const panel = messageNode.querySelector("#stack-trace-panel .tab-panel");
  is(panel.textContent, "", "StackTrace tab is empty");
}

async function testStackTrace(messageNode) {
  const stackTraceTab = messageNode.querySelector("#stack-trace-tab");
  ok(stackTraceTab, "StackTrace tab is available");

  // Select Stack Trace tab and check the content.
  stackTraceTab.click();
  await waitFor(
    () => messageNode.querySelector("#stack-trace-panel .frame-link"),
    "Wait for .frame-link to be rendered"
  );
}

// Security
function testEmptySecurity(messageNode) {
  const panel = messageNode.querySelector("#security-panel .tab-panel");
  is(panel.textContent, "", "Security tab is empty");
}

async function testSecurity(messageNode) {
  const securityTab = await waitFor(() =>
    messageNode.querySelector("#security-tab")
  );
  ok(securityTab, "Security tab is available");

  // Select Security tab and check the content.
  securityTab.click();
  await waitFor(
    () => messageNode.querySelector("#security-panel .treeTable .treeRow"),
    "Wait for #security-panel .treeTable .treeRow to be rendered"
  );
}

async function waitForSourceEditor(panel) {
  return waitUntil(() => {
    return !!panel.querySelector(".CodeMirror");
  });
}

function expandXhrMessage(node) {
  info(
    "Click on XHR message and wait for the network detail panel to be displayed"
  );
  node.querySelector(".url").click();
  return waitFor(
    () => node.querySelector(".network-info"),
    "Wait for .network-info to be rendered"
  );
}
