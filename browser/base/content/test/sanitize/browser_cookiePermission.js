const STORAGE = "storage";
const HOST_COOKIE = "host cookie";
const DOMAIN_COOKIE = "domain cookie";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Sanitizer} = ChromeUtils.import("resource:///modules/Sanitizer.jsm", {});
const {SiteDataTestUtils} = ChromeUtils.import("resource://testing-common/SiteDataTestUtils.jsm", {});

function hasIndexedDB(origin) {
  return new Promise(resolve => {
    let data = true;
    let uri = Services.io.newURI(origin);
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
    let request = indexedDB.openForPrincipal(principal, "TestDatabase", 1);
    request.onupgradeneeded = function(e) {
      data = false;
    };
    request.onsuccess = function(e) {
      resolve(data);
    };
  });
}

async function createData(host, what) {
  if (what == STORAGE) {
    let origin = "https://" + host;
    await SiteDataTestUtils.addToIndexedDB(origin);
    return;
  }

  if (what == HOST_COOKIE) {
    Services.cookies.add(host, "/test1", "foo", "bar",
      false, false, false, Date.now() + 24000 * 60 * 60, {},
      Ci.nsICookie2.SAMESITE_UNSET);
    return;
  }

  if (what == DOMAIN_COOKIE) {
    Services.cookies.add("." + host, "/test1", "foo", "bar",
      false, false, false, Date.now() + 24000 * 60 * 60, {},
      Ci.nsICookie2.SAMESITE_UNSET);
    return;
  }

  ok(false, "Invalid arguments");
}

async function hasData(host, what) {
  if (what == STORAGE) {
    return hasIndexedDB("https://" + host);
  }

  if (what == HOST_COOKIE || what == DOMAIN_COOKIE) {
    for (let cookie of Services.cookies.enumerator) {
      if (cookie.host.includes(host)) {
        return true;
      }
    }
    return false;
  }

  ok(false, "Invalid arguments");
  return false;
}

async function deleteOnShutdown(what) {
  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });

  // Let's force the session-only cookie pref.
  await SpecialPowers.pushPrefEnv({"set": [
    ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_SESSION],
  ]});

  // Let's create a tab with some IDB data.
  await createData("example.org", what);
  ok(await hasData("example.org", what), "We have data for example.org");

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  // All gone!
  ok(!(await hasData("example.org", what)), "We don't have data for example.org");

  // Clean up.
  await Sanitizer.sanitize([ "cookies", "offlineApps" ]);
}

add_task(async function deleteStorageOnShutdown() { await deleteOnShutdown(STORAGE); });
add_task(async function deleteHostCookieOnShutdown() { await deleteOnShutdown(HOST_COOKIE); });
add_task(async function deleteDomainCookieOnShutdown() { await deleteOnShutdown(DOMAIN_COOKIE); });

async function deleteWithCustomPermission(what) {
  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });

  // Let's force the session-only cookie pref.
  await SpecialPowers.pushPrefEnv({"set": [
    ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_SESSION],
  ]});

  let uri = Services.io.newURI("https://example.com");
  Services.perms.add(uri, "cookie", Ci.nsICookiePermission.ACCESS_ALLOW);

  // Let's create a couple of tabs with some IDB data.
  await createData("example.org", what);
  ok(await hasData("example.org", what), "We have data for example.org");
  await createData("example.com", what);
  ok(await hasData("example.com", what), "We have data for example.com");

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  // All gone!
  ok(!(await hasData("example.org", what)), "We don't have data for example.org");
  ok(await hasData("example.com", what), "We do have data for example.com");

  // Clean up.
  await Sanitizer.sanitize([ "cookies", "offlineApps" ]);

  // Remove custom permission
  uri = Services.io.newURI("https://example.com");
  Services.perms.remove(uri, "cookie");
}

add_task(async function deleteStorageWithCustomPermission() { await deleteWithCustomPermission(STORAGE); });
add_task(async function deleteHostCookieWithCustomPermission() { await deleteWithCustomPermission(HOST_COOKIE); });
add_task(async function deleteDomainCookieWithCustomPermission() { await deleteWithCustomPermission(DOMAIN_COOKIE); });

async function deleteOnlyCustomPermission(what) {
  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });

  // Let's force the session-only cookie pref.
  await SpecialPowers.pushPrefEnv({"set": [
    ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_NORMALLY],
  ]});

  let uri = Services.io.newURI("https://example.com");
  Services.perms.add(uri, "cookie", Ci.nsICookiePermission.ACCESS_SESSION);

  // Let's create a couple of tabs with some IDB data.
  await createData("example.org", what);
  ok(await hasData("example.org", what), "We have data for example.org");
  await createData("example.com", what);
  ok(await hasData("example.com", what), "We have data for example.com");

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  // All gone!
  ok(await hasData("example.org", what), "We do have for example.org");
  ok(!await hasData("example.com", what), "We don't have data for example.com");

  // Clean up.
  await Sanitizer.sanitize([ "cookies", "offlineApps" ]);

  // Remove custom permission
  uri = Services.io.newURI("https://example.com");
  Services.perms.remove(uri, "cookie");
}

add_task(async function deleteStorageOnlyCustomPermission() { await deleteOnlyCustomPermission(STORAGE); });
add_task(async function deleteHostCookieOnlyCustomPermission() { await deleteOnlyCustomPermission(HOST_COOKIE); });
add_task(async function deleteDomainCookieOnlyCustomPermission() { await deleteOnlyCustomPermission(DOMAIN_COOKIE); });
