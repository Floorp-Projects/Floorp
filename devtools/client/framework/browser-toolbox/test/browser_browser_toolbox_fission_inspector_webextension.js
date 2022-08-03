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

// Test that expanding a browser element of a webextension in the browser toolbox works
// as expected (See Bug 1696862).

add_task(async function() {
  const extension = ExtensionTestUtils.loadExtension({
    // manifest_version: 2,
    manifest: {
      sidebar_action: {
        default_title: "SideBarExtensionTest",
        default_panel: "sidebar.html",
      },
    },
    useAddonManager: "temporary",

    files: {
      "sidebar.html": `
        <!DOCTYPE html>
        <html class="sidebar-extension-test">
          <head>
            <meta charset="utf-8">
            <script src="sidebar.js"></script>
          </head>
          <body>
            <h1 id="sidebar-extension-h1">Sidebar Extension Test</h1>
          </body>
        </html>`,
      "sidebar.js": function() {
        window.onload = () => {
          // eslint-disable-next-line no-undef
          browser.test.sendMessage("sidebar-ready");
        };
      },
    },
  });
  await extension.startup();
  await extension.awaitMessage("sidebar-ready");

  ok(true, "Extension sidebar is displayed");

  // Forces the Browser Toolbox to open on the inspector by default
  await pushPref("devtools.browsertoolbox.panel", "inspector");
  // Enable Multiprocess Browser Toolbox
  await pushPref("devtools.browsertoolbox.scope", "everything");
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

  const nodeId = await ToolboxTask.spawn(null, async () => {
    /* global gToolbox */
    const inspector = gToolbox.getPanel("inspector");

    const nodeFront = await selectNodeInFrames(
      [
        "browser#sidebar",
        "browser#webext-panels-browser",
        "html.sidebar-extension-test h1",
      ],
      inspector
    );
    return nodeFront.id;
  });

  is(
    nodeId,
    "sidebar-extension-h1",
    "The Browser Toolbox can inspect a node in the webextension sidebar document"
  );

  await ToolboxTask.destroy();
  await extension.unload();
});
