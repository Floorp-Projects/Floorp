/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource:///modules/AppsUtils.jsm");

add_test(() => {
  let app = {
    name: "TestApp",
    csp: "aCsp",
    installOrigin: "http://installorigin.com",
    origin: "http://www.example.com",
    installTime: Date.now(),
    manifestURL: "http://www.example.com/manifest.webapp",
    appStatus: Ci.nsIPrincipal.APP_STATUS_NOT_INSTALLED,
    removable: false,
    id: 123,
    localId: 123,
    basePath: "/",
    progress: 1.0,
    installState: "installed",
    downloadAvailable: false,
    downloading: false,
    lastUpdateCheck: Date.now(),
    updateTime: Date.now(),
    etag: "aEtag",
    packageEtag: "aPackageEtag",
    manifestHash: "aManifestHash",
    packageHash: "aPackageHash",
    staged: false,
    installerAppId: 345,
    installerIsBrowser: false,
    storeId: "aStoreId",
    storeVersion: 1,
    role: "aRole",
    redirects: "aRedirects",
    kind: "aKind",
    enabled: true,
    sideloaded: false
  };

  let mozapp = new mozIApplication(app);

  Object.keys(app).forEach((key) => {
    if (key == "principal") {
      return;
    }
    Assert.equal(app[key], mozapp[key],
                 "app[" + key + "] should be equal to mozapp[" + key + "]");
  });

  Assert.ok(mozapp.principal, "app principal should exist");
  let expectedPrincipalOrigin = app.origin + "^appId=" + app.localId;
  Assert.equal(mozapp.principal.origin, expectedPrincipalOrigin,
               "app principal origin ok");
  Assert.equal(mozapp.principal.appId, app.localId, "app principal appId ok");
  Assert.equal(mozapp.principal.isInBrowserElement, false,
               "app principal isInBrowserElement ok");
  run_next_test();
});

function run_test() {
  run_next_test();
}
