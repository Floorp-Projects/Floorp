/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that opening the stacktrace details panel in the netmonitor and console
 * show the expected stacktraces.
 */

add_task(async function() {
  const URL = EXAMPLE_URL + "html_single-get-page.html";
  const REQUEST =
    "http://example.com/browser/devtools/client/netmonitor/test/request_0";

  const { tab, monitor } = await initNetMonitor(URL, {
    requestCount: 1,
  });

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Starting test... ");

  const allRequestsVisible = waitUntil(
    () => document.querySelectorAll(".request-list-item").length == 2
  );
  await refreshTab(tab);
  await allRequestsVisible;

  const onStackTracesVisible = waitUntil(
    () => document.querySelector("#stack-trace-panel .stack-trace .frame-link"),
    "Wait for the stacktrace to be rendered"
  );

  // Select the request initiated by html_single-get-page.html
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelector(
      `.request-list-item .requests-list-file[title="${REQUEST}"]`
    )
  );

  // Wait for the stack trace tab to show
  await waitUntil(() =>
    document.querySelector(".network-details-bar #stack-trace-tab")
  );

  clickOnSidebarTab(document, "stack-trace");

  await onStackTracesVisible;

  // Switch to the webconsole.
  const { hud } = await monitor.toolbox.selectTool("webconsole");

  const fetchRequestUrlNode = hud.ui.outputNode.querySelector(
    `.webconsole-output .cm-s-mozilla.message.network span[title="${REQUEST}"]`
  );
  fetchRequestUrlNode.click();

  const messageWrapper = fetchRequestUrlNode.closest(".message-body-wrapper");

  await waitFor(
    () => messageWrapper.querySelector(".network-info"),
    "Wait for .network-info to be rendered"
  );

  // Select stacktrace details panel and check the content.
  messageWrapper.querySelector("#stack-trace-tab").click();
  await waitFor(
    () => messageWrapper.querySelector("#stack-trace-panel .frame-link"),
    "Wait for stacktrace to be rendered"
  );

  return teardown(monitor);
});
