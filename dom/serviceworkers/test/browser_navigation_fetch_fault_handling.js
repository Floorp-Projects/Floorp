/**
 * This test file tests our automatic recovery and any related mitigating
 * heuristics that occur during intercepted navigation fetch request.
 * Specifically, we should be resetting interception so that we go to the
 * network in these cases and then potentially taking actions like unregistering
 * the ServiceWorker and/or clearing QuotaManager-managed storage for the
 * origin.
 *
 * See specific test permutations for specific details inline in the test.
 *
 * NOTE THAT CURRENTLY THIS TEST IS DISCUSSING MITIGATIONS THAT ARE NOT YET
 * IMPLEMENTED, JUST PLANNED.  These will be iterated on and added to the rest
 * of the stack of patches on Bug 1503072.
 *
 * ## Test Mechanics
 *
 * ### Fetch Fault Injection
 *
 * We expose:
 * - On nsIServiceWorkerInfo, the per-ServiceWorker XPCOM interface:
 *   - A mechanism for creating synthetic faults by setting the
 *     `nsIServiceWorkerInfo::testingInjectCancellation` attribute to a failing
 *     nsresult.  The fault is applied at the beginning of the steps to dispatch
 *     the fetch event on the global.
 *   - A count of the number of times we experienced these navigation faults
 *     that had to be reset as `nsIServiceWorkerInfo::navigationFaultCount`.
 *     (This would also include real faults, but we only expect to see synthetic
 *     faults in this test.)
 * - On nsIServiceWorkerRegistrationInfo, the per-registration XPCOM interface:
 *   - A readonly attribute that indicates how many times an origin storage
 *     usage check has been initiated.
 *
 * We also use:
 * - `nsIServiceWorkerManager::addListener(nsIServiceWorkerManagerListener)`
 *   allows our test to listen for the unregistration of registrations.  This
 *   allows us to be notified when unregistering or origin-clearing actions have
 *   been taken as a mitigation.
 *
 * ### General Test Approach
 *
 * For each test we:
 * - Ensure/confirm the testing origin has no QuotaManager storage in use.
 * - Install the ServiceWorker.
 * - If we are testing the situation where we want to simulate the origin being
 *   near its quota limit, we also generate Cache API and IDB storage usage
 *   sufficient to put our origin over the threshold.
 *   - We run a quota check on the origin after doing this in order to make sure
 *     that we did this correctly and that we properly constrained the limit for
 *     the origin.  We fail the test for test implementation reasons if we
 *     didn't accomplish this.
 * - Verify a fetch navigation to the SW works without any fault injection,
 *   producing a result produced by the ServiceWorker.
 * - Begin fault permutations in a loop, where for each pass of the loop:
 *   - We trigger a navigation which will result in an intercepted fetch
 *     which will fault.  We wait until the navigation completes.
 *   - We verify that we got the request from the network.
 *   - We verify that the ServiceWorker's navigationFaultCount increased.
 *   - If this the count at which we expect a mitigation to take place, we wait
 *     for the registration to become unregistered AND:
 *     - We check whether the storage for the origin was cleared or not, which
 *       indicates which mitigation of the following happened:
 *       - Unregister the registration directly.
 *       - Clear the origin's data which will also unregister the registration
 *         as a side effect.
 *     - We check whether the registration indicates an origin quota check
 *       happened or not.
 *
 * ### Disk Usage Limits
 *
 * In order to avoid gratuitous disk I/O and related overheads, we limit QM
 * ("temporary") storage to 10 MiB which ends up limiting group usage to 10 MiB.
 * This lets us set a threshold situation where we claim that a SW needs at
 * least 4 MiB of storage for installation/operation, meaning that any usage
 * beyond 6 MiB in the group will constitute a need to clear the group or
 * origin.  We fill with the storage with 8 MiB of artificial usage to this end,
 * storing 4 MiB in Cache API and 4 MiB in IDB.
 **/

// Because of the amount of I/O involved in this test, pernosco reproductions
// may experience timeouts without a timeout multiplier.
requestLongerTimeout(2);

/* import-globals-from browser_head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/serviceworkers/test/browser_head.js",
  this
);

// The origin we run the tests on.
const TEST_ORIGIN = "https://test1.example.org";
// An origin in the same group that impacts the usage of the TEST_ORIGIN.  Used
// to verify heuristics related to group-clearing (where clearing the
// TEST_ORIGIN itself would not be sufficient for us to mitigate quota limits
// being reached.)
const SAME_GROUP_ORIGIN = "https://test2.example.org";

const TEST_SW_SETUP = {
  origin: TEST_ORIGIN,
  // Page with a body textContent of "NETWORK" and has utils.js loaded.
  scope: "network_with_utils.html",
  // SW that serves a body with a textContent of "SERVICEWORKER" and
  // has utils.js loaded.
  script: "sw_respondwith_serviceworker.js",
};

const TEST_STORAGE_SETUP = {
  cacheBytes: 4 * 1024 * 1024, // 4 MiB
  idbBytes: 4 * 1024 * 1024, // 4 MiB
};

const FAULTS_BEFORE_MITIGATION = 3;

/**
 * Core test iteration logic.
 *
 * Parameters:
 * - name: Human readable name of the fault we're injecting.
 * - useError: The nsresult failure code to inject into fetch.
 * - errorPage: The "about" page that we expect errors to leave us on.
 * - consumeQuotaOrigin: If truthy, the origin to place the storage usage in.
 *   If falsey, we won't fill storage.
 */
async function do_fault_injection_test({
  name,
  useError,
  errorPage,
  consumeQuotaOrigin,
}) {
  info(
    `### testing: error: ${name} (${useError}) consumeQuotaOrigin: ${consumeQuotaOrigin}`
  );

  // ## Ensure/confirm the testing origins have no QuotaManager storage in use.
  await clear_qm_origin_group_via_clearData(TEST_ORIGIN);

  // ## Install the ServiceWorker
  const reg = await install_sw(TEST_SW_SETUP);
  const sw = reg.activeWorker;

  // ## Generate quota usage if appropriate
  if (consumeQuotaOrigin) {
    await consume_storage(consumeQuotaOrigin, TEST_STORAGE_SETUP);
  }

  // ## Verify normal navigation is served by the SW.
  info(`## Checking normal operation.`);
  {
    const debugTag = `err=${name}&fault=0`;
    const docInfo = await navigate_and_get_body(TEST_SW_SETUP, debugTag);
    is(
      docInfo.body,
      "SERVICEWORKER",
      "navigation without injected fault originates from ServiceWorker"
    );

    is(
      docInfo.controlled,
      true,
      "successfully intercepted navigation should be controlled"
    );
  }

  // Make sure the test is listening on the ServiceWorker unregistration, since
  // we expect it happens after navigation fault threshold reached.
  const unregisteredPromise = waitForUnregister(reg.scope);

  // Make sure the test is listening on the finish of quota checking, since we
  // expect it happens after navigation fault threshold reached.
  const quotaUsageCheckFinishPromise = waitForQuotaUsageCheckFinish(reg.scope);

  // ## Inject faults in a loop until expected mitigation.
  sw.testingInjectCancellation = useError;
  for (let iFault = 0; iFault < FAULTS_BEFORE_MITIGATION; iFault++) {
    info(`## Testing with injected fault number ${iFault + 1}`);
    // We should never have triggered an origin quota usage check before the
    // final fault injection.
    is(reg.quotaUsageCheckCount, 0, "No quota usage check yet");

    // Make sure our loads encode the specific
    const debugTag = `err=${name}&fault=${iFault + 1}`;

    const docInfo = await navigate_and_get_body(TEST_SW_SETUP, debugTag);
    // We should always be receiving network fallback.
    is(
      docInfo.body,
      "NETWORK",
      "navigation with injected fault originates from network"
    );

    is(docInfo.controlled, false, "bypassed pages shouldn't be controlled");

    // The fault count should have increased
    is(
      sw.navigationFaultCount,
      iFault + 1,
      "navigation fault increased (to expected value)"
    );
  }

  await unregisteredPromise;
  is(reg.unregistered, true, "registration should be unregistered");

  //is(reg.quotaUsageCheckCount, 1, "Quota usage check must be started");
  await quotaUsageCheckFinishPromise;

  if (consumeQuotaOrigin) {
    // Check that there is no longer any storage usaged by the origin in this
    // case.
    const originUsage = await get_qm_origin_usage(TEST_ORIGIN);
    ok(
      is_minimum_origin_usage(originUsage),
      "origin usage should be mitigated"
    );

    if (consumeQuotaOrigin === SAME_GROUP_ORIGIN) {
      const sameGroupUsage = await get_qm_origin_usage(SAME_GROUP_ORIGIN);
      Assert.strictEqual(
        sameGroupUsage,
        0,
        "same group usage should be mitigated"
      );
    }
  }
}

add_task(async function test_navigation_fetch_fault_handling() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.exemptFromPerDomainMax", true],
      ["dom.serviceWorkers.testing.enabled", true],
      ["dom.serviceWorkers.mitigations.bypass_on_fault", true],
      ["dom.serviceWorkers.mitigations.group_usage_headroom_kb", 5 * 1024],
      ["dom.quotaManager.testing", true],
      // We want the temporary global limit to be 10 MiB (the pref is in KiB).
      // This will result in the group limit also being 10 MiB because on small
      // disks we provide a group limit value of min(10 MiB, global limit).
      ["dom.quotaManager.temporaryStorage.fixedLimit", 10 * 1024],
    ],
  });

  // Need to reset the storages to make dom.quotaManager.temporaryStorage.fixedLimit
  // works.
  await qm_reset_storage();

  const quotaOriginVariations = [
    // Don't put us near the storage limit.
    undefined,
    // Put us near the storage limit in the SW origin itself.
    TEST_ORIGIN,
    // Put us near the storage limit in the SW origin's group but not the origin
    // itself.
    SAME_GROUP_ORIGIN,
  ];

  for (const consumeQuotaOrigin of quotaOriginVariations) {
    await do_fault_injection_test({
      name: "NS_ERROR_DOM_ABORT_ERR",
      useError: 0x80530014, // Not in `Cr`.
      // Abort errors manifest as about:blank pages.
      errorPage: "about:blank",
      consumeQuotaOrigin,
    });

    await do_fault_injection_test({
      name: "NS_ERROR_INTERCEPTION_FAILED",
      useError: 0x804b0064, // Not in `Cr`.
      // Interception failures manifest as corrupt content pages.
      errorPage: "about:neterror",
      consumeQuotaOrigin,
    });
  }

  // Cleanup: wipe the origin and group so all the ServiceWorkers go away.
  await clear_qm_origin_group_via_clearData(TEST_ORIGIN);
});
