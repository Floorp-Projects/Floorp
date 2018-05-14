/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/mochitest/";
const TEST_URI = TEST_PATH + TEST_FILE;

const NET_PREF = "devtools.webconsole.filter.net";
const XHR_PREF = "devtools.webconsole.filter.netxhr";

requestLongerTimeout(2);

Services.prefs.setBoolPref(NET_PREF, false);
Services.prefs.setBoolPref(XHR_PREF, true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref(NET_PREF);
  Services.prefs.clearUserPref(XHR_PREF);
});

let tabs = [{
  id: "headers",
  testEmpty: testEmptyHeaders,
  testContent: testHeaders,
}, {
  id: "cookies",
  testEmpty: testEmptyCookies,
  testContent: testCookies,
}, {
  id: "params",
  testEmpty: testEmptyParams,
  testContent: testParams,
}, {
  id: "response",
  testEmpty: testEmptyResponse,
  testContent: testResponse,
}, {
  id: "timings",
  testEmpty: testEmptyTimings,
  testContent: testTimings,
}, {
  id: "stack-trace",
  testEmpty: testEmptyStackTrace,
  testContent: testStackTrace,
}];

/**
 * Main test for checking HTTP logs in the Console panel.
 */
add_task(async function task() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const currentTab = gBrowser.selectedTab;
  let target = TargetFactory.forTab(currentTab);

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
  for (let tab of tabs) {
    await openRequestBeforeUpdates(target, hud, tab);
  }
});

async function openRequestAfterUpdates(target, hud) {
  let toolbox = gDevTools.getToolbox(target);

  let xhrUrl = TEST_PATH + "sjs_slow-response-test-server.sjs";
  let message = waitForMessage(hud, xhrUrl);

  // Fire an XHR POST request.
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.testXhrPostSlowResponse();
  });

  let { node: messageNode } = await message;

  info("Network message found.");

  await waitForRequestUpdates(toolbox);

  let payload = waitForPayloadReady(toolbox);

  // Expand network log
  let urlNode = messageNode.querySelector(".url");
  urlNode.click();

  let toggleButtonNode = messageNode.querySelector(".sidebar-toggle");
  ok(!toggleButtonNode, "Sidebar toggle button shouldn't be shown");

  await payload;
  await testNetworkMessage(toolbox, messageNode);
}

async function openRequestBeforeUpdates(target, hud, tab) {
  let toolbox = gDevTools.getToolbox(target);

  hud.jsterm.clearOutput(true);

  let xhrUrl = TEST_PATH + "sjs_slow-response-test-server.sjs";
  let message = waitForMessage(hud, xhrUrl);

  // Fire an XHR POST request.
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.testXhrPostSlowResponse();
  });

  let { node: messageNode } = await message;

  info("Network message found.");

  let updates = waitForRequestUpdates(toolbox);
  let payload = waitForPayloadReady(toolbox);

  // Set the default panel.
  const state = hud.ui.newConsoleOutput.getStore().getState();
  state.ui.networkMessageActiveTabId = tab.id;

  // Expand network log
  let urlNode = messageNode.querySelector(".url");
  urlNode.click();

  // Make sure the current tab is the expected one.
  let currentTab = messageNode.querySelector(`#${tab.id}-tab`);
  is(currentTab.getAttribute("aria-selected"), "true",
    "The correct tab is selected");

  // The tab should be empty now.
  tab.testEmpty(messageNode);

  // Wait till all updates and payload are received.
  await updates;
  await payload;

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
  await waitForLazyRequests(toolbox);
}

// Status Info

function testStatusInfo(messageNode) {
  let statusInfo = messageNode.querySelector(".status-info");
  ok(statusInfo, "Status info is not empty");
}

// Headers

function testEmptyHeaders(messageNode) {
  let emptyNotice = messageNode.querySelector("#headers-panel .empty-notice");
  ok(emptyNotice, "Headers tab is empty");
}

async function testHeaders(messageNode) {
  let headersTab = messageNode.querySelector("#headers-tab");
  ok(headersTab, "Headers tab is available");

  // Select Headers tab and check the content.
  headersTab.click();
  await waitUntil(() => {
    return !!messageNode.querySelector("#headers-panel .headers-overview");
  });
}

// Cookies

function testEmptyCookies(messageNode) {
  let emptyNotice = messageNode.querySelector("#cookies-panel .empty-notice");
  ok(emptyNotice, "Cookies tab is empty");
}

async function testCookies(messageNode) {
  let cookiesTab = messageNode.querySelector("#cookies-tab");
  ok(cookiesTab, "Cookies tab is available");

  // Select tab and check the content.
  cookiesTab.click();
  await waitUntil(() => {
    return !!messageNode.querySelector("#cookies-panel .treeValueCell");
  });
}

// Params

function testEmptyParams(messageNode) {
  let emptyNotice = messageNode.querySelector("#params-panel .empty-notice");
  ok(emptyNotice, "Params tab is empty");
}

async function testParams(messageNode) {
  let paramsTab = messageNode.querySelector("#params-tab");
  ok(paramsTab, "Params tab is available");

  // Select Params tab and check the content. CodeMirror initialization
  // is delayed to prevent UI freeze, so wait for a little while.
  paramsTab.click();
  let paramsPanel = messageNode.querySelector("#params-panel");
  await waitForSourceEditor(paramsPanel);
  let paramsContent = messageNode.querySelector(
    "#params-panel .panel-container .CodeMirror");
  ok(paramsContent, "Params content is available");
  ok(paramsContent.textContent.includes("Hello world!"), "Post body is correct");
}

// Response

function testEmptyResponse(messageNode) {
  let panel = messageNode.querySelector("#response-panel .tab-panel");
  is(panel.textContent, "", "Cookies tab is empty");
}

async function testResponse(messageNode) {
  let responseTab = messageNode.querySelector("#response-tab");
  ok(responseTab, "Response tab is available");

  // Select Response tab and check the content. CodeMirror initialization
  // is delayed, so again wait for a little while.
  responseTab.click();
  let responsePanel = messageNode.querySelector("#response-panel");
  await waitForSourceEditor(responsePanel);
  let responseContent = messageNode.querySelector(
    "#response-panel .editor-row-container .CodeMirror");
  ok(responseContent, "Response content is available");
  ok(responseContent.textContent, "Response text is available");
}

// Timings

function testEmptyTimings(messageNode) {
  let panel = messageNode.querySelector("#timings-panel .tab-panel");
  is(panel.textContent, "", "Timings tab is empty");
}

async function testTimings(messageNode) {
  let timingsTab = messageNode.querySelector("#timings-tab");
  ok(timingsTab, "Timings tab is available");

  // Select Timings tab and check the content.
  timingsTab.click();
  await waitUntil(() => {
    return !!messageNode.querySelector(
      "#timings-panel .timings-container .timings-label");
  });
  let timingsContent = messageNode.querySelector(
    "#timings-panel .timings-container .timings-label");
  ok(timingsContent, "Timings content is available");
  ok(timingsContent.textContent, "Timings text is available");
}

// Stack Trace

function testEmptyStackTrace(messageNode) {
  let panel = messageNode.querySelector("#stack-trace-panel .stack-trace");
  is(panel.textContent, "", "StackTrace tab is empty");
}

async function testStackTrace(messageNode) {
  let stackTraceTab = messageNode.querySelector("#stack-trace-tab");
  ok(stackTraceTab, "StackTrace tab is available");

  // Select Timings tab and check the content.
  stackTraceTab.click();
  await waitUntil(() => {
    return !!messageNode.querySelector("#stack-trace-panel .frame-link");
  });
}

// Waiting helpers

async function waitForPayloadReady(toolbox) {
  let {ui} = toolbox.getCurrentPanel().hud;
  return new Promise(resolve => {
    ui.jsterm.hud.on("network-request-payload-ready", () => {
      info("network-request-payload-ready received");
      resolve();
    });
  });
}

async function waitForSourceEditor(panel) {
  return waitUntil(() => {
    return !!panel.querySelector(".CodeMirror");
  });
}

async function waitForRequestUpdates(toolbox) {
  let {ui} = toolbox.getCurrentPanel().hud;
  return new Promise(resolve => {
    ui.jsterm.hud.on("network-message-updated", () => {
      info("network-message-updated received");
      resolve();
    });
  });
}

/**
 * Wait until all lazily fetch requests in netmonitor get finsished.
 * Otherwise test will be shutdown too early and cause failure.
 */
async function waitForLazyRequests(toolbox) {
  let {ui} = toolbox.getCurrentPanel().hud;
  let proxy = ui.jsterm.hud.proxy;
  return waitUntil(() => {
    return !proxy.networkDataProvider.lazyRequestData.size;
  });
}
