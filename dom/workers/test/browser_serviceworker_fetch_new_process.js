const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", true],
      ["browser.tabs.remote.separatedMozillaDomains", "example.org"],
      ["dom.ipc.processCount.privilegedmozilla", 1],
      ["dom.ipc.processCount.webIsolated", 1],
      ["dom.ipc.processPrelaunch.enabled", false],
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
      ["dom.serviceworkers.parent_intercept", true],
    ],
  });
});

function countRemoteType(remoteType) {
  return ChromeUtils.getAllDOMProcesses().filter(
    p => p.remoteType == remoteType
  ).length;
}

async function waitForWorkerIdleShutdown(swRegInfo, remoteType) {
  info(`waiting for ${remoteType} procs to shut down`);
  ok(swRegInfo.activeWorker, "worker isn't currently active?");
  is(
    countRemoteType(remoteType),
    1,
    `should have a single ${remoteType} process`
  );

  // Let's not wait too long for the process to shutdown.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.idle_timeout", 0],
      ["dom.serviceWorkers.idle_extended_timeout", 0],
    ],
  });

  // We need to cause the worker to re-evaluate its idle timeout. The easiest
  // way to do this I could think of is to attach and then detach the debugger
  // from the active worker.
  swRegInfo.activeWorker.attachDebugger();
  await new Promise(resolve => Cu.dispatch(resolve));
  swRegInfo.activeWorker.detachDebugger();

  // Eventually the length will reach 0, meaning we're done!
  await TestUtils.waitForCondition(
    () => countRemoteType(remoteType) == 0,
    "wait for the worker's process to shutdown"
  );

  is(
    countRemoteType(remoteType),
    0,
    "processes with `remoteType` type should have shut down"
  );

  // Make sure we never kill workers on idle except when this is called.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.idle_timeout", 299999],
      ["dom.serviceWorkers.idle_extended_timeout", 299999],
    ],
  });
}

async function do_test_sw(host, remoteType) {
  info(`entering test: host=${host}, remoteType=${remoteType}`);

  const prin = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(`https://${host}`),
    {}
  );
  const sw = `https://${host}/${DIRPATH}file_service_worker_fetch_synthetic.js`;
  const scope = `https://${host}/${DIRPATH}scope`;

  const swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );
  const swRegInfo = await swm.registerForTest(prin, scope, sw);
  swRegInfo.QueryInterface(Ci.nsIServiceWorkerRegistrationInfo);

  info(
    `service worker registered: ${JSON.stringify({
      principal: swRegInfo.principal.spec,
      scope: swRegInfo.scope,
    })}`
  );

  // Wait for the worker to install & shut down.
  await TestUtils.waitForCondition(
    () => swRegInfo.activeWorker,
    "wait for the worker to become active"
  );
  await waitForWorkerIdleShutdown(swRegInfo, remoteType);

  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    // NOTE: We intentionally trigger the navigation from content in order to
    // make sure frontend doesn't eagerly process-switch for us.
    SpecialPowers.spawn(browser, [scope], async scope => {
      content.location.href = `${scope}/intercepted.html`;
    });
    await BrowserTestUtils.browserLoaded(browser);

    is(
      countRemoteType(remoteType),
      1,
      "should have spawned a content process with the correct remoteType"
    );

    // Ensure the worker was loaded in this process.
    const workerDebuggerURLs = await SpecialPowers.spawn(
      browser,
      [sw],
      async url => {
        await content.navigator.serviceWorker.ready;
        const wdm = Cc[
          "@mozilla.org/dom/workers/workerdebuggermanager;1"
        ].getService(Ci.nsIWorkerDebuggerManager);

        return Array.from(wdm.getWorkerDebuggerEnumerator())
          .map(wd => {
            return wd.url;
          })
          .filter(swURL => swURL == url);
      }
    );
    Assert.deepEqual(
      workerDebuggerURLs,
      [sw],
      "The worker should be running in the correct child process"
    );

    // Unregister the worker.
    await SpecialPowers.spawn(browser, [], async () => {
      let registration = await content.navigator.serviceWorker.ready;
      await registration.unregister();
    });
  });

  await waitForWorkerIdleShutdown(swRegInfo, remoteType);
}

add_task(async function test() {
  await do_test_sw("example.org", "privilegedmozilla");
});
