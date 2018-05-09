ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Sanitizer} = ChromeUtils.import("resource:///modules/Sanitizer.jsm", {});
const {SiteDataTestUtils} = ChromeUtils.import("resource://testing-common/SiteDataTestUtils.jsm", {});

function hasIndexedDB(origin) {
  return new Promise(resolve => {
    let hasData = true;
    let uri = Services.io.newURI(origin);
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
    let request = indexedDB.openForPrincipal(principal, "TestDatabase", 1);
    request.onupgradeneeded = function(e) {
      hasData = false;
    };
    request.onsuccess = function(e) {
      resolve(hasData);
    };
  });
}

async function createData(host) {
  let origin = "https://" + host;
  await SiteDataTestUtils.addToIndexedDB(origin);
}

add_task(async function deleteAllOnShutdown() {
  // Let's force the session-only cookie pref.
  await SpecialPowers.pushPrefEnv({"set": [
    ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_SESSION],
  ]});

  // Let's create a tab with some IDB data.
  await createData("example.org");
  ok(await hasIndexedDB("https://example.org"),
    "We have indexedDB data for example.org");

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  // All gone!
  ok(!(await hasIndexedDB("https://example.org")),
    "We don't have indexedDB data for example.org");

  // Clean up.
  await Sanitizer.sanitize([ "cookies", "offlineApps" ]);
});

add_task(async function deleteAllWithCustomPermission() {
  // Let's force the session-only cookie pref.
  await SpecialPowers.pushPrefEnv({"set": [
    ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_SESSION],
  ]});

  let uri = Services.io.newURI("https://example.com");
  Services.perms.add(uri, "cookie", Ci.nsICookiePermission.ACCESS_ALLOW);

  // Let's create a couple of tabs with some IDB data.
  await createData("example.org");
  ok(await hasIndexedDB("https://example.org"),
    "We have indexedDB data for example.org");
  await createData("example.com");
  ok(await hasIndexedDB("https://example.com"),
    "We have indexedDB data for example.com");

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  // All gone!
  ok(!(await hasIndexedDB("https://example.org")),
    "We don't have indexedDB data for example.org");
  ok(await hasIndexedDB("https://example.com"),
    "We do have indexedDB data for example.com");

  // Clean up.
  await Sanitizer.sanitize([ "cookies", "offlineApps" ]);

  // Remove custom permission
  uri = Services.io.newURI("https://example.com");
  Services.perms.remove(uri, "cookie");
});

add_task(async function deleteOnlyCustomPermission() {
  // Let's force the session-only cookie pref.
  await SpecialPowers.pushPrefEnv({"set": [
    ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_NORMALLY],
  ]});

  let uri = Services.io.newURI("https://example.com");
  Services.perms.add(uri, "cookie", Ci.nsICookiePermission.ACCESS_SESSION);

  // Let's create a couple of tabs with some IDB data.
  await createData("example.org");
  ok(await hasIndexedDB("https://example.org"),
    "We have indexedDB data for example.org");
  await createData("example.com");
  ok(await hasIndexedDB("https://example.com"),
    "We have indexedDB data for example.com");

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  // All gone!
  ok(await hasIndexedDB("https://example.org"),
    "We do have indexedDB data for example.org");
  ok(!await hasIndexedDB("https://example.com"),
    "We don't have indexedDB data for example.com");

  // Clean up.
  await Sanitizer.sanitize([ "cookies", "offlineApps" ]);

  // Remove custom permission
  uri = Services.io.newURI("https://example.com");
  Services.perms.remove(uri, "cookie");
});
