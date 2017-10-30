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
