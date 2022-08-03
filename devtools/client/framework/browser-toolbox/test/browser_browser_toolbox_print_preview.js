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

// Test that the MultiProcessBrowserToolbox can be opened when print preview is
// started, and can select elements in the print preview document.
add_task(async function() {
  // Forces the Browser Toolbox to open on the inspector by default
  await pushPref("devtools.browsertoolbox.panel", "inspector");

  // Enable Multiprocess Browser Toolbox
  await pushPref("devtools.browsertoolbox.scope", "everything");

  // Open the tab *after* opening the Browser Toolbox in order to force creating the remote frames
  // late and exercise frame target watching code.
  await addTab(`data:text/html,<div id="test-div">PRINT PREVIEW TEST</div>`);

  info("Start the print preview for the current tab");
  document.getElementById("cmd_print").doCommand();

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

  const hasCloseButton = await ToolboxTask.spawn(null, async () => {
    /* global gToolbox */
    const inspector = gToolbox.getPanel("inspector");

    info("Select the #test-div element in the printpreview document");
    await selectNodeInFrames(
      ['browser[printpreview="true"]', "#test-div"],
      inspector
    );
    return !!gToolbox.doc.getElementById("toolbox-close");
  });
  ok(!hasCloseButton, "Browser toolbox doesn't have a close button");

  await ToolboxTask.destroy();
});
