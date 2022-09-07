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

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - has a frame list menu and the noautohide toolbar toggle button, and they
 *   can be used to switch the current target to the extension popup page.
 */
add_task(async function testWebExtensionsToolboxWebConsole() {
  await enableExtensionDebugging();

  // Bug 1686922: Disable the error count button to avoid intermittent failures.
  await pushPref("devtools.command-button-errorcount.enabled", false);

  is(
    Services.prefs.getBoolPref("ui.popup.disable_autohide"),
    false,
    "disable_autohide should be initially false"
  );

  const {
    document,
    tab,
    window: aboutDebuggingWindow,
  } = await openAboutDebugging();
  await selectThisFirefoxPage(
    document,
    aboutDebuggingWindow.AboutDebugging.store
  );

  const extension = await installTestAddon(document);

  const onBackgroundFunctionCalled = waitForExtensionTestMessage(
    extension,
    "onBackgroundFunctionCalled"
  );
  const onPopupPageFunctionCalled = waitForExtensionTestMessage(
    extension,
    "onPopupPageFunctionCalled"
  );

  info("Open a toolbox to debug the addon");
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    aboutDebuggingWindow,
    ADDON_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);
  const webconsole = await toolbox.selectTool("webconsole");

  info("Clicking the menu button to disable autohide");
  await disablePopupAutohide(toolbox);

  info("Check that console messages are evaluated in the background context");
  const consoleWrapper = webconsole.hud.ui.wrapper;
  consoleWrapper.dispatchEvaluateExpression("backgroundFunction()");
  await onBackgroundFunctionCalled;

  clickOnAddonWidget(ADDON_ID);

  info("Wait until the frames list button is displayed");
  const btn = await waitFor(() =>
    toolbox.doc.getElementById("command-button-frames")
  );

  info("Clicking the frame list button");
  btn.click();

  const menuList = toolbox.doc.getElementById("toolbox-frame-menu");
  const frames = Array.from(menuList.querySelectorAll(".command"));
  is(frames.length, 2, "Has the expected number of frames");

  const popupFrameBtn = frames
    .filter(frame => {
      return frame.querySelector(".label").textContent.endsWith("popup.html");
    })
    .pop();

  ok(popupFrameBtn, "Extension Popup frame found in the listed frames");

  info(
    "Click on the extension popup frame and wait for a `dom-complete` resource"
  );
  const {
    onDomCompleteResource,
  } = await waitForNextTopLevelDomCompleteResource(toolbox.commands);
  popupFrameBtn.click();
  await onDomCompleteResource;

  info("Execute `popupPageFunction()`");
  consoleWrapper.dispatchEvaluateExpression("popupPageFunction()");

  const args = await onPopupPageFunctionCalled;
  ok(true, "Received console message from the popup page function as expected");
  is(args[0], "onPopupPageFunctionCalled", "Got the expected console message");
  is(
    args[1] && args[1].name,
    ADDON_NAME,
    "Got the expected manifest from WebExtension API"
  );

  await closeAboutDevtoolsToolbox(document, devtoolsTab, aboutDebuggingWindow);

  is(
    Services.prefs.getBoolPref("ui.popup.disable_autohide"),
    false,
    "disable_autohide should be reset to false when the toolbox is closed"
  );

  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});

/**
 * Helper to wait for a specific message on an Extension instance.
 */
function waitForExtensionTestMessage(extension, expectedMessage) {
  return new Promise(done => {
    extension.on("test-message", function testLogListener(evt, ...args) {
      const [message] = args;

      if (message !== expectedMessage) {
        return;
      }

      extension.off("test-message", testLogListener);
      done(args);
    });
  });
}

/**
 * Install the addon used for this test.
 * Returns a Promise that resolve the Extension instance that was just
 * installed.
 */
async function installTestAddon(doc) {
  // Start watching for the extension on the Extension Management before we
  // install it.
  const onExtensionReady = waitForExtension(ADDON_NAME);

  // Install the extension.
  await installTemporaryExtensionFromXPI(
    {
      background() {
        const { browser } = this;
        window.backgroundFunction = function() {
          browser.test.sendMessage("onBackgroundFunctionCalled");
        };
      },
      extraProperties: {
        browser_action: {
          default_title: "WebExtension Popup Debugging",
          default_popup: "popup.html",
        },
      },
      files: {
        "popup.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="popup.js"></script>
          </head>
          <body>
            Background Page Body Test Content
          </body>
        </html>
      `,
        "popup.js": function() {
          const { browser } = this;
          window.popupPageFunction = function() {
            browser.test.sendMessage(
              "onPopupPageFunctionCalled",
              browser.runtime.getManifest()
            );
          };
        },
      },
      id: ADDON_ID,
      name: ADDON_NAME,
    },
    doc
  );

  // The onExtensionReady promise will resolve the extension instance.
  return onExtensionReady;
}

/**
 * Helper to retrieve the Extension instance.
 */
async function waitForExtension(addonName) {
  const { Management } = ChromeUtils.import(
    "resource://gre/modules/Extension.jsm"
  );

  return new Promise(resolve => {
    Management.on("startup", function listener(event, extension) {
      if (extension.name != addonName) {
        return;
      }

      Management.off("startup", listener);
      resolve(extension);
    });
  });
}

/**
 * Disables the popup autohide feature, which is mandatory to debug webextension
 * popups.
 */
function disablePopupAutohide(toolbox) {
  return new Promise(resolve => {
    toolbox.doc.getElementById("toolbox-meatball-menu-button").click();
    toolbox.doc.addEventListener(
      "popupshown",
      () => {
        const menuItem = toolbox.doc.getElementById(
          "toolbox-meatball-menu-noautohide"
        );
        menuItem.click();
        resolve();
      },
      { once: true }
    );
  });
}
