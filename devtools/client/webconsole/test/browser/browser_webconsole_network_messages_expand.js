/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "https://example.com/browser/devtools/client/webconsole/test/browser/";
const TEST_URI = TEST_PATH + TEST_FILE;
const XHR_URL = TEST_PATH + "sjs_slow-response-test-server.sjs";

requestLongerTimeout(2);

registerCleanupFunction(async function () {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

pushPref("devtools.webconsole.filter.net", false);
pushPref("devtools.webconsole.filter.netxhr", true);

/**
 * Main test for checking HTTP logs in the Console panel.
 */
add_task(async function task() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const messageNode = await doXhrAndExpand(hud);

  await testNetworkMessage(hud.toolbox, messageNode);

  await closeToolbox();
});

add_task(async function task() {
  info(
    "Verify that devtools.netmonitor.saveRequestAndResponseBodies=false disable response content collection"
  );
  await pushPref("devtools.netmonitor.saveRequestAndResponseBodies", false);
  const hud = await openNewTabAndConsole(TEST_URI);

  const messageNode = await doXhrAndExpand(hud);

  const responseTab = messageNode.querySelector("#response-tab");
  ok(responseTab, "Response tab is available");

  const {
    TEST_EVENTS,
  } = require("resource://devtools/client/netmonitor/src/constants.js");
  const onResponseContent = hud.ui.once(TEST_EVENTS.RECEIVED_RESPONSE_CONTENT);
  // Select Response tab and check the content.
  responseTab.click();

  // Even if response content aren't collected by NetworkObserver,
  // we do try to fetch the content via an RDP request, which
  // we try to wait for here.
  info("Wait for the async getResponseContent request");
  await onResponseContent;
  const responsePanel = messageNode.querySelector("#response-panel");

  // This is updated only after we tried to fetch the response content
  // and fired the getResponseContent request
  info("Wait for the empty response content");
  ok(
    responsePanel.querySelector("div.empty-notice"),
    "An empty notice is displayed instead of the response content"
  );
  const responseContent = messageNode.querySelector(
    "#response-panel .editor-row-container .CodeMirror"
  );
  ok(!responseContent, "Response content is really not displayed");

  await waitForLazyRequests(hud.toolbox);
  await closeToolbox();
});

async function doXhrAndExpand(hud) {
  // Execute XHR and expand it after all network
  // update events are received. Consequently,
  // check out content of all (HTTP details) tabs.
  const onMessage = waitForMessageByType(hud, XHR_URL, ".network");
  const onRequestUpdates = waitForRequestUpdates(hud);
  const onPayloadReady = waitForPayloadReady(hud);

  // Fire an XHR POST request.
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
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

  return messageNode;
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

function testStatusInfo(messageNode) {
  const statusInfo = messageNode.querySelector(".status-info");
  ok(statusInfo, "Status info is not empty");
}

// Headers
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
async function testSecurity(messageNode) {
  const securityTab = messageNode.querySelector("#security-tab");
  ok(securityTab, "Security tab is available");

  // Select Security tab and check the content.
  securityTab.click();
  await waitFor(
    () => messageNode.querySelector("#security-panel .treeTable .treeRow"),
    "Wait for #security-panel .treeTable .treeRow to be rendered"
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
  return hud.ui.once("network-messages-updated");
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
