/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "https://example.com/browser/devtools/client/webconsole/" + "test/browser/";
const TEST_URI = TEST_PATH + TEST_FILE;

pushPref("devtools.webconsole.filter.netxhr", true);

add_task(async function task() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const xhrUrl = TEST_PATH + "sjs_slow-response-test-server.sjs";

  info("Fire an XHR POST request from the console.");
  const { node: messageNode } = await executeAndWaitForMessage(
    hud,
    `
    xhrConsole = () => testXhrPostSlowResponse();
    xhrConsole();
  `,
    xhrUrl
  );

  ok(messageNode, "Network message found.");

  info("Expand the network message");
  await expandXhrMessage(messageNode);
  const stackTraceTab = messageNode.querySelector("#stack-trace-tab");
  ok(stackTraceTab, "StackTrace tab is available");

  stackTraceTab.click();
  const selector = "#stack-trace-panel .frame-link";
  await waitFor(() => messageNode.querySelector(selector));
  const frames = [...messageNode.querySelectorAll(selector)];

  is(frames.length, 4, "There's the expected frames");
  const functionNames = frames.map(
    f => f.querySelector(".frame-link-function-display-name").textContent
  );
  is(
    functionNames.join("|"),
    "makeXhr|testXhrPostSlowResponse|xhrConsole|<anonymous>",
    "The stacktrace does not have devtools' internal frames"
  );
});

function expandXhrMessage(node) {
  info(
    "Click on XHR message and wait for the network detail panel to be displayed"
  );
  node.querySelector(".url").click();
  return waitFor(() => node.querySelector("#stack-trace-tab"));
}
