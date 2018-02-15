"use strict";
const { SiteDataManager } = ChromeUtils.import("resource:///modules/SiteDataManager.jsm", {});
const { DownloadUtils } = ChromeUtils.import("resource://gre/modules/DownloadUtils.jsm", {});

// Test not displaying sites which store 0 byte and don't have persistent storage.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({ set: [["browser.storageManager.enabled", true]] });
  mockSiteDataManager.register(SiteDataManager, [
    {
      usage: 0,
      origin: "https://account.xyz.com",
      persisted: true
    },
    {
      usage: 0,
      origin: "https://shopping.xyz.com",
      persisted: false
    },
    {
      usage: 1024,
      origin: "http://cinema.bar.com",
      persisted: true
    },
    {
      usage: 1024,
      origin: "http://email.bar.com",
      persisted: false
    },
    {
      usage: 0,
      origin: "http://cookies.bar.com",
      cookies: 5,
      persisted: false
    },
  ]);
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
  mockSiteDataManager.register(SiteDataManager, [
    {
      usage: quotaUsage,
      origin: "https://account.xyz.com^userContextId=1",
      cookies: 2,
      persisted: true
    },
    {
      usage: quotaUsage,
      origin: "https://account.xyz.com",
      cookies: 1,
      persisted: false
    },
    {
      usage: quotaUsage,
      origin: "https://account.xyz.com:123",
      cookies: 1,
      persisted: false
    },
    {
      usage: quotaUsage,
      origin: "http://account.xyz.com",
      cookies: 1,
      persisted: false
    },
  ]);

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

  let columns = siteItems[0].querySelectorAll(".item-box > label");

  let expected = "account.xyz.com";
  is(columns[0].value, expected, "Should group and list sites by host");

  let prefStrBundle = frameDoc.getElementById("bundlePreferences");
  expected = prefStrBundle.getString("persistent");
  is(columns[1].value, expected, "Should mark persisted status across scheme, port and origin attributes");

  is(columns[2].value, "5", "Should group cookies across scheme, port and origin attributes");

  expected = prefStrBundle.getFormattedString("siteUsage",
    DownloadUtils.convertByteUnits(quotaUsage * mockSiteDataManager.fakeSites.length));
  is(columns[3].value, expected, "Should sum up usages across scheme, port and origin attributes");

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test sorting
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  mockSiteDataManager.register(SiteDataManager, [
    {
      usage: 1024,
      origin: "https://account.xyz.com",
      cookies: 6,
      persisted: true
    },
    {
      usage: 1024 * 2,
      origin: "https://books.foo.com",
      cookies: 0,
      persisted: false
    },
    {
      usage: 1024 * 3,
      origin: "http://cinema.bar.com",
      cookies: 3,
      persisted: true
    },
  ]);

  let updatePromise = promiseSiteDataManagerSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();

  // eslint-disable-next-line mozilla/no-cpows-in-tests
  let dialog = content.gSubDialog._topDialog;
  let dialogFrame = dialog._frame;
  let frameDoc = dialogFrame.contentDocument;
  let hostCol = frameDoc.getElementById("hostCol");
  let usageCol = frameDoc.getElementById("usageCol");
  let statusCol = frameDoc.getElementById("statusCol");
  let cookiesCol = frameDoc.getElementById("cookiesCol");
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
  assertSortByBaseDomain("ascending");
  hostCol.click();
  assertSortByBaseDomain("descending");

  // Test sorting on the permission status column
  cookiesCol.click();
  assertSortByCookies("ascending");
  cookiesCol.click();
  assertSortByCookies("descending");

  // Test sorting on the cookies column
  statusCol.click();
  assertSortByStatus("ascending");
  statusCol.click();
  assertSortByStatus("descending");

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  function assertSortByBaseDomain(order) {
    let siteItems = sitesList.getElementsByTagName("richlistitem");
    for (let i = 0; i < siteItems.length - 1; ++i) {
      let aHost = siteItems[i].getAttribute("host");
      let bHost = siteItems[i + 1].getAttribute("host");
      let a = findSiteByHost(aHost);
      let b = findSiteByHost(bHost);
      let result = a.baseDomain.localeCompare(b.baseDomain);
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

  function assertSortByCookies(order) {
    let siteItems = sitesList.getElementsByTagName("richlistitem");
    for (let i = 0; i < siteItems.length - 1; ++i) {
      let aHost = siteItems[i].getAttribute("host");
      let bHost = siteItems[i + 1].getAttribute("host");
      let a = findSiteByHost(aHost);
      let b = findSiteByHost(bHost);
      let result = a.cookies.length - b.cookies.length;
      if (order == "ascending") {
        Assert.lessOrEqual(result, 0, "Should sort sites in the ascending order by number of cookies");
      } else {
        Assert.greaterOrEqual(result, 0, "Should sort sites in the descending order by number of cookies");
      }
    }
  }

  function findSiteByHost(host) {
    return mockSiteDataManager.fakeSites.find(site => site.principal.URI.host == host);
  }
});
