/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = scopedCuImport("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

const ADDON_NOBG_ID = "test-devtools-webextension-nobg@mozilla.org";
const ADDON_NOBG_NAME = "test-devtools-webextension-nobg";

const {
  BrowserToolboxProcess,
} = ChromeUtils.import("resource://devtools/client/framework/ToolboxProcess.jsm");

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - the webextension developer toolbox is connected to a fallback page when the
 *   background page is not available (and in the fallback page document body contains
 *   the expected message, which warns the user that the current page is not a real
 *   webextension context);
 */
add_task(async function testWebExtensionsToolboxNoBackgroundPage() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI({
    // Do not pass any `background` script.
    id: ADDON_NOBG_ID,
    name: ADDON_NOBG_NAME,
  }, document);
  const target = findDebugTargetByText(ADDON_NOBG_NAME, document);

  info("Setup the toolbox test function as environment variable");
  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  env.set("MOZ_TOOLBOX_TEST_SCRIPT", "new " + toolboxTestScript);
  env.set("MOZ_TOOLBOX_TEST_ADDON_NOBG_NAME", ADDON_NOBG_NAME);
  registerCleanupFunction(() => {
    env.set("MOZ_TOOLBOX_TEST_SCRIPT", "");
    env.set("MOZ_TOOLBOX_TEST_ADDON_NOBG_NAME", "");
  });

  info("Click inspect to open the addon toolbox, wait for toolbox close event");
  const onToolboxClose = BrowserToolboxProcess.once("close");
  const inspectButton = target.querySelector(".js-debug-target-inspect-button");
  inspectButton.click();
  await onToolboxClose;

  // The test script will not close the toolbox and will timeout if it fails, so reaching
  // this point in the test is enough to assume the test was successful.
  ok(true, "Addon toolbox closed");

  await removeTemporaryExtension(ADDON_NOBG_NAME, document);
  await removeTab(tab);
});

// Be careful, this JS function is going to be executed in the addon toolbox,
// which lives in another process. So do not try to use any scope variable!
function toolboxTestScript() {
  /* eslint-disable no-undef */
  // This is webextension toolbox process. So we can't access mochitest framework.
  const waitUntil = async function(predicate, interval = 10) {
    if (await predicate()) {
      return true;
    }
    return new Promise(resolve => {
      toolbox.win.setTimeout(function() {
        waitUntil(predicate, interval).then(() => resolve(true));
      }, interval);
    });
  };

  // We pass the expected target name as an environment variable because this method
  // runs in a different process and cannot access the const defined in this test file.
  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  const expectedName = env.get("MOZ_TOOLBOX_TEST_ADDON_NOBG_NAME");

  const targetName = toolbox.target.name;
  const isAddonTarget = toolbox.target.isAddon;
  if (!(isAddonTarget && targetName === expectedName)) {
    dump(`Expected target name "${expectedName}", got ${targetName}`);
    throw new Error("Toolbox doesn't have the expected target");
  }

  toolbox.selectTool("inspector").then(async inspector => {
    let nodeActor;

    dump(`Wait the fallback window to be fully loaded\n`);
    await waitUntil(async () => {
      nodeActor = await inspector.walker.querySelector(inspector.walker.rootNode, "h1");
      return nodeActor && nodeActor.inlineTextChild;
    });

    dump("Got a nodeActor with an inline text child\n");
    const expectedValue = "Your addon does not have any document opened yet.";
    const actualValue = nodeActor.inlineTextChild._form.nodeValue;

    if (actualValue !== expectedValue) {
      throw new Error(
        `mismatched inlineTextchild value: "${actualValue}" !== "${expectedValue}"`
      );
    }

    dump("Got the expected inline text content in the selected node\n");

    await toolbox.destroy();
  }).catch((error) => {
    dump("Error while running code in the browser toolbox process:\n");
    dump(error + "\n");
    dump("stack:\n" + error.stack + "\n");
  });
  /* eslint-enable no-undef */
}
