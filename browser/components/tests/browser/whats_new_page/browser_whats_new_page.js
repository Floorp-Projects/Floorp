/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function whats_new_page() {
  // The test harness will use the current tab and remove the tab's history.
  // Since the page that is tested is opened prior to the test harness taking
  // over the current tab the active-update.xml specifies two pages to open by
  // having 'https://example.com/|https://example.com/' for the value of openURL
  // and then uses the first tab for the test.
  gBrowser.selectedTab = gBrowser.tabs[0];
  // The test harness also changes the page to about:blank so go back to the
  // page that was originally opened.
  gBrowser.goBack();
  // Wait for the page to go back to the original page.
  await TestUtils.waitForCondition(
    () =>
      gBrowser.selectedBrowser &&
      gBrowser.selectedBrowser.currentURI &&
      gBrowser.selectedBrowser.currentURI.spec == "https://example.com/",
    "Waiting for the expected page to reopen"
  );
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    "https://example.com/",
    "The what's new page's url should equal https://example.com/"
  );
  gBrowser.removeTab(gBrowser.selectedTab);

  let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
    Ci.nsIUpdateManager
  );
  await TestUtils.waitForCondition(
    () => !um.activeUpdate,
    "Waiting for the active update to be removed"
  );
  ok(!um.activeUpdate, "There should not be an active update");
  await TestUtils.waitForCondition(
    () => !!um.getUpdateAt(0),
    "Waiting for the active update to be moved to the update history"
  );
  ok(!!um.getUpdateAt(0), "There should be an update in the update history");

  // Leave no trace. Since this test modifies its support files put them back in
  // their original state.
  let alternatePath = Services.prefs.getCharPref("app.update.altUpdateDirPath");
  let testRoot = Services.prefs.getCharPref("mochitest.testRoot");
  let relativePath = alternatePath.substring("<test-root>".length);
  if (AppConstants.platform == "win") {
    relativePath = relativePath.replace(/\//g, "\\");
  }
  alternatePath = testRoot + relativePath;
  let updateDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  updateDir.initWithPath(alternatePath);

  let activeUpdateFile = updateDir.clone();
  activeUpdateFile.append("active-update.xml");
  await TestUtils.waitForCondition(
    () => !activeUpdateFile.exists(),
    "Waiting until the active-update.xml file does not exist"
  );

  let updatesFile = updateDir.clone();
  updatesFile.append("updates.xml");
  await TestUtils.waitForCondition(
    () => updatesFile.exists(),
    "Waiting until the updates.xml file exists"
  );

  let fos = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  let flags =
    FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_TRUNCATE;

  let stateSucceeded = "succeeded\n";
  let updateStatusFile = updateDir.clone();
  updateStatusFile.append("updates");
  updateStatusFile.append("0");
  updateStatusFile.append("update.status");
  updateStatusFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  fos.init(updateStatusFile, flags, FileUtils.PERMS_FILE, 0);
  fos.write(stateSucceeded, stateSucceeded.length);
  fos.close();

  let xmlContents =
    '<?xml version="1.0"?><updates xmlns="http://www.mozilla.org/2005/' +
    'app-update"><update xmlns="http://www.mozilla.org/2005/app-update" ' +
    'appVersion="99999999.0" buildID="20990101111111" channel="test" ' +
    'detailsURL="https://127.0.0.1/" displayVersion="1.0" installDate="' +
    '1555716429454" isCompleteUpdate="true" name="What\'s New Page Test" ' +
    'previousAppVersion="60.0" serviceURL="https://127.0.0.1/update.xml" ' +
    'type="minor" platformVersion="99999999.0" actions="showURL" ' +
    'openURL="https://example.com/|https://example.com/"><patch size="1" ' +
    'type="complete" URL="https://127.0.0.1/complete.mar" ' +
    'selected="true" state="pending"/></update></updates>\n';
  activeUpdateFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  fos.init(activeUpdateFile, flags, FileUtils.PERMS_FILE, 0);
  fos.write(xmlContents, xmlContents.length);
  fos.close();

  updatesFile.remove(false);
  Cc["@mozilla.org/updates/update-manager;1"]
    .getService(Ci.nsIUpdateManager)
    .QueryInterface(Ci.nsIObserver)
    .observe(null, "um-reload-update-data", "");
});
