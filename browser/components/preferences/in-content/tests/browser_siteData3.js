"use strict";
const { SiteDataManager } = Cu.import("resource:///modules/SiteDataManager.jsm", {});
const { DownloadUtils } = Cu.import("resource://gre/modules/DownloadUtils.jsm", {});

// Test not displaying sites which store 0 byte and don't have persistent storage.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({ set: [["browser.storageManager.enabled", true]] });
  mockSiteDataManager.register(SiteDataManager);
  mockSiteDataManager.fakeSites = [
    {
      usage: 0,
      principal: Services.scriptSecurityManager
        .createCodebasePrincipalFromOrigin("https://account.xyz.com"),
      persisted: true
    },
    {
      usage: 0,
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
  let doc = gBrowser.selectedBrowser.contentDocument;
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(doc, fakeHosts.filter(host => host != "shopping.xyz.com"));

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test grouping and listing sites across scheme, port and origin attributes by host
add_task(async function() {
  await SpecialPowers.pushPrefEnv({ set: [["browser.storageManager.enabled", true]] });
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
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatedPromise;
  await openSiteDataSettingsDialog();
  // eslint-disable-next-line mozilla/no-cpows-in-tests
  let win = gBrowser.selectedBrowser.contentWindow;
  let dialogFrame = win.gSubDialog._topDialog._frame;
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
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test sorting
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
      usage: 1024 * 2,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("https://books.foo.com"),
      persisted: false
    },
    {
      usage: 1024 * 3,
      principal: Services.scriptSecurityManager
                         .createCodebasePrincipalFromOrigin("http://cinema.bar.com"),
      persisted: true
    },
  ];

  let updatePromise = promiseSiteDataManagerSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();

  let dialog = content.gSubDialog._topDialog;
  let dialogFrame = dialog._frame;
  let frameDoc = dialogFrame.contentDocument;
  let hostCol = frameDoc.getElementById("hostCol");
  let usageCol = frameDoc.getElementById("usageCol");
  let statusCol = frameDoc.getElementById("statusCol");
  let sitesList = frameDoc.getElementById("sitesList");

  // Test default sorting
  assertSortByUsage("descending");

  // Test sorting on the usage column
  usageCol.click();
  assertSortByUsage("ascending");
  usageCol.click();
  assertSortByUsage("descending");

  // Test sorting on the host column
  hostCol.click();
  assertSortByHost("ascending");
  hostCol.click();
  assertSortByHost("descending");

  // Test sorting on the permission status column
  statusCol.click();
  assertSortByStatus("ascending");
  statusCol.click();
  assertSortByStatus("descending");

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function assertSortByHost(order) {
    let siteItems = sitesList.getElementsByTagName("richlistitem");
    for (let i = 0; i < siteItems.length - 1; ++i) {
      let aHost = siteItems[i].getAttribute("host");
      let bHost = siteItems[i + 1].getAttribute("host");
      let result = aHost.localeCompare(bHost);
      if (order == "ascending") {
        Assert.lessOrEqual(result, 0, "Should sort sites in the ascending order by host");
      } else {
        Assert.greaterOrEqual(result, 0, "Should sort sites in the descending order by host");
      }
    }
  }

  function assertSortByStatus(order) {
    let siteItems = sitesList.getElementsByTagName("richlistitem");
    for (let i = 0; i < siteItems.length - 1; ++i) {
      let aHost = siteItems[i].getAttribute("host");
      let bHost = siteItems[i + 1].getAttribute("host");
      let a = findSiteByHost(aHost);
      let b = findSiteByHost(bHost);
      let result = 0;
      if (a.persisted && !b.persisted) {
        result = 1;
      } else if (!a.persisted && b.persisted) {
        result = -1;
      }
      if (order == "ascending") {
        Assert.lessOrEqual(result, 0, "Should sort sites in the ascending order by permission status");
      } else {
        Assert.greaterOrEqual(result, 0, "Should sort sites in the descending order by permission status");
      }
    }
  }

  function assertSortByUsage(order) {
    let siteItems = sitesList.getElementsByTagName("richlistitem");
    for (let i = 0; i < siteItems.length - 1; ++i) {
      let aHost = siteItems[i].getAttribute("host");
      let bHost = siteItems[i + 1].getAttribute("host");
      let a = findSiteByHost(aHost);
      let b = findSiteByHost(bHost);
      let result = a.usage - b.usage;
      if (order == "ascending") {
        Assert.lessOrEqual(result, 0, "Should sort sites in the ascending order by usage");
      } else {
        Assert.greaterOrEqual(result, 0, "Should sort sites in the descending order by usage");
      }
    }
  }

  function findSiteByHost(host) {
    return mockSiteDataManager.fakeSites.find(site => site.principal.URI.host == host);
  }
});
