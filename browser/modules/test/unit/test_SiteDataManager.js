/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { SiteDataManager } = ChromeUtils.import(
  "resource:///modules/SiteDataManager.jsm"
);
const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const EXAMPLE_ORIGIN = "https://www.example.com";
const EXAMPLE_ORIGIN_2 = "https://example.org";
const EXAMPLE_ORIGIN_3 = "http://localhost:8000";

let p = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
  EXAMPLE_ORIGIN
);
let partitionKey = `(${p.scheme},${p.baseDomain})`;
let EXAMPLE_ORIGIN_2_PARTITIONED = Services.scriptSecurityManager.createContentPrincipal(
  Services.io.newURI(EXAMPLE_ORIGIN_2),
  {
    partitionKey,
  }
).origin;

add_task(function setup() {
  do_get_profile();
});

add_task(async function testGetSites() {
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN,
    name: "foo1",
    value: "bar1",
  });
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN,
    name: "foo2",
    value: "bar2",
  });

  // Cookie of EXAMPLE_ORIGIN_2 partitioned under EXAMPLE_ORIGIN.
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN_2_PARTITIONED,
    name: "foo3",
    value: "bar3",
  });
  // IndexedDB storage of EXAMPLE_ORIGIN_2 partitioned under EXAMPLE_ORIGIN.
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN, 4096);
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN_2_PARTITIONED, 4096);
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN_2,
    name: "foo",
    value: "bar",
  });
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
  // 4096 partitioned + 4096 unpartitioned.
  Assert.greater(site1.usage, 4096 * 2, "Has correct usage for example.com");
  Assert.equal(site1.persisted, false, "example.com is not persisted");
  Assert.equal(
    site1.cookies.length,
    3, // 2 top level, 1 partitioned.
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
  PermissionTestUtils.add(uri, "camera", Services.perms.ALLOW_ACTION);

  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN,
    name: "foo1",
    value: "bar1",
  });
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN,
    name: "foo2",
    value: "bar2",
  });
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN_2_PARTITIONED,
    name: "foo3",
    value: "bar3",
  });
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN, 4096);
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN_2_PARTITIONED, 4096);
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN_2,
    name: "foo",
    value: "bar",
  });
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN_2, 2048);
  await SiteDataTestUtils.persist(EXAMPLE_ORIGIN_2);
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN_3, 2048);

  await SiteDataManager.updateSites();

  let sites = await SiteDataManager.getSites();

  Assert.equal(sites.length, 3, "Has three sites.");

  await SiteDataManager.remove("localhost");

  sites = await SiteDataManager.getSites();

  Assert.equal(sites.length, 2, "Has two sites.");

  await SiteDataManager.remove(["www.example.com"]);

  sites = await SiteDataManager.getSites();

  Assert.equal(sites.length, 1, "Has one site.");
  Assert.equal(
    sites[0].baseDomain,
    "example.org",
    "Has not cleared data for example.org"
  );

  let usage = await SiteDataTestUtils.getQuotaUsage(EXAMPLE_ORIGIN);
  Assert.equal(usage, 0, "Has cleared quota usage for example.com");

  let cookies = Services.cookies.countCookiesFromHost("example.com");
  Assert.equal(cookies, 0, "Has cleared cookies for example.com");

  let perm = PermissionTestUtils.testPermission(uri, "persistent-storage");
  Assert.equal(
    perm,
    Services.perms.UNKNOWN_ACTION,
    "Cleared the persistent-storage permission."
  );
  perm = PermissionTestUtils.testPermission(uri, "camera");
  Assert.equal(
    perm,
    Services.perms.ALLOW_ACTION,
    "Did not clear other permissions."
  );

  PermissionTestUtils.remove(uri, "camera");
});

add_task(async function testRemoveSiteData() {
  let uri = Services.io.newURI(EXAMPLE_ORIGIN);
  PermissionTestUtils.add(uri, "camera", Services.perms.ALLOW_ACTION);

  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN,
    name: "foo1",
    value: "bar1",
  });
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN,
    name: "foo2",
    value: "bar2",
  });
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN_2_PARTITIONED,
    name: "foo3",
    value: "bar3",
  });
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN, 4096);
  await SiteDataTestUtils.addToIndexedDB(EXAMPLE_ORIGIN_2_PARTITIONED, 4096);
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN_2,
    name: "foo",
    value: "bar",
  });
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

  let perm = PermissionTestUtils.testPermission(uri, "persistent-storage");
  Assert.equal(
    perm,
    Services.perms.UNKNOWN_ACTION,
    "Cleared the persistent-storage permission."
  );
  perm = PermissionTestUtils.testPermission(uri, "camera");
  Assert.equal(
    perm,
    Services.perms.ALLOW_ACTION,
    "Did not clear other permissions."
  );

  PermissionTestUtils.remove(uri, "camera");
});
