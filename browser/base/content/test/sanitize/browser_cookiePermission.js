ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Sanitizer} = ChromeUtils.import("resource:///modules/Sanitizer.jsm", {});
const {SiteDataTestUtils} = ChromeUtils.import("resource://testing-common/SiteDataTestUtils.jsm", {});

function checkDataForAboutURL() {
  return new Promise(resolve => {
    let data = true;
    let uri = Services.io.newURI("about:newtab");
    let principal =
      Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
    let request = indexedDB.openForPrincipal(principal, "TestDatabase", 1);
    request.onupgradeneeded = function(e) {
      data = false;
    };
    request.onsuccess = function(e) {
      resolve(data);
    };
  });
}

function createIndexedDB(host, originAttributes) {
  return SiteDataTestUtils.addToIndexedDB("https://" + host, "foo", "bar",
                                          originAttributes);
}

function checkIndexedDB(host, originAttributes) {
  return new Promise(resolve => {
    let data = true;
    let uri = Services.io.newURI("https://" + host);
    let principal =
      Services.scriptSecurityManager.createCodebasePrincipal(uri,
                                                             originAttributes);
    let request = indexedDB.openForPrincipal(principal, "TestDatabase", 1);
    request.onupgradeneeded = function(e) {
      data = false;
    };
    request.onsuccess = function(e) {
      resolve(data);
    };
  });
}

function createHostCookie(host, originAttributes) {
  Services.cookies.add(host, "/test", "foo", "bar",
    false, false, false, Date.now() + 24000 * 60 * 60, originAttributes,
    Ci.nsICookie2.SAMESITE_UNSET);
}

function createDomainCookie(host, originAttributes) {
  Services.cookies.add("." + host, "/test", "foo", "bar",
    false, false, false, Date.now() + 24000 * 60 * 60, originAttributes,
    Ci.nsICookie2.SAMESITE_UNSET);
}

function checkCookie(host, originAttributes) {
  for (let cookie of Services.cookies.enumerator) {
    if (ChromeUtils.isOriginAttributesEqual(originAttributes,
                                            cookie.originAttributes) &&
        cookie.host.includes(host)) {
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

  // Custom permission without considering OriginAttributes
  if (opt.cookiePermission !== undefined) {
    let uri = Services.io.newURI("https://www.example.com");
    Services.perms.add(uri, "cookie", opt.cookiePermission);
  }

  // Let's create a tab with some data.
  await opt.createData((opt.fullHost ? "www." : "") + "example.org",
                       opt.originAttributes);
  ok(await opt.checkData((opt.fullHost ? "www." : "") + "example.org",
                         opt.originAttributes),
                         "We have data for www.example.org");
  await opt.createData((opt.fullHost ? "www." : "") + "example.com",
                       opt.originAttributes);
  ok(await opt.checkData((opt.fullHost ? "www." : "") + "example.com",
                         opt.originAttributes),
                         "We have data for www.example.com");

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  // All gone!
  is(!!(await opt.checkData((opt.fullHost ? "www." : "") + "example.org",
                            opt.originAttributes)),
                            opt.expectedForOrg, "Do we have data for www.example.org?");
  is(!!(await opt.checkData((opt.fullHost ? "www." : "") + "example.com",
                            opt.originAttributes)),
                            opt.expectedForCom, "Do we have data for www.example.com?");

  // Clean up.
  await Sanitizer.sanitize([ "cookies", "offlineApps" ]);

  if (opt.cookiePermission !== undefined) {
    let uri = Services.io.newURI("https://www.example.com");
    Services.perms.remove(uri, "cookie");
  }
}

let tests = [
  { name: "IDB",
    createData: createIndexedDB,
    checkData: checkIndexedDB },
  { name: "Host Cookie",
    createData: createHostCookie,
    checkData: checkCookie },
  { name: "Domain Cookie",
    createData: createDomainCookie,
    checkData: checkCookie },
];

let attributes = [
  {name: "default", oa: {}},
  {name: "container", oa: {userContextId: 1}},
];

// Delete all, no custom permission, data in example.com, cookie permission set
// for www.example.com
tests.forEach(methods => {
  attributes.forEach(originAttributes => {
    add_task(async function deleteStorageOnShutdown() {
      info(methods.name + ": Delete all, no custom permission, data in example.com, cookie permission set for www.example.com - OA: " + originAttributes.name);
      await deleteOnShutdown(
        { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
          createData: methods.createData,
          checkData: methods.checkData,
          originAttributes: originAttributes.oa,
          cookiePermission: undefined,
          expectedForOrg: false,
          expectedForCom: false,
          fullHost: false,
        });
    });
  });
});

// Delete all, no custom permission, data in www.example.com, cookie permission
// set for www.example.com
tests.forEach(methods => {
  attributes.forEach(originAttributes => {
    add_task(async function deleteStorageOnShutdown() {
      info(methods.name + ": Delete all, no custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " + originAttributes.name);
      await deleteOnShutdown(
        { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
          createData: methods.createData,
          checkData: methods.checkData,
          originAttributes: originAttributes.oa,
          cookiePermission: undefined,
          expectedForOrg: false,
          expectedForCom: false,
          fullHost: true,
        });
    });
  });
});

// All is session, but with ALLOW custom permission, data in example.com,
// cookie permission set for www.example.com
tests.forEach(methods => {
  attributes.forEach(originAttributes => {
    add_task(async function deleteStorageWithCustomPermission() {
      info(methods.name + ": All is session, but with ALLOW custom permission, data in example.com, cookie permission set for www.example.com - OA: " + originAttributes.name);
      await deleteOnShutdown(
        { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
          createData: methods.createData,
          checkData: methods.checkData,
          originAttributes: originAttributes.oa,
          cookiePermission: Ci.nsICookiePermission.ACCESS_ALLOW,
          expectedForOrg: false,
          expectedForCom: true,
          fullHost: false,
        });
    });
  });
});

// All is session, but with ALLOW custom permission, data in www.example.com,
// cookie permission set for www.example.com
tests.forEach(methods => {
  attributes.forEach(originAttributes => {
    add_task(async function deleteStorageWithCustomPermission() {
      info(methods.name + ": All is session, but with ALLOW custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " + originAttributes.name);
      await deleteOnShutdown(
        { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
          createData: methods.createData,
          checkData: methods.checkData,
          originAttributes: originAttributes.oa,
          cookiePermission: Ci.nsICookiePermission.ACCESS_ALLOW,
          expectedForOrg: false,
          expectedForCom: true,
          fullHost: true,
        });
    });
  });
});

// All is default, but with SESSION custom permission, data in example.com,
// cookie permission set for www.example.com
tests.forEach(methods => {
  attributes.forEach(originAttributes => {
    add_task(async function deleteStorageOnlyCustomPermission() {
      info(methods.name + ": All is default, but with SESSION custom permission, data in example.com, cookie permission set for www.example.com - OA: " + originAttributes.name);
      await deleteOnShutdown(
        { lifetimePolicy: Ci.nsICookieService.ACCEPT_NORMALLY,
          createData: methods.createData,
          checkData: methods.checkData,
          originAttributes: originAttributes.oa,
          cookiePermission: Ci.nsICookiePermission.ACCESS_SESSION,
          expectedForOrg: true,
          // expected data just for example.com when using indexedDB because
          // QuotaManager deletes for principal.
          expectedForCom: false,
          fullHost: false,
        });
    });
  });
});

// All is default, but with SESSION custom permission, data in www.example.com,
// cookie permission set for www.example.com
tests.forEach(methods => {
  attributes.forEach(originAttributes => {
    add_task(async function deleteStorageOnlyCustomPermission() {
      info(methods.name + ": All is default, but with SESSION custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " + originAttributes.name);
      await deleteOnShutdown(
        { lifetimePolicy: Ci.nsICookieService.ACCEPT_NORMALLY,
          createData: methods.createData,
          checkData: methods.checkData,
          originAttributes: originAttributes.oa,
          cookiePermission: Ci.nsICookiePermission.ACCESS_SESSION,
          expectedForOrg: true,
          expectedForCom: false,
          fullHost: true,
        });
    });
  });
});

// Session mode, but with unsupported custom permission, data in
// www.example.com, cookie permission set for www.example.com
tests.forEach(methods => {
  attributes.forEach(originAttributes => {
    add_task(async function deleteStorageOnlyCustomPermission() {
      info(methods.name + ": All is session only, but with unsupported custom custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " + originAttributes.name);
      await deleteOnShutdown(
        { lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
          createData: methods.createData,
          checkData: methods.checkData,
          originAttributes: originAttributes.oa,
          cookiePermission: 123, // invalid cookie permission
          expectedForOrg: false,
          expectedForCom: false,
          fullHost: true,
        });
    });
  });
});

add_task(async function deleteStorageInAboutURL() {
  info("Test about:newtab");

  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });

  await SpecialPowers.pushPrefEnv({"set": [
    ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_SESSION],
  ]});

  // Let's create a tab with some data.
  await SiteDataTestUtils.addToIndexedDB("about:newtab", "foo", "bar", {});

  ok(await checkDataForAboutURL(), "We have data for about:newtab");

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  ok(await checkDataForAboutURL(), "about:newtab data is not deleted.");

  // Clean up.
  await Sanitizer.sanitize([ "cookies", "offlineApps" ]);

  let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin("about:newtab");
  await new Promise(aResolve => {
    let req = Services.qms.clearStoragesForPrincipal(principal);
    req.callback = () => { aResolve(); };
  });
});

add_task(async function deleteStorageOnlyCustomPermissionInAboutURL() {
  info("Test about:newtab + permissions");

  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });

  await SpecialPowers.pushPrefEnv({"set": [
    ["network.cookie.lifetimePolicy", Ci.nsICookieService.ACCEPT_NORMALLY],
  ]});

  // Custom permission without considering OriginAttributes
  let uri = Services.io.newURI("about:newtab");
  Services.perms.add(uri, "cookie", Ci.nsICookiePermission.ACCESS_SESSION);

  // Let's create a tab with some data.
  await SiteDataTestUtils.addToIndexedDB("about:newtab", "foo", "bar", {});

  ok(await checkDataForAboutURL(), "We have data for about:newtab");

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  ok(await checkDataForAboutURL(), "about:newtab data is not deleted.");

  // Clean up.
  await Sanitizer.sanitize([ "cookies", "offlineApps" ]);

  let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin("about:newtab");
  await new Promise(aResolve => {
    let req = Services.qms.clearStoragesForPrincipal(principal);
    req.callback = () => { aResolve(); };
  });

  Services.perms.remove(uri, "cookie");
});
