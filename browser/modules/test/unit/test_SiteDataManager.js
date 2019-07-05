/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const EXAMPLE_ORIGIN = "https://www.example.com";
const EXAMPLE_ORIGIN_2 = "https://example.org";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { SiteDataManager } = ChromeUtils.import(
  "resource:///modules/SiteDataManager.jsm"
);
const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TestUtils",
  "resource://testing-common/TestUtils.jsm"
);

add_task(function setup() {
  do_get_profile();
});

add_task(async function testGetSites() {
  SiteDataTestUtils.addToCookies(EXAMPLE_ORIGIN, "foo1", "bar1");
  SiteDataTestUtils.addToCookies(EXAMPLE_ORIGIN, "foo2", "bar2");
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN, 4096);
  SiteDataTestUtils.addToCookies(EXAMPLE_ORIGIN_2, "foo", "bar");
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN_2, 2048);
  await SiteDataTestUtils.persist(EXAMPLE_ORIGIN_2);

  await SiteDataManager.updateSites();

  let sites = await SiteDataManager.getSites();

  let site1 = sites.find(site => site.baseDomain == "example.com");
  let site2 = sites.find(site => site.baseDomain == "example.org");

  Assert.equal(
    site1.baseDomain,
    "example.com",
    "Has the correct base domain for example.com"
  );
  Assert.equal(
    site1.host,
    "www.example.com",
    "Has the correct host for example.com"
  );
  Assert.greater(site1.usage, 4096, "Has correct usage for example.com");
  Assert.equal(site1.persisted, false, "example.com is not persisted");
  Assert.equal(
    site1.cookies.length,
    2,
    "Has correct number of cookies for example.com"
  );
  Assert.ok(
    typeof site1.lastAccessed.getDate == "function",
    "lastAccessed for example.com is a Date"
  );
  Assert.ok(
    site1.lastAccessed > Date.now() - 60 * 1000,
    "lastAccessed for example.com happened recently"
  );

  Assert.equal(
    site2.baseDomain,
    "example.org",
    "Has the correct base domain for example.org"
  );
  Assert.equal(
    site2.host,
    "example.org",
    "Has the correct host for example.org"
  );
  Assert.greater(site2.usage, 2048, "Has correct usage for example.org");
  Assert.equal(site2.persisted, true, "example.org is persisted");
  Assert.equal(
    site2.cookies.length,
    1,
    "Has correct number of cookies for example.org"
  );
  Assert.ok(
    typeof site2.lastAccessed.getDate == "function",
    "lastAccessed for example.org is a Date"
  );
  Assert.ok(
    site2.lastAccessed > Date.now() - 60 * 1000,
    "lastAccessed for example.org happened recently"
  );

  await SiteDataTestUtils.clear();
});

add_task(async function testGetTotalUsage() {
  await SiteDataManager.updateSites();
  let sites = await SiteDataManager.getSites();
  Assert.equal(sites.length, 0, "SiteDataManager is empty");

  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN, 4096);
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN_2, 2048);

  await SiteDataManager.updateSites();

  let usage = await SiteDataManager.getTotalUsage();

  Assert.greater(usage, 4096 + 2048, "Has the correct total usage.");

  await SiteDataTestUtils.clear();
});

add_task(async function testRemove() {
  await SiteDataManager.updateSites();

  let uri = Services.io.newURI(EXAMPLE_ORIGIN);
  Services.perms.add(uri, "camera", Services.perms.ALLOW_ACTION);

  SiteDataTestUtils.addToCookies(EXAMPLE_ORIGIN, "foo1", "bar1");
  SiteDataTestUtils.addToCookies(EXAMPLE_ORIGIN, "foo2", "bar2");
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN, 4096);
  SiteDataTestUtils.addToCookies(EXAMPLE_ORIGIN_2, "foo", "bar");
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN_2, 2048);
  await SiteDataTestUtils.persist(EXAMPLE_ORIGIN_2);

  await SiteDataManager.updateSites();

  let sites = await SiteDataManager.getSites();

  Assert.equal(sites.length, 2, "Has two sites.");

  await SiteDataManager.remove(["www.example.com"]);

  sites = await SiteDataManager.getSites();

  Assert.equal(sites.length, 1, "Has one site.");
  Assert.equal(
    sites[0].host,
    "example.org",
    "Has not cleared data for example.org"
  );

  let usage = await SiteDataTestUtils.getQuotaUsage(EXAMPLE_ORIGIN);
  Assert.equal(usage, 0, "Has cleared quota usage for example.com");

  let cookies = Services.cookies.countCookiesFromHost("example.com");
  Assert.equal(cookies, 0, "Has cleared cookies for example.com");

  let perm = Services.perms.testPermission(uri, "persistent-storage");
  Assert.equal(
    perm,
    Services.perms.UNKNOWN_ACTION,
    "Cleared the persistent-storage permission."
  );
  perm = Services.perms.testPermission(uri, "camera");
  Assert.equal(
    perm,
    Services.perms.ALLOW_ACTION,
    "Did not clear other permissions."
  );

  Services.perms.remove(uri, "camera");
});

add_task(async function testRemoveSiteData() {
  let uri = Services.io.newURI(EXAMPLE_ORIGIN);
  Services.perms.add(uri, "camera", Services.perms.ALLOW_ACTION);

  SiteDataTestUtils.addToCookies(EXAMPLE_ORIGIN, "foo1", "bar1");
  SiteDataTestUtils.addToCookies(EXAMPLE_ORIGIN, "foo2", "bar2");
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN, 4096);
  SiteDataTestUtils.addToCookies(EXAMPLE_ORIGIN_2, "foo", "bar");
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN_2, 2048);
  await SiteDataTestUtils.persist(EXAMPLE_ORIGIN_2);

  await SiteDataManager.updateSites();

  let sites = await SiteDataManager.getSites();

  Assert.equal(sites.length, 2, "Has two sites.");

  await SiteDataManager.removeSiteData();

  sites = await SiteDataManager.getSites();

  Assert.equal(sites.length, 0, "Has no sites.");

  let usage = await SiteDataTestUtils.getQuotaUsage(EXAMPLE_ORIGIN);
  Assert.equal(usage, 0, "Has cleared quota usage for example.com");

  usage = await SiteDataTestUtils.getQuotaUsage(EXAMPLE_ORIGIN_2);
  Assert.equal(usage, 0, "Has cleared quota usage for example.org");

  let cookies = Services.cookies.countCookiesFromHost("example.org");
  Assert.equal(cookies, 0, "Has cleared cookies for example.org");

  let perm = Services.perms.testPermission(uri, "persistent-storage");
  Assert.equal(
    perm,
    Services.perms.UNKNOWN_ACTION,
    "Cleared the persistent-storage permission."
  );
  perm = Services.perms.testPermission(uri, "camera");
  Assert.equal(
    perm,
    Services.perms.ALLOW_ACTION,
    "Did not clear other permissions."
  );

  Services.perms.remove(uri, "camera");
});
