/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const { createHttpServer } = AddonTestUtils;

// Force ServiceWorkerRegistrar to init by calling do_get_profile.
// (This has to be called before AddonTestUtils.init, because it does
// also call do_get_profile internally but it doesn't notify
// profile-after-change).
do_get_profile(true);

AddonTestUtils.init(this);

const server = createHttpServer({ hosts: ["localhost"] });

server.registerPathHandler("/sw.js", (request, response) => {
  info(`/sw.js is being requested: ${JSON.stringify(request)}`);
  response.setHeader("Content-Type", "application/javascript");
  response.write("");
});

add_task(async function setup_prefs() {
  // Enable nsIServiceWorkerManager.registerForTest.
  Services.prefs.setBoolPref("dom.serviceWorkers.testing.enabled", true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("dom.serviceWorkers.testing.enabled");
  });
});

/**
 * This test installs a ServiceWorker via test API and verify that the install
 * process spawns a new process. (Normally ServiceWorker installation won't
 * cause a new content process to be spawned because the call to register must
 * be coming from within an existing content process, but the registerForTest
 * API allows us to bypass this restriction.)
 *
 * This models the real-world situation of a push notification being received
 * from the network which results in a ServiceWorker being spawned without their
 * necessarily being an existing content process to host it (especially under Fission).
 */
add_task(async function launch_remoteworkers_in_new_processes() {
  const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );

  const ssm = Services.scriptSecurityManager;

  const initialChildCount = Services.ppmm.childCount;

  // A test service worker that should spawn a regular web content child process.
  const swRegInfoWeb = await swm.registerForTest(
    ssm.createContentPrincipal(Services.io.newURI("http://localhost"), {}),
    "http://localhost/scope",
    "http://localhost/sw.js"
  );
  swRegInfoWeb.QueryInterface(Ci.nsIServiceWorkerRegistrationInfo);

  info(
    `web content service worker registered: ${JSON.stringify({
      principal: swRegInfoWeb.principal.spec,
      scope: swRegInfoWeb.scope,
    })}`
  );

  info("Wait new process to be launched");
  await TestUtils.waitForCondition(() => {
    return Services.ppmm.childCount - initialChildCount >= 1;
  }, "wait for a new child processes to be started");

  // Wait both workers to become active to be sure that. besides spawning
  // the new child processes as expected, the two remote worker have been
  // able to run successfully (in other word their remote worker data did
  // pass successfull the IsRemoteTypeAllowed check in RemoteworkerChild).
  info("Wait for webcontent worker to become active");
  await TestUtils.waitForCondition(
    () => swRegInfoWeb.activeWorker,
    `wait workers for scope ${swRegInfoWeb.scope} to be active`
  );
});
