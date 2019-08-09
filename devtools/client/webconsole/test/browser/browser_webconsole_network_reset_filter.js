/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that network log messages bring up the network panel and select the
// right request even if it was previously filtered off.

"use strict";

const TEST_PATH =
  "http://example.com/browser/devtools/client/webconsole/" + "test/browser/";
const TEST_URI = "data:text/html;charset=utf8,<p>test file URI";

add_task(async function() {
  await pushPref("devtools.webconsole.filter.net", true);

  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");
  const hud = toolbox.getCurrentPanel().hud;

  const onMessages = waitForMessages({
    hud,
    messages: [
      { text: "running network console logging tests" },
      { text: "test-network.html" },
      { text: "testscript.js" },
    ],
  });

  info("Wait for document to load");
  await loadDocument(TEST_PATH + "test-network.html");

  info("Wait for expected messages to appear");
  await onMessages;

  const url = TEST_PATH + "testscript.js?foo";
  // The url as it appears in the webconsole, without the GET parameters
  const shortUrl = TEST_PATH + "testscript.js";

  info("Open the testscript.js request in the network monitor");
  await openMessageInNetmonitor(toolbox, hud, url, shortUrl);

  const netmonitor = toolbox.getCurrentPanel();

  info(
    "Wait for the netmonitor headers panel to appear as it spawn RDP requests"
  );
  await waitUntil(() =>
    netmonitor.panelWin.document.querySelector(
      "#headers-panel .headers-overview"
    )
  );

  info("Filter out the current request");
  const { store, windowRequire } = netmonitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.toggleRequestFilterType("js"));

  info("Select back the webconsole");
  await toolbox.selectTool("webconsole");
  is(toolbox.currentToolId, "webconsole", "Web console was selected");

  info("Open the testscript.js request again in the network monitor");
  await openMessageInNetmonitor(toolbox, hud, url, shortUrl);

  info(
    "Wait for the netmonitor headers panel to appear as it spawn RDP requests"
  );
  await waitUntil(() =>
    netmonitor.panelWin.document.querySelector(
      "#headers-panel .headers-overview"
    )
  );
});
