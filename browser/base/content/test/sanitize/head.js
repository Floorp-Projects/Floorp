var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.jsm",
  FormHistory: "resource://gre/modules/FormHistory.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
  SiteDataTestUtils: "resource://testing-common/SiteDataTestUtils.jsm",
});

function createIndexedDB(host, originAttributes) {
  let uri = Services.io.newURI("https://" + host);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    originAttributes
  );
  return SiteDataTestUtils.addToIndexedDB(principal.origin);
}

function checkIndexedDB(host, originAttributes) {
  return new Promise(resolve => {
    let data = true;
    let uri = Services.io.newURI("https://" + host);
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      originAttributes
    );
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
  Services.cookies.add(
    host,
    "/test",
    "foo",
    "bar",
    false,
    false,
    false,
    Date.now() + 24000 * 60 * 60,
    originAttributes,
    Ci.nsICookie.SAMESITE_NONE
  );
}

function createDomainCookie(host, originAttributes) {
  Services.cookies.add(
    "." + host,
    "/test",
    "foo",
    "bar",
    false,
    false,
    false,
    Date.now() + 24000 * 60 * 60,
    originAttributes,
    Ci.nsICookie.SAMESITE_NONE
  );
}

function checkCookie(host, originAttributes) {
  for (let cookie of Services.cookies.enumerator) {
    if (
      ChromeUtils.isOriginAttributesEqual(
        originAttributes,
        cookie.originAttributes
      ) &&
      cookie.host.includes(host)
    ) {
      return true;
    }
  }
  return false;
}

async function deleteOnShutdown(opt) {
  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.lifetimePolicy", opt.lifetimePolicy],
      ["browser.sanitizer.loglevel", "All"],
    ],
  });

  // Custom permission without considering OriginAttributes
  if (opt.cookiePermission !== undefined) {
    let uri = Services.io.newURI("https://www.example.com");
    Services.perms.add(uri, "cookie", opt.cookiePermission);
  }

  // Let's create a tab with some data.
  await opt.createData(
    (opt.fullHost ? "www." : "") + "example.org",
    opt.originAttributes
  );
  ok(
    await opt.checkData(
      (opt.fullHost ? "www." : "") + "example.org",
      opt.originAttributes
    ),
    "We have data for www.example.org"
  );
  await opt.createData(
    (opt.fullHost ? "www." : "") + "example.com",
    opt.originAttributes
  );
  ok(
    await opt.checkData(
      (opt.fullHost ? "www." : "") + "example.com",
      opt.originAttributes
    ),
    "We have data for www.example.com"
  );

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  // All gone!
  is(
    !!(await opt.checkData(
      (opt.fullHost ? "www." : "") + "example.org",
      opt.originAttributes
    )),
    opt.expectedForOrg,
    "Do we have data for www.example.org?"
  );
  is(
    !!(await opt.checkData(
      (opt.fullHost ? "www." : "") + "example.com",
      opt.originAttributes
    )),
    opt.expectedForCom,
    "Do we have data for www.example.com?"
  );

  // Clean up.
  await Sanitizer.sanitize(["cookies", "offlineApps"]);

  if (opt.cookiePermission !== undefined) {
    let uri = Services.io.newURI("https://www.example.com");
    Services.perms.remove(uri, "cookie");
  }
}

function runAllCookiePermissionTests(originAttributes) {
  let tests = [
    { name: "IDB", createData: createIndexedDB, checkData: checkIndexedDB },
    {
      name: "Host Cookie",
      createData: createHostCookie,
      checkData: checkCookie,
    },
    {
      name: "Domain Cookie",
      createData: createDomainCookie,
      checkData: checkCookie,
    },
  ];

  // Delete all, no custom permission, data in example.com, cookie permission set
  // for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageOnShutdown() {
      info(
        methods.name +
          ": Delete all, no custom permission, data in example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
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

  // Delete all, no custom permission, data in www.example.com, cookie permission
  // set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageOnShutdown() {
      info(
        methods.name +
          ": Delete all, no custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
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

  // All is session, but with ALLOW custom permission, data in example.com,
  // cookie permission set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageWithCustomPermission() {
      info(
        methods.name +
          ": All is session, but with ALLOW custom permission, data in example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
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

  // All is session, but with ALLOW custom permission, data in www.example.com,
  // cookie permission set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageWithCustomPermission() {
      info(
        methods.name +
          ": All is session, but with ALLOW custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
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

  // All is default, but with SESSION custom permission, data in example.com,
  // cookie permission set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageOnlyCustomPermission() {
      info(
        methods.name +
          ": All is default, but with SESSION custom permission, data in example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        lifetimePolicy: Ci.nsICookieService.ACCEPT_NORMALLY,
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

  // All is default, but with SESSION custom permission, data in www.example.com,
  // cookie permission set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageOnlyCustomPermission() {
      info(
        methods.name +
          ": All is default, but with SESSION custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        lifetimePolicy: Ci.nsICookieService.ACCEPT_NORMALLY,
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

  // Session mode, but with unsupported custom permission, data in
  // www.example.com, cookie permission set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageOnlyCustomPermission() {
      info(
        methods.name +
          ": All is session only, but with unsupported custom custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        lifetimePolicy: Ci.nsICookieService.ACCEPT_SESSION,
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
}
