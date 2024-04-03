"use strict";

const INSTALL_PAGE = `${BASE}/file_install_extensions.html`;

async function installMozAM(filename) {
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    INSTALL_PAGE
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

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

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [`${BASE}/${filename}`],
    async function (url) {
      await content.wrappedJSObject.installMozAM(url);
    }
  );

  await SpecialPowers.popPrefEnv();
}

add_task(() => testInstallMethod(installMozAM, "installAmo"));
