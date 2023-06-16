const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);

/**
 * We choose blob contents that will roundtrip cleanly through the `textContent`
 * of our returned HTML page.
 */
const TEST_BLOB_CONTENTS = `I'm a disk-backed test blob! Hooray!`;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Set preferences so that opening a page with the origin "example.org"
      // will result in a remoteType of "privilegedmozilla" for both the
      // page and the ServiceWorker.
      ["browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", true],
      ["browser.tabs.remote.separatedMozillaDomains", "example.org"],
      ["dom.ipc.processCount.privilegedmozilla", 1],
      ["dom.ipc.processPrelaunch.enabled", false],
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
      // ServiceWorker worker instances should stay alive until explicitly
      // caused to terminate by dropping these timeouts to 0 in
      // `waitForWorkerAndProcessShutdown`.
      ["dom.serviceWorkers.idle_timeout", 299999],
      ["dom.serviceWorkers.idle_extended_timeout", 299999],
    ],
  });
});

function countRemoteType(remoteType) {
  return ChromeUtils.getAllDOMProcesses().filter(
    p => p.remoteType == remoteType
  ).length;
}

/**
 * Helper function to get a list of all current processes and their remote
 * types.  Note that when in used in a templated literal that it is
 * synchronously invoked when the string is evaluated and captures system state
 * at that instant.
 */
function debugRemotes() {
  return ChromeUtils.getAllDOMProcesses()
    .map(p => p.remoteType || "parent")
    .join(",");
}

/**
 * Wait for there to be zero processes of the given remoteType.  This check is
 * considered successful if there are already no processes of the given type
 * at this very moment.
 */
async function waitForNoProcessesOfType(remoteType) {
  info(`waiting for there to be no ${remoteType} procs`);
  await TestUtils.waitForCondition(
    () => countRemoteType(remoteType) == 0,
    "wait for the worker's process to shutdown"
  );
}

/**
 * Given a ServiceWorkerRegistrationInfo with an active ServiceWorker that
 * has no active ExtendableEvents but would otherwise continue running thanks
 * to the idle keepalive:
 * - Assert that there is a ServiceWorker instance in the given registration's
 *   active slot.  (General invariant check.)
 * - Assert that a single process with the given remoteType currently exists.
 *   (This doesn't mean the SW is alive in that process, though this test
 *   verifies that via other checks when appropriate.)
 * - Induce the worker to shutdown by temporarily dropping the idle timeout to 0
 *   and causing the idle timer to be reset due to rapid debugger attach/detach.
 * - Wait for the the single process with the given remoteType to go away.
 * - Reset the idle timeouts back to their previous high values.
 */
async function waitForWorkerAndProcessShutdown(swRegInfo, remoteType) {
  info(`terminating worker and waiting for ${remoteType} procs to shut down`);
  ok(swRegInfo.activeWorker, "worker should be in the active slot");
  is(
    countRemoteType(remoteType),
    1,
    `should have a single ${remoteType} process but have: ${debugRemotes()}`
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
  await waitForNoProcessesOfType(remoteType);

  is(
    countRemoteType(remoteType),
    0,
    `processes with remoteType=${remoteType} type should have shut down`
  );

  // Make sure we never kill workers on idle except when this is called.
  await SpecialPowers.popPrefEnv();
}

async function do_test_sw(host, remoteType, swMode, fileBlob) {
  info(
    `### entering test: host=${host}, remoteType=${remoteType}, mode=${swMode}`
  );

  const prin = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(`https://${host}`),
    {}
  );
  const sw = `https://${host}/${DIRPATH}file_service_worker_fetch_synthetic.js`;
  const scope = `https://${host}/${DIRPATH}server_fetch_synthetic.sjs`;

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
  await waitForWorkerAndProcessShutdown(swRegInfo, remoteType);

  info(
    `test navigation interception with mode=${swMode} starting from about:blank`
  );
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    async browser => {
      // NOTE: We intentionally trigger the navigation from content in order to
      // make sure frontend doesn't eagerly process-switch for us.
      SpecialPowers.spawn(
        browser,
        [scope, swMode, fileBlob],
        // eslint-disable-next-line no-shadow
        async (scope, swMode, fileBlob) => {
          const pageUrl = `${scope}?mode=${swMode}`;
          if (!fileBlob) {
            content.location.href = pageUrl;
          } else {
            const doc = content.document;
            const formElem = doc.createElement("form");
            doc.body.appendChild(formElem);

            formElem.action = pageUrl;
            formElem.method = "POST";
            formElem.enctype = "multipart/form-data";

            const fileElem = doc.createElement("input");
            formElem.appendChild(fileElem);

            fileElem.type = "file";
            fileElem.name = "foo";

            fileElem.mozSetFileArray([fileBlob]);

            formElem.submit();
          }
        }
      );

      await BrowserTestUtils.browserLoaded(browser);

      is(
        countRemoteType(remoteType),
        1,
        `should have spawned a content process with remoteType=${remoteType}`
      );

      const { source, blobContents } = await SpecialPowers.spawn(
        browser,
        [],
        () => {
          return {
            source: content.document.getElementById("source").textContent,
            blobContents: content.document.getElementById("blob").textContent,
          };
        }
      );

      is(
        source,
        swMode === "synthetic" ? "ServiceWorker" : "ServerJS",
        "The page contents should come from the right place."
      );

      is(
        blobContents,
        fileBlob ? TEST_BLOB_CONTENTS : "",
        "The request blob contents should be the blob/empty as appropriate."
      );

      // Ensure the worker was loaded in this process.
      const workerDebuggerURLs = await SpecialPowers.spawn(
        browser,
        [sw],
        async url => {
          if (!content.navigator.serviceWorker.controller) {
            throw new Error("document not controlled!");
          }
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
      if (remoteType.startsWith("webServiceWorker=")) {
        Assert.notDeepEqual(
          workerDebuggerURLs,
          [sw],
          "Isolated workers should not be running in the content child process"
        );
      } else {
        Assert.deepEqual(
          workerDebuggerURLs,
          [sw],
          "The worker should be running in the correct child process"
        );
      }

      // Unregister the ServiceWorker.  The registration will continue to control
      // `browser` and therefore continue to exist and its worker to continue
      // running until the tab is closed.
      await SpecialPowers.spawn(browser, [], async () => {
        let registration = await content.navigator.serviceWorker.ready;
        await registration.unregister();
      });
    }
  );

  // Now that the controlled tab is closed and the registration has been
  // removed, the ServiceWorker will be made redundant which will forcibly
  // terminate it, which will result in the shutdown of the given content
  // process.  Wait for that to happen both as a verification and so the next
  // test has a sufficiently clean slate.
  await waitForNoProcessesOfType(remoteType);
}

/**
 * Create a File-backed blob.  This will happen synchronously from the main
 * thread, which isn't optimal, but the test blocks on this progress anyways.
 * Bug 1669578 has been filed on improving this idiom and avoiding the sync
 * writes.
 */
async function makeFileBlob(blobContents) {
  const tmpFile = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIDirectoryService)
    .QueryInterface(Ci.nsIProperties)
    .get("TmpD", Ci.nsIFile);
  tmpFile.append("test-file-backed-blob.txt");
  tmpFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  var outStream = Cc[
    "@mozilla.org/network/file-output-stream;1"
  ].createInstance(Ci.nsIFileOutputStream);
  outStream.init(
    tmpFile,
    0x02 | 0x08 | 0x20, // write, create, truncate
    0o666,
    0
  );
  outStream.write(blobContents, blobContents.length);
  outStream.close();

  const fileBlob = await File.createFromNsIFile(tmpFile);
  return fileBlob;
}

function getSWTelemetrySums() {
  let telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(
    Ci.nsITelemetry
  );
  let keyedhistograms = telemetry.getSnapshotForKeyedHistograms(
    "main",
    false
  ).parent;
  let keyedscalars = telemetry.getSnapshotForKeyedScalars("main", false).parent;
  // We're not looking at the distribution of the histograms, just that they changed
  return {
    SERVICE_WORKER_RUNNING_All: keyedhistograms.SERVICE_WORKER_RUNNING
      ? keyedhistograms.SERVICE_WORKER_RUNNING.All.sum
      : 0,
    SERVICE_WORKER_RUNNING_Fetch: keyedhistograms.SERVICE_WORKER_RUNNING
      ? keyedhistograms.SERVICE_WORKER_RUNNING.Fetch.sum
      : 0,
    SERVICEWORKER_RUNNING_MAX_All: keyedscalars["serviceworker.running_max"]
      ? keyedscalars["serviceworker.running_max"].All
      : 0,
    SERVICEWORKER_RUNNING_MAX_Fetch: keyedscalars["serviceworker.running_max"]
      ? keyedscalars["serviceworker.running_max"].Fetch
      : 0,
  };
}

add_task(async function test() {
  // Can't test telemetry without this since we may not be on the nightly channel
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  registerCleanupFunction(() => {
    Services.telemetry.canRecordExtended = oldCanRecord;
  });

  let initialSums = getSWTelemetrySums();

  // ## Isolated Privileged Process
  // Trigger a straightforward intercepted navigation with no request body that
  // returns a synthetic response.
  await do_test_sw("example.org", "privilegedmozilla", "synthetic", null);

  // Trigger an intercepted navigation with FormData containing an
  // <input type="file"> which will result in the request body containing a
  // RemoteLazyInputStream which will be consumed in the content process by the
  // ServiceWorker while generating the synthetic response.
  const fileBlob = await makeFileBlob(TEST_BLOB_CONTENTS);
  await do_test_sw("example.org", "privilegedmozilla", "synthetic", fileBlob);

  // Trigger an intercepted navigation with FormData containing an
  // <input type="file"> which will result in the request body containing a
  // RemoteLazyInputStream which will be relayed back to the parent process
  // via direct invocation of fetch() on the event.request but without any
  // cloning.
  await do_test_sw("example.org", "privilegedmozilla", "fetch", fileBlob);

  // Same as the above but cloning the request before fetching it.
  await do_test_sw("example.org", "privilegedmozilla", "clone", fileBlob);

  // ## Fission Isolation
  if (Services.appinfo.fissionAutostart) {
    // ## ServiceWorker isolation
    const isolateUrl = "example.com";
    const isolateRemoteType = `webServiceWorker=https://` + isolateUrl;
    await do_test_sw(isolateUrl, isolateRemoteType, "synthetic", null);
    await do_test_sw(isolateUrl, isolateRemoteType, "synthetic", fileBlob);
  }
  let telemetrySums = getSWTelemetrySums();
  info(JSON.stringify(telemetrySums));
  info(
    "Initial Running All: " +
      initialSums.SERVICE_WORKER_RUNNING_All +
      ", Fetch: " +
      initialSums.SERVICE_WORKER_RUNNING_Fetch
  );
  info(
    "Initial Max Running All: " +
      initialSums.SERVICEWORKER_RUNNING_MAX_All +
      ", Fetch: " +
      initialSums.SERVICEWORKER_RUNNING_MAX_Fetch
  );
  info(
    "Running All: " +
      telemetrySums.SERVICE_WORKER_RUNNING_All +
      ", Fetch: " +
      telemetrySums.SERVICE_WORKER_RUNNING_Fetch
  );
  info(
    "Max Running All: " +
      telemetrySums.SERVICEWORKER_RUNNING_MAX_All +
      ", Fetch: " +
      telemetrySums.SERVICEWORKER_RUNNING_MAX_Fetch
  );
  ok(
    telemetrySums.SERVICE_WORKER_RUNNING_All >
      initialSums.SERVICE_WORKER_RUNNING_All,
    "ServiceWorker running count changed"
  );
  ok(
    telemetrySums.SERVICE_WORKER_RUNNING_Fetch >
      initialSums.SERVICE_WORKER_RUNNING_Fetch,
    "ServiceWorker running count changed"
  );
  // We don't use ok()'s for MAX because MAX may have been set before we
  // set canRecordExtended, and if so we won't record a new value unless
  // the max increases again.
});
