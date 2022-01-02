const HELPER_PAGE_URL =
  "https://example.com/browser/dom/tests/browser/page_localstorage.html";
const HELPER_PAGE_COOP_COEP_URL =
  "https://example.com/browser/dom/tests/browser/page_localstorage_coop+coep.html";
const HELPER_PAGE_ORIGIN = "https://example.com/";

let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "/helper_localStorage.js", this);

/* import-globals-from helper_localStorage.js */

// We spin up a ton of child processes.
requestLongerTimeout(4);

/**
 * Verify the basics of our multi-e10s localStorage support with fission.
 * We are focused on whitebox testing two things.
 * When this is being written, broadcast filtering is not in place, but the test
 * is intended to attempt to verify that its implementation does not break things.
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
 * According to current fission implementation, same origin pages will be loaded
 * by the same process, which process type is webIsolated=. And thanks to
 * Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy headers support,
 * it is possible to load the same origin page by a special process, which type
 * is webCOOP+COEP=. These are the only two processes can be used to test
 * localStroage consistency between tabs in different tabs.
 *
 * We use the two child pages for testing, page_localstorage.html and
 * page_localstorage_coop+coep.html. Their content are the same, but
 * page_localstorage_coop+coep.html will be loaded with its ^headers^ file.
 * These pages provide followings
 * - can be instructed to listen for and record "storage" events
 * - can be instructed to issue a series of localStorage writes
 * - can be instructed to return the current entire localStorage contents
 *
 * To test localStorage consistency, four subtests are used.
 * Test case 1: one writer tab and one reader tab
 *   The writer tab issues a series of write operations, then verify the
 *   localStorage contents from the reader tab.
 *
 * Test case 2: one writer tab and one listener tab
 *   The writer tab issues a series of write operations, then verify the recorded
 *   storage events from the listener tab.
 *
 * Test case 3: one writeThenRead tab and one readThenWrite tab
 *   The writeThenRead first issues a series write of operations, and then verify
 *   the recorded storage events and localStorage contents from readThenWrite
 *   tab. After that readThenWrite tab issues a series of write operations, then
 *   verify the results from writeThenRead tab.
 *
 * Test case 4: one writer tab and one lateOpenSeesPreload tab
 *   The writer tab issues a series write of operations. Then open the
 *   lateOpenSeesPreload tab to make sure preloads exists.
 */

/**
 * Shared constants for test cases
 */
const noSentinelCheck = null;
const initialSentinel = "initial";
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

const lastWriteSentinel = "lastWrite";
const lastWriteMutations = [
  ["lastStays", "20", null],
  ["lastDeleted", "21", null],
  ["lastClobbered", "lastPre", null],
  ["lastClobbered", "lastPost", "lastPre"],
  ["lastDeleted", null, "21"],
];
const lastWriteState = Object.assign({}, initialWriteState, {
  lastStays: "20",
  lastClobbered: "lastPost",
});

/**
 * Test case 1: one writer tab and one reader tab
 * Test steps
 *   1. Clear origin storage to make sure no data and preloads.
 *   2. Open the writer and reader tabs and verify preloads do not exist.
 *      Open writer tab in webIsolated= process
 *      Open reader tab in webCOOP+COEP= process
 *   3. Issue a series write operations in the writer tab, and then verify the
 *      storage state on the tab.
 *   4. Verify the storage state on the reader tab.
 *   5. Issue another series write operations in the writer tab, and then verify
 *      the storage state on the tab.
 *   6. Verify the storage state on the reader tab.
 *   7. Close tabs and clear origin storage.
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
  await clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);

  // Make sure mOriginsHavingData gets updated.
  await triggerAndWaitForLocalStorageFlush();

  // - Open tabs.  Don't configure any of them yet.
  const knownTabs = new KnownTabs();
  const writerTab = await openTestTab(
    HELPER_PAGE_URL,
    "writer",
    knownTabs,
    true
  );
  const readerTab = await openTestTab(
    HELPER_PAGE_COOP_COEP_URL,
    "reader",
    knownTabs,
    true
  );
  // Sanity check that preloading did not occur in the tabs.
  await verifyTabPreload(writerTab, false, HELPER_PAGE_ORIGIN);
  await verifyTabPreload(readerTab, false, HELPER_PAGE_ORIGIN);

  // - Issue the initial batch of writes and verify.
  info("initial writes");
  await mutateTabStorage(writerTab, initialWriteMutations, initialSentinel);

  // We expect the writer tab to have the correct state because it just did the
  // writes.  We do not perform a sentinel-check because the writes should be
  // locally available and consistent.
  await verifyTabStorageState(writerTab, initialWriteState, noSentinelCheck);
  // We expect the reader tab to retrieve the current localStorage state from
  // the database.
  await verifyTabStorageState(readerTab, initialWriteState, initialSentinel);

  // - Issue last set of writes from writerTab.
  info("last set of writes");
  await mutateTabStorage(writerTab, lastWriteMutations, lastWriteSentinel);

  // The writer performed the writes, no need to wait for the sentinel.
  await verifyTabStorageState(writerTab, lastWriteState, noSentinelCheck);
  // We need to wait for the sentinel to show up for the reader.
  await verifyTabStorageState(readerTab, lastWriteState, lastWriteSentinel);

  // - Clean up.
  await cleanupTabs(knownTabs);

  clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);
});

/**
 * Test case 2: one writer tab and one linsener tab
 * Test steps
 *   1. Clear origin storage to make sure no data and preloads.
 *   2. Open the writer and listener tabs and verify preloads do not exist.
 *      Open writer tab in webIsolated= process
 *      Open listener tab in webCOOP+COEP= process
 *   3. Ask the listener tab to listen and record storage events.
 *   4. Issue a series write operations in the writer tab, and then verify the
 *      storage state on the tab.
 *   5. Verify the storage events record from the listener tab is as expected.
 *   6. Verify the storage state on the listener tab.
 *   7. Ask the listener tab to listen and record storage events.
 *   8. Issue another series write operations in the writer tab, and then verify
 *      the storage state on the tab.
 *   9. Verify the storage events record from the listener tab is as expected.
 *   10. Verify the storage state on the listener tab.
 *   11. Close tabs and clear origin storage.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processPrelaunch.enabled", false],
      ["dom.storage.testing", true],
    ],
  });

  // Ensure that there is no localstorage data or potential false positives for
  // localstorage preloads by forcing the origin to be cleared prior to the
  // start of our test.
  await clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);

  // Make sure mOriginsHavingData gets updated.
  await triggerAndWaitForLocalStorageFlush();

  // - Open tabs.  Don't configure any of them yet.
  const knownTabs = new KnownTabs();
  const writerTab = await openTestTab(
    HELPER_PAGE_URL,
    "writer",
    knownTabs,
    true
  );
  const listenerTab = await openTestTab(
    HELPER_PAGE_COOP_COEP_URL,
    "listener",
    knownTabs,
    true
  );
  // Sanity check that preloading did not occur in the tabs.
  await verifyTabPreload(writerTab, false, HELPER_PAGE_ORIGIN);
  await verifyTabPreload(listenerTab, false, HELPER_PAGE_ORIGIN);

  // - Ask the listener tab to listen and record the storage events..
  await recordTabStorageEvents(listenerTab, initialSentinel);

  // - Issue the initial batch of writes and verify.
  info("initial writes");
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
    listenerTab,
    initialWriteMutations,
    initialSentinel
  );
  await verifyTabStorageState(listenerTab, initialWriteState, noSentinelCheck);

  // - Ask the listener tab to listen and record the storage events.
  await recordTabStorageEvents(listenerTab, lastWriteSentinel);

  // - Issue last set of writes from writerTab.
  info("last set of writes");
  await mutateTabStorage(writerTab, lastWriteMutations, lastWriteSentinel);

  // The writer performed the writes, no need to wait for the sentinel.
  await verifyTabStorageState(writerTab, lastWriteState, noSentinelCheck);
  // Wait for the sentinel event to be received, then check.
  await verifyTabStorageEvents(
    listenerTab,
    lastWriteMutations,
    lastWriteSentinel
  );
  await verifyTabStorageState(listenerTab, lastWriteState, noSentinelCheck);

  // - Clean up.
  await cleanupTabs(knownTabs);

  clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);
});

/**
 * Test case 3: one writeThenRead tab and one readThenWrite tab
 * Test steps
 *   1. Clear origin storage to make sure no data and preloads.
 *   2. Open the writeThenRead and readThenWrite tabs and verify preloads do not
 *      exist.
 *      Open writeThenRead tab in webIsolated= process
 *      Open readThenWrite tab in webCOOP+COEP= process
 *   3. Ask the readThenWrite tab to listen and record storage events.
 *   4. Issue a series write operations in the writeThenRead tab, and then verify
 *      the storage state on the tab.
 *   5. Verify the storage events record from the readThenWrite tab is as
 *      expected.
 *   6. Verify the storage state on the readThenWrite tab.
 *   7. Ask the writeThenRead tab to listen and record storage events.
 *   8. Issue another series write operations in the readThenWrite tab, and then
 *      verify the storage state on the tab.
 *   9. Verify the storage events record from the writeThenRead tab is as
 *      expected.
 *   10. Verify the storage state on the writeThenRead tab.
 *   11. Close tabs and clear origin storage.
 **/
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processPrelaunch.enabled", false],
      ["dom.storage.testing", true],
    ],
  });

  // Ensure that there is no localstorage data or potential false positives for
  // localstorage preloads by forcing the origin to be cleared prior to the
  // start of our test.
  await clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);

  // Make sure mOriginsHavingData gets updated.
  await triggerAndWaitForLocalStorageFlush();

  // - Open tabs.  Don't configure any of them yet.
  const knownTabs = new KnownTabs();
  const writeThenReadTab = await openTestTab(
    HELPER_PAGE_URL,
    "writerthenread",
    knownTabs,
    true
  );
  const readThenWriteTab = await openTestTab(
    HELPER_PAGE_COOP_COEP_URL,
    "readthenwrite",
    knownTabs,
    true
  );
  // Sanity check that preloading did not occur in the tabs.
  await verifyTabPreload(writeThenReadTab, false, HELPER_PAGE_ORIGIN);
  await verifyTabPreload(readThenWriteTab, false, HELPER_PAGE_ORIGIN);

  // - Ask readThenWrite tab to listen and record storageEvents.
  await recordTabStorageEvents(readThenWriteTab, initialSentinel);

  // - Issue the initial batch of writes and verify.
  info("initial writes");
  await mutateTabStorage(
    writeThenReadTab,
    initialWriteMutations,
    initialSentinel
  );

  // We expect the writer tab to have the correct state because it just did the
  // writes.  We do not perform a sentinel-check because the writes should be
  // locally available and consistent.
  await verifyTabStorageState(
    writeThenReadTab,
    initialWriteState,
    noSentinelCheck
  );

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
    readThenWriteTab,
    initialWriteMutations,
    initialSentinel
  );
  await verifyTabStorageState(
    readThenWriteTab,
    initialWriteState,
    noSentinelCheck
  );

  // - Issue last set of writes from writerTab.
  info("last set of writes");
  await recordTabStorageEvents(writeThenReadTab, lastWriteSentinel);

  await mutateTabStorage(
    readThenWriteTab,
    lastWriteMutations,
    lastWriteSentinel
  );

  // The writer performed the writes, no need to wait for the sentinel.
  await verifyTabStorageState(
    readThenWriteTab,
    lastWriteState,
    noSentinelCheck
  );
  // Wait for the sentinel event to be received, then check.
  await verifyTabStorageEvents(
    writeThenReadTab,
    lastWriteMutations,
    lastWriteSentinel
  );
  await verifyTabStorageState(
    writeThenReadTab,
    lastWriteState,
    noSentinelCheck
  );

  // - Clean up.
  await cleanupTabs(knownTabs);

  clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);
});

/**
 * Test case 4: one writerRead tab and one lateOpenSeesPreload tab
 * Test steps
 *   1. Clear origin storage to make sure no data and preloads.
 *   2. Open the writer tab and verify preloads do not exist.
 *      Open writer tab in webIsolated= process
 *   3. Issue a series write operations in the writer tab, and then verify the
 *      storage state on the tab.
 *   4. Issue another series write operations in the writer tab, and then verify
 *      the storage state on the tab.
 *   5. Open lateOpenSeesPreload tab in webCOOP+COEP process
 *   6. Verify the preloads on the lateOpenSeesPreload tab
 *   7. Close tabs and clear origin storage.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processPrelaunch.enabled", false],
      ["dom.storage.testing", true],
    ],
  });

  // Ensure that there is no localstorage data or potential false positives for
  // localstorage preloads by forcing the origin to be cleared prior to the
  // start of our test.
  await clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);

  // Make sure mOriginsHavingData gets updated.
  await triggerAndWaitForLocalStorageFlush();

  // - Open tabs.  Don't configure any of them yet.
  const knownTabs = new KnownTabs();
  const writerTab = await openTestTab(
    HELPER_PAGE_URL,
    "writer",
    knownTabs,
    true
  );
  // Sanity check that preloading did not occur in the tabs.
  await verifyTabPreload(writerTab, false, HELPER_PAGE_ORIGIN);

  // - Configure the tabs.

  // - Issue the initial batch of writes and verify.
  info("initial writes");
  await mutateTabStorage(writerTab, initialWriteMutations, initialSentinel);

  // We expect the writer tab to have the correct state because it just did the
  // writes.  We do not perform a sentinel-check because the writes should be
  // locally available and consistent.
  await verifyTabStorageState(writerTab, initialWriteState, noSentinelCheck);

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
  const lateOpenSeesPreload = await openTestTab(
    HELPER_PAGE_COOP_COEP_URL,
    "lateOpenSeesPreload",
    knownTabs,
    true
  );
  await verifyTabPreload(lateOpenSeesPreload, true, HELPER_PAGE_ORIGIN);

  // - Clean up.
  await cleanupTabs(knownTabs);

  clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);
});
