"use strict";

async function installFile(filename) {
  const ChromeRegistry = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
    Ci.nsIChromeRegistry
  );
  let chromeUrl = Services.io.newURI(gTestPath);
  let fileUrl = ChromeRegistry.convertChromeURL(chromeUrl);
  let file = fileUrl.QueryInterface(Ci.nsIFileURL).file;
  file.leafName = filename;

  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  MockFilePicker.setFiles([file]);
  MockFilePicker.afterOpenCallback = MockFilePicker.cleanup;

  let { document } = await BrowserOpenAddonsMgr("addons://list/extension");

  // Do the install...
  await waitAboutAddonsViewLoaded(document);
  let installButton = document.querySelector('[action="install-from-file"]');
  installButton.click();
}

add_task(async function test_install_extension_from_local_file() {
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
  ok(
    firstInstallId != null && !isNaN(firstInstallId),
    "There was an installId found"
  );
});
