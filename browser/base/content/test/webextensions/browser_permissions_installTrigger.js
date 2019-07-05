"use strict";

const INSTALL_PAGE = `${BASE}/file_install_extensions.html`;

async function installTrigger(filename) {
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, INSTALL_PAGE);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  ContentTask.spawn(
    gBrowser.selectedBrowser,
    `${BASE}/${filename}`,
    async function(url) {
      content.wrappedJSObject.installTrigger(url);
    }
  );
}

add_task(() => testInstallMethod(installTrigger, "installAmo"));
