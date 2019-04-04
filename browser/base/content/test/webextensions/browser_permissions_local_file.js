"use strict";

async function installFile(filename) {
  const ChromeRegistry = Cc["@mozilla.org/chrome/chrome-registry;1"]
                                     .getService(Ci.nsIChromeRegistry);
  let chromeUrl = Services.io.newURI(gTestPath);
  let fileUrl = ChromeRegistry.convertChromeURL(chromeUrl);
  let file = fileUrl.QueryInterface(Ci.nsIFileURL).file;
  file.leafName = filename;

  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  MockFilePicker.setFiles([file]);
  MockFilePicker.afterOpenCallback = MockFilePicker.cleanup;

  await BrowserOpenAddonsMgr("addons://list/extension");
  let contentWin = gBrowser.selectedTab.linkedBrowser.contentWindow;

  // Do the install...
  contentWin.gViewController.doCommand("cmd_installFromFile");
}

add_task(async function test_install_extension_from_local_file() {
  // Clear any telemetry data that might be from a separate test.
  Services.telemetry.clearEvents();

  // Listen for the first installId so we can check it later.
  let firstInstallId = null;
  AddonManager.addInstallListener({
    onNewInstall(install) {
      firstInstallId = install.installId;
      AddonManager.removeInstallListener(this);
    },
  });

  // Install the add-ons.
  await testInstallMethod(installFile, "installLocal");

  // Check we got an installId.
  ok(firstInstallId != null && !isNaN(firstInstallId), "There was an installId found");

  // Check the telemetry.
  let snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true);

  // Make sure we got some data.
  ok(snapshot.parent && snapshot.parent.length > 0, "Got parent telemetry events in the snapshot");

  // Only look at the related events after stripping the timestamp and category.
  let relatedEvents = snapshot.parent
    .filter(([timestamp, category, method, object]) =>
      category == "addonsManager" && method == "action" && object == "aboutAddons")
    .map(relatedEvent => relatedEvent.slice(4, 6));

  // testInstallMethod installs the extension three times.
  Assert.deepEqual(relatedEvents, [
    [firstInstallId.toString(), {action: "installFromFile", view: "list"}],
    [(++firstInstallId).toString(), {action: "installFromFile", view: "list"}],
    [(++firstInstallId).toString(), {action: "installFromFile", view: "list"}],
  ], "The telemetry is recorded correctly");
});
