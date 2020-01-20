/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL =
  "data:text/html;charset=utf8,test for showToolbox called while tool is opened";
const lazyToolId = "testtool1";

registerCleanupFunction(() => {
  gDevTools.unregisterTool(lazyToolId);
});

// Delay to wait before the lazy tool should finish
const TOOL_OPEN_DELAY = 3000;

class LazyDevToolsPanel extends DevToolPanel {
  constructor(iframeWindow, toolbox) {
    super(iframeWindow, toolbox);
  }

  async open() {
    await wait(TOOL_OPEN_DELAY);
    this.emit("ready");
    return this;
  }
}

function isPanelReady(toolbox, toolId) {
  return !!toolbox.getPanel(toolId);
}

/**
 * Test that showToolbox will wait until the specified tool is completely read before
 * returning. See Bug 1543907.
 */
add_task(async function automaticallyBindTexbox() {
  // We have to disable CSP for this test otherwise the CSP of
  // about:devtools-toolbox will block the data: url.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.csp.enable", false],
      ["csp.skip_about_page_has_csp_assert", true],
    ],
  });

  info(
    "Registering a tool with an input field and making sure the context menu works"
  );

  gDevTools.registerTool({
    id: lazyToolId,
    isTargetSupported: () => true,
    url: `data:text/html;charset=utf8,Lazy tool`,
    label: "Lazy",
    build: function(iframeWindow, toolbox) {
      this.panel = new LazyDevToolsPanel(iframeWindow, toolbox);
      return this.panel.open();
    },
  });

  const toolbox = await openNewTabAndToolbox(URL, "inspector");
  const onLazyToolReady = toolbox.once(lazyToolId + "-ready");
  toolbox.selectTool(lazyToolId);

  info("Wait until toolbox considers the current tool is the lazy tool");
  await waitUntil(() => toolbox.currentToolId == lazyToolId);

  ok(!isPanelReady(toolbox, lazyToolId), "lazyTool should not be ready yet");
  await gDevTools.showToolbox(toolbox.target, lazyToolId);
  ok(
    isPanelReady(toolbox, lazyToolId),
    "lazyTool should not ready after showToolbox"
  );

  // Make sure lazyTool is ready before leaving the test.
  await onLazyToolReady;
});
