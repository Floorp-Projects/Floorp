/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 380852 - Delete permission manager entries in Clear Recent History

const { Sanitizer } = ChromeUtils.import("resource:///modules/Sanitizer.jsm");
const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "sas",
  "@mozilla.org/storage/activity-service;1",
  "nsIStorageActivityService"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "swm",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

const oneHour = 3600000000;
const fiveHours = oneHour * 5;

const itemsToClear = ["cookies", "offlineApps"];

function hasIndexedDB(origin) {
  return new Promise(resolve => {
    let hasData = true;
    let uri = Services.io.newURI(origin);
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );
    let request = indexedDB.openForPrincipal(principal, "TestDatabase", 1);
    request.onupgradeneeded = function(e) {
      hasData = false;
    };
    request.onsuccess = function(e) {
      resolve(hasData);
    };
  });
}

function waitForUnregister(host) {
  return new Promise(resolve => {
    let listener = {
      onUnregister: registration => {
        if (registration.principal.host != host) {
          return;
        }
        swm.removeListener(listener);
        resolve(registration);
      },
    };
    swm.addListener(listener);
  });
}

async function createData(host) {
  let origin = "https://" + host;
  let dummySWURL =
    getRootDirectory(gTestPath).replace("chrome://mochitests/content", origin) +
    "dummy.js";

  await SiteDataTestUtils.addToIndexedDB(origin);
  await SiteDataTestUtils.addServiceWorker(dummySWURL);
}

function moveOriginInTime(principals, endDate, host) {
  for (let i = 0; i < principals.length; ++i) {
    let principal = principals.queryElementAt(i, Ci.nsIPrincipal);
    if (principal.host == host) {
      sas.moveOriginInTime(principal, endDate - fiveHours);
      return true;
    }
  }
  return false;
}

add_task(async function testWithRange() {
  // We have intermittent occurrences of NS_ERROR_ABORT being
  // thrown at closing database instances when using Santizer.sanitize().
  // This does not seem to impact cleanup, since our tests run fine anyway.
  PromiseTestUtils.allowMatchingRejectionsGlobally(/NS_ERROR_ABORT/);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.exemptFromPerDomainMax", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
  });

  // The service may have picked up activity from prior tests in this run.
  // Clear it.
  sas.testOnlyReset();

  let endDate = Date.now() * 1000;
  let principals = sas.getActiveOrigins(endDate - oneHour, endDate);
  is(principals.length, 0, "starting from clear activity state");

  info("sanitize: " + itemsToClear.join(", "));
  await Sanitizer.sanitize(itemsToClear, { ignoreTimespan: false });

  await createData("example.org");
  await createData("example.com");

  endDate = Date.now() * 1000;
  principals = sas.getActiveOrigins(endDate - oneHour, endDate);
  ok(!!principals, "We have an active origin.");
  ok(principals.length >= 2, "We have an active origin.");

  let found = 0;
  for (let i = 0; i < principals.length; ++i) {
    let principal = principals.queryElementAt(i, Ci.nsIPrincipal);
    if (principal.host == "example.org" || principal.host == "example.com") {
      found++;
    }
  }

  is(found, 2, "Our origins are active.");

  ok(
    await hasIndexedDB("https://example.org"),
    "We have indexedDB data for example.org"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.org"),
    "We have serviceWorker data for example.org"
  );

  ok(
    await hasIndexedDB("https://example.com"),
    "We have indexedDB data for example.com"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.com"),
    "We have serviceWorker data for example.com"
  );

  // Let's move example.com in the past.
  ok(
    moveOriginInTime(principals, endDate, "example.com"),
    "Operation completed!"
  );

  let p = waitForUnregister("example.org");

  // Clear it
  info("sanitize: " + itemsToClear.join(", "));
  await Sanitizer.sanitize(itemsToClear, { ignoreTimespan: false });
  await p;

  ok(
    !(await hasIndexedDB("https://example.org")),
    "We don't have indexedDB data for example.org"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://example.org"),
    "We don't have serviceWorker data for example.org"
  );

  ok(
    await hasIndexedDB("https://example.com"),
    "We still have indexedDB data for example.com"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.com"),
    "We still have serviceWorker data for example.com"
  );

  // We have to move example.com in the past because how we check IDB triggers
  // a storage activity.
  ok(
    moveOriginInTime(principals, endDate, "example.com"),
    "Operation completed!"
  );

  // Let's call the clean up again.
  info("sanitize again to ensure clearing doesn't expand the activity scope");
  await Sanitizer.sanitize(itemsToClear, { ignoreTimespan: false });

  ok(
    await hasIndexedDB("https://example.com"),
    "We still have indexedDB data for example.com"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.com"),
    "We still have serviceWorker data for example.com"
  );

  ok(
    !(await hasIndexedDB("https://example.org")),
    "We don't have indexedDB data for example.org"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://example.org"),
    "We don't have serviceWorker data for example.org"
  );

  sas.testOnlyReset();

  // Clean up.
  await Sanitizer.sanitize(itemsToClear);
});

add_task(async function testExceptionsOnShutdown() {
  await createData("example.org");
  await createData("example.com");

  // Set exception for example.org to not get cleaned
  let originALLOW = "https://example.org";
  PermissionTestUtils.add(
    originALLOW,
    "cookie",
    Ci.nsICookiePermission.ACCESS_ALLOW
  );

  ok(
    await hasIndexedDB("https://example.org"),
    "We have indexedDB data for example.org"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.org"),
    "We have serviceWorker data for example.org"
  );

  ok(
    await hasIndexedDB("https://example.com"),
    "We have indexedDB data for example.com"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.com"),
    "We have serviceWorker data for example.com"
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.sanitizer.loglevel", "All"],
      ["privacy.clearOnShutdown.offlineApps", true],
      ["privacy.sanitize.sanitizeOnShutdown", true],
    ],
  });
  // Clear it
  await Sanitizer.runSanitizeOnShutdown();
  // Data for example.org should not have been cleared
  ok(
    await hasIndexedDB("https://example.org"),
    "We still have indexedDB data for example.org"
  );
  ok(
    SiteDataTestUtils.hasServiceWorkers("https://example.org"),
    "We still have serviceWorker data for example.org"
  );
  // Data for example.com should be cleared
  ok(
    !(await hasIndexedDB("https://example.com")),
    "We don't have indexedDB data for example.com"
  );
  ok(
    !SiteDataTestUtils.hasServiceWorkers("https://example.com"),
    "We don't have serviceWorker data for example.com"
  );
});
