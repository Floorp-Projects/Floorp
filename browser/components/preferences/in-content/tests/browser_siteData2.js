"use strict";
const { SiteDataManager } = Cu.import("resource:///modules/SiteDataManager.jsm", {});

const REMOVE_DIALOG_URL = "chrome://browser/content/preferences/siteDataRemoveSelected.xul";
const { DownloadUtils } = Cu.import("resource://gre/modules/DownloadUtils.jsm", {});

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

// Test selecting and removing all sites one by one
add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  mockSiteDataManager.register(SiteDataManager);
  mockSiteDataManager.fakeSites = [
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://shopping.xyz.com"),
      persisted: false
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://cinema.bar.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://email.bar.com"),
      persisted: false
    },
  ];
  let fakeHosts = mockSiteDataManager.fakeSites.map(site => site.principal.URI.host);

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
  assertSitesListed(doc, fakeHosts);

  // Test the "Cancel" button
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  removeAllSitesOneByOne();
  assertAllSitesNotListed();
  cancelBtn.doCommand();
  yield settingsDialogClosePromise;
  yield openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeHosts);

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
  assertSitesListed(doc, fakeHosts);

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

  mockSiteDataManager.unregister();
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
  mockSiteDataManager.register(SiteDataManager);
  mockSiteDataManager.fakeSites = [
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://shopping.xyz.com"),
      persisted: false
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://cinema.bar.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://email.bar.com"),
      persisted: false
    },
  ];
  let fakeHosts = mockSiteDataManager.fakeSites.map(site => site.principal.URI.host);

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
  assertSitesListed(doc, fakeHosts);

  // Test the "Cancel" button
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  removeSelectedSite(fakeHosts.slice(0, 2));
  assertSitesListed(doc, fakeHosts.slice(2));
  cancelBtn.doCommand();
  yield settingsDialogClosePromise;
  yield openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeHosts);

  // Test the "Save Changes" button but canceling save
  removeDialogOpenPromise = promiseWindowDialogOpen("cancel", REMOVE_DIALOG_URL);
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeSelectedSite(fakeHosts.slice(0, 2));
  assertSitesListed(doc, fakeHosts.slice(2));
  saveBtn.doCommand();
  yield removeDialogOpenPromise;
  yield settingsDialogClosePromise;
  yield openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeHosts);

  // Test the "Save Changes" button and accepting save
  removeDialogOpenPromise = promiseWindowDialogOpen("accept", REMOVE_DIALOG_URL);
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = doc.getElementById("dialogFrame").contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeSelectedSite(fakeHosts.slice(0, 2));
  assertSitesListed(doc, fakeHosts.slice(2));
  saveBtn.doCommand();
  yield removeDialogOpenPromise;
  yield settingsDialogClosePromise;
  yield openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeHosts.slice(2));

  mockSiteDataManager.unregister();
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function removeSelectedSite(hosts) {
    frameDoc = doc.getElementById("dialogFrame").contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let sitesList = frameDoc.getElementById("sitesList");
    hosts.forEach(host => {
      let site = sitesList.querySelector(`richlistitem[host="${host}"]`);
      if (site) {
        site.click();
        removeBtn.doCommand();
      } else {
        ok(false, `Should not select and remove inexistent site of ${host}`);
      }
    });
  }
});

// Test searching and then removing only visible sites
add_task(function* () {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  mockSiteDataManager.register(SiteDataManager);
  mockSiteDataManager.fakeSites = [
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://shopping.xyz.com"),
      persisted: false
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://cinema.bar.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://email.bar.com"),
      persisted: false
    },
  ];
  let fakeHosts = mockSiteDataManager.fakeSites.map(site => site.principal.URI.host);

  let updatePromise = promiseSiteDataManagerSitesUpdated();
  yield openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  yield updatePromise;
  yield openSiteDataSettingsDialog();

  // Search "foo" to only list foo.com sites
  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = doc.getElementById("dialogFrame").contentDocument;
  let searchBox = frameDoc.getElementById("searchBox");
  searchBox.value = "xyz";
  searchBox.doCommand();
  assertSitesListed(doc, fakeHosts.filter(host => host.includes("xyz")));

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
  assertSitesListed(doc, fakeHosts.filter(host => !host.includes("xyz")));

  mockSiteDataManager.unregister();
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test grouping and listing sites across scheme, port and origin attributes by host
add_task(function *() {
  yield SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  const quotaUsage = 1024;
  mockSiteDataManager.register(SiteDataManager);
  mockSiteDataManager.fakeSites = [
    {
      usage: quotaUsage,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com^userContextId=1"),
      persisted: true
    },
    {
      usage: quotaUsage,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com"),
      persisted: false
    },
    {
      usage: quotaUsage,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://account.xyz.com:123"),
      persisted: false
    },
    {
      usage: quotaUsage,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://account.xyz.com"),
      persisted: false
    },
  ];

  let updatedPromise = promiseSiteDataManagerSitesUpdated();
  yield openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  yield updatedPromise;
  yield openSiteDataSettingsDialog();
  let dialogFrame = gBrowser.selectedBrowser.contentDocument.getElementById("dialogFrame");
  let frameDoc = dialogFrame.contentDocument;

  let siteItems = frameDoc.getElementsByTagName("richlistitem");
  is(siteItems.length, 1, "Should group sites across scheme, port and origin attributes");

  let expected = "account.xyz.com";
  let host = siteItems[0].getAttribute("host");
  is(host, expected, "Should group and list sites by host");

  let prefStrBundle = frameDoc.getElementById("bundlePreferences");
  expected = prefStrBundle.getFormattedString("siteUsage",
    DownloadUtils.convertByteUnits(quotaUsage * mockSiteDataManager.fakeSites.length));
  let usage = siteItems[0].getAttribute("usage");
  is(usage, expected, "Should sum up usages across scheme, port and origin attributes");

  expected = prefStrBundle.getString("persistent");
  let status = siteItems[0].getAttribute("status");
  is(status, expected, "Should mark persisted status across scheme, port and origin attributes");

  mockSiteDataManager.unregister();
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
