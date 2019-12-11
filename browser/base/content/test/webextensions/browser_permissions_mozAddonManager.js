"use strict";

const INSTALL_PAGE = `${BASE}/file_install_extensions.html`;

async function installMozAM(filename) {
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, INSTALL_PAGE);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  await ContentTask.spawn(
    gBrowser.selectedBrowser,
    `${BASE}/${filename}`,
    async function(url) {
      await content.wrappedJSObject.installMozAM(url);
    }
  );
}

add_task(() => testInstallMethod(installMozAM, "installAmo"));
