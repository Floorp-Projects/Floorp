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
  MockFilePicker.init(window.browsingContext);
  MockFilePicker.setFiles([file]);
  MockFilePicker.afterOpenCallback = MockFilePicker.cleanup;

  let { document } = await BrowserAddonUI.openAddonsMgr(
    "addons://list/extension"
  );

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

  await SpecialPowers.pushPrefEnv({
    set: [
      // This test asserts that the extension icon is in the install dialog
      // and so it requires the signature checks to be enabled (otherwise the
      // extension icon is expected to be replaced by a warning icon) and the
      // two test extension used by this test (browser_webext_nopermissions.xpi
      // and browser_webext_permissions.xpi) are signed using AMO stage signatures.
      ["xpinstall.signatures.dev-root", true],
    ],
  });

  // Install the add-ons.
  await testInstallMethod(installFile, "installLocal");

  await SpecialPowers.popPrefEnv();

  // Check we got an installId.
  ok(
    firstInstallId != null && !isNaN(firstInstallId),
    "There was an installId found"
  );
});
