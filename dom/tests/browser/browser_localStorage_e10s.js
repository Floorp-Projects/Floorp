const HELPER_PAGE_URL =
  "http://example.com/browser/dom/tests/browser/page_localstorage_e10s.html";
const HELPER_PAGE_ORIGIN = "http://example.com/";

let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "/helper_localStorage_e10s.js",
                                    this);

/**
 * Wait for a LocalStorage flush to occur.  This notification can occur as a
 * result of any of:
 * - The normal, hardcoded 5-second flush timer.
 * - InsertDBOp seeing a preload op for an origin with outstanding changes.
 * - Us generating a "domstorage-test-flush-force" observer notification.
 */

 /* import-globals-from helper_localStorage_e10s.js */

function waitForLocalStorageFlush() {
  if (Services.lsm.nextGenLocalStorageEnabled) {
    return new Promise(resolve => executeSoon(resolve));
  }

  return new Promise(function(resolve) {
    let observer = {
      observe() {
        SpecialPowers.removeObserver(observer, "domstorage-test-flushed");
        resolve();
      },
    };
    SpecialPowers.addObserver(observer, "domstorage-test-flushed");
  });
}

/**
 * Trigger and wait for a flush.  This is only necessary for forcing
 * mOriginsHavingData to be updated.  Normal operations exposed to content know
 * to automatically flush when necessary for correctness.
 *
 * The notification we're waiting for to verify flushing is fundamentally
 * ambiguous (see waitForLocalStorageFlush), so we actually trigger the flush
 * twice and wait twice.  In the event there was a race, there will be 3 flush
 * notifications, but correctness is guaranteed after the second notification.
 */
function triggerAndWaitForLocalStorageFlush() {
  if (Services.lsm.nextGenLocalStorageEnabled) {
    return new Promise(resolve => executeSoon(resolve));
  }

  SpecialPowers.notifyObservers(null, "domstorage-test-flush-force");
  // This first wait is ambiguous...
  return waitForLocalStorageFlush().then(function() {
    // So issue a second flush and wait for that.
    SpecialPowers.notifyObservers(null, "domstorage-test-flush-force");
    return waitForLocalStorageFlush();
  });
}

/**
 * Clear the origin's storage so that "OriginsHavingData" will return false for
 * our origin.  Note that this is only the case for AsyncClear() which is
 * explicitly issued against a cache, or AsyncClearAll() which we can trigger
 * by wiping all storage.  However, the more targeted domain clearings that
 * we can trigger via observer, AsyncClearMatchingOrigin and
 * AsyncClearMatchingOriginAttributes will not clear the hashtable entry for
 * the origin.
 *
 * So we explicitly access the cache here in the parent for the origin and issue
 * an explicit clear.  Clearing all storage might be a little easier but seems
 * like asking for intermittent failures.
 */
function clearOriginStorageEnsuringNoPreload() {
  let principal =
    Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(
      HELPER_PAGE_ORIGIN);

  if (Services.lsm.nextGenLocalStorageEnabled) {
    let request =
      Services.qms.clearStoragesForPrincipal(principal, "default", "ls");
    let promise = new Promise(resolve => {
      request.callback = () => {
        resolve();
      };
    });
    return promise;
  }

  // We want to use createStorage to force the cache to be created so we can
  // issue the clear.  It's possible for getStorage to return false but for the
  // origin preload hash to still have our origin in it.
  let storage = Services.domStorageManager.createStorage(null, principal, principal, "");
  storage.clear();

  // We also need to trigger a flush os that mOriginsHavingData gets updated.
  // The inherent flush race is fine here because
  return triggerAndWaitForLocalStorageFlush();
}

async function verifyTabPreload(knownTab, expectStorageExists) {
  let storageExists = await ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    HELPER_PAGE_ORIGIN,
    function(origin) {
      let principal =
        Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(
          origin);
      if (Services.lsm.nextGenLocalStorageEnabled) {
        return Services.lsm.isPreloaded(principal);
      }
      return !!Services.domStorageManager.getStorage(null, principal, principal);
    });
  is(storageExists, expectStorageExists, "Storage existence === preload");
}

/**
 * Instruct the given tab to execute the given series of mutations.  For
 * simplicity, the mutations representation matches the expected events rep.
 */
async function mutateTabStorage(knownTab, mutations, sentinelValue) {
  await ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    { mutations, sentinelValue },
    function(args) {
      return content.wrappedJSObject.mutateStorage(Cu.cloneInto(args, content));
    });
}

/**
 * Instruct the given tab to add a "storage" event listener and record all
 * received events.  verifyTabStorageEvents is the corresponding method to
 * check and assert the recorded events.
 */
async function recordTabStorageEvents(knownTab, sentinelValue) {
  await ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    sentinelValue,
    function(sentinelValue) {
      return content.wrappedJSObject.listenForStorageEvents(sentinelValue);
    });
}

/**
 * Retrieve the current localStorage contents perceived by the tab and assert
 * that they match the provided expected state.
 *
 * If maybeSentinel is non-null, it's assumed to be a string that identifies the
 * value we should be waiting for the sentinel key to take on.  This is
 * necessary because we cannot make any assumptions about when state will be
 * propagated to the given process.  See the comments in
 * page_localstorage_e10s.js for more context.  In general, a sentinel value is
 * required for correctness unless the process in question is the one where the
 * writes were performed or verifyTabStorageEvents was used.
 */
async function verifyTabStorageState(knownTab, expectedState, maybeSentinel) {
  let actualState = await ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    maybeSentinel,
    function(maybeSentinel) {
      return content.wrappedJSObject.getStorageState(maybeSentinel);
    });

  for (let [expectedKey, expectedValue] of Object.entries(expectedState)) {
    ok(actualState.hasOwnProperty(expectedKey), "key present: " + expectedKey);
    is(actualState[expectedKey], expectedValue, "value correct");
  }
  for (let actualKey of Object.keys(actualState)) {
    if (!expectedState.hasOwnProperty(actualKey)) {
      ok(false, "actual state has key it shouldn't have: " + actualKey);
    }
  }
}

/**
 * Retrieve and clear the storage events recorded by the tab and assert that
 * they match the provided expected events.  For simplicity, the expected events
 * representation is the same as that used by mutateTabStorage.
 *
 * Note that by convention for test readability we are passed a 3rd argument of
 * the sentinel value, but we don't actually care what it is.
 */
async function verifyTabStorageEvents(knownTab, expectedEvents) {
  let actualEvents = await ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    {},
    function() {
      return content.wrappedJSObject.returnAndClearStorageEvents();
    });

  is(actualEvents.length, expectedEvents.length, "right number of events");
  for (let i = 0; i < actualEvents.length; i++) {
    let [actualKey, actualNewValue, actualOldValue] = actualEvents[i];
    let [expectedKey, expectedNewValue, expectedOldValue] = expectedEvents[i];
    is(actualKey, expectedKey, "keys match");
    is(actualNewValue, expectedNewValue, "new values match");
    is(actualOldValue, expectedOldValue, "old values match");
  }
}

// We spin up a ton of child processes.
requestLongerTimeout(4);

/**
 * Verify the basics of our multi-e10s localStorage support.  We are focused on
 * whitebox testing two things.  When this is being written, broadcast filtering
 * is not in place, but the test is intended to attempt to verify that its
 * implementation does not break things.
 *
 * 1) That pages see the same localStorage state in a timely fashion when
 *    engaging in non-conflicting operations.  We are not testing races or
 *    conflict resolution; the spec does not cover that.
 *
 * 2) That there are no edge-cases related to when the Storage instance is
 *    created for the page or the StorageCache for the origin.  (StorageCache is
 *    what actually backs the Storage binding exposed to the page.)  This
 *    matters because the following reasons can exist for them to be created:
 *    - Preload, on the basis of knowing the origin uses localStorage.  The
 *      interesting edge case is when we have the same origin open in different
 *      processes and the origin starts using localStorage when it did not
 *      before.  Preload will not have instantiated bindings, which could impact
 *      correctness.
 *    - The page accessing localStorage for read or write purposes.  This is the
 *      obvious, boring one.
 *    - The page adding a "storage" listener.  This is less obvious and
 *      interacts with the preload edge-case mentioned above.  The page needs to
 *      hear "storage" events even if the page has not touched localStorage
 *      itself and its origin had nothing stored in localStorage when the page
 *      was created.
 *
 * We use the same simple child page in all tabs that:
 * - can be instructed to listen for and record "storage" events
 * - can be instructed to issue a series of localStorage writes
 * - can be instructed to return the current entire localStorage contents
 *
 * We open the 5 following tabs:
 * - Open a "writer" tab that does not listen for "storage" events and will
 *   issue only writes.
 * - Open a "listener" tab instructed to listen for "storage" events
 *   immediately.  We expect it to capture all events.
 * - Open an "reader" tab that does not listen for "storage" events and will
 *   only issue reads when instructed.
 * - Open a "lateWriteThenListen" tab that initially does nothing.  We will
 *   later tell it to issue a write and then listen for events to make sure it
 *   captures the later events.
 * - Open "lateOpenSeesPreload" tab after we've done everything and ensure that
 *   it preloads/precaches the data without us having touched localStorage or
 *   added an event listener.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Stop the preallocated process manager from speculatively creating
      // processes.  Our test explicitly asserts on whether preload happened or
      // not for each tab's process.  This information is loaded and latched by
      // the StorageDBParent constructor which the child process's
      // LocalStorageManager() constructor causes to be created via a call to
      // LocalStorageCache::StartDatabase().  Although the service is lazily
      // created and should not have been created prior to our opening the tab,
      // it's safest to ensure the process simply didn't exist before we ask for
      // it.
      //
      // This is done in conjunction with our use of forceNewProcess when
      // opening tabs.  There would be no point if we weren't also requesting a
      // new process.
      ["dom.ipc.processPrelaunch.enabled", false],
      // Enable LocalStorage's testing API so we can explicitly trigger a flush
      // when needed.
      ["dom.storage.testing", true],
    ],
  });

  // Ensure that there is no localstorage data or potential false positives for
  // localstorage preloads by forcing the origin to be cleared prior to the
  // start of our test.
  await clearOriginStorageEnsuringNoPreload();

  // Make sure mOriginsHavingData gets updated.
  await triggerAndWaitForLocalStorageFlush();

  // - Open tabs.  Don't configure any of them yet.
  const knownTabs = new KnownTabs();
  const writerTab = await openTestTabInOwnProcess(HELPER_PAGE_URL, "writer",
    knownTabs);
  const listenerTab = await openTestTabInOwnProcess(HELPER_PAGE_URL, "listener",
    knownTabs);
  const readerTab = await openTestTabInOwnProcess(HELPER_PAGE_URL, "reader",
    knownTabs);
  const lateWriteThenListenTab = await openTestTabInOwnProcess(HELPER_PAGE_URL,
    "lateWriteThenListen", knownTabs);

  // Sanity check that preloading did not occur in the tabs.
  await verifyTabPreload(writerTab, false);
  await verifyTabPreload(listenerTab, false);
  await verifyTabPreload(readerTab, false);

  // - Configure the tabs.
  const initialSentinel = "initial";
  const noSentinelCheck = null;
  await recordTabStorageEvents(listenerTab, initialSentinel);

  // - Issue the initial batch of writes and verify.
  info("initial writes");
  const initialWriteMutations = [
    // [key (null=clear), newValue (null=delete), oldValue (verification)]
    ["getsCleared", "1", null],
    ["alsoGetsCleared", "2", null],
    [null, null, null],
    ["stays", "3", null],
    ["clobbered", "pre", null],
    ["getsDeletedLater", "4", null],
    ["getsDeletedImmediately", "5", null],
    ["getsDeletedImmediately", null, "5"],
    ["alsoStays", "6", null],
    ["getsDeletedLater", null, "4"],
    ["clobbered", "post", "pre"],
  ];
  const initialWriteState = {
    stays: "3",
    clobbered: "post",
    alsoStays: "6",
  };

  await mutateTabStorage(writerTab, initialWriteMutations, initialSentinel);

  // We expect the writer tab to have the correct state because it just did the
  // writes.  We do not perform a sentinel-check because the writes should be
  // locally available and consistent.
  await verifyTabStorageState(writerTab, initialWriteState, noSentinelCheck);
  // We expect the listener tab to have heard all events despite preload not
  // having occurred and despite not issuing any reads or writes itself.  We
  // intentionally check the events before the state because we're most
  // interested in adding the listener having had a side-effect of subscribing
  // to changes for the process.
  //
  // We ensure it had a chance to hear all of the events because we told
  // recordTabStorageEvents to listen for the given sentinel.  The state check
  // then does not need to do a sentinel check.
  await verifyTabStorageEvents(
    listenerTab, initialWriteMutations, initialSentinel);
  await verifyTabStorageState(
    listenerTab, initialWriteState, noSentinelCheck);
  // We expect the reader tab to retrieve the current localStorage state from
  // the database.  Because of the above checks, we are confident that the
  // writes have hit PBackground and therefore that the (synchronous) state
  // retrieval contains all the data we need.  No sentinel-check is required.
  await verifyTabStorageState(readerTab, initialWriteState, noSentinelCheck);

  // - Issue second set of writes from lateWriteThenListen
  // This tests that our new tab that begins by issuing only writes is building
  // on top of the existing state (although we don't verify that until after the
  // next set of mutations).  We also verify that the initial "writerTab" that
  // was our first tab and started with only writes sees the writes, even though
  // it did not add an event listener.

  info("late writes");
  const lateWriteSentinel = "lateWrite";
  const lateWriteMutations = [
    ["lateStays", "10", null],
    ["lateClobbered", "latePre", null],
    ["lateDeleted", "11", null],
    ["lateClobbered", "lastPost", "latePre"],
    ["lateDeleted", null, "11"],
  ];
  const lateWriteState = Object.assign({}, initialWriteState, {
    lateStays: "10",
    lateClobbered: "lastPost",
  });

  await recordTabStorageEvents(listenerTab, lateWriteSentinel);

  await mutateTabStorage(
    lateWriteThenListenTab, lateWriteMutations, lateWriteSentinel);

  // Verify the writer tab saw the writes.  It has to wait for the sentinel to
  // appear before checking.
  await verifyTabStorageState(writerTab, lateWriteState, lateWriteSentinel);
  // Wait for the sentinel event before checking the events and then the state.
  await verifyTabStorageEvents(
    listenerTab, lateWriteMutations, lateWriteSentinel);
  await verifyTabStorageState(listenerTab, lateWriteState, noSentinelCheck);
  // We need to wait for the sentinel to show up for the reader.
  await verifyTabStorageState(readerTab, lateWriteState, lateWriteSentinel);

  // - Issue last set of writes from writerTab.
  info("last set of writes");
  const lastWriteSentinel = "lastWrite";
  const lastWriteMutations = [
    ["lastStays", "20", null],
    ["lastDeleted", "21", null],
    ["lastClobbered", "lastPre", null],
    ["lastClobbered", "lastPost", "lastPre"],
    ["lastDeleted", null, "21"],
  ];
  const lastWriteState = Object.assign({}, lateWriteState, {
    lastStays: "20",
    lastClobbered: "lastPost",
  });

  await recordTabStorageEvents(listenerTab, lastWriteSentinel);
  await recordTabStorageEvents(lateWriteThenListenTab, lastWriteSentinel);

  await mutateTabStorage(writerTab, lastWriteMutations, lastWriteSentinel);

  // The writer performed the writes, no need to wait for the sentinel.
  await verifyTabStorageState(writerTab, lastWriteState, noSentinelCheck);
  // Wait for the sentinel event to be received, then check.
  await verifyTabStorageEvents(
    listenerTab, lastWriteMutations, lastWriteSentinel);
  await verifyTabStorageState(listenerTab, lastWriteState, noSentinelCheck);
  // We need to wait for the sentinel to show up for the reader.
  await verifyTabStorageState(readerTab, lastWriteState, lastWriteSentinel);
  // Wait for the sentinel event to be received, then check.
  await verifyTabStorageEvents(
    lateWriteThenListenTab, lastWriteMutations, lastWriteSentinel);
  await verifyTabStorageState(
    lateWriteThenListenTab, lastWriteState, noSentinelCheck);

  // - Force a LocalStorage DB flush so mOriginsHavingData is updated.
  // mOriginsHavingData is only updated when the storage thread runs its
  // accumulated operations during the flush.  If we don't initiate and ensure
  // that a flush has occurred before moving on to the next step,
  // mOriginsHavingData may not include our origin when it's sent down to the
  // child process.
  info("flush to make preload check work");
  await triggerAndWaitForLocalStorageFlush();

  // - Open a fresh tab and make sure it sees the precache/preload
  info("late open preload check");
  const lateOpenSeesPreload = await openTestTabInOwnProcess(HELPER_PAGE_URL,
    "lateOpenSeesPreload", knownTabs);
  await verifyTabPreload(lateOpenSeesPreload, true);

  // - Clean up.
  await cleanupTabs(knownTabs);

  clearOriginStorageEnsuringNoPreload();
});
