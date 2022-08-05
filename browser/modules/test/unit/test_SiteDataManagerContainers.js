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

const EXAMPLE_ORIGIN = "https://www.example.com";
const EXAMPLE_ORIGIN_2 = "https://example.org";

add_task(function setup() {
  do_get_profile();
});

add_task(async function testGetSitesByContainers() {
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN,
    name: "foo1",
    value: "bar1",
    originAttributes: { userContextId: "1" },
  });
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN,
    name: "foo2",
    value: "bar2",
    originAttributes: { userContextId: "2" },
  });
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN,
    name: "foo3",
    value: "bar3",
    originAttributes: { userContextId: "2" },
  });
  SiteDataTestUtils.addToCookies({
    origin: EXAMPLE_ORIGIN_2,
    name: "foo",
    value: "bar",
    originAttributes: { userContextId: "3" },
  });

  await SiteDataTestUtils.addToIndexedDB(
    EXAMPLE_ORIGIN + "^userContextId=1",
    4096
  );
  await SiteDataTestUtils.addToIndexedDB(
    EXAMPLE_ORIGIN_2 + "^userContextId=3",
    2048
  );

  await SiteDataManager.updateSites();

  let sites = await SiteDataManager.getSites();

  let site1Container1 = sites
    .find(site => site.baseDomain == "example.com")
    .containersData.get(1);

  let site1Container2 = sites
    .find(site => site.baseDomain == "example.com")
    .containersData.get(2);

  let site2Container3 = sites
    .find(site => site.baseDomain == "example.org")
    .containersData.get(3);

  Assert.equal(
    sites.reduce(
      (accumulator, site) => accumulator + site.containersData.size,
      0
    ),
    3,
    "Has the correct number of sites by containers"
  );

  Assert.equal(
    site1Container1.cookiesBlocked,
    1,
    "Has the correct number of cookiesBlocked by containers"
  );

  Assert.greater(
    site1Container1.quotaUsage,
    4096,
    "Has correct usage for example.com^userContextId=1"
  );

  Assert.ok(
    typeof site1Container1.lastAccessed.getDate == "function",
    "lastAccessed for example.com^userContextId=1 is a Date"
  );
  Assert.ok(
    site1Container1.lastAccessed > Date.now() - 60 * 1000,
    "lastAccessed for example.com^userContextId=1 happened recently"
  );

  Assert.equal(
    site1Container2.cookiesBlocked,
    2,
    "Has the correct number of cookiesBlocked by containers"
  );

  Assert.equal(
    site1Container2.quotaUsage,
    0,
    "Has correct usage for example.org^userContextId=2"
  );

  Assert.ok(
    typeof site1Container2.lastAccessed.getDate == "function",
    "lastAccessed for example.com^userContextId=2 is a Date"
  );

  Assert.equal(
    site2Container3.cookiesBlocked,
    1,
    "Has the correct number of cookiesBlocked by containers"
  );

  Assert.greater(
    site2Container3.quotaUsage,
    2048,
    "Has correct usage for example.org^userContextId=3"
  );

  Assert.ok(
    typeof site2Container3.lastAccessed.getDate == "function",
    "lastAccessed for example.org^userContextId=3 is a Date"
  );
  Assert.ok(
    site2Container3.lastAccessed > Date.now() - 60 * 1000,
    "lastAccessed for example.org^userContextId=3 happened recently"
  );

  await SiteDataTestUtils.clear();
});
