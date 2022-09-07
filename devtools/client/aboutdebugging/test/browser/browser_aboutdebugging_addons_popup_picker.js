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

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

/**
 * Check that the node picker can be used when dynamically navigating to a
 * webextension popup.
 */
add_task(async function testNodePickerInExtensionPopup() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  // Note that this extension should not define a background script in order to
  // reproduce the issue. Otherwise opening the popup does not trigger an auto
  // navigation from DevTools and you have to use the "Disable Popup Auto Hide"
  // feature which works around the bug tested here.
  await installTemporaryExtensionFromXPI(
    {
      extraProperties: {
        browser_action: {
          default_title: "WebExtension with popup",
          default_popup: "popup.html",
        },
      },
      files: {
        "popup.html": `<!DOCTYPE html>
        <html>
          <body>
            <div id="pick-me"
                 style="width:100px; height: 60px; background-color: #f5e8fc">
              Pick me!
            </div>
          </body>
        </html>
      `,
      },
      id: ADDON_ID,
      name: ADDON_NAME,
    },
    document
  );

  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);
  const inspector = await toolbox.getPanel("inspector");

  info("Start the node picker");
  await toolbox.nodePicker.start();

  info("Open the webextension popup");
  // Clicking on the addon popup will trigger a navigation between the DevTools
  // fallback document and the popup document.
  // Wait until the inspector was fully reloaded and for the node-picker to be
  // restarted.
  const nodePickerRestarted = toolbox.nodePicker.once(
    "node-picker-webextension-target-restarted"
  );
  const reloaded = inspector.once("reloaded");
  clickOnAddonWidget(ADDON_ID);
  await reloaded;
  await nodePickerRestarted;

  const popup = await waitFor(() =>
    gBrowser.ownerDocument.querySelector(".webextension-popup-browser")
  );

  info("Pick an element inside the webextension popup");
  const onNewNodeFront = inspector.selection.once("new-node-front");
  BrowserTestUtils.synthesizeMouseAtCenter(
    "#pick-me",
    {},
    popup.browsingContext
  );
  const nodeFront = await onNewNodeFront;
  is(nodeFront.id, "pick-me", "The expected node front was selected");

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});
