/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

/* import-globals-from ../../../inspector/test/shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

// On debug test machine, it takes about 50s to run the test.
requestLongerTimeout(4);

/**
 * Check that different-site iframes can be expanded in the Omniscient Browser
 * Toolbox. The test is supposed to run successfully with or without fission.
 * Pass --enable-fission to ./mach test to enable fission when running this
 * test locally.
 */
add_task(async function() {
  const ToolboxTask = await initBrowserToolboxTask({
    enableBrowserToolboxFission: true,
  });
  await ToolboxTask.importFunctions({
    getNodeFront,
    getNodeFrontInFrames,
    selectNode,
    // selectNodeInFrames depends on selectNode, getNodeFront, getNodeFrontInFrames.
    selectNodeInFrames,
  });

  const tab = await addTab(
    `http://example.com/browser/devtools/client/framework/browser-toolbox/test/doc_browser_toolbox_fission_contentframe_inspector_page.html`
  );

  // Set a custom attribute on the tab's browser, in order to easily select it in the markup view
  tab.linkedBrowser.setAttribute("test-tab", "true");

  const testAttribute = await ToolboxTask.spawn(null, async () => {
    /* global gToolbox */
    const inspector = await gToolbox.selectTool("inspector");
    const onSidebarSelect = inspector.sidebar.once("select");
    inspector.sidebar.select("computedview");
    await onSidebarSelect;

    info("Select the test element nested in the remote iframe");
    const nodeFront = await selectNodeInFrames(
      ['browser[remote="true"][test-tab]', "iframe", "#inside-iframe"],
      inspector
    );

    return nodeFront.getAttribute("test-attribute");
  });

  is(
    testAttribute,
    "fission",
    "Could successfully read attribute on a node inside a remote iframe"
  );

  await ToolboxTask.destroy();
});
