/* eslint-disable mozilla/no-arbitrary-setTimeout */
let NS_OSX_PICTURE_DOCUMENTS_DIR = "Pct";
let NS_MAC_USER_LIB_DIR = "ULibDir";


function onPageLoad() {
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIDirectoryServiceProvider);

  let desktopBackgroundDb = dirSvc.getFile(NS_MAC_USER_LIB_DIR, {});
  desktopBackgroundDb.append("Application Support");
  desktopBackgroundDb.append("Dock");
  let desktopBackgroundDbBackup = desktopBackgroundDb.clone();
  desktopBackgroundDb.append("desktoppicture.db");
  desktopBackgroundDbBackup.append("desktoppicture.db.backup");

  ok(desktopBackgroundDb.exists(),
     "Desktop background database must exist for test to run.");

  if (desktopBackgroundDbBackup.exists()) {
    desktopBackgroundDbBackup.remove(false);
  }

  desktopBackgroundDb.copyTo(null, desktopBackgroundDbBackup.leafName);

  let wpFile = dirSvc.getFile(NS_OSX_PICTURE_DOCUMENTS_DIR, {});
  wpFile.append("logo.png");
  if (wpFile.exists()) {
    wpFile.remove(false);
  }

  let shell = Cc["@mozilla.org/browser/shell-service;1"].
              getService(Ci.nsIShellService);

  // eslint-disable-next-line mozilla/no-cpows-in-tests
  let image = gBrowser.contentDocumentAsCPOW.images[0];
  shell.setDesktopBackground(image, 0, "logo.png");

  setTimeout(function() {
    ok(wpFile.exists(), "Desktop background was written to disk.");

    desktopBackgroundDbBackup.moveTo(null, desktopBackgroundDb.leafName);
    wpFile.remove(false);

    // Restart Dock to reload previous background image.
    let killall = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    killall.initWithPath("/usr/bin/killall");
    let dockArg = ["Dock"];
    let process =
      Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
    process.init(killall);
    process.run(true, dockArg, 1);

    gBrowser.removeCurrentTab();
    finish();
  }, 1000);
}

function test() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(onPageLoad, false, "about:logo");
  gBrowser.loadURI("about:logo");

  waitForExplicitFinish();
}
