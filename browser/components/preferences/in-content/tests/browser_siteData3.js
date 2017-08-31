"use strict";

// Test search on the host column
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  mockSiteDataManager.register();
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

  let updatePromise = promiseSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  await updatePromise;
  await openSettingsDialog();

  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = gBrowser.selectedBrowser.contentDocument;
  let frameDoc = win.gSubDialog._topDialog._frame.contentDocument;
  let searchBox = frameDoc.getElementById("searchBox");

  searchBox.value = "xyz";
  searchBox.doCommand();
  assertSitesListed(doc, fakeHosts.filter(host => host.includes("xyz")));

  searchBox.value = "bar";
  searchBox.doCommand();
  assertSitesListed(doc, fakeHosts.filter(host => host.includes("bar")));

  searchBox.value = "";
  searchBox.doCommand();
  assertSitesListed(doc, fakeHosts);

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test not displaying sites which store 0 byte and don't have persistent storage.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.storageManager.enabled", true]]});
  mockSiteDataManager.register();
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

  let updatePromise = promiseSitesUpdated();
  let doc = gBrowser.selectedBrowser.contentDocument;
  await openPreferencesViaOpenPreferencesAPI("advanced", "networkTab", { leaveOpen: true });
  await updatePromise;
  await openSettingsDialog();
  assertSitesListed(doc, fakeHosts.filter(host => host != "shopping.xyz.com"));

  mockSiteDataManager.unregister();
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
