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
const ADDON_PATH = "addons/test-devtools-webextension/manifest.json";

const {
  BrowserToolboxProcess
} = ChromeUtils.import("resource://devtools/client/framework/ToolboxProcess.jsm", {});

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - the webextension developer toolbox has a working Inspector panel, with the
 *   background page as default target;
 */
add_task(async function testWebExtensionsToolboxInspector() {
  const {
    tab, document, debugBtn,
  } = await setupTestAboutDebuggingWebExtension(ADDON_NAME, ADDON_PATH);

  // Be careful, this JS function is going to be executed in the addon toolbox,
  // which lives in another process. So do not try to use any scope variable!
  const env = Cc["@mozilla.org/process/environment;1"]
        .getService(Ci.nsIEnvironment);
  const testScript = function() {
    /* eslint-disable no-undef */
    toolbox.selectTool("inspector")
      .then(inspector => {
        return inspector.walker.querySelector(inspector.walker.rootNode, "body");
      })
      .then((nodeActor) => {
        if (!nodeActor) {
          throw new Error("nodeActor not found");
        }

        dump("Got a nodeActor\n");

        if (!(nodeActor.inlineTextChild)) {
          throw new Error("inlineTextChild not found");
        }

        dump("Got a nodeActor with an inline text child\n");

        const expectedValue = "Background Page Body Test Content";
        const actualValue = nodeActor.inlineTextChild._form.nodeValue;

        if (String(actualValue).trim() !== String(expectedValue).trim()) {
          throw new Error(
            `mismatched inlineTextchild value: "${actualValue}" !== "${expectedValue}"`
          );
        }

        dump("Got the expected inline text content in the selected node\n");
        return Promise.resolve();
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

  const onToolboxClose = BrowserToolboxProcess.once("close");
  debugBtn.click();
  await onToolboxClose;

  ok(true, "Addon toolbox closed");

  await uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});
  await closeAboutDebugging(tab);
});
