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

add_task(() => testInstallMethod(installFile, "installLocal"));
