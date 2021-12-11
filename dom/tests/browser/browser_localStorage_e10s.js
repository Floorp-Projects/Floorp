const HELPER_PAGE_URL =
  "http://example.com/browser/dom/tests/browser/page_localstorage.html";
const HELPER_PAGE_ORIGIN = "http://example.com/";

let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "/helper_localStorage.js", this);

/* import-globals-from helper_localStorage.js */

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
    HELPER_PAGE_URL,
    "listener",
    knownTabs,
    true
  );
  const readerTab = await openTestTab(
    HELPER_PAGE_URL,
    "reader",
    knownTabs,
    true
  );
  const lateWriteThenListenTab = await openTestTab(
    HELPER_PAGE_URL,
    "lateWriteThenListen",
    knownTabs,
    true
  );

  // Sanity check that preloading did not occur in the tabs.
  await verifyTabPreload(writerTab, false, HELPER_PAGE_ORIGIN);
  await verifyTabPreload(listenerTab, false, HELPER_PAGE_ORIGIN);
  await verifyTabPreload(readerTab, false, HELPER_PAGE_ORIGIN);

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
    listenerTab,
    initialWriteMutations,
    initialSentinel
  );
  await verifyTabStorageState(listenerTab, initialWriteState, noSentinelCheck);
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
    lateWriteThenListenTab,
    lateWriteMutations,
    lateWriteSentinel
  );

  // Verify the writer tab saw the writes.  It has to wait for the sentinel to
  // appear before checking.
  await verifyTabStorageState(writerTab, lateWriteState, lateWriteSentinel);
  // Wait for the sentinel event before checking the events and then the state.
  await verifyTabStorageEvents(
    listenerTab,
    lateWriteMutations,
    lateWriteSentinel
  );
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
    listenerTab,
    lastWriteMutations,
    lastWriteSentinel
  );
  await verifyTabStorageState(listenerTab, lastWriteState, noSentinelCheck);
  // We need to wait for the sentinel to show up for the reader.
  await verifyTabStorageState(readerTab, lastWriteState, lastWriteSentinel);
  // Wait for the sentinel event to be received, then check.
  await verifyTabStorageEvents(
    lateWriteThenListenTab,
    lastWriteMutations,
    lastWriteSentinel
  );
  await verifyTabStorageState(
    lateWriteThenListenTab,
    lastWriteState,
    noSentinelCheck
  );

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
    HELPER_PAGE_URL,
    "lateOpenSeesPreload",
    knownTabs,
    true
  );
  await verifyTabPreload(lateOpenSeesPreload, true, HELPER_PAGE_ORIGIN);

  // - Clean up.
  await cleanupTabs(knownTabs);

  clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);
});
