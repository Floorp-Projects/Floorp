const HELPER_PAGE_URL =
  "http://example.com/browser/dom/tests/browser/page_localstorage_e10s.html";
const HELPER_PAGE_ORIGIN = "http://example.com/";

// Simple tab wrapper abstracting our messaging mechanism;
class KnownTab {
  constructor(name, tab) {
    this.name = name;
    this.tab = tab;
  }

  cleanup() {
    this.tab = null;
  }
}

// Simple data structure class to help us track opened tabs and their pids.
class KnownTabs {
  constructor() {
    this.byPid = new Map();
    this.byName = new Map();
  }

  cleanup() {
    this.byPid = null;
    this.byName = null;
  }
}

/**
 * Open our helper page in a tab in its own content process, asserting that it
 * really is in its own process.
 */
function* openTestTabInOwnProcess(name, knownTabs) {
  let url = HELPER_PAGE_URL + '?' + encodeURIComponent(name);
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let pid = tab.linkedBrowser.frameLoader.tabParent.osPid;
  ok(!knownTabs.byName.has(name), "tab needs its own name: " + name);
  ok(!knownTabs.byPid.has(pid), "tab needs to be in its own process: " + pid);

  let knownTab = new KnownTab(name, tab);
  knownTabs.byPid.set(pid, knownTab);
  knownTabs.byName.set(name, knownTab);
  return knownTab;
}

/**
 * Close all the tabs we opened.
 */
function* cleanupTabs(knownTabs) {
  for (let knownTab of knownTabs.byName.values()) {
    yield BrowserTestUtils.removeTab(knownTab.tab);
    knownTab.cleanup();
  }
  knownTabs.cleanup();
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
  // We want to use createStorage to force the cache to be created so we can
  // issue the clear.  It's possible for getStorage to return false but for the
  // origin preload hash to still have our origin in it.
  let storage = Services.domStorageManager.createStorage(null, principal, "");
  storage.clear();
  // We don't need to wait for anything.  The clear call will have queued the
  // clear operation on the database thread, and the child process requests
  // for origins will likewise be answered via the database thread.
}

function* verifyTabPreload(knownTab, expectStorageExists) {
  let storageExists = yield ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    HELPER_PAGE_ORIGIN,
    function(origin) {
      let principal =
        Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(
          origin);
      return !!Services.domStorageManager.getStorage(null, principal);
    });
  is(storageExists, expectStorageExists, "Storage existence === preload");
}

/**
 * Instruct the given tab to execute the given series of mutations.  For
 * simplicity, the mutations representation matches the expected events rep.
 */
function* mutateTabStorage(knownTab, mutations) {
  yield ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    { mutations },
    function(args) {
      return content.wrappedJSObject.mutateStorage(args.mutations);
    });
}

/**
 * Instruct the given tab to add a "storage" event listener and record all
 * received events.  verifyTabStorageEvents is the corresponding method to
 * check and assert the recorded events.
 */
function* recordTabStorageEvents(knownTab) {
  yield ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    {},
    function() {
      return content.wrappedJSObject.listenForStorageEvents();
    });
}

/**
 * Retrieve the current localStorage contents perceived by the tab and assert
 * that they match the provided expected state.
 */
function* verifyTabStorageState(knownTab, expectedState) {
  let actualState = yield ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    {},
    function() {
      return content.wrappedJSObject.getStorageState();
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
 */
function* verifyTabStorageEvents(knownTab, expectedEvents) {
  let actualEvents = yield ContentTask.spawn(
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
add_task(function*() {
  // - Boost process count so all of our tabs get new processes.
  // Our test wants to assert things about the precache status which is only
  // populated at process startup and never updated.  (Per analysis at
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1312022 this still makes
  // sense.)  https://bugzilla.mozilla.org/show_bug.cgi?id=1312022 introduced
  // a mechanism for keeping an arbitrary number of processes alive, modifying
  // all browser chrome tests to keep alive whatever dom.ipc.processCount is set
  // to.  The mechanism was slightly modified later to be type-based, so now
  // it's "dom.ipc.keepProcessesAlive.web" we care about.
  //
  // Our options for ensuring we get a new process are to either:
  // 1) Try and push keepalive down to 1 and kill off the processes that are
  //    already hanging around.
  // 2) Just bump the process count up enough so that every tab we open will
  //    get a new process.
  //
  // The first option turns out to be hard to get right.  Specifically,
  // although one can set the keepalive and process counts to 1 and open and
  // close tabs to try and trigger process termination down to 1, since we don't
  // know how many processes might exist, we can't reliably listen for observer
  // notifications of their shutdown to ensure we're avoiding shutdown races.
  // (If there are races then the processes won't actually be shut down.)  So
  // it's easiest to just boost the limit.
  let keepAliveCount =
    SpecialPowers.getIntPref("dom.ipc.keepProcessesAlive.web", 1);
  let safeProcessCount = keepAliveCount + 6;
  info("dom.ipc.keepProcessesAlive.web is " + keepAliveCount + ", boosting " +
       "process count temporarily to " + safeProcessCount);

  // (There's already one about:blank page open and we open 5 new tabs, so 6
  // processes.  Actually, 7, just in case.)
  yield SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processCount", safeProcessCount],
      ["dom.ipc.processCount.web", safeProcessCount]
    ]
  });

  // Ensure that there is no localstorage data or potential false positives for
  // localstorage preloads by forcing the origin to be cleared prior to the
  // start of our test.
  clearOriginStorageEnsuringNoPreload();

  // - Open tabs.  Don't configure any of them yet.
  const knownTabs = new KnownTabs();
  const writerTab = yield* openTestTabInOwnProcess("writer", knownTabs);
  const listenerTab = yield* openTestTabInOwnProcess("listener", knownTabs);
  const readerTab = yield* openTestTabInOwnProcess("reader", knownTabs);
  const lateWriteThenListenTab = yield* openTestTabInOwnProcess(
    "lateWriteThenListen", knownTabs);

  // Sanity check that preloading did not occur in the tabs.
  yield* verifyTabPreload(writerTab, false);
  yield* verifyTabPreload(listenerTab, false);
  yield* verifyTabPreload(readerTab, false);

  // - Configure the tabs.
  yield* recordTabStorageEvents(listenerTab);

  // - Issue the initial batch of writes and verify.
  const initialWriteMutations = [
    //[key (null=clear), newValue (null=delete), oldValue (verification)]
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
    ["clobbered", "post", "pre"]
  ];
  const initialWriteState = {
    stays: "3",
    clobbered: "post",
    alsoStays: "6"
  };

  yield* mutateTabStorage(writerTab, initialWriteMutations);

  yield* verifyTabStorageState(writerTab, initialWriteState);
  yield* verifyTabStorageEvents(listenerTab, initialWriteMutations);
  yield* verifyTabStorageState(listenerTab, initialWriteState);
  yield* verifyTabStorageState(readerTab, initialWriteState);

  // - Issue second set of writes from lateWriteThenListen
  const lateWriteMutations = [
    ["lateStays", "10", null],
    ["lateClobbered", "latePre", null],
    ["lateDeleted", "11", null],
    ["lateClobbered", "lastPost", "latePre"],
    ["lateDeleted", null, "11"]
  ];
  const lateWriteState = Object.assign({}, initialWriteState, {
    lateStays: "10",
    lateClobbered: "lastPost"
  });

  yield* mutateTabStorage(lateWriteThenListenTab, lateWriteMutations);
  yield* recordTabStorageEvents(lateWriteThenListenTab);

  yield* verifyTabStorageState(writerTab, lateWriteState);
  yield* verifyTabStorageEvents(listenerTab, lateWriteMutations);
  yield* verifyTabStorageState(listenerTab, lateWriteState);
  yield* verifyTabStorageState(readerTab, lateWriteState);

  // - Issue last set of writes from writerTab.
  const lastWriteMutations = [
    ["lastStays", "20", null],
    ["lastDeleted", "21", null],
    ["lastClobbered", "lastPre", null],
    ["lastClobbered", "lastPost", "lastPre"],
    ["lastDeleted", null, "21"]
  ];
  const lastWriteState = Object.assign({}, lateWriteState, {
    lastStays: "20",
    lastClobbered: "lastPost"
  });

  yield* mutateTabStorage(writerTab, lastWriteMutations);

  yield* verifyTabStorageState(writerTab, lastWriteState);
  yield* verifyTabStorageEvents(listenerTab, lastWriteMutations);
  yield* verifyTabStorageState(listenerTab, lastWriteState);
  yield* verifyTabStorageState(readerTab, lastWriteState);
  yield* verifyTabStorageEvents(lateWriteThenListenTab, lastWriteMutations);
  yield* verifyTabStorageState(lateWriteThenListenTab, lastWriteState);

  // - Open a fresh tab and make sure it sees the precache/preload
  const lateOpenSeesPreload =
    yield* openTestTabInOwnProcess("lateOpenSeesPreload", knownTabs);
  yield* verifyTabPreload(lateOpenSeesPreload, true);

  // - Clean up.
  yield* cleanupTabs(knownTabs);

  clearOriginStorageEnsuringNoPreload();
});
