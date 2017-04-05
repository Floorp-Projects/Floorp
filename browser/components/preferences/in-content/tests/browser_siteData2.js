"use strict";

const REMOVE_DIALOG_URL = "chrome://browser/content/preferences/siteDataRemoveSelected.xul";

// Test selecting and removing all sites one by one
add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  let fakeOrigins = [
    "https://news.foo.com/",
    "https://mails.bar.com/",
    "https://videos.xyz.com/",
    "https://books.foo.com/",
    "https://account.bar.com/",
    "https://shopping.xyz.com/"
  ];
  fakeOrigins.forEach(origin => addPersistentStoragePerm(origin));

  let updatePromise = promiseSiteDataManagerSitesUpdated();
  yield openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  yield updatePromise;
  yield openSiteDataSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = null;
  let saveBtn = null;
  let cancelBtn = null;
  let settingsDialogClosePromise = null;

  // Test the initial state
  assertAllSitesListed();

  // Test the "Cancel" button
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  removeAllSitesOneByOne();
  assertAllSitesNotListed();
  cancelBtn.doCommand();
  yield settingsDialogClosePromise;
  yield openSiteDataSettingsDialog();
  assertAllSitesListed();

  // Test the "Save Changes" button but cancelling save
  let cancelPromise = promiseAlertDialogOpen("cancel");
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeAllSitesOneByOne();
  assertAllSitesNotListed();
  saveBtn.doCommand();
  yield cancelPromise;
  yield settingsDialogClosePromise;
  yield openSiteDataSettingsDialog();
  assertAllSitesListed();

  // Test the "Save Changes" button and accepting save
  let acceptPromise = promiseAlertDialogOpen("accept");
  settingsDialogClosePromise = promiseSettingsDialogClose();
  updatePromise = promiseSiteDataManagerSitesUpdated();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeAllSitesOneByOne();
  assertAllSitesNotListed();
  saveBtn.doCommand();
  yield acceptPromise;
  yield settingsDialogClosePromise;
  yield updatePromise;
  yield openSiteDataSettingsDialog();
  assertAllSitesNotListed();

  // Always clean up the fake origins
  fakeOrigins.forEach(origin => removePersistentStoragePerm(origin));
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function removeAllSitesOneByOne() {
    frameDoc = doc.getElementById("dialogFrame").contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let sitesList = frameDoc.getElementById("sitesList");
    let sites = sitesList.getElementsByTagName("richlistitem");
    for (let i = sites.length - 1; i >= 0; --i) {
      sites[i].click();
      removeBtn.doCommand();
    }
  }

  function assertAllSitesListed() {
    frameDoc = doc.getElementById("dialogFrame").contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let removeAllBtn = frameDoc.getElementById("removeAll");
    let sitesList = frameDoc.getElementById("sitesList");
    let sites = sitesList.getElementsByTagName("richlistitem");
    is(sites.length, fakeOrigins.length, "Should list all sites");
    is(removeBtn.disabled, false, "Should enable the removeSelected button");
    is(removeAllBtn.disabled, false, "Should enable the removeAllBtn button");
  }

  function assertAllSitesNotListed() {
    frameDoc = doc.getElementById("dialogFrame").contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let removeAllBtn = frameDoc.getElementById("removeAll");
    let sitesList = frameDoc.getElementById("sitesList");
    let sites = sitesList.getElementsByTagName("richlistitem");
    is(sites.length, 0, "Should not list all sites");
    is(removeBtn.disabled, true, "Should disable the removeSelected button");
    is(removeAllBtn.disabled, true, "Should disable the removeAllBtn button");
  }
});

// Test selecting and removing partial sites
add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  let fakeOrigins = [
    "https://news.foo.com/",
    "https://mails.bar.com/",
    "https://videos.xyz.com/",
    "https://books.foo.com/",
    "https://account.bar.com/",
    "https://shopping.xyz.com/"
  ];
  fakeOrigins.forEach(origin => addPersistentStoragePerm(origin));

  let updatePromise = promiseSiteDataManagerSitesUpdated();
  yield openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  yield updatePromise;
  yield openSiteDataSettingsDialog();

  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = null;
  let saveBtn = null;
  let cancelBtn = null;
  let removeDialogOpenPromise = null;
  let settingsDialogClosePromise = null;

  // Test the initial state
  assertSitesListed(doc, fakeOrigins);

  // Test the "Cancel" button
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  removeSelectedSite(fakeOrigins.slice(0, 4));
  assertSitesListed(doc, fakeOrigins.slice(4));
  cancelBtn.doCommand();
  yield settingsDialogClosePromise;
  yield openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeOrigins);

  // Test the "Save Changes" button but canceling save
  removeDialogOpenPromise = promiseWindowDialogOpen("cancel", REMOVE_DIALOG_URL);
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeSelectedSite(fakeOrigins.slice(0, 4));
  assertSitesListed(doc, fakeOrigins.slice(4));
  saveBtn.doCommand();
  yield removeDialogOpenPromise;
  yield settingsDialogClosePromise;
  yield openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeOrigins);

  // Test the "Save Changes" button and accepting save
  removeDialogOpenPromise = promiseWindowDialogOpen("accept", REMOVE_DIALOG_URL);
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeSelectedSite(fakeOrigins.slice(0, 4));
  assertSitesListed(doc, fakeOrigins.slice(4));
  saveBtn.doCommand();
  yield removeDialogOpenPromise;
  yield settingsDialogClosePromise;
  yield openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeOrigins.slice(4));

  // Always clean up the fake origins
  fakeOrigins.forEach(origin => removePersistentStoragePerm(origin));
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function removeSelectedSite(origins) {
    frameDoc = doc.getElementById("dialogFrame").contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let sitesList = frameDoc.getElementById("sitesList");
    origins.forEach(origin => {
      let site = sitesList.querySelector(`richlistitem[data-origin="${origin}"]`);
      if (site) {
        site.click();
        removeBtn.doCommand();
      } else {
        ok(false, `Should not select and remove inexisted site of ${origin}`);
      }
    });
  }
});

add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  let fakeOrigins = [
    "https://news.foo.com/",
    "https://books.foo.com/",
    "https://mails.bar.com/",
    "https://account.bar.com/",
    "https://videos.xyz.com/",
    "https://shopping.xyz.com/"
  ];
  fakeOrigins.forEach(origin => addPersistentStoragePerm(origin));

  let updatePromise = promiseSiteDataManagerSitesUpdated();
  yield openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  yield updatePromise;
  yield openSiteDataSettingsDialog();

  // Search "foo" to only list foo.com sites
  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = doc.getElementById("dialogFrame").contentDocument;
  let searchBox = frameDoc.getElementById("searchBox");
  searchBox.value = "foo";
  searchBox.doCommand();
  assertSitesListed(doc, fakeOrigins.slice(0, 2));

  // Test only removing all visible sites listed
  updatePromise = promiseSiteDataManagerSitesUpdated();
  let acceptRemovePromise = promiseWindowDialogOpen("accept", REMOVE_DIALOG_URL);
  let settingsDialogClosePromise = promiseSettingsDialogClose();
  let removeAllBtn = frameDoc.getElementById("removeAll");
  let saveBtn = frameDoc.getElementById("save");
  removeAllBtn.doCommand();
  saveBtn.doCommand();
  yield acceptRemovePromise;
  yield settingsDialogClosePromise;
  yield updatePromise;
  yield openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeOrigins.slice(2));

  // Always clean up the fake origins
  fakeOrigins.forEach(origin => removePersistentStoragePerm(origin));
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

function promiseSettingsDialogClose() {
  return new Promise(resolve => {
    let doc = gBrowser.selectedBrowser.contentDocument;
    let dialogOverlay = doc.getElementById("dialogOverlay");
    let win = content.gSubDialog._frame.contentWindow;
    win.addEventListener("unload", function unload() {
      if (win.document.documentURI === "chrome://browser/content/preferences/siteDataSettings.xul") {
        isnot(dialogOverlay.style.visibility, "visible", "The Settings dialog should be hidden");
        resolve();
      }
    }, { once: true });
  });
}

function removePersistentStoragePerm(origin) {
  let uri = NetUtil.newURI(origin);
  let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
  Services.perms.removeFromPrincipal(principal, "persistent-storage");
}
