const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

// We will be removing the ["cookies","offlineApps"] option once we remove the
// old clear history dialog in Bug 1856418 - Remove all old clear data dialog boxes
let prefs = [["cookiesAndStorage"], ["cookies", "offlineApps"]];

function checkDataForAboutURL() {
  return new Promise(resolve => {
    let data = true;
    let uri = Services.io.newURI("about:newtab");
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );
    let request = indexedDB.openForPrincipal(principal, "TestDatabase", 1);
    request.onupgradeneeded = function (e) {
      data = false;
    };
    request.onsuccess = function (e) {
      resolve(data);
    };
  });
}

for (let itemsToClear of prefs) {
  add_task(async function deleteStorageInAboutURL() {
    info("Test about:newtab");

    // Let's clean up all the data.
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
    });

    await SpecialPowers.pushPrefEnv({
      set: [["browser.sanitizer.loglevel", "All"]],
    });

    // Let's create a tab with some data.
    await SiteDataTestUtils.addToIndexedDB("about:newtab", "foo", "bar", {});

    ok(await checkDataForAboutURL(), "We have data for about:newtab");

    // Cleaning up.
    await Sanitizer.runSanitizeOnShutdown();

    ok(await checkDataForAboutURL(), "about:newtab data is not deleted.");

    // Clean up.
    await Sanitizer.sanitize(itemsToClear);

    let principal =
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        "about:newtab"
      );
    await new Promise(aResolve => {
      let req = Services.qms.clearStoragesForPrincipal(principal);
      req.callback = () => {
        aResolve();
      };
    });
  });

  add_task(async function deleteStorageOnlyCustomPermissionInAboutURL() {
    info("Test about:newtab + permissions");

    // Let's clean up all the data.
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
    });

    await SpecialPowers.pushPrefEnv({
      set: [["browser.sanitizer.loglevel", "All"]],
    });

    // Custom permission without considering OriginAttributes
    let uri = Services.io.newURI("about:newtab");
    PermissionTestUtils.add(
      uri,
      "cookie",
      Ci.nsICookiePermission.ACCESS_SESSION
    );

    // Let's create a tab with some data.
    await SiteDataTestUtils.addToIndexedDB("about:newtab", "foo", "bar", {});

    ok(await checkDataForAboutURL(), "We have data for about:newtab");

    // Cleaning up.
    await Sanitizer.runSanitizeOnShutdown();

    ok(await checkDataForAboutURL(), "about:newtab data is not deleted.");

    // Clean up.
    await Sanitizer.sanitize(itemsToClear);

    let principal =
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        "about:newtab"
      );
    await new Promise(aResolve => {
      let req = Services.qms.clearStoragesForPrincipal(principal);
      req.callback = () => {
        aResolve();
      };
    });

    PermissionTestUtils.remove(uri, "cookie");
  });
}
