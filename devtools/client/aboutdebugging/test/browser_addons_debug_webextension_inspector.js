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
 * - the webextension developer toolbox has a working Inspector panel, with the
 *   background page as default target;
 */
add_task(async function testWebExtensionsToolboxInspector() {
  const addonFile = ExtensionTestCommon.generateXPI({
    background: function() {
      document.body.innerText = "Background Page Body Test Content";
    },
    manifest: {
      name: ADDON_NAME,
      applications: {
        gecko: {id: ADDON_ID},
      },
    },
  });
  registerCleanupFunction(() => addonFile.remove(false));

  const {
    tab, document, debugBtn,
  } = await setupTestAboutDebuggingWebExtension(ADDON_NAME, addonFile);

  const onToolboxReady = gDevTools.once("toolbox-ready");
  const onToolboxClose = gDevTools.once("toolbox-destroyed");
  debugBtn.click();
  const toolbox = await onToolboxReady;
  testScript(toolbox);

  await onToolboxClose;
  ok(true, "Addon toolbox closed");

  await uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});
  await closeAboutDebugging(tab);
});

const testScript = function(toolbox) {
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
};
