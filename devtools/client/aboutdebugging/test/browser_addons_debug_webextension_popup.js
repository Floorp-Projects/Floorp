/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

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
  const addonFile = ExtensionTestCommon.generateXPI({
    background: function() {
      const {browser} = this;
      window.myWebExtensionShowPopup = function() {
        browser.test.sendMessage("readyForOpenPopup");
      };
    },
    manifest: {
      name: ADDON_NAME,
      applications: {
        gecko: {id: ADDON_ID},
      },
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
        const {browser} = this;
        window.myWebExtensionPopupAddonFunction = function() {
          browser.test.sendMessage("popupPageFunctionCalled",
                                   browser.runtime.getManifest());
        };
      },
    },
  });
  registerCleanupFunction(() => addonFile.remove(false));

  let onReadyForOpenPopup;
  let onPopupCustomMessage;

  is(Services.prefs.getBoolPref("ui.popup.disable_autohide"), false,
     "disable_autohide shoult be initially false");

  Management.on("startup", function listener(event, extension) {
    if (extension.name != ADDON_NAME) {
      return;
    }

    Management.off("startup", listener);

    function waitForExtensionTestMessage(expectedMessage) {
      return new Promise(done => {
        extension.on("test-message", function testLogListener(evt, ...args) {
          const [message ] = args;

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
  } = await setupTestAboutDebuggingWebExtension(ADDON_NAME, addonFile);

  const onToolboxReady = gDevTools.once("toolbox-ready");
  const onToolboxClose = gDevTools.once("toolbox-destroyed");
  debugBtn.click();
  const toolbox = await onToolboxReady;
  testScript(toolbox);

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
  info("Addon toolbox closed");

  is(Services.prefs.getBoolPref("ui.popup.disable_autohide"), false,
     "disable_autohide should be reset to false when the toolbox is closed");

  await uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});
  await closeAboutDebugging(tab);
});

const testScript = function(toolbox) {
  let jsterm;
  const popupFramePromise = new Promise(resolve => {
    const listener = data => {
      if (data.frames.some(({url}) => url && url.endsWith("popup.html"))) {
        toolbox.target.off("frame-update", listener);
        resolve();
      }
    };
    toolbox.target.on("frame-update", listener);
  });

  const waitForFrameListUpdate = toolbox.target.once("frame-update");

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
           btn.click();

           const menuList = toolbox.doc.getElementById("toolbox-frame-menu");
           const frames = Array.from(menuList.querySelectorAll(".command"));

           if (frames.length != 2) {
             throw Error(`Number of frames found is wrong: ${frames.length} != 2`);
           }

           const popupFrameBtn = frames.filter((frame) => {
             return frame.querySelector(".label").textContent.endsWith("popup.html");
           }).pop();

           if (!popupFrameBtn) {
             throw Error("Extension Popup frame not found in the listed frames");
           }

           const waitForNavigated = toolbox.target.once("navigate");
           popupFrameBtn.click();
           // Clicking the menu item may do highlighting.
           await waitUntil(() => toolbox.highlighter);
           await Promise.race([toolbox.highlighter.once("node-highlight"), wait(1000)]);
           await waitForNavigated;

           await jsterm.execute("myWebExtensionPopupAddonFunction()");

           await toolbox.destroy();
         })
         .catch((error) => {
           dump("Error while running code in the browser toolbox process:\n");
           dump(error + "\n");
           dump("stack:\n" + error.stack + "\n");
         });
};
