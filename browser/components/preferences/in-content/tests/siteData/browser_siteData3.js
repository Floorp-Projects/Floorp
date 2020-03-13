"use strict";

// Test not displaying sites which store 0 byte and don't have persistent storage.
add_task(async function test_exclusions() {
  let hosts = await addTestData([
    {
      usage: 0,
      origin: "https://account.xyz.com",
      persisted: true,
    },
    {
      usage: 0,
      origin: "https://shopping.xyz.com",
      persisted: false,
    },
    {
      usage: 1024,
      origin: "http://cinema.bar.com",
      persisted: true,
    },
    {
      usage: 1024,
      origin: "http://email.bar.com",
      persisted: false,
    },
    {
      usage: 0,
      origin: "http://cookies.bar.com",
      cookies: 5,
      persisted: false,
    },
  ]);

  let updatePromise = promiseSiteDataManagerSitesUpdated();
  let doc = gBrowser.selectedBrowser.contentDocument;
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();
  assertSitesListed(
    doc,
    hosts.filter(host => host != "shopping.xyz.com")
  );

  await SiteDataTestUtils.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test grouping and listing sites across scheme, port and origin attributes by host
add_task(async function test_grouping() {
  let quotaUsage = 7000000;
  await addTestData([
    {
      usage: quotaUsage,
      origin: "https://account.xyz.com^userContextId=1",
      cookies: 2,
      persisted: true,
    },
    {
      usage: quotaUsage,
      origin: "https://account.xyz.com",
      cookies: 1,
      persisted: false,
    },
    {
      usage: quotaUsage,
      origin: "https://account.xyz.com:123",
      cookies: 1,
      persisted: false,
    },
    {
      usage: quotaUsage,
      origin: "http://account.xyz.com",
      cookies: 1,
      persisted: false,
    },
  ]);

  let updatedPromise = promiseSiteDataManagerSitesUpdated();
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatedPromise;
  await openSiteDataSettingsDialog();
  let win = gBrowser.selectedBrowser.contentWindow;
  let dialogFrame = win.gSubDialog._topDialog._frame;
  let frameDoc = dialogFrame.contentDocument;

  let siteItems = frameDoc.getElementsByTagName("richlistitem");
  is(
    siteItems.length,
    1,
    "Should group sites across scheme, port and origin attributes"
  );

  let columns = siteItems[0].querySelectorAll(".item-box > label");

  let expected = "account.xyz.com";
  is(columns[0].value, expected, "Should group and list sites by host");

  is(
    columns[1].value,
    "5",
    "Should group cookies across scheme, port and origin attributes"
  );

  let [value, unit] = DownloadUtils.convertByteUnits(quotaUsage * 4);
  let l10nAttributes = frameDoc.l10n.getAttributes(columns[2]);
  is(
    l10nAttributes.id,
    "site-storage-persistent",
    "Should show the site as persistent if one origin is persistent."
  );
  // The shown quota can be slightly larger than the raw data we put in (though it should
  // never be smaller), but that doesn't really matter to us since we only want to test that
  // the site data dialog accumulates this into a single column.
  ok(
    parseFloat(l10nAttributes.args.value) >= parseFloat(value),
    "Should show the correct accumulated quota size."
  );
  is(
    l10nAttributes.args.unit,
    unit,
    "Should show the correct quota size unit."
  );

  await SiteDataTestUtils.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test sorting
add_task(async function test_sorting() {
  let testData = [
    {
      baseDomain: "xyz.com",
      usage: 1024,
      origin: "https://account.xyz.com",
      cookies: 6,
      persisted: true,
    },
    {
      baseDomain: "foo.com",
      usage: 1024 * 2,
      origin: "https://books.foo.com",
      cookies: 0,
      persisted: false,
    },
    {
      baseDomain: "bar.com",
      usage: 1024 * 3,
      origin: "http://cinema.bar.com",
      cookies: 3,
      persisted: true,
    },
  ];

  await addTestData(testData);

  let updatePromise = promiseSiteDataManagerSitesUpdated();

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await updatePromise;
  await openSiteDataSettingsDialog();

  let dialog = content.gSubDialog._topDialog;
  let dialogFrame = dialog._frame;
  let frameDoc = dialogFrame.contentDocument;
  let hostCol = frameDoc.getElementById("hostCol");
  let usageCol = frameDoc.getElementById("usageCol");
  let cookiesCol = frameDoc.getElementById("cookiesCol");
  let sitesList = frameDoc.getElementById("sitesList");

  function getHostOrder() {
    let siteItems = sitesList.getElementsByTagName("richlistitem");
    return Array.from(siteItems).map(item => item.getAttribute("host"));
  }

  // Test default sorting by usage, descending.
  Assert.deepEqual(
    getHostOrder(),
    ["cinema.bar.com", "books.foo.com", "account.xyz.com"],
    "Has sorted descending by usage"
  );

  // Test sorting on the usage column
  usageCol.click();
  Assert.deepEqual(
    getHostOrder(),
    ["account.xyz.com", "books.foo.com", "cinema.bar.com"],
    "Has sorted ascending by usage"
  );
  usageCol.click();
  Assert.deepEqual(
    getHostOrder(),
    ["cinema.bar.com", "books.foo.com", "account.xyz.com"],
    "Has sorted descending by usage"
  );

  // Test sorting on the host column
  hostCol.click();
  Assert.deepEqual(
    getHostOrder(),
    ["cinema.bar.com", "books.foo.com", "account.xyz.com"],
    "Has sorted ascending by base domain"
  );
  hostCol.click();
  Assert.deepEqual(
    getHostOrder(),
    ["account.xyz.com", "books.foo.com", "cinema.bar.com"],
    "Has sorted descending by base domain"
  );

  // Test sorting on the cookies column
  cookiesCol.click();
  Assert.deepEqual(
    getHostOrder(),
    ["books.foo.com", "cinema.bar.com", "account.xyz.com"],
    "Has sorted ascending by cookies"
  );
  cookiesCol.click();
  Assert.deepEqual(
    getHostOrder(),
    ["account.xyz.com", "cinema.bar.com", "books.foo.com"],
    "Has sorted descending by cookies"
  );

  await SiteDataTestUtils.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
