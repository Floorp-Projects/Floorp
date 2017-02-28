"use strict";

const INSTALL_PAGE = `${BASE}/file_install_extensions.html`;

async function installTrigger(filename) {
  gBrowser.selectedBrowser.loadURI(INSTALL_PAGE);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  ContentTask.spawn(gBrowser.selectedBrowser, `${BASE}/${filename}`, function*(url) {
    content.wrappedJSObject.installTrigger(url);
  });
}

add_task(() => testInstallMethod(installTrigger, "installAmo"));
