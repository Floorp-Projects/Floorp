/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";
const ADDON_MANIFEST_PATH = "addons/test-devtools-webextension/manifest.json";

const {
  BrowserToolboxProcess
} = Cu.import("resource://devtools/client/framework/ToolboxProcess.jsm", {});

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

add_task(function* testWebExtensionsToolboxSwitchToPopup() {
  let {
    tab, document, debugBtn,
  } = yield setupTestAboutDebuggingWebExtension(ADDON_NAME, ADDON_MANIFEST_PATH);

  let onReadyForOpenPopup = new Promise(done => {
    Services.obs.addObserver(function listener(message, topic) {
      let apiMessage = message.wrappedJSObject;
      if (!apiMessage.originAttributes ||
          apiMessage.originAttributes.addonId != ADDON_ID) {
        return;
      }

      if (apiMessage.arguments[0] == "readyForOpenPopup") {
        Services.obs.removeObserver(listener, "console-api-log-event");
        done();
      }
    }, "console-api-log-event", false);
  });

  // Be careful, this JS function is going to be executed in the addon toolbox,
  // which lives in another process. So do not try to use any scope variable!
  let env = Cc["@mozilla.org/process/environment;1"]
        .getService(Ci.nsIEnvironment);
  let testScript = function () {
    /* eslint-disable no-undef */

    let jsterm;
    let popupFramePromise;

    toolbox.selectTool("webconsole")
      .then(console => {
        dump(`Clicking the noautohide button\n`);
        toolbox.doc.getElementById("command-button-noautohide").click();
        dump(`Clicked the noautohide button\n`);

        popupFramePromise = new Promise(resolve => {
          let listener = (event, data) => {
            if (data.frames.some(({url}) => url && url.endsWith("popup.html"))) {
              toolbox.target.off("frame-update", listener);
              resolve();
            }
          };
          toolbox.target.on("frame-update", listener);
        });

        let waitForFrameListUpdate = new Promise((done) => {
          toolbox.target.once("frame-update", () => {
            done(console);
          });
        });

        jsterm = console.hud.jsterm;
        jsterm.execute("myWebExtensionShowPopup()");

        // Wait the initial frame update (which list the background page).
        return waitForFrameListUpdate;
      })
      .then((console) => {
        // Wait the new frame update (once the extension popup has been opened).
        return popupFramePromise;
      })
      .then(() => {
        dump(`Clicking the frame list button\n`);
        let btn = toolbox.doc.getElementById("command-button-frames");
        let menu = toolbox.showFramesMenu({target: btn});
        dump(`Clicked the frame list button\n`);
        return menu.once("open").then(() => {
          return menu;
        });
      })
      .then(frameMenu => {
        let frames = frameMenu.items;

        if (frames.length != 2) {
          throw Error(`Number of frames found is wrong: ${frames.length} != 2`);
        }

        let popupFrameBtn = frames.filter((frame) => {
          return frame.label.endsWith("popup.html");
        }).pop();

        if (!popupFrameBtn) {
          throw Error("Extension Popup frame not found in the listed frames");
        }

        let waitForNavigated = toolbox.target.once("navigate");

        popupFrameBtn.click();

        return waitForNavigated;
      })
      .then(() => {
        return jsterm.execute("myWebExtensionPopupAddonFunction()");
      })
      .then(() => toolbox.destroy())
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

  // Wait for a notification sent by a script evaluated the test addon via
  // the web console.
  let onPopupCustomMessage = new Promise(done => {
    Services.obs.addObserver(function listener(message, topic) {
      let apiMessage = message.wrappedJSObject;
      if (!apiMessage.originAttributes ||
          apiMessage.originAttributes.addonId != ADDON_ID) {
        return;
      }

      if (apiMessage.arguments[0] == "Popup page function called") {
        Services.obs.removeObserver(listener, "console-api-log-event");
        done(apiMessage.arguments);
      }
    }, "console-api-log-event", false);
  });

  let onToolboxClose = BrowserToolboxProcess.once("close");

  debugBtn.click();

  yield onReadyForOpenPopup;

  let browserActionId = makeWidgetId(ADDON_ID) + "-browser-action";
  let browserActionEl = window.document.getElementById(browserActionId);

  ok(browserActionEl, "Got the browserAction button from the browser UI");
  browserActionEl.click();
  info("Clicked on the browserAction button");

  let args = yield onPopupCustomMessage;
  ok(true, "Received console message from the popup page function as expected");
  is(args[0], "Popup page function called", "Got the expected console message");
  is(args[1] && args[1].name, ADDON_NAME,
     "Got the expected manifest from WebExtension API");

  yield onToolboxClose;

  ok(true, "Addon toolbox closed");

  yield uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});
  yield closeAboutDebugging(tab);
});
