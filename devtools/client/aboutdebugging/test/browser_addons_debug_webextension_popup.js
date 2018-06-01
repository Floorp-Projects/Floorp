/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = scopedCuImport("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";
const ADDON_MANIFEST_PATH = "addons/test-devtools-webextension/manifest.json";

const {
  BrowserToolboxProcess
} = ChromeUtils.import("resource://devtools/client/framework/ToolboxProcess.jsm", {});

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

/**
 * Returns the widget id for an extension with the passed id.
 */
function makeWidgetId(id) {
  id = id.toLowerCase();
  return id.replace(/[^a-z0-9_-]/g, "_");
}

add_task(async function testWebExtensionsToolboxSwitchToPopup() {
  let onReadyForOpenPopup;
  let onPopupCustomMessage;

  Management.on("startup", function listener(event, extension) {
    if (extension.name != ADDON_NAME) {
      return;
    }

    Management.off("startup", listener);

    function waitForExtensionTestMessage(expectedMessage) {
      return new Promise(done => {
        extension.on("test-message", function testLogListener(evt, ...args) {
          const [message, ] = args;

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
    onPopupCustomMessage = waitForExtensionTestMessage("popupPageFunctionCalled");
  });

  const {
    tab, document, debugBtn,
  } = await setupTestAboutDebuggingWebExtension(ADDON_NAME, ADDON_MANIFEST_PATH);

  // Be careful, this JS function is going to be executed in the addon toolbox,
  // which lives in another process. So do not try to use any scope variable!
  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);

  const testScript = function() {
    /* eslint-disable no-undef */

    let jsterm;
    let popupFramePromise;

    toolbox.selectTool("webconsole")
      .then(async (console) => {
        const clickNoAutoHideMenu = () => {
          return new Promise(resolve => {
            toolbox.doc.getElementById("toolbox-meatball-menu-button").click();
            toolbox.doc.addEventListener("popupshown", () => {
              const menuItem =
                toolbox.doc.getElementById("toolbox-meatball-menu-noautohide");
              menuItem.click();
              resolve();
            }, { once: true });
          });
        };

        dump(`Clicking the menu button\n`);
        await clickNoAutoHideMenu();
        dump(`Clicked the menu button\n`);

        popupFramePromise = new Promise(resolve => {
          const listener = data => {
            if (data.frames.some(({url}) => url && url.endsWith("popup.html"))) {
              toolbox.target.off("frame-update", listener);
              resolve();
            }
          };
          toolbox.target.on("frame-update", listener);
        });

        const waitForFrameListUpdate = toolbox.target.once("frame-update");

        jsterm = console.hud.jsterm;
        jsterm.execute("myWebExtensionShowPopup()");

        await Promise.all([
          // Wait the initial frame update (which list the background page).
          waitForFrameListUpdate,
          // Wait the new frame update (once the extension popup has been opened).
          popupFramePromise,
        ]);

        dump(`Clicking the frame list button\n`);
        const btn = toolbox.doc.getElementById("command-button-frames");
        const frameMenu = await toolbox.showFramesMenu({target: btn});
        dump(`Clicked the frame list button\n`);

        await frameMenu.once("open");

        const frames = frameMenu.items;

        if (frames.length != 2) {
          throw Error(`Number of frames found is wrong: ${frames.length} != 2`);
        }

        const popupFrameBtn = frames.filter((frame) => {
          return frame.label.endsWith("popup.html");
        }).pop();

        if (!popupFrameBtn) {
          throw Error("Extension Popup frame not found in the listed frames");
        }

        const waitForNavigated = toolbox.target.once("navigate");

        popupFrameBtn.click();

        await waitForNavigated;

        await jsterm.execute("myWebExtensionPopupAddonFunction()");

        await toolbox.destroy();
      })
      .catch((error) => {
        dump("Error while running code in the browser toolbox process:\n");
        dump(error + "\n");
        dump("stack:\n" + error.stack + "\n");
      });
    /* eslint-enable no-undef */
  };
  env.set("MOZ_TOOLBOX_TEST_SCRIPT", "new " + testScript);
  registerCleanupFunction(() => {
    env.set("MOZ_TOOLBOX_TEST_SCRIPT", "");
  });

  const onToolboxClose = BrowserToolboxProcess.once("close");

  debugBtn.click();

  await onReadyForOpenPopup;

  const browserActionId = makeWidgetId(ADDON_ID) + "-browser-action";
  const browserActionEl = window.document.getElementById(browserActionId);

  ok(browserActionEl, "Got the browserAction button from the browser UI");
  browserActionEl.click();
  info("Clicked on the browserAction button");

  const args = await onPopupCustomMessage;
  ok(true, "Received console message from the popup page function as expected");
  is(args[0], "popupPageFunctionCalled", "Got the expected console message");
  is(args[1] && args[1].name, ADDON_NAME,
     "Got the expected manifest from WebExtension API");

  await onToolboxClose;

  ok(true, "Addon toolbox closed");

  await uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});
  await closeAboutDebugging(tab);
});
