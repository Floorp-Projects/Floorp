/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global browser */

// This tests the browser.experiments.urlbar.lastBrowserUpdateDate WebExtension
// Experiment API.  The parts related to the updates.xml file are adapted from
// browser_policy_override_postupdatepage.js and other similar tests.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  ProfileAge: "resource://gre/modules/ProfileAge.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "updateManager",
  "@mozilla.org/updates/update-manager;1",
  "nsIUpdateManager"
);

const DATE_MS = 1368255600000;

add_task(async function setUp() {
  let originalTimes = (await ProfileAge())._times;
  registerCleanupFunction(async () => {
    removeUpdatesFile();
    let age = await ProfileAge();
    age._times = originalTimes;
    await age.writeTimes();
  });
});

// no update history, same profile created and first-use date
add_task(async function noHistory_sameCreateAndFirstUse() {
  removeUpdatesFile();
  reloadUpdateManagerData();
  Assert.equal(updateManager.updateCount, 0);
  await updateProfileAge(DATE_MS, DATE_MS);
  await checkExtension(DATE_MS);
});

// no update history, different profile created and first-use dates
add_task(async function noHistory_differentCreateAndFirstUse() {
  removeUpdatesFile();
  reloadUpdateManagerData();
  Assert.equal(updateManager.updateCount, 0);
  await updateProfileAge(DATE_MS - 30000, DATE_MS);
  await checkExtension(DATE_MS);
});

// update history
add_task(async function history() {
  removeUpdatesFile();
  writeUpdatesFile(DATE_MS);
  reloadUpdateManagerData();
  Assert.equal(updateManager.updateCount, 1);
  await updateProfileAge(DATE_MS - 60000, DATE_MS - 30000);
  await checkExtension(DATE_MS);
});

async function checkExtension(expectedDate) {
  let ext = await loadExtension(async () => {
    let date = await browser.experiments.urlbar.lastBrowserUpdateDate();
    browser.test.sendMessage("date", date);
  });
  let date = await ext.awaitMessage("date");
  Assert.equal(date, expectedDate);
  await ext.unload();
}

function getUpdatesFile() {
  let updateRootDir = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
  let updatesFile = updateRootDir.clone();
  updatesFile.append("updates.xml");
  return updatesFile;
}

function removeUpdatesFile() {
  try {
    getUpdatesFile().remove(false);
  } catch (e) {}
}

function reloadUpdateManagerData() {
  updateManager
    .QueryInterface(Ci.nsIObserver)
    .observe(null, "um-reload-update-data", "");
}

function getUpdatesXML(installDate) {
  return `<?xml version="1.0"?>
  <updates xmlns="http://www.mozilla.org/2005/app-update">
    <update appVersion="1.0" buildID="20080811053724" channel="nightly"
            displayVersion="Version 1.0" installDate="${installDate}"
            isCompleteUpdate="true" name="Update Test 1.0" type="minor"
            detailsURL="http://example.com/" previousAppVersion="1.0"
            serviceURL="https://example.com/"
            statusText="The Update was successfully installed"
            foregroundDownload="true">
      <patch type="complete" URL="http://example.com/" size="775"
             selected="true" state="succeeded"/>
    </update>
  </updates>`;
}

function writeUpdatesFile(installDate) {
  const PERMS_FILE = 0o644;
  const MODE_WRONLY = 0x02;
  const MODE_CREATE = 0x08;
  const MODE_TRUNCATE = 0x20;
  let file = getUpdatesFile();
  if (!file.exists()) {
    file.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  }
  let fos = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  let flags = MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE;
  fos.init(file, flags, PERMS_FILE, 0);
  let xml = getUpdatesXML(installDate);
  fos.write(xml, xml.length);
  fos.close();
}

async function updateProfileAge(created, firstUse) {
  let age = await ProfileAge();
  age._times = { created, firstUse };
  await age.writeTimes();
}
