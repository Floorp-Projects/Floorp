/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

// On debug test machine, it takes about 50s to run the test.
requestLongerTimeout(4);

// Check that CSS rules are displayed with the proper source label in the
// browser toolbox.
add_task(async function () {
  // Forces the Browser Toolbox to open on the inspector by default
  await pushPref("devtools.browsertoolbox.panel", "inspector");
  // Enable Multiprocess Browser Toolbox
  await pushPref("devtools.browsertoolbox.scope", "everything");

  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({
    getNodeFront,
    getNodeFrontInFrames,
    getRuleViewLinkByIndex,
    selectNode,
    // selectNodeInFrames depends on selectNode, getNodeFront, getNodeFrontInFrames.
    selectNodeInFrames,
    waitUntil,
  });

  // This is a simple test page, which contains a <div> with a CSS rule `color: red`
  // coming from a dedicated stylesheet.
  const tab = await addTab(
    `https://example.com/browser/devtools/client/framework/browser-toolbox/test/doc_browser_toolbox_ruleview_stylesheet.html`
  );

  // Set a custom attribute on the tab's browser, in order to easily select it in the markup view
  tab.linkedBrowser.setAttribute("test-tab", "true");

  info(
    "Get the source label for a rule displayed in the Browser Toolbox ruleview"
  );
  const sourceLabel = await ToolboxTask.spawn(null, async () => {
    /* global gToolbox */
    const inspector = gToolbox.getPanel("inspector");

    info("Select the rule view");
    const onSidebarSelect = inspector.sidebar.once("select");
    inspector.sidebar.select("ruleview");
    await onSidebarSelect;

    info("Select a DIV element in the test page");
    await selectNodeInFrames(
      ['browser[remote="true"][test-tab]', "div"],
      inspector
    );

    info("Retrieve the sourceLabel for the rule at index 1");
    const ruleView = inspector.getPanel("ruleview").view;
    await waitUntil(() => getRuleViewLinkByIndex(ruleView, 1));
    const sourceLabelEl = getRuleViewLinkByIndex(ruleView, 1).querySelector(
      ".ruleview-rule-source-label"
    );

    return sourceLabelEl.textContent;
  });

  is(
    sourceLabel,
    "style_browser_toolbox_ruleview_stylesheet.css:1",
    "source label has the expected value in the ruleview"
  );

  await ToolboxTask.destroy();
});
