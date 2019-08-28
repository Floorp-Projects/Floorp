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
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

// This is a migration from:
// https://searchfox.org/mozilla-central/source/devtools/client/aboutdebugging/test/browser_addons_debug_webextension_popup.js

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - when the debug button is clicked on a webextension, the opened toolbox
 *   has a working webconsole with the background page as default target;
 * - the webextension developer toolbox has a working Inspector panel, with the
 *   background page as default target;
 * - the webextension developer toolbox is connected to a fallback page when the
 *   background page is not available (and in the fallback page document body contains
 *   the expected message, which warns the user that the current page is not a real
 *   webextension context);
 * - the webextension developer toolbox has a frame list menu and the noautohide toolbar
 *   toggle button, and they can be used to switch the current target to the extension
 *   popup page.
 */
add_task(async function testWebExtensionsToolboxWebConsole() {
  await enableExtensionDebugging();

  is(
    Services.prefs.getBoolPref("ui.popup.disable_autohide"),
    false,
    "disable_autohide should be initially false"
  );

  info("Create promises waiting for the messages emitted by the test addon");
  let onReadyForOpenPopup;
  let onPopupCustomMessage;
  const { Management } = ChromeUtils.import(
    "resource://gre/modules/Extension.jsm",
    null
  );
  Management.on("startup", function listener(event, extension) {
    if (extension.name != ADDON_NAME) {
      return;
    }

    Management.off("startup", listener);

    function waitForExtensionTestMessage(expectedMessage) {
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

    // Wait for the test script running in the browser toolbox process
    // to be ready for selecting the popup page in the frame list selector.
    onReadyForOpenPopup = waitForExtensionTestMessage("readyForOpenPopup");

    // Wait for a notification sent by a script evaluated the test addon via
    // the web console.
    onPopupCustomMessage = waitForExtensionTestMessage(
      "popupPageFunctionCalled"
    );
  });

  const {
    document,
    tab,
    window: aboutDebuggingWindow,
  } = await openAboutDebugging();
  await selectThisFirefoxPage(
    document,
    aboutDebuggingWindow.AboutDebugging.store
  );

  await installTemporaryExtensionFromXPI(
    {
      background: function() {
        const { browser } = this;
        window.myWebExtensionShowPopup = function() {
          browser.test.sendMessage("readyForOpenPopup");
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
          window.myWebExtensionPopupAddonFunction = function() {
            browser.test.sendMessage(
              "popupPageFunctionCalled",
              browser.runtime.getManifest()
            );
          };
        },
      },
      id: ADDON_ID,
      name: ADDON_NAME,
    },
    document
  );

  info("Open a toolbox to debug the addon");
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    aboutDebuggingWindow,
    ADDON_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);

  const onToolboxClose = gDevTools.once("toolbox-destroyed");
  toolboxTestScript(toolbox, devtoolsTab);

  info("Wait until the addon popup is opened from the test script");
  await onReadyForOpenPopup;

  const widgetId = ADDON_ID.toLowerCase().replace(/[^a-z0-9_-]/g, "_");
  const browserActionId = widgetId + "-browser-action";
  const browserActionEl = window.document.getElementById(browserActionId);

  ok(browserActionEl, "Got the browserAction button from the browser UI");
  browserActionEl.click();
  info("Clicked on the browserAction button");

  const args = await onPopupCustomMessage;
  ok(true, "Received console message from the popup page function as expected");
  is(args[0], "popupPageFunctionCalled", "Got the expected console message");
  is(
    args[1] && args[1].name,
    ADDON_NAME,
    "Got the expected manifest from WebExtension API"
  );

  info("Wait for the toolbox to close");
  await onToolboxClose;

  is(
    Services.prefs.getBoolPref("ui.popup.disable_autohide"),
    false,
    "disable_autohide should be reset to false when the toolbox is closed"
  );

  // The test script will not close the toolbox and will timeout if it fails, so reaching
  // this point in the test is enough to assume the test was successful.
  ok(true, "Addon toolbox closed");

  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});

async function toolboxTestScript(toolbox, devtoolsTab) {
  const popupFramePromise = new Promise(resolve => {
    const listener = data => {
      if (data.frames.some(({ url }) => url && url.endsWith("popup.html"))) {
        toolbox.target.off("frame-update", listener);
        resolve();
      }
    };
    toolbox.target.on("frame-update", listener);
  });

  const waitForFrameListUpdate = toolbox.target.once("frame-update");

  toolbox
    .selectTool("webconsole")
    .then(async console => {
      const clickNoAutoHideMenu = () => {
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
      };

      dump(`Clicking the menu button\n`);
      await clickNoAutoHideMenu();
      dump(`Clicked the menu button\n`);

      const consoleWrapper = console.hud.ui.wrapper;
      consoleWrapper.dispatchEvaluateExpression("myWebExtensionShowPopup()");

      await Promise.all([
        // Wait the initial frame update (which list the background page).
        waitForFrameListUpdate,
        // Wait the new frame update (once the extension popup has been opened).
        popupFramePromise,
      ]);

      dump(`Clicking the frame list button\n`);
      const btn = toolbox.doc.getElementById("command-button-frames");
      btn.click();

      const menuList = toolbox.doc.getElementById("toolbox-frame-menu");
      const frames = Array.from(menuList.querySelectorAll(".command"));

      if (frames.length != 2) {
        throw Error(`Number of frames found is wrong: ${frames.length} != 2`);
      }

      const popupFrameBtn = frames
        .filter(frame => {
          return frame
            .querySelector(".label")
            .textContent.endsWith("popup.html");
        })
        .pop();

      if (!popupFrameBtn) {
        throw Error("Extension Popup frame not found in the listed frames");
      }

      const waitForNavigated = toolbox.target.once("navigate");
      popupFrameBtn.click();
      await waitForNavigated;
      consoleWrapper.dispatchEvaluateExpression(
        "myWebExtensionPopupAddonFunction()"
      );

      info("Wait for all pending requests to settle on the DebuggerClient");
      await toolbox.target.client.waitForRequestsToSettle();

      await removeTab(devtoolsTab);
    })
    .catch(error => {
      dump("Error while running code in the browser toolbox process:\n");
      dump(error + "\n");
      dump("stack:\n" + error.stack + "\n");
    });
}
