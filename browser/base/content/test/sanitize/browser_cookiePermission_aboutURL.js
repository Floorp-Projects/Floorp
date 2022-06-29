const { Sanitizer } = ChromeUtils.import("resource:///modules/Sanitizer.jsm");
const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);

function checkDataForAboutURL() {
  return new Promise(resolve => {
    let data = true;
    let uri = Services.io.newURI("about:newtab");
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
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
  await Sanitizer.sanitize(["cookies", "offlineApps"]);

  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
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
  PermissionTestUtils.add(uri, "cookie", Ci.nsICookiePermission.ACCESS_SESSION);

  // Let's create a tab with some data.
  await SiteDataTestUtils.addToIndexedDB("about:newtab", "foo", "bar", {});

  ok(await checkDataForAboutURL(), "We have data for about:newtab");

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  ok(await checkDataForAboutURL(), "about:newtab data is not deleted.");

  // Clean up.
  await Sanitizer.sanitize(["cookies", "offlineApps"]);

  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
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
