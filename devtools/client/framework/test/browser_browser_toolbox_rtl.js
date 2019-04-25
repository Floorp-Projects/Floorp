/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

// On debug test slave, it takes about 50s to run the test.
requestLongerTimeout(4);

// Test that DevTools panels are rendered in "rtl" (right-to-left) in the Browser Toolbox.
add_task(async function() {
  await setupPreferencesForBrowserToolbox();

  // Wait for a notification sent by a script evaluated in the webconsole
  // of the browser toolbox.
  const onCustomMessage = new Promise(resolve => {
    Services.obs.addObserver(function listener(target, aTop, data) {
      Services.obs.removeObserver(listener, "browser-toolbox-inspector-dir");
      resolve(data);
    }, "browser-toolbox-inspector-dir");
  });

  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  env.set("MOZ_TOOLBOX_TEST_SCRIPT", "new function() {(" + testScript + ")();}");
  registerCleanupFunction(() => env.set("MOZ_TOOLBOX_TEST_SCRIPT", ""));

  const { BrowserToolboxProcess } = ChromeUtils.import("resource://devtools/client/framework/ToolboxProcess.jsm");

  let closePromise;
  await new Promise(onRun => {
    closePromise = new Promise(onClose => {
      info("Opening the browser toolbox");
      BrowserToolboxProcess.init(onClose, onRun);
    });
  });
  info("Browser toolbox started");

  const inspectorPanelDirection = await onCustomMessage;
  info("Received the custom message");
  is(inspectorPanelDirection, "rtl", "Inspector panel has the expected direction");

  await closePromise;
  info("Browser toolbox process just closed");
  is(BrowserToolboxProcess.getBrowserToolboxSessionState(), false, "No session state after closing");
});

// Be careful, this JS function is going to be executed in the addon toolbox,
// which lives in another process. So do not try to use any scope variable!
async function testScript() {
  /* global toolbox */
  // Get the current direction of the inspector panel.
  const inspector = await toolbox.selectTool("inspector");
  const dir = inspector.panelDoc.dir;

  // Switch to the webconsole to send the result to the main test.
  const webconsole = await toolbox.selectTool("webconsole");
  const js =
    `Services.obs.notifyObservers(null, "browser-toolbox-inspector-dir", "${dir}");`;
  await webconsole.hud.jsterm.execute(js);

  // Destroy the toolbox.
  await toolbox.destroy();
}
