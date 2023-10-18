/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that pressing various page reload keyboard shortcuts always works when devtools
// has focus, no matter if it's undocked or docked, and whatever the tool selected (this
// is to avoid tools from overriding the page reload shortcuts).
// This test also serves as a safety net checking that tools just don't explode when the
// page is reloaded.
// It is therefore quite long to run.

requestLongerTimeout(10);
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);

// allow a context error because it is harmless. This could likely be removed in the next patch because it is a symptom of events coming from the target-list and debugger targets module...
PromiseTestUtils.allowMatchingRejectionsGlobally(/Page has navigated/);

const TEST_URL =
  "data:text/html;charset=utf-8," +
  "<html><head><title>Test reload</title></head>" +
  "<body><h1>Testing reload from devtools</h1></body></html>";

const { Toolbox } = require("resource://devtools/client/framework/toolbox.js");
const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);

// Track how many page reloads we've sent to the page.
var reloadsSent = 0;

add_task(async function () {
  await addTab(TEST_URL);
  const tab = gBrowser.selectedTab;
  const toolIDs = await getSupportedToolIds(tab);

  info(
    "Display the toolbox, docked at the bottom, with the first tool selected"
  );
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: toolIDs[0],
    hostType: Toolbox.HostType.BOTTOM,
  });

  info(
    "Listen to page reloads to check that they are indeed sent by the toolbox"
  );
  let reloadDetected = 0;
  const reloadCounter = msg => {
    reloadDetected++;
    info("Detected reload #" + reloadDetected);
    is(
      reloadDetected,
      reloadsSent,
      "Detected the right number of reloads in the page"
    );
  };

  const removeLoadListener = BrowserTestUtils.addContentEventListener(
    gBrowser.selectedBrowser,
    "load",
    reloadCounter,
    {}
  );

  info("Start testing with the toolbox docked");
  // Note that we actually only test 1 tool in docked mode, to cut down on test time.
  await testOneTool(toolbox, toolIDs[toolIDs.length - 1]);

  info("Switch to undocked mode");
  await toolbox.switchHost(Toolbox.HostType.WINDOW);
  toolbox.win.focus();

  info("Now test with the toolbox undocked");
  for (const toolID of toolIDs) {
    await testOneTool(toolbox, toolID);
  }

  info("Switch back to docked mode");
  await toolbox.switchHost(Toolbox.HostType.BOTTOM);

  removeLoadListener();

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});

async function testOneTool(toolbox, toolID) {
  info(`Select tool ${toolID}`);
  await toolbox.selectTool(toolID);

  assertThemeStyleSheet(toolbox, toolID);

  await testReload("toolbox.reload.key", toolbox);
  await testReload("toolbox.reload2.key", toolbox);
  await testReload("toolbox.forceReload.key", toolbox);
  await testReload("toolbox.forceReload2.key", toolbox);
}

async function testReload(shortcut, toolbox) {
  info(`Reload with ${shortcut}`);

  await sendToolboxReloadShortcut(L10N.getStr(shortcut), toolbox);
  reloadsSent++;
}

/**
 * As opening all panels is an expensive operation, reuse this test in order
 * to add a few assertions around panel's stylesheets.
 * Ensure the proper ordering of the theme stylesheet. `global.css` should come
 * first if it exists, then the theme.
 */
function assertThemeStyleSheet(toolbox, toolID) {
  const iframe = toolbox.doc.getElementById("toolbox-panel-iframe-" + toolID);
  const styleSheets = iframe.contentDocument.querySelectorAll(
    `link[rel="stylesheet"]`
  );
  ok(
    !!styleSheets.length,
    `The panel ${toolID} should have at least have one stylesheet`
  );

  // In the web console, we have a special case where global.css is registered very first
  if (styleSheets[0].href === "chrome://global/skin/global.css") {
    is(styleSheets[1].href, "chrome://devtools/skin/light-theme.css");
  } else {
    // Otherwise, in all other panels, the theme file is registered very first
    is(styleSheets[0].href, "chrome://devtools/skin/light-theme.css");
  }
}
