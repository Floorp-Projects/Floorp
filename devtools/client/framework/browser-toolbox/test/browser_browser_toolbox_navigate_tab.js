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

// Test that the Browser Toolbox still works after navigating a content tab
add_task(async function () {
  // Forces the Browser Toolbox to open on the inspector by default
  await pushPref("devtools.browsertoolbox.panel", "inspector");

  await testNavigate("everything");
  await testNavigate("parent-process");
});

async function testNavigate(browserToolboxScope) {
  await pushPref("devtools.browsertoolbox.scope", browserToolboxScope);

  const tab = await addTab(
    `data:text/html,<div>NAVIGATE TEST - BEFORE: ${browserToolboxScope}</div>`
  );
  // Set the scope on the browser element to assert it easily in the Toolbox
  // task.
  tab.linkedBrowser.setAttribute("data-test-scope", browserToolboxScope);

  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({
    getNodeFront,
    selectNode,
  });

  const hasBrowserContainerTask = async ({ scope, hasNavigated }) => {
    /* global gToolbox */
    const inspector = await gToolbox.selectTool("inspector");
    info("Select the test browser element in the inspector");
    let selector = `browser[data-test-scope="${scope}"]`;
    if (hasNavigated) {
      selector += `[navigated="true"]`;
    }
    const nodeFront = await getNodeFront(selector, inspector);
    await selectNode(nodeFront, inspector);
    const browserContainer = inspector.markup.getContainer(nodeFront);
    return !!browserContainer;
  };

  info("Select the test browser in the Browser Toolbox (before navigation)");
  const hasContainerBeforeNavigation = await ToolboxTask.spawn(
    { scope: browserToolboxScope, hasNavigated: false },
    hasBrowserContainerTask
  );
  ok(
    hasContainerBeforeNavigation,
    "Found a valid container for the browser element before navigation"
  );

  info("Navigate the test tab to another data-uri");
  await navigateTo(
    `data:text/html,<div>NAVIGATE TEST - AFTER: ${browserToolboxScope}</div>`
  );
  tab.linkedBrowser.setAttribute("navigated", "true");

  info("Select the test browser in the Browser Toolbox (after navigation)");
  const hasContainerAfterNavigation = await ToolboxTask.spawn(
    { scope: browserToolboxScope, hasNavigated: true },
    hasBrowserContainerTask
  );
  ok(
    hasContainerAfterNavigation,
    "Found a valid container for the browser element after navigation"
  );

  await ToolboxTask.destroy();
}
