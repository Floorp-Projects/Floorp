"use strict";

const INSTALL_PAGE = `${BASE}/file_install_extensions.html`;

async function installTrigger(filename) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.InstallTrigger.enabled", true],
      ["extensions.InstallTriggerImpl.enabled", true],
      // Relax the user input requirements while running this test.
      ["xpinstall.userActivation.required", false],
      // This test asserts that the extension icon is in the install dialog
      // and so it requires the signature checks to be enabled (otherwise the
      // extension icon is expected to be replaced by a warning icon) and the
      // two test extension used by this test (browser_webext_nopermissions.xpi
      // and browser_webext_permissions.xpi) are signed using AMO stage signatures.
      ["xpinstall.signatures.dev-root", true],
    ],
  });
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    INSTALL_PAGE
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [`${BASE}/${filename}`],
    async function (url) {
      content.wrappedJSObject.installTrigger(url);
    }
  );
}

add_task(() => testInstallMethod(installTrigger, "installAmo"));
