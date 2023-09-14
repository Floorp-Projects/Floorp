"use strict";

const INSTALL_PAGE = `${BASE}/file_install_extensions.html`;

async function installTrigger(filename) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.InstallTrigger.enabled", true],
      ["extensions.InstallTriggerImpl.enabled", true],
      // Relax the user input requirements while running this test.
      ["xpinstall.userActivation.required", false],
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
