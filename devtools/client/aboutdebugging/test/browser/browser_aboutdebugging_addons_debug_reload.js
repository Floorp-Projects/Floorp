/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);

// Check that addon browsers can be reloaded via the toolbox reload shortcuts
add_task(async function testWebExtensionToolboxReload() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      background() {
        console.log("background script executed " + Math.random());
      },
      id: ADDON_ID,
      name: ADDON_NAME,
    },
    document
  );

  const {
    devtoolsDocument,
    devtoolsTab,
    devtoolsWindow,
  } = await openAboutDevtoolsToolbox(document, tab, window, ADDON_NAME);
  const toolbox = getToolbox(devtoolsWindow);

  ok(
    devtoolsDocument.querySelector(".qa-reload-button"),
    "Reload button is visible"
  );
  ok(
    !devtoolsDocument.querySelector(".qa-back-button"),
    "Back button is hidden"
  );
  ok(
    !devtoolsDocument.querySelector(".qa-forward-button"),
    "Forward button is hidden"
  );
  ok(
    !devtoolsDocument.querySelector(".debug-target-url-form"),
    "URL form is hidden"
  );
  ok(
    devtoolsDocument.getElementById("toolbox-meatball-menu-noautohide"),
    "Disable popup autohide button is displayed"
  );
  ok(
    !devtoolsDocument.getElementById(
      "toolbox-meatball-menu-pseudo-locale-accented"
    ),
    "Accented locale is not displayed (only on browser toolbox)"
  );

  const webconsole = await toolbox.selectTool("webconsole");
  const { hud } = webconsole;

  info("Wait for the initial background message to appear in the console");
  const initialMessage = await waitFor(() =>
    findMessagesByType(hud, "background script executed", ".console-api")
  );
  ok(initialMessage, "Found the expected message from the background script");

  const waitForLoadedPanelsReload = await watchForLoadedPanelsReload(toolbox);

  info("Reload the addon using a toolbox reload shortcut");
  toolbox.win.focus();
  synthesizeKeyShortcut(L10N.getStr("toolbox.reload.key"), toolbox.win);

  info("Wait until a new background log message is logged");
  const secondMessage = await waitFor(() => {
    const newMessage = findMessagesByType(
      hud,
      "background script executed",
      ".console-api"
    );
    if (newMessage && newMessage !== initialMessage) {
      return newMessage;
    }
    return false;
  });

  await waitForLoadedPanelsReload();

  info("Reload via the debug target info bar button");
  clickReload(devtoolsDocument);

  info("Wait until yet another background log message is logged");
  await waitFor(() => {
    const newMessage = findMessagesByType(
      hud,
      "background script executed",
      ".console-api"
    );
    return (
      newMessage &&
      newMessage !== initialMessage &&
      newMessage !== secondMessage
    );
  });

  await waitForLoadedPanelsReload();

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});

function clickReload(devtoolsDocument) {
  devtoolsDocument.querySelector(".qa-reload-button").click();
}
