/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - always on top is enabled by default and can be toggled off
 */
add_task(async function testWebExtensionsToolbox() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      background() {
        document.body.innerText = "Background Page Body Test Content";
      },
      id: ADDON_ID,
      name: ADDON_NAME,
    },
    document
  );

  info("Open a toolbox to debug the addon");
  const { devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NAME
  );

  const toolbox = getToolbox(devtoolsWindow);

  ok(
    isWindowAlwaysOnTop(devtoolsWindow),
    "The toolbox window is always on top"
  );
  const toggleButton = toolbox.doc.querySelector(".toolbox-always-on-top");
  ok(!!toggleButton, "The always on top toggle button is visible");
  ok(
    toggleButton.classList.contains("checked"),
    "The button is highlighted to report that always on top is enabled"
  );

  // When running the test, the devtools window might not be focused, while it does IRL.
  // Force it to be focused to better reflect the default behavior.
  info("Force focusing the devtools window");
  devtoolsWindow.focus();

  // As we update the button with a debounce, we have to wait for it to updates
  await waitFor(
    () => toggleButton.classList.contains("toolbox-is-focused"),
    "Wait for the button to be highlighting that the toolbox is focused (the button isn't highlighted)"
  );
  ok(true, "Expected class is added when toolbox is focused");

  info("Focus the browser window");
  window.focus();

  await waitFor(
    () => !toggleButton.classList.contains("toolbox-is-focused"),
    "Wait for the button to be highlighting that the toolbox is no longer focused (the button is highlighted)"
  );
  ok(true, "Focused class is removed when browser window gets focused");

  info("Re-focus the DevTools window");
  devtoolsWindow.focus();

  await waitFor(
    () => toggleButton.classList.contains("toolbox-is-focused"),
    "Wait for the button to be re-highlighting that the toolbox is focused"
  );

  const onToolboxReady = gDevTools.once("toolbox-ready");
  info("Click on the button");
  toggleButton.click();

  info("Wait for a new toolbox to be created");
  const secondToolbox = await onToolboxReady;

  ok(
    !isWindowAlwaysOnTop(secondToolbox.win),
    "The toolbox window is no longer always on top"
  );
  const secondToggleButton = secondToolbox.doc.querySelector(
    ".toolbox-always-on-top"
  );
  ok(!!secondToggleButton, "The always on top toggle button is still visible");

  ok(
    !secondToggleButton.classList.contains("checked"),
    "The button is no longer highlighted to report that always on top is disabled"
  );

  await closeWebExtAboutDevtoolsToolbox(secondToolbox.win, window);
  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});

// Check if the window has the "alwaysontop" chrome flag
function isWindowAlwaysOnTop(window) {
  return (
    window.docShell.treeOwner
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIAppWindow).chromeFlags &
    Ci.nsIWebBrowserChrome.CHROME_ALWAYS_ON_TOP
  );
}
