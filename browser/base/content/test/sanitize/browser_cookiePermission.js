ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Sanitizer} = ChromeUtils.import("resource:///modules/Sanitizer.jsm", {});
const {SiteDataTestUtils} = ChromeUtils.import("resource://testing-common/SiteDataTestUtils.jsm", {});

function createIndexedDB(host) {
  return SiteDataTestUtils.addToIndexedDB("https://" + host);
}

function checkIndexedDB(host) {
  return new Promise(resolve => {
    let data = true;
    let uri = Services.io.newURI("https://" + host);
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

function createHostCookie(host) {
  Services.cookies.add(host, "/test", "foo", "bar",
    false, false, false, Date.now() + 24000 * 60 * 60, {},
    Ci.nsICookie2.SAMESITE_UNSET);
}

function createDomainCookie(host) {
  Services.cookies.add("." + host, "/test", "foo", "bar",
    false, false, false, Date.now() + 24000 * 60 * 60, {},
    Ci.nsICookie2.SAMESITE_UNSET);
}

function checkCookie(host) {
  for (let cookie of Services.cookies.enumerator) {
    if (cookie.host.includes(host)) {
      return true;
    }
  }
  return false;
}

async function deleteOnShutdown(opt) {
  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });

  await SpecialPowers.pushPrefEnv({"set": [
    ["network.cookie.lifetimePolicy", opt.lifetimePolicy],
  ]});

  // Custom permission.
  if (opt.cookiePermission !== undefined) {
    let uri = Services.io.newURI("https://www.example.com");
    Services.perms.add(uri, "cookie", opt.cookiePermission);
  }

  // Let's create a tab with some data.
  await opt.createData((opt.fullHost ? "www." : "") + "example.org");
  ok(await opt.checkData((opt.fullHost ? "www." : "") + "example.org"),
                         "We have data for www.example.org");
  await opt.createData((opt.fullHost ? "www." : "") + "example.com");
  ok(await opt.checkData((opt.fullHost ? "www." : "") + "example.com"),
                         "We have data for www.example.com");

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  // All gone!
  is(!!(await opt.checkData((opt.fullHost ? "www." : "") + "example.org")),
                            opt.expectedForOrg, "Do we have data for www.example.org?");
  is(!!(await opt.checkData((opt.fullHost ? "www." : "") + "example.com")),
                            opt.expectedForCom, "Do we have data for www.example.com?");

  // Clean up.
  await Sanitizer.sanitize([ "cookies", "offlineApps" ]);

  if (opt.cookiePermission !== undefined) {
    let uri = Services.io.newURI("https://www.example.com");
    Services.perms.remove(uri, "cookie");
  }
}

// IDB: Delete all, no custom permission, data in example.com, cookie
// permission set for www.example.com
add_task(async function deleteStorageOnShutdown() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createIndexedDB,
      checkData: checkIndexedDB,
      cookiePermission: undefined,
      expectedForOrg: false,
      expectedForCom: false,
      fullHost: false,
    });
});

// IDB: Delete all, no custom permission, data in www.example.com, cookie
// permission set for www.example.com
add_task(async function deleteStorageOnShutdown() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createIndexedDB,
      checkData: checkIndexedDB,
      cookiePermission: undefined,
      expectedForOrg: false,
      expectedForCom: false,
      fullHost: true,
    });
});

// Host Cookie: Delete all, no custom permission, data in example.com, cookie
// permission set for www.example.com
add_task(async function deleteHostCookieOnShutdown() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createHostCookie,
      checkData: checkCookie,
      cookiePermission: undefined,
      expectedForOrg: false,
      expectedForCom: false,
      fullHost: false,
    });
});

// Host Cookie: Delete all, no custom permission, data in www.example.com,
// cookie permission set for www.example.com
add_task(async function deleteHostCookieOnShutdown() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createHostCookie,
      checkData: checkCookie,
      cookiePermission: undefined,
      expectedForOrg: false,
      expectedForCom: false,
      fullHost: true,
    });
});

// Domain Cookie: Delete all, no custom permission, data in example.com, cookie
// permission set for www.example.com
add_task(async function deleteDomainCookieOnShutdown() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createDomainCookie,
      checkData: checkCookie,
      cookiePermission: undefined,
      expectedForOrg: false,
      expectedForCom: false,
      fullHost: false,
    });
});

// Domain Cookie: Delete all, no custom permission, data in www.example.com,
// cookie permission set for www.example.com
add_task(async function deleteDomainCookieOnShutdown() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createDomainCookie,
      checkData: checkCookie,
      cookiePermission: undefined,
      expectedForOrg: false,
      expectedForCom: false,
      fullHost: true,
    });
});

// IDB: All is session, but with ALLOW custom permission, data in example.com,
// cookie permission set for www.example.com
add_task(async function deleteStorageWithCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createIndexedDB,
      checkData: checkIndexedDB,
      cookiePermission: Ci.nsICookiePermission.ACCESS_ALLOW,
      expectedForOrg: false,
      expectedForCom: true,
      fullHost: false,
    });
});

// IDB: All is session, but with ALLOW custom permission, data in
// www.example.com, cookie permission set for www.example.com
add_task(async function deleteStorageWithCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createIndexedDB,
      checkData: checkIndexedDB,
      cookiePermission: Ci.nsICookiePermission.ACCESS_ALLOW,
      expectedForOrg: false,
      expectedForCom: true,
      fullHost: true,
    });
});

// Host Cookie: All is session, but with ALLOW custom permission, data in
// example.com, cookie permission set for www.example.com
add_task(async function deleteHostCookieWithCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createHostCookie,
      checkData: checkCookie,
      cookiePermission: Ci.nsICookiePermission.ACCESS_ALLOW,
      expectedForOrg: false,
      expectedForCom: true,
      fullHost: false,
    });
});

// Host Cookie: All is session, but with ALLOW custom permission, data in
// www.example.com, cookie permission set for www.example.com
add_task(async function deleteHostCookieWithCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createHostCookie,
      checkData: checkCookie,
      cookiePermission: Ci.nsICookiePermission.ACCESS_ALLOW,
      expectedForOrg: false,
      expectedForCom: true,
      fullHost: true,
    });
});

// Domain Cookie: All is session, but with ALLOW custom permission, data in
// example.com, cookie permission set for www.example.com
add_task(async function deleteDomainCookieWithCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createDomainCookie,
      checkData: checkCookie,
      cookiePermission: Ci.nsICookiePermission.ACCESS_ALLOW,
      expectedForOrg: false,
      expectedForCom: true,
      fullHost: false,
    });
});

// Domain Cookie: All is session, but with ALLOW custom permission, data in
// www.example.com, cookie permission set for www.example.com
add_task(async function deleteDomainCookieWithCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
      createData: createDomainCookie,
      checkData: checkCookie,
      cookiePermission: Ci.nsICookiePermission.ACCESS_ALLOW,
      expectedForOrg: false,
      expectedForCom: true,
      fullHost: true,
    });
});

// IDB: All is default, but with SESSION custom permission, data in
// example.com, cookie permission set for www.example.com
add_task(async function deleteStorageOnlyCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_NORMALLY,
      createData: createIndexedDB,
      checkData: checkIndexedDB,
      cookiePermission: Ci.nsICookiePermission.ACCESS_SESSION,
      expectedForOrg: true,
      expectedForCom: true,
      fullHost: false,
    });
});

// IDB: All is default, but with SESSION custom permission, data in
// www.example.com, cookie permission set for www.example.com
add_task(async function deleteStorageOnlyCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_NORMALLY,
      createData: createIndexedDB,
      checkData: checkIndexedDB,
      cookiePermission: Ci.nsICookiePermission.ACCESS_SESSION,
      expectedForOrg: true,
      expectedForCom: false,
      fullHost: true,
    });
});

// Host Cookie: All is default, but with SESSION custom permission, data in
// example.com, cookie permission set for www.example.com
add_task(async function deleteHostCookieOnlyCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_NORMALLY,
      createData: createHostCookie,
      checkData: checkCookie,
      cookiePermission: Ci.nsICookiePermission.ACCESS_SESSION,
      expectedForOrg: true,
      expectedForCom: false,
      fullHost: false,
    });
});

// Host Cookie: All is default, but with SESSION custom permission, data in
// www.example.com, cookie permission set for www.example.com
add_task(async function deleteHostCookieOnlyCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_NORMALLY,
      createData: createHostCookie,
      checkData: checkCookie,
      cookiePermission: Ci.nsICookiePermission.ACCESS_SESSION,
      expectedForOrg: true,
      expectedForCom: false,
      fullHost: true,
    });
});

// Domain Cookie: All is default, but with SESSION custom permission, data in
// example.com, cookie permission set for www.example.com
add_task(async function deleteDomainCookieOnlyCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_NORMALLY,
      createData: createDomainCookie,
      checkData: checkCookie,
      cookiePermission: Ci.nsICookiePermission.ACCESS_SESSION,
      expectedForOrg: true,
      expectedForCom: false,
      fullHost: false,
    });
});

// Domain Cookie: All is default, but with SESSION custom permission, data in
// www.example.com, cookie permission set for www.example.com
add_task(async function deleteDomainCookieOnlyCustomPermission() {
  await deleteOnShutdown(
    { lifetimePolicy: Ci.nsICookieService.ACCEPT_NORMALLY,
      createData: createDomainCookie,
      checkData: checkCookie,
      cookiePermission: Ci.nsICookiePermission.ACCESS_SESSION,
      expectedForOrg: true,
      expectedForCom: false,
      fullHost: true,
    });
});
