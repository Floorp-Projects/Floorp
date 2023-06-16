add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", true],
      ["browser.tabs.remote.separatedMozillaDomains", "example.org"],
      ["dom.ipc.processCount.web", 1],
      ["dom.ipc.processCount.privilegedmozilla", 1],
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
  });
});

// This test attempts to verify proper placement of spawned remoteworkers
// by spawning them and then verifying that they were spawned in the expected
// process by way of nsIWorkerDebuggerManager enumeration.
//
// Unfortunately, there's no other way to introspect where a worker global was
// spawned at this time. (devtools just ends up enumerating all workers in all
// processes and we don't want to depend on devtools in this test).
//
// As a result, this test currently only tests situations where it's known that
// a remote worker will be spawned in the same process that is initiating its
// spawning.
//
// This should be enhanced in the future.
add_task(async function test_serviceworker() {
  const basePath = "browser/dom/workers/test";
  const pagePath = `${basePath}/file_service_worker_container.html`;
  const scriptPath = `${basePath}/file_service_worker.js`;

  Services.ppmm.releaseCachedProcesses();

  async function runWorkerInProcess() {
    function getActiveWorkerURLs() {
      const wdm = Cc[
        "@mozilla.org/dom/workers/workerdebuggermanager;1"
      ].getService(Ci.nsIWorkerDebuggerManager);

      const workerDebuggerUrls = Array.from(
        wdm.getWorkerDebuggerEnumerator()
      ).map(wd => {
        return wd.url;
      });

      return workerDebuggerUrls;
    }

    return new Promise(resolve => {
      content.navigator.serviceWorker.ready.then(({ active }) => {
        const { port1, port2 } = new content.MessageChannel();
        active.postMessage("webpage->serviceworker", [port2]);
        port1.onmessage = evt => {
          resolve({
            msg: evt.data,
            workerUrls: getActiveWorkerURLs(),
          });
        };
      });
    }).then(async res => {
      // Unregister the service worker used in this test.
      const registration = await content.navigator.serviceWorker.ready;
      await registration.unregister();
      return res;
    });
  }

  const testCaseList = [
    // TODO: find a reasonable way to test the non-privileged scenario
    // (because more than 1 process is usually available and the worker
    // can be launched in a different one from the one where the tab
    // is running).
    /*{
      remoteType: "web",
      hostname: "example.com",
    },*/
    {
      remoteType: "privilegedmozilla",
      hostname: `example.org`,
    },
  ];

  for (const testCase of testCaseList) {
    const { remoteType, hostname } = testCase;

    info(`Test remote serviceworkers launch selects a ${remoteType} process`);

    const tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: `https://${hostname}/${pagePath}`,
    });

    is(
      tab.linkedBrowser.remoteType,
      remoteType,
      `Got the expected remoteType for ${hostname} tab`
    );

    const results = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [],
      runWorkerInProcess
    );

    Assert.deepEqual(
      results,
      {
        msg: "serviceworker-reply",
        workerUrls: [`https://${hostname}/${scriptPath}`],
      },
      `Got the expected results for ${hostname} tab`
    );

    BrowserTestUtils.removeTab(tab);
  }
});
