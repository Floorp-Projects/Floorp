"use strict";
const { SiteDataManager } = Cu.import("resource:///modules/SiteDataManager.jsm", {});
const REMOVE_DIALOG_URL = "chrome://browser/content/preferences/siteDataRemoveSelected.xul";

/* eslint-disable mozilla/no-cpows-in-tests */

function promiseSettingsDialogClose() {
  return new Promise(resolve => {
    let win = gBrowser.selectedBrowser.contentWindow;
    let dialogOverlay = win.gSubDialog._topDialog._overlay;
    let dialogWin = win.gSubDialog._topDialog._frame.contentWindow;
    dialogWin.addEventListener("unload", function unload() {
      if (dialogWin.document.documentURI === "chrome://browser/content/preferences/siteDataSettings.xul") {
        isnot(dialogOverlay.style.visibility, "visible", "The Settings dialog should be hidden");
        resolve();
      }
    }, { once: true });
  });
}

function assertAllSitesNotListed(win) {
  let frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  let removeBtn = frameDoc.getElementById("removeSelected");
  let removeAllBtn = frameDoc.getElementById("removeAll");
  let sitesList = frameDoc.getElementById("sitesList");
  let sites = sitesList.getElementsByTagName("richlistitem");
  is(sites.length, 0, "Should not list all sites");
  is(removeBtn.disabled, true, "Should disable the removeSelected button");
  is(removeAllBtn.disabled, true, "Should disable the removeAllBtn button");
}

// Test selecting and removing all sites one by one
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
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
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();

  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = null;
  let saveBtn = null;
  let cancelBtn = null;
  let settingsDialogClosePromise = null;

  // Test the initial state
  assertSitesListed(doc, fakeHosts);

  // Test the "Cancel" button
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  removeAllSitesOneByOne();
  assertAllSitesNotListed(win);
  cancelBtn.doCommand();
  await settingsDialogClosePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeHosts);

  // Test the "Save Changes" button but cancelling save
  let cancelPromise = promiseAlertDialogOpen("cancel");
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeAllSitesOneByOne();
  assertAllSitesNotListed(win);
  saveBtn.doCommand();
  await cancelPromise;
  await settingsDialogClosePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeHosts);

  // Test the "Save Changes" button and accepting save
  let acceptPromise = promiseAlertDialogOpen("accept");
  settingsDialogClosePromise = promiseSettingsDialogClose();
  updatePromise = promiseSiteDataManagerSitesUpdated();
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeAllSitesOneByOne();
  assertAllSitesNotListed(win);
  saveBtn.doCommand();
  await acceptPromise;
  await settingsDialogClosePromise;
  await updatePromise;
  await openSiteDataSettingsDialog();
  assertAllSitesNotListed(win);

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function removeAllSitesOneByOne() {
    frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
    let removeBtn = frameDoc.getElementById("removeSelected");
    let sitesList = frameDoc.getElementById("sitesList");
    let sites = sitesList.getElementsByTagName("richlistitem");
    for (let i = sites.length - 1; i >= 0; --i) {
      sites[i].click();
      removeBtn.doCommand();
    }
  }
});

// Test selecting and removing partial sites
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
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
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://s3-us-west-2.amazonaws.com"),
      persisted: true
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://127.0.0.1"),
      persisted: false
    },
    {
      usage: 1024,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://[0:0:0:0:0:0:0:1]"),
      persisted: true
    },
  ];
  let fakeHosts = mockSiteDataManager.fakeSites.map(site => site.principal.URI.host);

  let updatePromise = promiseSiteDataManagerSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();

  let win = gBrowser.selectedBrowser.contentWindow;
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
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  cancelBtn = frameDoc.getElementById("cancel");
  removeSelectedSite(fakeHosts.slice(0, 2));
  assertSitesListed(doc, fakeHosts.slice(2));
  cancelBtn.doCommand();
  await settingsDialogClosePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeHosts);

  // Test the "Save Changes" button but canceling save
  removeDialogOpenPromise = promiseWindowDialogOpen("cancel", REMOVE_DIALOG_URL);
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeSelectedSite(fakeHosts.slice(0, 2));
  assertSitesListed(doc, fakeHosts.slice(2));
  saveBtn.doCommand();
  await removeDialogOpenPromise;
  await settingsDialogClosePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeHosts);

  // Test the "Save Changes" button and accepting save
  removeDialogOpenPromise = promiseWindowDialogOpen("accept", REMOVE_DIALOG_URL);
  settingsDialogClosePromise = promiseSettingsDialogClose();
  frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  saveBtn = frameDoc.getElementById("save");
  removeSelectedSite(fakeHosts.slice(0, 2));
  assertSitesListed(doc, fakeHosts.slice(2));
  saveBtn.doCommand();
  await removeDialogOpenPromise;
  await settingsDialogClosePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeHosts.slice(2));

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function removeSelectedSite(hosts) {
    frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
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
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
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
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();

  // Search "foo" to only list foo.com sites
  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
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
  await acceptRemovePromise;
  await settingsDialogClosePromise;
  await updatePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeHosts.filter(host => !host.includes("xyz")));

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test dynamically clearing all site data
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
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
  ];
  let fakeHosts = mockSiteDataManager.fakeSites.map(site => site.principal.URI.host);

  // Test the initial state
  let updatePromise = promiseSiteDataManagerSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();
  let doc = gBrowser.selectedBrowser.contentDocument;
  assertSitesListed(doc, fakeHosts);

  // Add more sites dynamically
  mockSiteDataManager.fakeSites.push({
    usage: 1024,
    principal: Services.scriptSecurityManager
                       .createCodebasePrincipalFromOrigin("http://cinema.bar.com"),
    persisted: true
  }, {
    usage: 1024,
    principal: Services.scriptSecurityManager
                       .createCodebasePrincipalFromOrigin("http://email.bar.com"),
    persisted: false
  });

  // Test clearing all site data dynamically
  let win = gBrowser.selectedBrowser.contentWindow;
  let frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  updatePromise = promiseSiteDataManagerSitesUpdated();
  let acceptRemovePromise = promiseAlertDialogOpen("accept");
  let settingsDialogClosePromise = promiseSettingsDialogClose();
  let removeAllBtn = frameDoc.getElementById("removeAll");
  let saveBtn = frameDoc.getElementById("save");
  removeAllBtn.doCommand();
  saveBtn.doCommand();
  await acceptRemovePromise;
  await settingsDialogClosePromise;
  await updatePromise;
  await openSiteDataSettingsDialog();
  assertAllSitesNotListed(win);

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
