/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SiteDataManager: "resource:///modules/SiteDataManager.sys.mjs",
  SiteDataTestUtils: "resource://testing-common/SiteDataTestUtils.sys.mjs",
  PermissionTestUtils: "resource://testing-common/PermissionTestUtils.sys.mjs",
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  FileTestUtils: "resource://testing-common/FileTestUtils.sys.mjs",
  Sanitizer: "resource:///modules/Sanitizer.sys.mjs",
});

const TEST_DOWNLOAD_FILE_NAME = "test-download.txt";
let fileURL;
let downloadList;
var nowMs = Date.now();
const hoursTodayInMs = new Date() - new Date(new Date().setHours(0, 0, 0, 0));

const EXAMPLE_ORIGIN = "https://www.example.com";
const EXAMPLE_ORIGIN_2 = "https://example.org";
const EXAMPLE_ORIGIN_3 = "http://localhost:8000";
const EXAMPLE_ORIGIN_4 = "http://localhost:3000";
const EXAMPLE_ORIGIN_5 = "http://example.net";

const TIMESPANS = [
  "TIMESPAN_HOUR",
  "TIMESPAN_2HOURS",
  "TIMESPAN_4HOURS",
  "TIMESPAN_TODAY",
  "TIMESPAN_EVERYTHING",
];

let oneHourInMs = 60 * 60 * 1000;
let today = nowMs - new Date().setHours(0, 0, 0, 0);

let p =
  Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    EXAMPLE_ORIGIN
  );
let partitionKey = `(${p.scheme},${p.baseDomain})`;
let EXAMPLE_ORIGIN_2_PARTITIONED =
  Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(EXAMPLE_ORIGIN_2),
    {
      partitionKey,
    }
  ).origin;

/**
 * Created a temporary file URL to add to downloads
 *
 * @returns {Object} file URL
 */
function createDownloadFileURL() {
  if (!fileURL) {
    let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
    file.append("foo.txt");
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

    fileURL = Services.io.newFileURI(file);
  }

  return fileURL;
}

/**
 * Creates a new download list to populate
 *
 * @returns download list to update
 */
async function createDownloadList() {
  if (!downloadList) {
    Downloads._promiseListsInitialized = null;
    Downloads._lists = {};
    Downloads._summaries = {};

    downloadList = await Downloads.getList(Downloads.ALL);
  }

  return downloadList;
}

/**
 * Add sites through the site data manager based on the site last accessed array
 *
 * @param {number[]} siteLastAccessedArr - array of time's when the site was last accessed
 *  e.g. [2, 2, 4, 24 ...]
 *  We currently do not have a 24 hour option in the dialog, so we convert 24 to today
 * @param {Object} testSiteUsageDataObj - Object to populate and update
 * @returns testSiteUsageData Object with the quota usage for each timespan
 */
async function addSitesWithLastAccessed(
  siteLastAccessedArr,
  testSiteUsageDataObj
) {
  let SITE_ORIGINS = [
    EXAMPLE_ORIGIN,
    EXAMPLE_ORIGIN_2,
    EXAMPLE_ORIGIN_5,
    EXAMPLE_ORIGIN_3,
    EXAMPLE_ORIGIN_4,
    EXAMPLE_ORIGIN_2_PARTITIONED,
  ];

  let staticUsage = 4096;
  // Add a time buffer so the site access falls within the time range
  const buffer = 100000;

  // Change lastAccessed of sites
  for (let index = 0; index < siteLastAccessedArr.length; index++) {
    testSiteUsageDataObj[siteLastAccessedArr[index]] += staticUsage;
    // subtract 1 to make sure the added site is within the time range
    let lastAccessedTime = Sanitizer.timeSpanMsMap[siteLastAccessedArr[index]];
    let site = SiteDataManager._testInsertSite(SITE_ORIGINS[index], {
      quotaUsage: staticUsage,
      lastAccessed: (nowMs - lastAccessedTime + buffer) * 1000,
    });
    Assert.ok(
      site,
      `Site added successfully with last accessed ${new Date(
        site.lastAccessed
      )}`
    );
  }

  let cumulativeSum = 0;
  for (let key in testSiteUsageDataObj) {
    cumulativeSum += testSiteUsageDataObj[key];
    testSiteUsageDataObj[key] = cumulativeSum;
  }
  // account for items being added within today that overlap other time ranges
  // this is to account for edge cases for when this test might be run near after midnight
  // We only care about the timespans until TIMESPAN_TODAY (i<3)
  if (hoursTodayInMs < Sanitizer.timeSpanMsMap.TIMESPAN_HOUR) {
    testSiteUsageDataObj.TIMESPAN_HOUR += testSiteUsageDataObj.TIMESPAN_TODAY;
  }
  if (hoursTodayInMs < Sanitizer.timeSpanMsMap.TIMESPAN_2HOURS) {
    testSiteUsageDataObj.TIMESPAN_2HOURS += testSiteUsageDataObj.TIMESPAN_TODAY;
  }
  if (hoursTodayInMs < Sanitizer.timeSpanMsMap.TIMESPAN_4HOURS) {
    testSiteUsageDataObj.TIMESPAN_4HOURS += testSiteUsageDataObj.TIMESPAN_TODAY;
  }

  return testSiteUsageDataObj;
}

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

add_task(async function testDownloadDataSizeByRange() {
  Services.prefs.setBoolPref("privacy.cpd.downloads", true);

  let url = createDownloadFileURL();
  let list = await createDownloadList();

  let testSizes = [3, 1, 2, 2, 2];
  let timeMultiplier = [0.5, 2, 4, 24, 56];

  // Add a time buffer so the download falls within the time range
  let buffer = 100000;
  let totalItems = 0;

  for (let i = 0; i < testSizes.length; i++) {
    totalItems += testSizes[i];
    let setTime = nowMs - oneHourInMs * timeMultiplier[i] + buffer;
    if (timeMultiplier[i] === 24) {
      setTime = nowMs - today + buffer;
    }
    // Add downloads to downloads list
    for (let j = 0; j < testSizes[i]; j++) {
      let download = await Downloads.createDownload({
        source: { url: url.spec, isPrivate: false },
        target: {
          path: FileTestUtils.getTempFile(TEST_DOWNLOAD_FILE_NAME).path,
        },
        startTime: {
          getTime: _ => {
            return setTime;
          },
        },
      });

      Assert.ok(
        !!download,
        `The download for ${timeMultiplier[i]} was created successfully!`
      );

      list.add(download);
    }
  }

  let items = await list.getAll();
  Assert.equal(items.length, totalItems, "Items were added to the list");

  let rangedSize = await SiteDataManager.getDownloadCountForTimeRanges(
    TIMESPANS
  );

  let compareSizes = [0, 0, 0, 0, 0];

  // account for items being added within today that overlap other time ranges
  // this is to account for edge cases for when this test might be run near after midnight
  // we only care about i = 3 since that is the index where TIMESPAN_TODAY lives
  if (hoursTodayInMs < Sanitizer.timeSpanMsMap.TIMESPAN_HOUR) {
    compareSizes[0] += testSizes[3];
  }
  if (hoursTodayInMs < Sanitizer.timeSpanMsMap.TIMESPAN_2HOURS) {
    compareSizes[1] += testSizes[3];
  }
  if (hoursTodayInMs < Sanitizer.timeSpanMsMap.TIMESPAN_4HOURS) {
    compareSizes[2] += testSizes[3];
  }
  // add all the sizes to reflect total size for a range
  for (let max = 0; max < testSizes.length; max++) {
    for (let i = 0; i <= max; i++) {
      compareSizes[max] += testSizes[i];
    }
  }

  // check the timerange array against the expected sizes
  // we use TIMESPANS[] to get the key for rangedSize we are comparing
  for (let i = 0; i < testSizes.length; i++) {
    Assert.equal(
      compareSizes[i],
      rangedSize[TIMESPANS[i]],
      "Checking time range array"
    );
  }

  // Clear last 4 hours
  Services.prefs.setIntPref(Sanitizer.PREF_TIMESPAN, Sanitizer.TIMESPAN_4HOURS);
  await Sanitizer.sanitize(null, { ignoreTimespan: false });

  // reset testsizes
  testSizes[0] = 0;
  testSizes[1] = 0;
  testSizes[2] = 0;

  rangedSize = await SiteDataManager.getDownloadCountForTimeRanges(TIMESPANS);

  compareSizes = [0, 0, 0, 0, 0];

  // account for items being added within today that overlap other time ranges
  // this is to account for edge cases for when this test might be run near after midnight
  // we only care about i = 3 since that is the index where TIMESPAN_TODAY lives
  if (hoursTodayInMs < Sanitizer.timeSpanMsMap.TIMESPAN_HOUR) {
    compareSizes[0] += testSizes[3];
  }
  if (hoursTodayInMs < Sanitizer.timeSpanMsMap.TIMESPAN_2HOURS) {
    compareSizes[1] += testSizes[3];
  }
  if (hoursTodayInMs < Sanitizer.timeSpanMsMap.TIMESPAN_4HOURS) {
    compareSizes[2] += testSizes[3];
  }
  // add all the sizes to reflect total size for a range
  for (let max = 0; max < testSizes.length; max++) {
    for (let i = 0; i <= max; i++) {
      compareSizes[max] += testSizes[i];
    }
  }

  // check the timerange array against the expected sizes
  // we use TIMESPANS[] to get the key for rangedSize we are comparing
  for (let i = 0; i < testSizes.length; i++) {
    Assert.equal(
      compareSizes[i],
      rangedSize[TIMESPANS[i]],
      "Checking time range array"
    );
  }

  // Clear all downloads
  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOWNLOADS,
      value => {
        Assert.equal(value, 0);
        resolve();
      }
    );
  });

  items = await list.getAll();
  Assert.equal(items.length, 0, "Delete all downloaded items for test");
});

add_task(async function testGetQuotaUsageByRange() {
  await SiteDataManager.updateSites();

  let sites = await SiteDataManager.getSites();
  Assert.equal(sites.length, 0, "SiteDataManager is empty");

  let testSiteUsageData = {
    TIMESPAN_HOUR: 0,
    TIMESPAN_2HOURS: 0,
    TIMESPAN_4HOURS: 0,
    TIMESPAN_TODAY: 0,
    TIMESPAN_EVERYTHING: 0,
  };

  testSiteUsageData = await addSitesWithLastAccessed(
    [
      "TIMESPAN_HOUR",
      "TIMESPAN_2HOURS",
      "TIMESPAN_2HOURS",
      "TIMESPAN_4HOURS",
      "TIMESPAN_TODAY",
      "TIMESPAN_EVERYTHING",
    ],
    testSiteUsageData
  );

  let usageObj = await SiteDataManager.getQuotaUsageForTimeRanges(TIMESPANS);

  let index = 0;
  for (let key in testSiteUsageData) {
    Assert.equal(
      usageObj[TIMESPANS[index]],
      testSiteUsageData[key],
      `Usage data is correct for ${key} hrs`
    );
    index++;
  }

  await SiteDataTestUtils.clear();
});
